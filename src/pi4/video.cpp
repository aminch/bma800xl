//
// video.cpp
//
// See video.h for what this is and the sources it follows.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "video.h"

#include <circle/logger.h>
#include <string.h>

static const char FromVideo[] = "video";

Pi4Video::Pi4Video(void)
    : m_pFrameBuffer(0), m_nDepth(0), m_nWidth(0), m_nHeight(0), m_nPitch(0),
      m_nBackPage(1), m_pSource(0), m_nSourceWidth(0), m_nSourceHeight(0),
      m_nViewX(0), m_nViewY(0), m_nViewW(0), m_nViewH(0), m_nDestX(0),
      m_nDestY(0), m_nDestW(0), m_nRepeat(1), m_pColumn(0) {
  memset(m_Palette, 0, sizeof m_Palette);
  memset(m_Palette565, 0, sizeof m_Palette565);
}

Pi4Video::~Pi4Video(void) {
  delete m_pFrameBuffer;
  delete[] m_pSource;
  delete[] m_pColumn;
}

bool Pi4Video::Initialize(void) {
  // A width and height of 0 make CBcmFrameBuffer ask the firmware for the
  // display's size (PROPTAG_GET_DISPLAY_DIMENSIONS). Double buffered: the
  // virtual height is twice the display's, and SetVirtualOffset() shows one
  // half or the other. 8-bit indexed colour is what atari800 draws; 16-bit
  // is the fall back if the firmware refuses it.
  static const unsigned depths[] = {8, 16};
  for (unsigned i = 0; i < sizeof depths / sizeof depths[0]; i++) {
    m_pFrameBuffer = new CBcmFrameBuffer(0, 0, depths[i], 0, 0, 0, TRUE);
    if (m_pFrameBuffer->Initialize()) {
      m_nDepth = depths[i];
      break;
    }
    delete m_pFrameBuffer;
    m_pFrameBuffer = 0;
  }
  if (m_pFrameBuffer == 0) {
    CLogger::Get()->Write(FromVideo, LogError, "Cannot allocate a framebuffer");
    return false;
  }
  m_nWidth = m_pFrameBuffer->GetWidth();
  m_nHeight = m_pFrameBuffer->GetHeight();
  m_nPitch = m_pFrameBuffer->GetPitch();
  CLogger::Get()->Write(FromVideo, LogNotice, "Framebuffer %ux%u, %u bit",
                        m_nWidth, m_nHeight, m_nDepth);
  ClearPages();
  m_pFrameBuffer->SetVirtualOffset(0, 0);
  m_nBackPage = 1;
  return true;
}

uint8_t *Pi4Video::Page(unsigned page) const {
  return (uint8_t *)(uintptr)m_pFrameBuffer->GetBuffer() +
         (size_t)page * m_nHeight * m_nPitch;
}

void Pi4Video::ClearPages(void) {
  // Index 0 is black in the Atari's palette; RGB565 0 is black too.
  for (unsigned page = 0; page < 2; page++) {
    memset(Page(page), 0, (size_t)m_nHeight * m_nPitch);
  }
}

int Pi4Video::Allocate(int width, int height, uint8_t **pixels, int *pitch) {
  if (m_pFrameBuffer == 0) {
    return -1;
  }
  delete[] m_pSource;
  m_pSource = new uint8_t[(size_t)width * height];
  memset(m_pSource, 0, (size_t)width * height);
  m_nSourceWidth = width;
  m_nSourceHeight = height;
  *pixels = m_pSource;
  *pitch = width;
  SetView(0, 0, width, height, (double)width / height, 1.0);
  return 0;
}

// The view fills the display's height (times SCALE) in whole multiples of
// its own height, so every Atari line is the same number of display lines.
// The width follows the aspect ratio; columns are picked nearest
// neighbour.
void Pi4Video::SetView(int x, int y, int w, int h, double aspect,
                       double scale) {
  if (m_pFrameBuffer == 0 || w <= 0 || h <= 0) {
    return;
  }
  if (scale <= 0.1 || scale > 1.0) {
    scale = 1.0;
  }
  m_nViewX = x;
  m_nViewY = y;
  m_nViewW = w;
  m_nViewH = h;

  unsigned repeat = (unsigned)(m_nHeight * scale) / (unsigned)h;
  if (repeat == 0) {
    repeat = 1;
  }
  unsigned dest_h = repeat * h;
  if (dest_h > m_nHeight) {
    dest_h = m_nHeight;
  }
  unsigned dest_w = (unsigned)(dest_h * aspect + 0.5);
  if (dest_w > m_nWidth) {
    dest_w = m_nWidth;
  }
  m_nRepeat = repeat;
  m_nDestW = dest_w;
  m_nDestX = (m_nWidth - dest_w) / 2;
  m_nDestY = (m_nHeight - dest_h) / 2;
  BuildColumnMap();
  ClearPages();
}

void Pi4Video::BuildColumnMap(void) {
  delete[] m_pColumn;
  m_pColumn = new uint16_t[m_nDestW];
  for (unsigned i = 0; i < m_nDestW; i++) {
    m_pColumn[i] = (uint16_t)(m_nViewX + (i * (unsigned)m_nViewW) / m_nDestW);
  }
}

void Pi4Video::SetPalette(int index, uint32_t rgb) {
  if (index < 0 || index > 255) {
    return;
  }
  m_Palette[index] = rgb & 0xffffff;
  unsigned r = (rgb >> 16) & 0xff;
  unsigned g = (rgb >> 8) & 0xff;
  unsigned b = rgb & 0xff;
  m_Palette565[index] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void Pi4Video::UpdatePalette(void) {
  if (m_pFrameBuffer == 0 || m_nDepth != 8) {
    return;  // 16-bit: the colours are converted while drawing.
  }
  // The firmware's palette entries are 0xAABBGGRR (red in the low byte,
  // as CBcmFrameBuffer::SetPalette() builds them).
  for (unsigned i = 0; i < 256; i++) {
    uint32_t rgb = m_Palette[i];
    uint32_t rgba = ((rgb >> 16) & 0xff) | (rgb & 0xff00) |
                    ((rgb & 0xff) << 16) | 0xff000000u;
    m_pFrameBuffer->SetPalette32((u8)i, rgba);
  }
  m_pFrameBuffer->UpdatePalette();
}

void Pi4Video::Present(bool wait_vsync) {
  if (m_pFrameBuffer == 0 || m_pSource == 0 || m_pColumn == 0) {
    return;
  }
  uint8_t *page = Page(m_nBackPage);
  const unsigned bytes = m_nDepth / 8;
  uint8_t line[4096 * 2];
  if (m_nDestW * bytes > sizeof line) {
    return;
  }

  for (int y = 0; y < m_nViewH; y++) {
    const uint8_t *src = m_pSource + (size_t)(m_nViewY + y) * m_nSourceWidth;
    if (m_nDepth == 8) {
      for (unsigned i = 0; i < m_nDestW; i++) {
        line[i] = src[m_pColumn[i]];
      }
    } else {
      uint16_t *line16 = (uint16_t *)line;
      for (unsigned i = 0; i < m_nDestW; i++) {
        line16[i] = m_Palette565[src[m_pColumn[i]]];
      }
    }
    uint8_t *dst = page + (size_t)(m_nDestY + y * m_nRepeat) * m_nPitch +
                   m_nDestX * bytes;
    for (unsigned r = 0; r < m_nRepeat; r++) {
      memcpy(dst, line, m_nDestW * bytes);
      dst += m_nPitch;
    }
  }

  // Show the page just drawn, then wait for the vertical blank: after it
  // the other page is off screen and free to draw the next frame into.
  m_pFrameBuffer->SetVirtualOffset(0, m_nBackPage * m_nHeight);
  if (wait_vsync) {
    m_pFrameBuffer->WaitForVerticalSync();
  }
  m_nBackPage ^= 1;
}

void Pi4Video::DisplaySize(int *width, int *height) const {
  *width = (int)m_nWidth;
  *height = (int)m_nHeight;
}

void Pi4Video::Blank(void) {
  if (m_pFrameBuffer == 0) {
    return;
  }
  ClearPages();
}
