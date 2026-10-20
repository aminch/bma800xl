//
// crt.h
//
// The picture drawn by the 3D GPU of the Pi 4 or Pi 5: the Atari's 8-bit
// indexed screen is turned into colour in a texture by the ARM, and the GPU
// scales it to the output, with crt-pi (tools/v3dshader/crt.frag) or as a
// plain copy, sharp or smooth. The same pictures as src/av/scaler.h draws
// on the ARM cores, which the self-test checks.
//
// Pixels are 32-bit words 0xAARRGGBB, as the framebuffer and the HVS take
// them. The GPU sees them as red in the low byte, but every channel is
// treated alike, so they come out in the order they went in.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_V3D_CRT_H_
#define BMA800XL_V3D_CRT_H_

#include "draw.h"

#include <stdint.h>

namespace v3d {

class Crt {
public:
  Crt(void);

  // Sets up for views up to MAX_VIEW_WIDTH x MAX_VIEW_HEIGHT.
  bool Initialize(Gpu *gpu, Renderer *renderer, unsigned max_view_width,
                  unsigned max_view_height);

  // As Pi4Scaler: crt: the crt-pi look; smooth: bilinear filtering;
  // curvature: crt-pi's curved screen (with crt only).
  void SetMode(bool crt, bool smooth, bool curvature);

  // The source is 8-bit indexed (BYTES_PER_PIXEL 1) or RGB565 (2),
  // SRC_PITCH bytes per line; the part shown starts at (VX, VY) and is
  // VW x VH; it is drawn DW x DH.
  bool Configure(unsigned src_pitch, int vx, int vy, int vw, int vh, unsigned dw,
                 unsigned dh, unsigned bytes_per_pixel = 1);

  // For an indexed source; RGB is 0xRRGGBB.
  void SetPalette(int index, uint32_t rgb);

  // Draws a frame from SRC into TARGET with its top left corner at (X, Y);
  // the rest of TARGET is cleared to black. Waits for the GPU.
  // TARGET's memory must be reachable by the GPU (Gpu::Reachable()).
  bool Draw(const uint8_t *src, const Target &target, int x, int y);

  // Draws a test picture in every crt-pi mode and compares it with
  // Pi4Scaler::Reference(). Logs the results.
  bool Test(void);

private:
  void Fill(const uint8_t *src);

  Gpu *m_pGpu;
  Renderer *m_pRenderer;
  Texture m_Texture;
  bool m_bReady;
  bool m_bCrt, m_bSmooth, m_bCurvature;
  unsigned m_nPitch;
  unsigned m_nBytesPerPixel;
  int m_nViewX, m_nViewY, m_nViewW, m_nViewH;
  unsigned m_nDestW, m_nDestH;
  uint32_t m_Palette[256];  // 0xFFRRGGBB

  // Time spent on the frames drawn so far (microseconds), logged once.
  unsigned m_nFrames;
  uint64_t m_nFillUs, m_nBinUs, m_nRenderUs;
};

} // namespace v3d

#endif
