//
// kernel.cpp
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//
// Parts of the USB, frame buffer and sound handling are based on BMC64's
// kernel.cpp, and those parts remain under the Apache License 2.0
// (sdcard/licenses/Apache-2.0.txt).
// Gamepad names and individual buttons use Circle's usb/usbgamepad.h and
// device.h interfaces.
//

#include "kernel.h"
#include "gamepad_profiles.h"
#include "logging.h"
#include "network.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <circle/bcmpropertytags.h>
#include <circle/bcmwatchdog.h>
#include <circle/machineinfo.h>
#include <circle/memio.h>
#if RASPPI >= 5
#include <circle/bcm2712.h>
#endif
#include <circle/startup.h>
#include <circle/synchronize.h>
#include <circle/usb/usbhid.h>
#include <circle_glue.h>

extern "C" {
// From src/port/platform.c: the emulator's entry point, which runs on core
// 1, and its Reboot / Shut Down request.
void bma800xl_main(void);
void bma_request_power(int power_off);
}

static const char FromKernel[] = "bma800xl";

// Sample rate the VideoCore plays audio at.
#define HW_SAMPLE_RATE 44100

// HID usage of the first modifier key (left control); the eight modifier
// bits map to usages 0xE0..0xE7.
#define USAGE_FIRST_MODIFIER 0xE0

// GPIO joystick wiring, the same as BMC64's "GPIO Config 1" (in BMC64's
// code that is config_0_joystickPins; its config_1 is the keyboard PCB).
// Pins are active low with pull ups. Order: up, down, left, right, fire.
static const unsigned kJoyGpio[2][5] = {
    {17, 18, 27, 22, 23}, // Atari joystick port 1
    {5, 6, 12, 13, 19},   // Atari joystick port 2
};

CKernel *CKernel::s_pThis = 0;

// Core 1 counts up s_Core1Beat while it makes progress and names what it is
// doing in s_Core1Stage, so core 0 can log where it got stuck if it stops.
static volatile unsigned s_Core1Beat = 0;
static const char *volatile s_Core1Stage = "starting";

static inline void Core1Beat(const char *stage) {
  s_Core1Stage = stage;
  s_Core1Beat = s_Core1Beat + 1;
}

#ifdef ARM_ALLOW_MULTI_CORE

//
// CEmulatorCore
//

CEmulatorCore::CEmulatorCore(CMemorySystem *pMemorySystem)
    : CMultiCoreSupport(pMemorySystem), m_bLaunch(false) {}

void CEmulatorCore::Launch(void) {
  DataSyncBarrier();
  m_bLaunch = true;
  DataSyncBarrier();
}

void CEmulatorCore::Run(unsigned nCore) {
  if (nCore == 1) {
    while (!m_bLaunch) {
      DataMemBarrier();
    }
    bma800xl_main();
  }
#if RASPPI >= 4
  // Cores 2 and 3 help draw smoothed and CRT frames (src/av/video.h).
  if (nCore >= 2) {
    Pi4Video::WorkerLoop(nCore);
  }
#endif

  // Nothing else runs here.
  for (;;) {
    asm volatile("wfi");
  }
}

#else

//
// CUSBPlugAndPlayTask
//

void CUSBPlugAndPlayTask::Run(void) {
  for (;;) {
    CKernel::Get()->UpdateUSBPlugAndPlay();
    CScheduler::Get()->MsSleep(100);
  }
}

#endif

//
// CKernel
//

CKernel::CKernel(void)
    :
#if RASPPI >= 5
      // Circle's default on the Pi 5 is the 3-pin debug connector, which
      // the Pi 500 lacks; device 0 is GPIO14/15, as on the other boards.
      mSerial(0, FALSE, 0),
#endif
      mLog(&mSerial), mTimer(&mInterrupt), mLogger(mKernelOptions.GetLogLevel(), &mTimer),
      mGPIOManager(&mInterrupt),
#if RASPPI <= 3
      mVCHIQ(CMemorySystem::Get(), &mInterrupt),
#endif
      mEMMC(&mInterrupt, &mTimer, &mActLED),
      mUSBHCI(&mInterrupt, &mTimer, TRUE), mConsole(&mSerial, &mLog),
#ifdef ARM_ALLOW_MULTI_CORE
      mEmulatorCore(CMemorySystem::Get()),
#endif
#if RASPPI <= 3
      mSound(0), mSoundChannels(0), mVolume(VCHIQ_SOUND_VOLUME_DEFAULT),
      mAudioOut(VCHIQSoundDestinationAuto),
#endif
      m_MergedModifiers(0),
      m_KeyQueueHead(0), m_KeyQueueTail(0), m_bGPIOJoysticks(false) {
  s_pThis = this;
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    m_pKeyboard[i] = 0;
    m_KeyboardIndex[i] = i;
    m_KeyboardModifiers[i] = 0;
    memset(m_KeyboardKeys[i], 0, sizeof m_KeyboardKeys[i]);
  }
  memset(m_KeyDown, 0, sizeof m_KeyDown);
  for (unsigned i = 0; i < HOST_PADS; i++) {
    m_pGamePad[i] = 0;
    m_PadState[i] = -1;
    memset(&m_PadInfo[i], 0, sizeof m_PadInfo[i]);
    m_PadButtons[i] = 0;
    memset(&m_PadInput[i], 0, sizeof m_PadInput[i]);
    m_PadAxisX[i] = 0;
    m_PadAxisY[i] = 1;
  }
  for (unsigned i = 0; i < MAX_MICE; i++) {
    m_pMouse[i] = 0;
    m_MouseIndex[i] = i;
    m_MouseButtons[i] = 0;
  }
  memset(m_pJoyPins, 0, sizeof m_pJoyPins);

#if RASPPI <= 3
  // The emulator's display is the lowest dispmanx layer.
  mFrameBuffer.SetLayer(0);
  mFrameBuffer.SetTransparency(false);
#endif
}

CKernel::~CKernel(void) {}

bool CKernel::Initialize(void) {
  if (!mSerial.Initialize(115200)) {
    return false;
  }
  if (!mInterrupt.Initialize()) {
    return false;
  }
  if (!mLogger.Initialize(&mLog)) {
    return false;
  }
  mLogger.RegisterPanicHandler(PanicHandler);
  mLogger.Write(FromKernel, LogNotice, "BMA800XL " BMA800XL_VERSION " (" BMA800XL_GIT
                ", atari800 " A8_VERSION ")");

  if (!mTimer.Initialize()) {
    return false;
  }
  if (!mGPIOManager.Initialize()) {
    return false;
  }
#if RASPPI <= 3
  if (!mVCHIQ.Initialize()) {
    return false;
  }
#endif
  if (!mEMMC.Initialize()) {
    return false;
  }
  if (f_mount(&mFileSystem, "SD:", 1) != FR_OK) {
    mLogger.Write(FromKernel, LogError, "Cannot mount the SD card");
    return false;
  }
  // Settings saved by the BMA800XL menu win over cmdline.txt.
  mOptions.LoadSettings("SD:/bma800xl.cfg");

  // Until here the log went to the serial port. No setting means no log.
  const char *logging = mOptions.Get("enable_logging", "off");
  if (strcmp(logging, "file") == 0) {
    SetLogDestination(HOST_LOG_FILE);
  } else if (strcmp(logging, "uart") != 0) {
    SetLogDestination(HOST_LOG_OFF);
  }
  if (!mConsole.Initialize()) {
    return false;
  }
  CGlueStdioInit(mConsole);
  mLog.Flush();

  // Before USB, as BMC64 does: the Pi 3's Ethernet is a USB device and
  // joins the network stack once USB finds it.
  NetworkInitialize(&mOptions);
  mLog.Flush();

  if (!mUSBHCI.Initialize()) {
    return false;
  }
  mLog.Flush();

  const char *audio_out = mOptions.Get("audio_out", "auto");

  // Volume in percent (0..100) of full scale.
  int volume = mOptions.GetInt("volume", 100);
  if (volume < 0) {
    volume = 0;
  } else if (volume > 100) {
    volume = 100;
  }

#if RASPPI >= 4
  // Display and sound are set up here, on core 0: Circle's sound drivers
  // must be started on core 0 (see src/av/sound.h).
  Pi4Sound::Output sound_output = Pi4Sound::Output::Auto;
  if (strcmp(audio_out, "hdmi") == 0) {
    sound_output = Pi4Sound::Output::Hdmi;
  } else if (strcmp(audio_out, "analog") == 0 || strcmp(audio_out, "headphones") == 0) {
    sound_output = Pi4Sound::Output::Headphones;
  }
  // video_driver: kms (the default), hvs or firmware (docs/PI4.md).
  const char *video_driver = mOptions.Get("video_driver", "kms");
  Pi4Video::Driver driver = Pi4Video::Driver::Kms;
  if (strcmp(video_driver, "hvs") == 0) {
    driver = Pi4Video::Driver::Hvs;
  } else if (strcmp(video_driver, "firmware") == 0) {
    driver = Pi4Video::Driver::Firmware;
  }
  if (!mVideo.Initialize(mOptions.GetBool("crt_shader", false),
                         mOptions.GetBool("smooth", false),
                         mOptions.GetBool("crt_curvature", false), driver)) {
    return false;
  }
  mLog.Flush();
  // Without sound the emulator still runs.
  mPi4Sound.Initialize(&mInterrupt, volume, sound_output);
  mLog.Flush();
#else
  if (strcmp(audio_out, "hdmi") == 0) {
    mAudioOut = VCHIQSoundDestinationHDMI;
  } else if (strcmp(audio_out, "analog") == 0 ||
             strcmp(audio_out, "headphones") == 0) {
    mAudioOut = VCHIQSoundDestinationHeadphones;
  }
  // VCHIQ volume is in 1/100 dB; map 0..100% onto -60..0 dB.
  mVolume = volume == 0 ? VCHIQ_SOUND_VOLUME_MIN : (volume - 100) * 60;

  FrameBufferLayer::Initialize();
#endif

  if (mOptions.GetBool("gpio_joysticks", false)) {
    SetupGPIOJoysticks();
  }

#ifdef ARM_ALLOW_MULTI_CORE
  bool ok = mEmulatorCore.Initialize();
#else
  bool ok = true;
#endif
  mLog.Flush();
  return ok;
}

void CKernel::UpdateUSBPlugAndPlay(void) {
  if (mUSBHCI.UpdatePlugAndPlay()) {
    SetupUSBKeyboards();
    SetupUSBGamePads();
    SetupUSBMice();
  }
}

TShutdownMode CKernel::Run(void) {
  SetupUSBKeyboards();
  SetupUSBGamePads();
  SetupUSBMice();
  mLog.Flush();
  mStarting = false;

#ifndef ARM_ALLOW_MULTI_CORE
  // One core (the Pi Zero): the emulator runs here and yields once a frame,
  // which lets the USB task and Circle's other tasks run.
  new CUSBPlugAndPlayTask;
  bma800xl_main();
  return ShutdownHalt;
#else
  mEmulatorCore.Launch();

#if RASPPI >= 4
  // The 3D GPU draws the smooth and CRT pictures instead of the ARM (unless
  // crt_gpu=0 in cmdline.txt; crt_gpu_test=1 also checks its pictures
  // against the ARM's). Started here, with the emulator running on core 1,
  // which also writes the log.
  if (mOptions.GetBool("crt_gpu", true) && mGpu.Initialize() && mGpu.ClearTest() &&
      mRenderer.Initialize(&mGpu, 4096, 2160) &&
      mCrt.Initialize(&mGpu, &mRenderer, 384, 288)) {
    bool ok = true;
    if (mOptions.GetBool("crt_gpu_test", false)) {
      ok = mRenderer.CopyTest() && mCrt.Test();
    }
    if (ok) {
      mVideo.UseGpu(&mCrt);
    }
  }
#endif

  // Core 0 only looks after USB devices coming and going, and watches that
  // the emulator on core 1 keeps going. Interrupts keep being handled here.
  unsigned last_beat = 0;
  unsigned last_change = CTimer::GetClockTicks();
  bool stalled = false;
  for (;;) {
    UpdateUSBPlugAndPlay();

    unsigned beat = s_Core1Beat;
    unsigned now = CTimer::GetClockTicks();
    if (beat != last_beat) {
      if (stalled) {
        mLogger.Write(FromKernel, LogNotice, "Emulator running again after %u ms",
                      (now - last_change) / (CLOCKHZ / 1000));
        stalled = false;
      }
      last_beat = beat;
      last_change = now;
    } else if (beat != 0 && !stalled && now - last_change > 3 * CLOCKHZ) {
      mLogger.Write(FromKernel, LogWarning,
                    "Emulator has not moved for 3 s; last seen: %s",
                    (const char *)s_Core1Stage);
      stalled = true;
      // Core 1 writes the log file; while it is stuck (other than in the
      // file system), core 0 writes it, so the log shows where.
      if (strcmp((const char *)s_Core1Stage, "writing the log file") != 0) {
        mLog.Flush();
      }
    }
#if RASPPI >= 5
    CheckPowerButton();
#endif

    asm volatile("wfi");
  }

  return ShutdownHalt;
#endif
}

#if RASPPI >= 5
// The Pi 5's power button, and the Pi 500's power key, as Shut Down: GPIO
// 20 of the BCM2712's own GPIO block ("gio", Circle's ARM_GPIO1_BASE),
// low while pressed, held for 50 ms (the device trees' debounce time). It
// counts only once it has been seen released.
// Sources: Raspberry Pi Linux device trees, bcm2712-rpi-5-b.dts
// (pwr_button: gpio-keys, KEY_POWER, <&gio 20 GPIO_ACTIVE_LOW>,
// debounce-interval 50, pull-up) and bcm2712-rpi-500.dts (the same key);
// bcm2712.dtsi (gio at 0x7D508500); Circle, include/circle/bcm2712.h
// (ARM_GPIO1_DATA0, the first bank's levels).
void CKernel::CheckPowerButton(void) {
  const u32 Button = 1u << 20;
  bool down = (read32(ARM_GPIO1_DATA0) & Button) == 0;
  unsigned now = CTimer::GetClockTicks();
  if (!down) {
    m_bPowerButtonSeenUp = true;
    m_nPowerButtonDown = 0;
    return;
  }
  if (!m_bPowerButtonSeenUp) {
    return;
  }
  if (m_nPowerButtonDown == 0) {
    m_nPowerButtonDown = now | 1;
  } else if (now - m_nPowerButtonDown >= CLOCKHZ / 20) {
    mLogger.Write(FromKernel, LogNotice, "Power button: Shut Down");
    bma_request_power(1);
    m_bPowerButtonSeenUp = false;
  }
}
#endif

//
// USB keyboards
//

void CKernel::SetupUSBKeyboards(void) {
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    if (m_pKeyboard[i] != 0) {
      continue;
    }
    char name[16];
    snprintf(name, sizeof name, "ukbd%u", i + 1);
    m_pKeyboard[i] =
        (CUSBKeyboardDevice *)mDeviceNameService.GetDevice(name, FALSE);
    if (m_pKeyboard[i] != 0) {
      m_pKeyboard[i]->RegisterRemovedHandler(KeyboardRemovedHandler,
                                             &m_KeyboardIndex[i]);
      m_pKeyboard[i]->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw, FALSE,
                                                  &m_KeyboardIndex[i]);
      mLogger.Write(FromKernel, LogNotice, "Keyboard %u connected", i + 1);
    }
  }
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pContext;
  if (k == 0 || i >= MAX_KEYBOARDS) {
    return;
  }
  // Release everything that keyboard was holding down.
  static const unsigned char no_keys[6] = {0, 0, 0, 0, 0, 0};
  KeyStatusHandlerRaw(0, no_keys, pContext);
  k->m_pKeyboard[i] = 0;
}

void CKernel::PushKeyEvent(int usage, int pressed) {
  unsigned head = m_KeyQueueHead;
  unsigned next = (head + 1) % KEY_QUEUE_SIZE;
  if (next == m_KeyQueueTail) {
    return; // Full: the emulator is not reading. Drop the event.
  }
  m_KeyQueue[head] = (unsigned short)(usage | (pressed ? 0x100 : 0));
  DataMemBarrier();
  m_KeyQueueHead = next;
}

// Recomputes which keys are down across all keyboards and queues an event for
// every key whose state changed.
void CKernel::UpdateMergedKeys(void) {
  unsigned char down[256];
  unsigned char modifiers = 0;
  memset(down, 0, sizeof down);
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    modifiers |= m_KeyboardModifiers[i];
    for (unsigned j = 0; j < 6; j++) {
      unsigned char key = m_KeyboardKeys[i][j];
      if (key > 3) { // 0 none, 1..3 are error codes
        down[key] = 1;
      }
    }
  }
  for (unsigned bit = 0; bit < 8; bit++) {
    down[USAGE_FIRST_MODIFIER + bit] = (modifiers >> bit) & 1;
  }

  for (unsigned usage = 4; usage < 256; usage++) {
    if (down[usage] != m_KeyDown[usage]) {
      m_KeyDown[usage] = down[usage];
      PushKeyEvent(usage, down[usage]);
    }
  }
  m_MergedModifiers = modifiers;
}

// USB interrupt context, core 0.
void CKernel::KeyStatusHandlerRaw(unsigned char ucModifiers,
                                  const unsigned char RawKeys[6], void *pArg) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pArg;
  if (k == 0 || i >= MAX_KEYBOARDS) {
    return;
  }
  // Too many keys at once: the report carries no key information.
  for (unsigned j = 0; j < 6; j++) {
    if (RawKeys[j] == 0x01) {
      return;
    }
  }
  k->m_KeyboardModifiers[i] = ucModifiers;
  memcpy(k->m_KeyboardKeys[i], RawKeys, 6);
  k->UpdateMergedKeys();
}

int CKernel::KeyEvent(int *usage, int *pressed) {
  Core1Beat("reading keys");
  unsigned tail = m_KeyQueueTail;
  if (tail == m_KeyQueueHead) {
    return 0;
  }
  DataMemBarrier();
  unsigned short event = m_KeyQueue[tail];
  m_KeyQueueTail = (tail + 1) % KEY_QUEUE_SIZE;
  *usage = event & 0xff;
  *pressed = (event & 0x100) != 0;
  return 1;
}

int CKernel::KeyDown(int usage) {
  return usage >= 0 && usage < 256 ? m_KeyDown[usage] : 0;
}

unsigned CKernel::KeyModifiers(void) { return m_MergedModifiers; }

//
// USB gamepads
//

// Axis ranges and availability follow Circle's usbgamepad.h reports.
static void ReadPadAxes(const TGamePadState *state, bool known,
                        struct host_pad_input *input) {
  memset(input, 0, sizeof *input);
  input->has_dpad = known || state->nhats > 0;
  for (int axis = 0; axis < state->naxes && axis < HOST_PAD_AXES; axis++) {
    input->axes[axis].value = state->axes[axis].value;
    input->axes[axis].minimum = state->axes[axis].minimum;
    input->axes[axis].maximum = state->axes[axis].maximum;
    if (state->axes[axis].maximum > state->axes[axis].minimum)
      input->axis_mask |= 1u << axis;
  }
}

void CKernel::SetupUSBGamePads(void) {
  for (unsigned i = 0; i < HOST_PADS; i++) {
    if (m_pGamePad[i] != 0) {
      continue;
    }
    char name[16];
    snprintf(name, sizeof name, "upad%u", i + 1);
    m_pGamePad[i] =
        (CUSBGamePadDevice *)mDeviceNameService.GetDevice(name, FALSE);
    if (m_pGamePad[i] != 0) {
      CString *vendor = m_pGamePad[i]->GetDevice()->GetName(DeviceNameVendor);
      const char *product = m_pGamePad[i]->GetProperty(CDevice::PropertyProduct);
      if (product == 0) {
        product = "USB gamepad";
      }
      bool known = (m_pGamePad[i]->GetProperties() & GamePadPropertyIsKnown) != 0;
      const char *usb_id = vendor != 0 ? (const char *)*vendor : "";
      const bma_gamepad_profile *profile = BMA_GamepadProfile(usb_id, known);
      m_PadAxisX[i] = profile->axis_x;
      m_PadAxisY[i] = profile->axis_y;
      const TGamePadState *initial = m_pGamePad[i]->GetInitialState();
      unsigned count = initial->nbuttons > 0 ? initial->nbuttons : 0;
      uint32_t mask = count >= 32 ? UINT32_MAX : (UINT32_C(1) << count) - 1;
      struct host_pad_input input;
      ReadPadAxes(initial, known, &input);
      for (unsigned axis = 0; axis < HOST_PAD_AXES; axis++)
        input.axes[axis].value = ((int64_t)input.axes[axis].minimum
                                 + input.axes[axis].maximum) / 2;
      m_Lock.Acquire();
      snprintf(m_PadInfo[i].name, sizeof m_PadInfo[i].name, "%s",
               profile->name != 0 ? profile->name : product);
      snprintf(m_PadInfo[i].usb_id, sizeof m_PadInfo[i].usb_id, "%s", usb_id);
      m_PadInfo[i].known = known;
      m_PadInfo[i].button_mask = known ? 0xfff : host_pad_generic_buttons(mask);
      m_PadInfo[i].generation++;
      m_PadButtons[i] = 0;
      m_PadInput[i] = input;
      m_PadInfo[i].axis_mask = input.axis_mask;
      m_PadInfo[i].has_dpad = input.has_dpad;
      m_PadState[i] = 0;
      m_Lock.Release();
      m_pGamePad[i]->RegisterRemovedHandler(GamePadRemovedHandler);
      m_pGamePad[i]->RegisterStatusHandler(GamePadStatusHandler);
      mLogger.Write(FromKernel, LogNotice,
                    "Gamepad %u connected: %s (%s)%s, axes %d/%d", i + 1,
                    product, vendor != 0 ? (const char *)*vendor : "?",
                    (m_pGamePad[i]->GetProperties() & GamePadPropertyIsKnown)
                        ? "" : " (generic)",
                    m_PadAxisX[i], m_PadAxisY[i]);
      delete vendor;
    }
  }
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext) {
  CKernel *k = s_pThis;
  if (k == 0) {
    return;
  }
  for (unsigned i = 0; i < HOST_PADS; i++) {
    if (k->m_pGamePad[i] == pDevice) {
      k->m_pGamePad[i] = 0;
      k->m_Lock.Acquire();
      k->m_PadState[i] = -1;
      k->m_PadButtons[i] = 0;
      k->m_Lock.Release();
    }
  }
}

// Direction of an axis: -1, 0 or 1, with a dead zone of half the range.
static int AxisDirection(const TGamePadState *pState, int axis) {
  if (axis >= pState->naxes) {
    return 0;
  }
  int min = pState->axes[axis].minimum;
  int max = pState->axes[axis].maximum;
  if (max <= min) {
    return 0;
  }
  int centre = (min + max) / 2;
  int threshold = (max - min) / 4;
  int value = pState->axes[axis].value;
  if (value < centre - threshold) {
    return -1;
  }
  if (value > centre + threshold) {
    return 1;
  }
  return 0;
}

// USB interrupt context, core 0.
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex,
                                   const TGamePadState *pState) {
  CKernel *k = s_pThis;
  if (k == 0 || nDeviceIndex >= HOST_PADS || k->m_pGamePad[nDeviceIndex] == 0) {
    return;
  }

  int state = 0;
  unsigned b = pState->buttons;
  bool known = (k->m_pGamePad[nDeviceIndex]->GetProperties() &
                GamePadPropertyIsKnown) != 0;

  if (known) {
    if (b & GamePadButtonA) state |= HOST_PAD_A;
    if (b & GamePadButtonB) state |= HOST_PAD_B;
    if (b & GamePadButtonX) state |= HOST_PAD_X;
    if (b & GamePadButtonY) state |= HOST_PAD_Y;
    if (b & (GamePadButtonLB | GamePadButtonLT)) state |= HOST_PAD_L;
    if (b & (GamePadButtonRB | GamePadButtonRT)) state |= HOST_PAD_R;
    if (b & (GamePadButtonSelect | GamePadButtonMinus)) state |= HOST_PAD_SELECT;
    if (b & (GamePadButtonStart | GamePadButtonPlus)) state |= HOST_PAD_START;
    if (b & GamePadButtonGuide) state |= HOST_PAD_HOME;
    if (b & GamePadButtonUp) state |= HOST_PAD_UP;
    if (b & GamePadButtonDown) state |= HOST_PAD_DOWN;
    if (b & GamePadButtonLeft) state |= HOST_PAD_LEFT;
    if (b & GamePadButtonRight) state |= HOST_PAD_RIGHT;
  } else {
    // Unknown pads report buttons in their own order. This layout suits most
    // cheap USB pads: 0..3 face buttons, 4/5 shoulders, 8 select, 9 start.
    if (b & (1u << 0)) state |= HOST_PAD_A;
    if (b & (1u << 1)) state |= HOST_PAD_B;
    if (b & (1u << 2)) state |= HOST_PAD_X;
    if (b & (1u << 3)) state |= HOST_PAD_Y;
    if (b & ((1u << 4) | (1u << 6))) state |= HOST_PAD_L;
    if (b & ((1u << 5) | (1u << 7))) state |= HOST_PAD_R;
    if (b & (1u << 8)) state |= HOST_PAD_SELECT;
    if (b & (1u << 9)) state |= HOST_PAD_START;
    if (b & (1u << 12)) state |= HOST_PAD_HOME;
  }

  // Hats: 0 is up, counting clockwise in 45 degree steps; anything else
  // is centred.
  if (pState->nhats > 0) {
    switch (pState->hats[0]) {
    case 0: state |= HOST_PAD_UP; break;
    case 1: state |= HOST_PAD_UP | HOST_PAD_RIGHT; break;
    case 2: state |= HOST_PAD_RIGHT; break;
    case 3: state |= HOST_PAD_DOWN | HOST_PAD_RIGHT; break;
    case 4: state |= HOST_PAD_DOWN; break;
    case 5: state |= HOST_PAD_DOWN | HOST_PAD_LEFT; break;
    case 6: state |= HOST_PAD_LEFT; break;
    case 7: state |= HOST_PAD_UP | HOST_PAD_LEFT; break;
    default: break;
    }
  }

  // The left stick (axes 0 and 1 on most unknown pads), or the D-pad on
  // pads that report it as axes (gamepad_profiles.c).
  struct host_pad_input input;
  ReadPadAxes(pState, known, &input);
  input.dpad = (state >> 12) & 0x0f;
  int x = AxisDirection(pState, known ? (int)GamePadAxisLeftX
                                      : k->m_PadAxisX[nDeviceIndex]);
  int y = AxisDirection(pState, known ? (int)GamePadAxisLeftY
                                      : k->m_PadAxisY[nDeviceIndex]);
  if (x < 0) state |= HOST_PAD_LEFT;
  if (x > 0) state |= HOST_PAD_RIGHT;
  if (y < 0) state |= HOST_PAD_UP;
  if (y > 0) state |= HOST_PAD_DOWN;

  uint32_t buttons;
  if (known) {
    buttons = state & 0xff;
    buttons &= ~(HOST_PAD_L | HOST_PAD_R);
    if (b & GamePadButtonLB) buttons |= 1u << 4;
    if (b & GamePadButtonRB) buttons |= 1u << 5;
    if (b & GamePadButtonLT) buttons |= 1u << 8;
    if (b & GamePadButtonRT) buttons |= 1u << 9;
    if (b & GamePadButtonL3) buttons |= 1u << 10;
    if (b & GamePadButtonR3) buttons |= 1u << 11;
    if (b & GamePadButtonTouchpad) buttons |= 1u << 13;
  } else {
    buttons = host_pad_generic_buttons(b);
  }
  k->m_Lock.Acquire();
  k->m_PadState[nDeviceIndex] = state;
  k->m_PadButtons[nDeviceIndex] = buttons;
  k->m_PadInfo[nDeviceIndex].button_mask |= buttons;
  k->m_PadInput[nDeviceIndex] = input;
  k->m_PadInfo[nDeviceIndex].axis_mask = input.axis_mask;
  k->m_PadInfo[nDeviceIndex].has_dpad = input.has_dpad;
  k->m_Lock.Release();
}

int CKernel::PadState(int pad) {
  if (pad < 0 || pad >= HOST_PADS) {
    return -1;
  }
  return m_PadState[pad];
}

int CKernel::PadInfo(int pad, struct host_pad_info *info) {
  if (pad < 0 || pad >= HOST_PADS) {
    return 0;
  }
  m_Lock.Acquire();
  int connected = m_PadState[pad] >= 0;
  if (connected) {
    *info = m_PadInfo[pad];
  }
  m_Lock.Release();
  return connected;
}

uint32_t CKernel::PadButtons(int pad) {
  if (pad < 0 || pad >= HOST_PADS) {
    return 0;
  }
  m_Lock.Acquire();
  uint32_t buttons = m_PadButtons[pad];
  m_Lock.Release();
  return buttons;
}

int CKernel::PadInput(int pad, struct host_pad_input *input) {
  if (pad < 0 || pad >= HOST_PADS)
    return 0;
  m_Lock.Acquire();
  int connected = m_PadState[pad] >= 0;
  if (connected)
    *input = m_PadInput[pad];
  m_Lock.Release();
  return connected;
}

//
// USB mice
//

// The emulator sees the mice where they were this long ago (longer than
// the time between reports), and one report's movement covers at most
// kMouseMaxSpan. In clock ticks (microseconds).
static const unsigned kMouseDelay = 20000;
static const unsigned kMouseMaxSpan = 32000;

void CKernel::SetupUSBMice(void) {
  for (unsigned i = 0; i < MAX_MICE; i++) {
    if (m_pMouse[i] != 0) {
      continue;
    }
    char name[16];
    snprintf(name, sizeof name, "mouse%u", i + 1);
    CMouseDevice *mouse =
        (CMouseDevice *)mDeviceNameService.GetDevice(name, FALSE);
    if (mouse != 0) {
      mouse->RegisterRemovedHandler(MouseRemovedHandler, &m_MouseIndex[i]);
      mouse->RegisterStatusHandler(MouseStatusHandler, &m_MouseIndex[i]);
      m_pMouse[i] = mouse;
      mLogger.Write(FromKernel, LogNotice, "Mouse %u connected", i + 1);
    }
  }
}

void CKernel::MouseRemovedHandler(CDevice *pDevice, void *pContext) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pContext;
  if (k == 0 || i >= MAX_MICE) {
    return;
  }
  k->m_Lock.Acquire();
  k->m_pMouse[i] = 0;
  k->m_MouseButtons[i] = 0;
  k->m_Lock.Release();
}

// USB interrupt context, core 0.
void CKernel::MouseStatusHandler(unsigned nButtons, int nDisplacementX,
                                 int nDisplacementY, int nWheelMove,
                                 void *pArg) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pArg;
  if (k == 0 || i >= MAX_MICE) {
    return;
  }
  unsigned buttons = 0;
  if (nButtons & MOUSE_BUTTON_LEFT) buttons |= HOST_MOUSE_LEFT;
  if (nButtons & MOUSE_BUTTON_RIGHT) buttons |= HOST_MOUSE_RIGHT;
  if (nButtons & MOUSE_BUTTON_MIDDLE) buttons |= HOST_MOUSE_MIDDLE;
  unsigned now = CTimer::GetClockTicks();
  k->m_Lock.Acquire();
  k->m_MouseButtons[i] = buttons;
  if (nDisplacementX != 0 || nDisplacementY != 0) {
    // The movement covers the time since the last report, or at most
    // kMouseMaxSpan when the mouse was resting.
    unsigned start = k->m_MouseLastReport;
    if (now - start > kMouseMaxSpan) {
      start = now - kMouseMaxSpan;
    }
    k->m_MouseX += nDisplacementX;
    k->m_MouseY += nDisplacementY;
    TMouseReport &r = k->m_MouseHistory[k->m_MouseReports % MOUSE_HISTORY];
    r.start = start;
    r.end = now;
    r.x = k->m_MouseX;
    r.y = k->m_MouseY;
    r.dx = nDisplacementX;
    r.dy = nDisplacementY;
    k->m_MouseReports++;
  }
  k->m_MouseLastReport = now;
  k->m_Lock.Release();
}

// Core 1, once a frame: the movement since the last call, taken from the
// mice's position kMouseDelay ago, so a frame gets the same share of a
// steady movement whether one report or two arrived during it.
unsigned CKernel::Mouse(int *dx, int *dy) {
  unsigned buttons = 0;
  unsigned when = CTimer::GetClockTicks() - kMouseDelay;
  m_Lock.Acquire();
  int x = m_MouseReadX, y = m_MouseReadY;
  unsigned kept = m_MouseReports < MOUSE_HISTORY ? m_MouseReports : MOUSE_HISTORY;
  for (unsigned n = 1; n <= kept; n++) {
    const TMouseReport &r = m_MouseHistory[(m_MouseReports - n) % MOUSE_HISTORY];
    if ((int)(when - r.start) < 0) {
      // Started after WHEN: the position is from before it.
      x = r.x - r.dx;
      y = r.y - r.dy;
      continue;
    }
    if ((int)(when - r.end) >= 0 || r.end == r.start) {
      x = r.x;
      y = r.y;
    } else {
      // Part way through: take back the part still to come.
      int left = (int)(r.end - when);
      int span = (int)(r.end - r.start);
      x = r.x - r.dx * left / span;
      y = r.y - r.dy * left / span;
    }
    break;
  }
  *dx = x - m_MouseReadX;
  *dy = y - m_MouseReadY;
  m_MouseReadX = x;
  m_MouseReadY = y;
  for (unsigned i = 0; i < MAX_MICE; i++) {
    buttons |= m_MouseButtons[i];
  }
  m_Lock.Release();
  return buttons;
}

//
// GPIO joysticks
//

void CKernel::SetupGPIOJoysticks(void) {
  for (unsigned port = 0; port < 2; port++) {
    for (unsigned i = 0; i < 5; i++) {
      m_pJoyPins[port][i] = new CGPIOPin(kJoyGpio[port][i],
                                         GPIOModeInputPullUp, &mGPIOManager);
    }
  }
  m_bGPIOJoysticks = true;
  mLogger.Write(FromKernel, LogNotice, "GPIO joysticks enabled");
}

int CKernel::GpioJoystick(int port) {
  if (!m_bGPIOJoysticks || port < 0 || port > 1) {
    return 0;
  }
  static const int bits[5] = {HOST_JOY_UP, HOST_JOY_DOWN, HOST_JOY_LEFT,
                              HOST_JOY_RIGHT, HOST_JOY_FIRE};
  int state = 0;
  for (unsigned i = 0; i < 5; i++) {
    if (m_pJoyPins[port][i]->Read() == LOW) {
      state |= bits[i];
    }
  }
  return state;
}

//
// Video
//

#if RASPPI >= 4

int CKernel::VideoInit(int width, int height, uint8_t **pixels, int *pitch) {
  return mVideo.Allocate(width, height, pixels, pitch);
}

void CKernel::VideoSetView(int x, int y, int w, int h, double aspect) {
  mVideo.SetView(x, y, w, h, aspect, mOptions.GetDouble("scale", 1.0));
}

void CKernel::VideoSetPalette(int index, uint32_t rgb) {
  mVideo.SetPalette(index, rgb);
}

void CKernel::DisplaySize(int *width, int *height) {
  mVideo.DisplaySize(width, height);
}

void CKernel::VideoUpdatePalette(void) { mVideo.UpdatePalette(); }

bool CKernel::DisplayMatchRate(double fps) {
  Core1Beat("setting the video mode");
  // The Pi 5's firmware ignores hdmi_mode: the size is chosen here.
  unsigned lines = 0;
  const char *resolution = mOptions.Get("resolution", "auto");
  if (strcmp(resolution, "1920x1080") == 0) {
    lines = 1080;
  } else if (strcmp(resolution, "1280x720") == 0) {
    lines = 720;
  }
  bool changed = mVideo.MatchRate(fps, Pi4Sound::SAMPLE_RATE, lines);
  Core1Beat("emulating");
  return changed;
}

bool CKernel::DisplayResolutionChoice(void) { return mVideo.ResolutionChoice(); }

void CKernel::VideoPresent(int wait_vsync) {
  // Let Circle's tasks (the network, the web UI) run once per frame.
  Core1Beat("running Circle tasks");
  CScheduler::Get()->Yield();
  Core1Beat("writing the log file");
  mLog.Drain();
  Core1Beat("showing a frame (framebuffer)");
  mVideo.Present(wait_vsync != 0);
  Core1Beat("emulating");
}

//
// Sound
//

unsigned CKernel::SoundOpen(unsigned freq, int channels,
                            unsigned latency_frames) {
  (void)freq; // HDMI runs at Pi4Sound::SAMPLE_RATE.
  return mPi4Sound.Open(channels, latency_frames);
}

void CKernel::SoundClose(void) {
  // HDMI sound keeps running (silent when nothing is written); it can only
  // be started on core 0.
}

unsigned CKernel::SoundFreeFrames(void) { return mPi4Sound.FreeFrames(); }

void CKernel::SoundWrite(const int16_t *samples, unsigned frames) {
  Core1Beat("writing sound (HDMI)");
  mPi4Sound.Write(samples, frames);
  Core1Beat("emulating");
}

#else

int CKernel::VideoInit(int width, int height, uint8_t **pixels, int *pitch) {
  if (mOptions.GetBool("crt_shader", false)) {
    mFrameBuffer.SetUsesShader(true);
  }
  // The firmware's own scaling kernel is soft, so sharp pixels need the
  // nearest neighbour kernel set explicitly (as BMC64 does).
  FrameBufferLayer::SetInterpolation(mOptions.GetBool("smooth", false) ? 1 : 0);
  int ret = mFrameBuffer.Allocate(0 /* 8 bit indexed */, pixels, width,
                                  height, pitch);
  if (ret != 0) {
    mLogger.Write(FromKernel, LogError, "Cannot allocate frame buffer");
    return ret;
  }
  if (mFrameBuffer.UsesShader()) {
    // BMC64's default CRT look: scanlines and fake gamma, optional curvature.
    mFrameBuffer.SetShaderParams(
        mOptions.GetBool("crt_curvature", false), 0.10f, 0.15f, 0 /* mask */,
        0.70f, true /* gamma */, true /* fake gamma */, true /* scanlines */,
        true /* multisample */, 6.0f, 0.12f, 1.5f, 2.4f, 2.2f,
        false /* sharper */, mOptions.GetBool("smooth", false));
  }
  for (int i = 0; i < 256; i++) {
    mFrameBuffer.SetPalette(i, (uint16_t)0);
  }
  mFrameBuffer.UpdatePalette();
  return 0;
}

void CKernel::VideoSetView(int x, int y, int w, int h, double aspect) {
  bool showing = mFrameBuffer.Showing();
  if (showing) {
    mFrameBuffer.Hide();
  }
  mFrameBuffer.SetSrcRect(x, y, w, h);
  // Fill the display's height (times "scale"), with the width following
  // the aspect ratio.
  double scale = mOptions.GetDouble("scale", 1.0);
  if (scale <= 0.1 || scale > 1.0) {
    scale = 1.0;
  }
  mFrameBuffer.SetStretch(aspect, scale, 0, 0, 0, 0);
  mFrameBuffer.Show();
}

void CKernel::VideoSetPalette(int index, uint32_t rgb) {
  unsigned r = (rgb >> 16) & 0xff;
  unsigned g = (rgb >> 8) & 0xff;
  unsigned b = rgb & 0xff;
  uint16_t rgb565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  mFrameBuffer.SetPalette((uint8_t)index, rgb565);
}

void CKernel::DisplaySize(int *width, int *height) {
  int fb_w, fb_h, src_w, src_h, dst_w, dst_h;
  mFrameBuffer.GetDimensions(width, height, &fb_w, &fb_h, &src_w, &src_h,
                             &dst_w, &dst_h);
}

void CKernel::VideoUpdatePalette(void) { mFrameBuffer.UpdatePalette(); }

bool CKernel::DisplayMatchRate(double fps) {
  (void)fps; // The mode is the firmware's (config.txt).
  return false;
}

bool CKernel::DisplayResolutionChoice(void) { return false; }

void CKernel::VideoPresent(int wait_vsync) {
  // Let Circle's tasks (VCHIQ in particular) run once per frame.
  Core1Beat("running Circle tasks");
  CScheduler::Get()->Yield();
  Core1Beat("writing the log file");
  mLog.Drain();
  Core1Beat("showing a frame (dispmanx)");
  mFrameBuffer.FrameReady(wait_vsync ? 1 : 0);
  FrameBufferLayer::SwapResources(wait_vsync != 0, &mFrameBuffer, nullptr);
  Core1Beat("emulating");
}

//
// Sound
//

unsigned CKernel::SoundOpen(unsigned freq, int channels,
                            unsigned latency_frames) {
  (void)freq; // The VideoCore always runs at HW_SAMPLE_RATE.
  if (mSound == 0) {
    mSound = new VchiqSound(&mVCHIQ, HW_SAMPLE_RATE, mAudioOut);
  } else if (mSound->PlaybackActive()) {
    if (channels == mSoundChannels) {
      return HW_SAMPLE_RATE;
    }
    mSound->CancelPlayback();
  }
  mSoundChannels = channels;
  if (!mSound->Playback(mVolume, channels, latency_frames)) {
    mLogger.Write(FromKernel, LogError, "Cannot start sound");
    return 0;
  }
  return HW_SAMPLE_RATE;
}

void CKernel::SoundClose(void) {
  // The VCHIQ audio service is left open; reopening it is slow.
}

unsigned CKernel::SoundFreeFrames(void) {
  return mSound != 0 && mSound->PlaybackActive() ? mSound->FreeFrames() : 0;
}

void CKernel::SoundWrite(const int16_t *samples, unsigned frames) {
  if (mSound != 0 && mSound->PlaybackActive()) {
    Core1Beat("writing sound (VCHIQ)");
    mSound->WriteFrames(samples, frames);
    Core1Beat("emulating");
  }
}

#endif

//
// System
//

double CKernel::Time(void) {
  return (double)CTimer::GetClockTicks64() / CLOCKHZ;
}

void CKernel::Reboot(void) {
  mLogger.Write(FromKernel, LogNotice, "Rebooting");
  mLog.Close();
  reboot();
}

void CKernel::PowerOff(void) {
#if RASPPI >= 4
  mLogger.Write(FromKernel, LogNotice, "Powering off");
  mLog.Close();
  mVideo.Blank();
  CTimer::SimpleMsDelay(100);   // the log out of the UART
  // First tell the firmware, as Linux does before every reboot, halt or
  // power off: the mailbox tag RPI_FIRMWARE_NOTIFY_REBOOT, 0x00030048, no
  // data (drivers/firmware/raspberrypi.c, rpi_firmware_notify_reboot and
  // rpi_firmware_shutdown; include/soc/bcm2835/raspberrypi-firmware.h).
  // Circle's GetTag() needs room for at least one value, so the simple tag.
  {
    CBcmPropertyTags tags;
    TPropertyTagSimple tag;
    tags.GetTag(0x00030048, &tag, sizeof tag, 0);
  }
  // Then a restart into "partition 63", which the firmware takes as a halt:
  // the same as Linux's power off on the Raspberry Pi (bcm2835_wdt.c,
  // bcm2835_power_off). The Pi 4 B's bootloader then halts, waking on
  // GPIO3 (WAKE_ON_GPIO=1, its default; with 0 and POWER_OFF_ON_HALT=1 it
  // switches its outputs off); the Pi 400's switches off, and its power key
  // turns it back on (rpi-eeprom firmware-2711/release-notes.md). The Pi 5,
  // 500 and 500+ halt until their power button or key is pressed (Raspberry
  // Pi documentation, "Raspberry Pi 5 power button"). Circle:
  // CBcmWatchdog.
  CBcmWatchdog watchdog;
  watchdog.Restart(CBcmWatchdog::PartitionHalt);
#else
  mLogger.Write(FromKernel, LogNotice, "Halting; safe to switch off");
  mLog.Close();
  // Blank the picture so it is clear the Pi has stopped.
  mFrameBuffer.Hide();
  halt();
#endif
}

// A crash during start up: the log so far, with the crash report, goes to
// the card before the Pi halts.
void CKernel::PanicHandler(void) { FlushStartupLog(); }

void FlushStartupLog(void) {
  CKernel *kernel = CKernel::Get();
  if (kernel != 0 && kernel->mStarting) {
    kernel->mLog.Flush();
  }
}

bool CKernel::SetLogDestination(int destination) {
  if (!mLog.SetDestination(destination)) {
    mSerial.Write("Cannot open " LOG_FILE "\n", sizeof("Cannot open " LOG_FILE "\n") - 1);
    return false;
  }
  if (destination == HOST_LOG_FILE) {
    // The file starts with what was logged since boot.
    char text[512];
    int len;
    while ((len = mLogger.Read(text, sizeof text)) > 0) {
      mLog.WriteFile(text, len);
    }
  }
  return true;
}

//
// host.h: C entry points for the emulator.
//

extern "C" {

const char *host_option(const char *name, const char *default_value) {
  Options *o = Options::Get();
  return o ? o->Get(name, default_value) : default_value;
}

int host_option_int(const char *name, int default_value) {
  Options *o = Options::Get();
  return o ? o->GetInt(name, default_value) : default_value;
}

double host_option_double(const char *name, double default_value) {
  Options *o = Options::Get();
  return o ? o->GetDouble(name, default_value) : default_value;
}

int host_option_set(const char *name, const char *value) {
  Options *o = Options::Get();
  return o && o->Set(name, value);
}

int host_options_save(void) {
  Options *o = Options::Get();
  return o && o->SaveSettings();
}

const char *host_model_name(void) {
  return CMachineInfo::Get()->GetMachineName();
}

void host_display_size(int *width, int *height) {
  CKernel::Get()->DisplaySize(width, height);
}

int host_display_match_rate(double fps) {
  return CKernel::Get()->DisplayMatchRate(fps) ? 1 : 0;
}

int host_display_resolution_choice(void) {
  return CKernel::Get()->DisplayResolutionChoice() ? 1 : 0;
}

void host_network_status(char *text, unsigned size) {
  static const char *const kStatus[] = {
      "Off",
      "Ethernet starting",
      "Ethernet: waiting for address",
      "Ethernet connected",
      "Ethernet not found",
      "Ethernet failed",
      "Wi-Fi: no network chosen",
      "Wi-Fi starting",
      "Wi-Fi failed",
      "Wi-Fi failed",
      "Wi-Fi starting",
      "Wi-Fi failed",
      "Wi-Fi connecting",
      "Wi-Fi connected",
      "Wi-Fi: no connection",
      "Wi-Fi not found",
      "Wi-Fi not started",
      "Wi-Fi not supported",
      "Wi-Fi: firmware missing",
  };
  char ip[32];
  int status = circle_get_network_status();
  if (circle_get_network_ip_address(ip, sizeof ip)) {
    snprintf(text, size, "%s %s",
             status == NETWORK_ETHERNET_CONNECTED ? "Ethernet" : "Wi-Fi", ip);
  } else if (status >= 0 &&
             status < (int)(sizeof kStatus / sizeof kStatus[0])) {
    snprintf(text, size, "%s", kStatus[status]);
  } else {
    snprintf(text, size, "Unknown");
  }
}

double host_time(void) { return CKernel::Get()->Time(); }

void host_sleep_us(unsigned us) {
  // Sleep in small steps so Circle's tasks keep running.
  double end = host_time() + us / 1000000.0;
  for (;;) {
    Core1Beat("sleeping");
    CScheduler::Get()->Yield();
    CKernel::Get()->DrainLog();
    double left = end - host_time();
    if (left <= 0) {
      break;
    }
    unsigned step = left > 0.001 ? 1000 : (unsigned)(left * 1000000.0);
    if (step > 0) {
      CTimer::SimpleusDelay(step);
    }
  }
}

void host_yield(void) { CScheduler::Get()->Yield(); }

int host_video_init(int width, int height, uint8_t **pixels, int *pitch) {
  return CKernel::Get()->VideoInit(width, height, pixels, pitch);
}

void host_video_set_view(int x, int y, int w, int h, double aspect) {
  CKernel::Get()->VideoSetView(x, y, w, h, aspect);
}

void host_video_set_palette(int index, uint32_t rgb) {
  CKernel::Get()->VideoSetPalette(index, rgb);
}

void host_video_update_palette(void) { CKernel::Get()->VideoUpdatePalette(); }

void host_video_present(int wait_vsync) {
  CKernel::Get()->VideoPresent(wait_vsync);
}

unsigned host_sound_open(unsigned freq, int channels, unsigned latency_frames) {
  return CKernel::Get()->SoundOpen(freq, channels, latency_frames);
}

void host_sound_close(void) { CKernel::Get()->SoundClose(); }

unsigned host_sound_free_frames(void) {
  return CKernel::Get()->SoundFreeFrames();
}

void host_sound_write(const int16_t *samples, unsigned frames) {
  CKernel::Get()->SoundWrite(samples, frames);
}

int host_key_event(int *usage, int *pressed) {
  return CKernel::Get()->KeyEvent(usage, pressed);
}

int host_key_down(int usage) { return CKernel::Get()->KeyDown(usage); }

unsigned host_key_modifiers(void) { return CKernel::Get()->KeyModifiers(); }

int host_pad_state(int pad) { return CKernel::Get()->PadState(pad); }
int host_pad_info(int pad, struct host_pad_info *info) {
  return CKernel::Get()->PadInfo(pad, info);
}
uint32_t host_pad_buttons(int pad) { return CKernel::Get()->PadButtons(pad); }
int host_pad_input(int pad, struct host_pad_input *input) {
  return CKernel::Get()->PadInput(pad, input);
}

unsigned host_mouse(int *dx, int *dy) { return CKernel::Get()->Mouse(dx, dy); }

int host_gpio_joystick(int port) { return CKernel::Get()->GpioJoystick(port); }

void host_reboot(void) { CKernel::Get()->Reboot(); }

void host_power_off(void) { CKernel::Get()->PowerOff(); }

extern int circle_get_network_ip_address(char *address, unsigned int size);

int host_network_address(char *address, unsigned size) {
  return circle_get_network_ip_address(address, size);
}

void host_log(const char *message) {
  CLogger::Get()->Write(FromKernel, LogNotice, "%s", message);
}

int host_log_destination(void) { return CKernel::Get()->GetLogDestination(); }

int host_set_log_destination(int destination) {
  return CKernel::Get()->SetLogDestination(destination);
}

} // extern "C"
