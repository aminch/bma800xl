//
// modeset.cpp
//
// See modeset.h.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "modeset.h"

#include "regs.h"

#include <circle/bcmpropertytags.h>
#include <circle/logger.h>
#include <circle/memio.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <string.h>

namespace kms {

static const char FromModeSet[] = "kms";

namespace {

inline uint32_t Rd(uintptr_t a) { return read32(a); }
inline void Wr(uintptr_t a, uint32_t v) { write32(a, v); }
inline void SetBits(uintptr_t a, uint32_t bits) { write32(a, read32(a) | bits); }
inline void ClearBits(uintptr_t a, uint32_t bits) { write32(a, read32(a) & ~bits); }
// Replaces bits HIGH:LOW of the register at A with VALUE.
inline void Put(uintptr_t a, unsigned high, unsigned low, uint32_t value) {
  uint32_t mask = Field(0xFFFFFFFFu, high, low);
  write32(a, (read32(a) & ~mask) | Field(value, high, low));
}

uint32_t GetClockRate(uint32_t id) {
  CBcmPropertyTags tags;
  TPropertyTagClockRate tag;
  tag.nClockId = id;
  if (!tags.GetTag(PROPTAG_GET_CLOCK_RATE, &tag, sizeof tag, 4)) {
    return 0;
  }
  return tag.nRate;
}

bool SetClockRate(uint32_t id, uint32_t hz) {
  CBcmPropertyTags tags;
  TPropertyTagSetClockRate tag;
  tag.nClockId = id;
  tag.nRate = hz;
  tag.nSkipSettingTurbo = SKIP_SETTING_TURBO;
  return tags.GetTag(PROPTAG_SET_CLOCK_RATE, &tag, sizeof tag, 12);
}

} // namespace

ModeSetter::ModeSetter(void)
    : m_Pv(FromBus(pv::Bus)), m_Core(FromBus(hdmi::CoreBus)),
      m_Dvp(FromBus(hdmi::DvpBus)), m_Phy(FromBus(hdmi::PhyBus)),
      m_Rm(FromBus(hdmi::RmBus)), m_Ram(FromBus(hdmi::RamBus)),
      m_Csc(FromBus(hdmi::CscBus)), m_Hd(FromBus(hdmi::HdBus)),
      m_nSavedPackets(0), m_bEdidRead(false), m_nPixelHz(0) {
  memset(&m_Edid, 0, sizeof m_Edid);
}

bool ModeSetter::WaitFor(uintptr_t reg, uint32_t mask, bool set,
                         unsigned timeout_ms) {
  u64 start = CTimer::GetClockTicks64();
  u64 limit = (u64)timeout_ms * (CLOCKHZ / 1000);
  while (((Rd(reg) & mask) != 0) != set) {
    if (CTimer::GetClockTicks64() - start > limit) {
      return false;
    }
  }
  return true;
}

// Pixel valve 2's timings, read back (horizontal ones are in pairs).
bool ModeSetter::Current(Timing *timing) {
  if (!(Rd(m_Pv + pv::Control) & pv::Enable) ||
      !(Rd(m_Pv + pv::VControl) & pv::VideoEnable)) {
    return false;
  }
  uint32_t horza = Rd(m_Pv + pv::HorzA), horzb = Rd(m_Pv + pv::HorzB);
  uint32_t verta = Rd(m_Pv + pv::VertA), vertb = Rd(m_Pv + pv::VertB);
  unsigned ppc = pv::PixelsPerClock;
  unsigned h_active = GetField(horzb, 15, 0) * ppc;
  unsigned h_total = (GetField(horzb, 31, 16) + GetField(horzb, 15, 0) +
                      GetField(horza, 15, 0) + GetField(horza, 31, 16)) * ppc;
  unsigned v_active = GetField(vertb, 15, 0);
  unsigned v_total = GetField(vertb, 31, 16) + v_active + GetField(verta, 15, 0) +
                     GetField(verta, 31, 16);
  const Timing *known = FindMode(h_active, h_total, v_active, v_total);
  if (known == 0) {
    CLogger::Get()->Write(FromModeSet, LogNotice,
                          "Video mode %ux%u (totals %ux%u) isn't one BMA800XL switches",
                          h_active, v_active, h_total, v_total);
    return false;
  }
  *timing = *known;
  if (m_nPixelHz != 0) {
    timing->pixel_hz = m_nPixelHz;
  }
  return true;
}

const Edid &ModeSetter::GetEdid(void) {
  if (!m_bEdidRead) {
    m_bEdidRead = true;
    static uint8_t data[4 * 128];
    unsigned blocks = 0;
    for (unsigned b = 0; b < 4; b++) {
      CBcmPropertyTags tags;
      TPropertyTagEDIDBlock tag;
      tag.nBlockNumber = b;
      if (!tags.GetTag(PROPTAG_GET_EDID_BLOCK, &tag, sizeof tag, 4) ||
          tag.nStatus != EDID_STATUS_SUCCESS) {
        break;
      }
      memcpy(data + 128 * b, tag.Block, 128);
      blocks++;
    }
    ParseEdid(data, blocks, &m_Edid);
    CLogger::Get()->Write(FromModeSet, LogNotice, "EDID: %u block(s), %s, %u video modes",
                          blocks, !m_Edid.valid ? "not valid" : m_Edid.hdmi ? "HDMI" : "DVI",
                          m_Edid.vic_count);
  }
  return m_Edid;
}

bool ModeSetter::Set(Hvs &hvs, const Timing &timing, unsigned audio_fs,
                     void (*reinstall)(void *), void *param) {
  Timing old;
  if (!Current(&old)) {
    return false;
  }
  bool hdmi_sink = GetEdid().hdmi;
  if (Apply(hvs, timing, hdmi_sink, audio_fs, reinstall, param)) {
    m_nPixelHz = timing.pixel_hz;
    CLogger::Get()->Write(FromModeSet, LogNotice, "Video mode VIC %u, %ux%u at %u Hz pixel clock (%.3f Hz)",
                          timing.vic, timing.h_active, timing.v_active,
                          timing.pixel_hz, timing.Refresh());
    return true;
  }
  CLogger::Get()->Write(FromModeSet, LogError, "Setting the video mode failed; going back");
  if (Apply(hvs, old, hdmi_sink, audio_fs, reinstall, param)) {
    m_nPixelHz = old.pixel_hz;
  }
  return false;
}

// Disables the pixel valve and HDMI.
void ModeSetter::Disable(Hvs &hvs) {
  m_nSavedPackets = Rd(m_Core + hdmi::RamPacketConfig);

  ClearBits(m_Pv + pv::VControl, pv::VideoEnable);
  WaitFor(m_Pv + pv::VControl, pv::VideoEnable, false, 1);
  // Otherwise a pixel can stick in a FIFO between the PV and HDMI.
  CTimer::SimpleMsDelay(20);

  Wr(m_Core + hdmi::RamPacketConfig, 0);
  SetBits(m_Hd + hdmi::VidCtl, hdmi::VidClearRgb);
  CTimer::SimpleMsDelay(1);
  ClearBits(m_Hd + hdmi::VidCtl, hdmi::VidEnable);

  ClearBits(m_Pv + pv::Control, pv::Enable);
  SetBits(m_Pv + pv::Control, pv::FifoClear);

  hvs.StopChannel();

  SetBits(m_Hd + hdmi::VidCtl, hdmi::VidBlankPixels);
  Wr(m_Phy + hdmi::PhyResetCtl, hdmi::PhyLaneResets);
  Wr(m_Phy + hdmi::PhyPowerdownCtl, Bit(10));
}

// Raises the HDMI state machine and BVB clocks if they are below
// what the TMDS rate needs.
void ModeSetter::SetClocks(uint64_t tmds_hz) {
  uint32_t hsm = (uint32_t)(tmds_hz / 100 * 101);
  if (hsm < hdmi::HsmMinHz) {
    hsm = hdmi::HsmMinHz;
  }
  uint32_t bvb = tmds_hz > 297000000 ? 300000000
                 : tmds_hz > 148500000 ? 150000000 : 75000000;
  if (GetClockRate(hdmi::ClockHsm) < hsm) {
    SetClockRate(hdmi::ClockHsm, hsm);
  }
  if (GetClockRate(hdmi::ClockBvb) < bvb) {
    SetClockRate(hdmi::ClockBvb, bvb);
  }
}

// Sets up the PHY for the TMDS rate.
void ModeSetter::SetPhy(uint64_t tmds_hz) {
  PhySettings phy;
  ComputePhy(tmds_hz, &phy);

  Wr(m_Phy + hdmi::PhyResetCtl, hdmi::PhyLaneResets);
  Wr(m_Phy + hdmi::PhyPowerdownCtl, Bit(10));
  Wr(m_Phy + hdmi::PhyPowerdownCtl, hdmi::PhyRndgenPowerdown);
  ClearBits(m_Phy + hdmi::PhyResetCtl, hdmi::PhyLaneResets);

  SetBits(m_Rm + hdmi::RmControl,
      hdmi::RmFreezeCounters | hdmi::RmLoadIntegrator | hdmi::RmFreeRun);
  Put(m_Phy + hdmi::PhyCalibration1, 27, 0, 0);
  Put(m_Phy + hdmi::PhyCalibration2, 27, 0, 0);
  Wr(m_Rm + hdmi::RmOffset, Field(phy.rm_offset, 30, 0) | hdmi::RmOffsetOnly);
  Wr(m_Phy + hdmi::PhyClkDiv, Field(phy.vco_divider, 15, 8));
  Wr(m_Phy + hdmi::PhyCalibration4, Field(0xE14, 31, 16) | Field(0xE147, 15, 0));

  Wr(m_Phy + hdmi::PhyPllCtl0, hdmi::PllVcoClockEnable | hdmi::PllVcoContinuous |
                                   hdmi::PllMash11 |
                                   (phy.vco_high ? Bit(hdmi::PllVcoSelLow) : 0));
  uintptr_t ctl1 = m_Phy + hdmi::PhyPllCtl1;
  Put(ctl1, 27, 16, 0x8A);     // CPP
  Put(ctl1, 15, 14, 1);        // doubler delay
  Put(ctl1, 12, 11, 3);        // post reset select
  SetBits(ctl1, hdmi::PllDoublerEnable);
  Put(m_Rm + hdmi::RmFormat, 25, 24, 2);
  Put(m_Phy + hdmi::PhyPllCfg, 3, 0, 1);
  Wr(m_Phy + hdmi::PhyTmdsClkWordSel, 0);

  Wr(m_Phy + hdmi::PhyCtl3, Field(phy.icp, 5, 0) | Field(1, 9, 8) |
                                Field(1, 11, 10) | Field(3, 7, 6) |
                                Field(4, 19, 17) | Field(6, 16, 12));

  // Lanes 0, 1, 2, clock: pre-emphasis and main driver; res_sel;
  // termination (with the VCO gain).
  Wr(m_Phy + hdmi::PhyCtl0,
     Field(phy.preemphasis[0], 15, 13) | Field(phy.main_driver[0], 12, 8) |
         Field(phy.preemphasis[1], 23, 21) | Field(phy.main_driver[1], 20, 16) |
         Field(phy.preemphasis[2], 31, 29) | Field(phy.main_driver[2], 28, 24) |
         Field(phy.preemphasis[3], 7, 5) | Field(phy.main_driver[3], 4, 0));
  uintptr_t phy1 = m_Phy + hdmi::PhyCtl1;
  Put(phy1, 9, 5, phy.res_sel[0]);
  Put(phy1, 14, 10, phy.res_sel[1]);
  Put(phy1, 19, 15, phy.res_sel[2]);
  Put(phy1, 4, 0, phy.res_sel[3]);
  Wr(m_Phy + hdmi::PhyCtl2,
     Field(phy.termination[0], 7, 4) | Field(phy.termination[1], 11, 8) |
         Field(phy.termination[2], 15, 12) | Field(phy.termination[3], 3, 0) |
         Field(phy.vco_gain, 19, 16));

  // HDMI0's lanes in order.
  Wr(m_Phy + hdmi::PhyChannelSwap, Field(0, 1, 0) | Field(1, 5, 4) |
                                       Field(2, 9, 8) | Field(3, 13, 12));

  ClearBits(m_Phy + hdmi::PhyResetCtl, hdmi::PhyPllResetb | hdmi::PhyPllDivResetb);
  SetBits(m_Phy + hdmi::PhyResetCtl, hdmi::PhyPllResetb | hdmi::PhyPllDivResetb);
}

// HDMI's timings, set before the pixel valve.
void ModeSetter::SetHdmiTimings(const Timing &t) {
  Wr(m_Core + hdmi::HorzA, (t.v_positive ? Bit(15) : 0) | (t.h_positive ? Bit(14) : 0) |
                               Field(t.h_front, 28, 16) | Field(t.h_active, 13, 0));
  Wr(m_Core + hdmi::HorzB, Field(t.h_back, 26, 16) | Field(t.h_sync, 10, 0));
  uint32_t verta = Field(t.v_sync, 28, 24) | Field(t.v_front, 22, 16) |
                   Field(t.v_active, 12, 0);
  Wr(m_Core + hdmi::VertA0, verta);
  Wr(m_Core + hdmi::VertA1, verta);
  Wr(m_Core + hdmi::VertB0, Field(0, 29, 16) | Field(t.v_back, 8, 0));
  Wr(m_Core + hdmi::VertB1, Field(t.HTotal() / 2, 29, 16) | Field(t.v_back, 8, 0));

  Put(m_Core + hdmi::DeepColorConfig1, 10, 8, 2);   // pack phase
  Put(m_Core + hdmi::DeepColorConfig1, 3, 0, 0);    // 8 bits a colour
  Put(m_Core + hdmi::GcpWord1, 15, 8, 0);
  Put(m_Core + hdmi::GcpWord1, 7, 0, hdmi::GcpClearAvMute);
  SetBits(m_Core + hdmi::GcpConfig, hdmi::GcpEnable);
  Put(m_Core + hdmi::MiscControl, 3, 0, 0);         // no pixel repetition
  Wr(m_Dvp + hdmi::ClockStop, 0);
}

// Sets up and enables the pixel valve.
void ModeSetter::SetPixelValve(const Timing &t) {
  unsigned ppc = pv::PixelsPerClock;
  Wr(m_Pv + pv::HorzA, Field(t.h_back / ppc, 31, 16) | Field(t.h_sync / ppc, 15, 0));
  Wr(m_Pv + pv::HorzB, Field(t.h_front / ppc, 31, 16) | Field(t.h_active / ppc, 15, 0));
  Wr(m_Pv + pv::VertA, Field(t.v_back, 31, 16) | Field(t.v_sync, 15, 0));
  Wr(m_Pv + pv::VertB, Field(t.v_front, 31, 16) | Field(t.v_active, 15, 0));
  Wr(m_Pv + pv::VControl, pv::Continuous);
  Wr(m_Pv + pv::VsyncdEven, 0);
  Wr(m_Pv + pv::MuxCfg, Field(pv::MuxNoSwap, 5, 2));

  // FIFO level: its size less the HVS's latency of 6 three-byte pixels.
  unsigned level = pv::FifoBytes - 3 * pv::HvsLatencyPixels;
  Wr(m_Pv + pv::Control, pv::FifoClear |
                             Field(level >> 6, pv::FifoLevelHighHigh, pv::FifoLevelHighLow) |
                             Field(level & 0x3F, pv::FifoLevelHigh, pv::FifoLevelLow) |
                             Field(0, pv::FormatHigh, pv::FormatLow) |
                             Field(0, pv::PixelRepHigh, pv::PixelRepLow) |
                             pv::ClearAtStart | pv::TriggerUnderflow | pv::WaitHstart |
                             Field(0, pv::ClockSelectHigh, pv::ClockSelectLow));
  SetBits(m_Pv + pv::Control, pv::Enable);
}

// Colour conversion: RGB, limited (16-235) or full range.
void ModeSetter::SetColour(bool limited) {
  const uint32_t one = limited ? 0x1B7C : 0x2000;   // 219/255 or 1, 2.13
  const uint32_t offset = limited ? 0x400 : 0;      // 16 or 0, 9.6
  Wr(m_Dvp + hdmi::VecInterfaceCfg, 0);
  Wr(m_Dvp + hdmi::VecInterfaceXbar, hdmi::XbarRgb);
  // Rows of (c1, c2, c3, offset), two 16-bit values a word, low first.
  for (unsigned row = 0; row < 3; row++) {
    uint32_t c[4] = {0, 0, 0, offset};
    c[row] = one;
    Wr(m_Csc + hdmi::Csc12_11 + 8 * row, c[1] << 16 | c[0]);
    Wr(m_Csc + hdmi::Csc12_11 + 8 * row + 4, c[3] << 16 | c[2]);
  }
  Wr(m_Csc + hdmi::CscChannelCtl, 0);
  Wr(m_Csc + hdmi::CscCtl, hdmi::CscEnable | Field(hdmi::CscModeCustom, 1, 0));
}

// The AVI InfoFrame, in slot 2.
bool ModeSetter::WriteInfoFrame(const Timing &timing) {
  const unsigned slot = 0x82 - 0x80;
  uint8_t frame[AviLength];
  AviInfoFrame(timing, frame);
  uint32_t words[PacketWords];
  PacketRamWords(frame, AviLength, words);

  ClearBits(m_Core + hdmi::RamPacketConfig, Bit(slot));
  if (!WaitFor(m_Core + hdmi::RamPacketStatus, Bit(slot), false, 100)) {
    return false;
  }
  uintptr_t base = m_Ram + slot * hdmi::PacketStride;
  for (unsigned i = 0; i < PacketWords; i++) {
    Wr(base + 4 * i, words[i]);
  }
  SetBits(m_Core + hdmi::RamPacketConfig, Bit(slot));
  return WaitFor(m_Core + hdmi::RamPacketStatus, Bit(slot), true, 100);
}

bool ModeSetter::RecenterFifo(void) {
  uint32_t drift = Rd(m_Core + hdmi::FifoCtl) & hdmi::FifoWritable;
  for (int i = 0; i < 2; i++) {
    Wr(m_Core + hdmi::FifoCtl, drift & ~hdmi::FifoRecenter);
    Wr(m_Core + hdmi::FifoCtl, drift | hdmi::FifoRecenter);
    if (i == 0) {
      CTimer::SimpleMsDelay(1);
    }
  }
  return WaitFor(m_Core + hdmi::FifoCtl, hdmi::FifoRecenterDone, true, 10);
}

bool ModeSetter::Apply(Hvs &hvs, const Timing &t, bool hdmi_sink, unsigned audio_fs,
                       void (*reinstall)(void *), void *param) {
  Disable(hvs);

  // HVS channel: the picture's list, then the channel itself.
  if (reinstall != 0) {
    reinstall(param);
  }
  hvs.StartChannel(t.h_active, t.v_active);

  // HDMI0 before the PV.
  SetClocks(t.pixel_hz);
  SetPhy(t.pixel_hz);
  SetBits(m_Core + hdmi::SchedulerControl,
      hdmi::SchedManualFormat | hdmi::SchedIgnoreVsyncPredicts);
  SetHdmiTimings(t);

  SetPixelValve(t);

  // HDMI0 after the PV is enabled, before video.
  SetColour(hdmi_sink);
  Wr(m_Core + hdmi::FifoCtl, hdmi::FifoMasterSlave);

  SetBits(m_Pv + pv::VControl, pv::VideoEnable);

  // HDMI0 after video enable.
  uint32_t vid = Rd(m_Hd + hdmi::VidCtl) & ~(hdmi::VidVsyncLow | hdmi::VidHsyncLow);
  Wr(m_Hd + hdmi::VidCtl, vid | hdmi::VidEnable | hdmi::VidClearRgb |
                              hdmi::VidUnderflowEnable | hdmi::VidFrameCounterReset |
                              hdmi::VidBlankInsert |
                              (t.v_positive ? 0 : hdmi::VidVsyncLow) |
                              (t.h_positive ? 0 : hdmi::VidHsyncLow));
  ClearBits(m_Hd + hdmi::VidCtl, hdmi::VidBlankPixels);

  bool ok = true;
  if (hdmi_sink) {
    SetBits(m_Core + hdmi::SchedulerControl, hdmi::SchedHdmiMode);
    ok = WaitFor(m_Core + hdmi::SchedulerControl, hdmi::SchedHdmiActive, true, 1000);
    // Packet RAM on, with the packets that were being sent (Circle's audio
    // InfoFrame) still on; then the AVI InfoFrame.
    Wr(m_Core + hdmi::RamPacketConfig,
       hdmi::RamPacketEnable | (m_nSavedPackets & 0xFFFF & ~Bit(0x82 - 0x80)));
    ok = WriteInfoFrame(t) && ok;
  } else {
    ClearBits(m_Core + hdmi::RamPacketConfig, hdmi::RamPacketEnable);
    ClearBits(m_Core + hdmi::SchedulerControl, hdmi::SchedHdmiMode);
    ok = WaitFor(m_Core + hdmi::SchedulerControl, hdmi::SchedHdmiActive, false, 1000);
  }
  ok = RecenterFifo() && ok;

  // HDMI sound's clock regeneration for the new TMDS rate (Circle uses
  // external CTS).
  uint32_t crp = Rd(m_Core + hdmi::CrpCfg);
  if (audio_fs != 0 && (crp & Bit(24))) {
    uint32_t cts = AudioCts(t.pixel_hz, crp & 0xFFFFF, audio_fs);
    Wr(m_Core + hdmi::Cts0, cts);
    Wr(m_Core + hdmi::Cts1, cts);
  }
  return ok;
}

} // namespace kms
