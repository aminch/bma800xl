//
// sound.h
//
// The Pi 4's sound: Circle's HDMI sound driver (CHDMISoundBaseDevice),
// which sends the samples to HDMI0 by DMA, without the VideoCore firmware.
// The emulator's samples go into the driver's queue (Write()).
//
// Sources:
//  - Circle, include/circle/sound/hdmisoundbasedevice.h and
//    lib/sound/hdmisoundbasedevice.cpp (R. Stange, GPL 3): the HDMI driver;
//    HDMI0 only on the Pi 4, chunk sizes in multiples of 384 samples.
//  - Circle, include/circle/sound/soundbasedevice.h: the queue
//    (AllocateQueueFrames, SetWriteFormat, Write, GetQueueFramesAvail) and
//    the rule that only Write(), GetQueueFramesAvail() and IsActive() may
//    be called on other cores than core 0.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4_SOUND_H_
#define BMA800XL_PI4_SOUND_H_

#include <circle/interrupt.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <stdint.h>

class Pi4Sound {
public:
  enum { SAMPLE_RATE = 48000 };

  Pi4Sound(void);
  ~Pi4Sound(void);

  // Core 0, at start up. VOLUME is 0..100 percent.
  bool Initialize(CInterruptSystem *pInterrupt, int volume);

  // Core 1, from the emulator (see host.h). LATENCY_FRAMES is how much
  // sound to keep queued, in frames at 44100 Hz.
  unsigned Open(int channels, unsigned latency_frames);
  unsigned FreeFrames(void);
  void Write(const int16_t *samples, unsigned frames);

private:
  CHDMISoundBaseDevice *m_pDevice;
  int m_nChannels;        // what the emulator writes: 1 or 2
  unsigned m_nLatency;    // frames at SAMPLE_RATE
  int m_nGain;            // 0..256
};

#endif
