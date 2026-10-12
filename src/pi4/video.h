//
// video.h
//
// The Pi 4's display: Circle's framebuffer driver (CBcmFrameBuffer), which
// asks the VideoCore firmware for a framebuffer through the mailbox
// property interface. The firmware puts it on the screen; BMA800XL scales
// the Atari picture into it.
//
// Sources:
//  - Circle, include/circle/bcmframebuffer.h and lib/bcmframebuffer.cpp
//    (R. Stange, GPL 3): the framebuffer class, its palette, page flipping
//    with SetVirtualOffset() and WaitForVerticalSync().
//  - Raspberry Pi firmware wiki, "Mailbox property interface"
//    (https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface):
//    the property tags those methods send (allocate buffer, set depth,
//    set virtual offset, set palette, wait for vsync).
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4_VIDEO_H_
#define BMA800XL_PI4_VIDEO_H_

#include <circle/bcmframebuffer.h>
#include <stdint.h>

class Pi4Video {
public:
  Pi4Video(void);
  ~Pi4Video(void);

  // Core 0, at start up: takes a double buffered framebuffer the size of
  // the display.
  bool Initialize(void);

  // Core 1, from the emulator (see host.h).
  int Allocate(int width, int height, uint8_t **pixels, int *pitch);
  void SetView(int x, int y, int w, int h, double aspect, double scale);
  void SetPalette(int index, uint32_t rgb);
  void UpdatePalette(void);
  void Present(bool wait_vsync);
  void DisplaySize(int *width, int *height) const;
  // Shows a black screen (before halting).
  void Blank(void);

private:
  void ClearPages(void);
  void BuildColumnMap(void);
  uint8_t *Page(unsigned page) const;

  CBcmFrameBuffer *m_pFrameBuffer;
  unsigned m_nDepth;      // 8 (indexed) or 16 (RGB565)
  unsigned m_nWidth;      // display size in pixels
  unsigned m_nHeight;
  unsigned m_nPitch;      // bytes per framebuffer line
  unsigned m_nBackPage;   // page drawn next: 0 or 1

  // The emulator draws into this 8-bit indexed buffer.
  uint8_t *m_pSource;
  int m_nSourceWidth;
  int m_nSourceHeight;

  // The part of it shown, and where it goes on the screen.
  int m_nViewX, m_nViewY, m_nViewW, m_nViewH;
  unsigned m_nDestX, m_nDestY, m_nDestW, m_nRepeat;
  uint16_t *m_pColumn;    // source column for each destination column

  uint32_t m_Palette[256];   // 0xRRGGBB
  uint16_t m_Palette565[256];
};

#endif
