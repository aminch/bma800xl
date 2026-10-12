//
// modeset.h
//
// The direct display driver's mode setting (stage 2): re-times HDMI0 to
// another mode of the same size, through pixel valve 2, the HDMI encoder
// and its PHY, with the HVS channel restarted to match. Only modes BMA800XL
// knows (modes.h) are switched between, starting from a mode it
// recognises; if a step doesn't complete, the old mode is put back.
//
// The steps and their order follow the behaviour of the Linux vc4 display
// driver.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4KMS_MODESET_H_
#define BMA800XL_PI4KMS_MODESET_H_

#include "hvs.h"
#include "modes.h"

#include <stdint.h>

namespace kms {

class ModeSetter {
public:
  ModeSetter(void);

  // The mode HDMI0 is running, if it is one BMA800XL knows (with its
  // nominal pixel clock, or the last one set here).
  bool Current(Timing *timing);

  // The display's EDID, through the firmware (read once).
  const Edid &GetEdid(void);

  // Switches to TIMING (same size as now). AUDIO_FS is the sample rate of
  // the HDMI sound, for the audio clock (0: none). REINSTALL is called
  // between stopping and restarting the HVS channel, to put the picture's
  // display list back. False if it failed (the old mode is then restored
  // if possible).
  bool Set(Hvs &hvs, const Timing &timing, unsigned audio_fs,
           void (*reinstall)(void *), void *param);

private:
  bool Apply(Hvs &hvs, const Timing &timing, bool hdmi_sink, unsigned audio_fs,
             void (*reinstall)(void *), void *param);
  void Disable(Hvs &hvs);
  void SetClocks(uint64_t tmds_hz);
  void SetPhy(uint64_t tmds_hz);
  void SetHdmiTimings(const Timing &timing);
  void SetPixelValve(const Timing &timing);
  void SetColour(bool limited);
  bool WriteInfoFrame(const Timing &timing);
  bool RecenterFifo(void);
  bool WaitFor(uintptr_t reg, uint32_t mask, bool set, unsigned timeout_ms);

  uintptr_t m_Pv, m_Core, m_Dvp, m_Phy, m_Rm, m_Ram, m_Csc, m_Hd;
  uint32_t m_nSavedPackets;   // RAM_PACKET_CONFIG before disabling
  bool m_bEdidRead;
  Edid m_Edid;
  uint32_t m_nPixelHz;        // the pixel clock last set here (0: firmware's)
};

} // namespace kms

#endif
