//
// cl.h
//
// Writes V3D 4.2 control list packets: an opcode byte, then the packet's
// fields at their bit positions (little endian). Only the packets
// BMA800XL uses.
//
// Packet codes, field positions and values from Mesa,
// src/broadcom/cle/v3d_packet.xml (Copyright (C) 2016 Broadcom, MIT
// licence); packing rules from src/broadcom/cle/gen_pack_header.py.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4V3D_CL_H_
#define BMA800XL_PI4V3D_CL_H_

#include <stdint.h>
#include <string.h>

namespace v3d {

// Values of enumerated fields.
const unsigned InternalBpp32 = 0;
const unsigned InternalType8 = 2;        // 8-bit unsigned normalised
const unsigned InternalDepth24 = 1;
const unsigned ClampNone = 0;
const unsigned OutputRgba8 = 27;
const unsigned MemoryRaster = 0;
const unsigned BufferRt0 = 0;
const unsigned BufferNone = 8;

// A control list being written into memory the GPU reads.
class Cl {
public:
  Cl(void *buffer, unsigned size)
      : m_pStart((uint8_t *)buffer), m_pPos((uint8_t *)buffer),
        m_pEnd((uint8_t *)buffer + size), m_bOverflow(false) {}

  uint32_t Start(void) const { return (uint32_t)(uintptr_t)m_pStart; }
  uint32_t Here(void) const { return (uint32_t)(uintptr_t)m_pPos; }
  unsigned Used(void) const { return (unsigned)(m_pPos - m_pStart); }
  bool Overflow(void) const { return m_bOverflow; }

  // Packets with no fields.
  void Halt(void) { Op(0); }
  void EndOfRendering(void) { Op(13); }
  void ReturnFromSubList(void) { Op(18); }
  void FlushVcdCache(void) { Op(19); }
  void EndOfLoads(void) { Op(26); }
  void EndOfTileMarker(void) { Op(27); }
  void TileCoordinatesImplicit(void) { Op(125); }

  void StartAddressOfGenericTileList(uint32_t start, uint32_t end) {
    Fields f;
    f.Set(0, 32, start);
    f.Set(32, 32, end);
    Emit(20, f);
  }

  void SupertileCoordinates(unsigned column, unsigned row) {
    Fields f;
    f.Set(0, 8, column);
    f.Set(8, 8, row);
    Emit(23, f);
  }

  void ClearTileBuffers(bool render_targets, bool z_stencil) {
    Fields f;
    f.Set(0, 1, render_targets);
    f.Set(1, 1, z_stencil);
    Emit(25, f);
  }

  // Store Tile Buffer General: BUFFER to raster memory at ADDRESS, STRIDE
  // bytes a line.
  void StoreTileBuffer(unsigned buffer, uint32_t address, unsigned format,
                       unsigned stride) {
    Fields f;
    f.Set(0, 4, buffer);
    f.Set(4, 3, MemoryRaster);
    f.Set(12, 6, format);
    f.Set(28, 20, stride);
    f.Set(64, 32, address);
    Emit(29, f);
  }

  // Tile Rendering Mode Cfg (Common), sub-id 0.
  void RenderingModeCommon(unsigned width, unsigned height, unsigned targets,
                           unsigned max_bpp) {
    Fields f;
    f.Set(0, 4, 0);
    f.Set(4, 4, targets - 1);
    f.Set(8, 16, width);
    f.Set(24, 16, height);
    f.Set(40, 2, max_bpp);
    f.Set(46, 1, 1);                    // early Z disable
    f.Set(47, 4, InternalDepth24);
    Emit(121, f, 8);
  }

  // Tile Rendering Mode Cfg (Color), sub-id 1: render target 0 only.
  void RenderingModeColor(unsigned bpp, unsigned type, unsigned clamp) {
    Fields f;
    f.Set(0, 4, 1);
    f.Set(4, 2, bpp);
    f.Set(6, 4, type);
    f.Set(10, 2, clamp);
    Emit(121, f, 8);
  }

  // Tile Rendering Mode Cfg (ZS Clear Values), sub-id 2.
  void RenderingModeZsClear(float z, unsigned stencil) {
    uint32_t zbits;
    memcpy(&zbits, &z, 4);
    Fields f;
    f.Set(0, 4, 2);
    f.Set(8, 8, stencil);
    f.Set(16, 32, zbits);
    Emit(121, f, 8);
  }

  // Tile Rendering Mode Cfg (Clear Colors Part1), sub-id 3: the low 56
  // bits of the clear colour of render target 0.
  void RenderingModeClearColor(uint32_t low, uint32_t next24) {
    Fields f;
    f.Set(0, 4, 3);
    f.Set(4, 4, 0);
    f.Set(8, 32, low);
    f.Set(40, 24, next24);
    Emit(121, f, 8);
  }

  void MulticoreSupertileCfg(unsigned tiles_x, unsigned tiles_y,
                             unsigned supertile_w, unsigned supertile_h) {
    unsigned st_x = (tiles_x + supertile_w - 1) / supertile_w;
    unsigned st_y = (tiles_y + supertile_h - 1) / supertile_h;
    Fields f;
    f.Set(0, 8, supertile_w - 1);
    f.Set(8, 8, supertile_h - 1);
    f.Set(16, 8, st_x);
    f.Set(24, 8, st_y);
    f.Set(32, 12, tiles_x);
    f.Set(44, 12, tiles_y);
    f.Set(61, 3, 0);                    // one bin tile list
    Emit(122, f);
  }

  // The address is 64-byte aligned; set number in the low bits.
  void MulticoreTileListSetBase(uint32_t address) {
    Fields f;
    f.Set(0, 32, address & ~63u);
    Emit(123, f);
  }

  void TileCoordinates(unsigned column, unsigned row) {
    Fields f;
    f.Set(0, 12, column);
    f.Set(12, 12, row);
    Emit(124, f);
  }

  void TileListInitialBlockSize(unsigned size_enum, bool auto_chain) {
    Fields f;
    f.Set(0, 2, size_enum);
    f.Set(2, 1, auto_chain);
    Emit(126, f);
  }

private:
  // Up to 15 bytes of fields.
  struct Fields {
    uint8_t bytes[15];
    unsigned length;   // bytes used
    Fields(void) : length(0) { memset(bytes, 0, sizeof bytes); }
    void Set(unsigned start, unsigned bits, uint64_t value) {
      for (unsigned i = 0; i < bits; i++) {
        if ((value >> i) & 1) {
          bytes[(start + i) / 8] |= (uint8_t)(1u << ((start + i) % 8));
        }
      }
      unsigned end = (start + bits + 7) / 8;
      if (end > length) {
        length = end;
      }
    }
  };

  void Op(uint8_t opcode) {
    if (m_pPos + 1 > m_pEnd) {
      m_bOverflow = true;
      return;
    }
    *m_pPos++ = opcode;
  }

  // LENGTH: the packet's field bytes, when its last fields are padding.
  void Emit(uint8_t opcode, const Fields &f, unsigned length = 0) {
    unsigned n = length > f.length ? length : f.length;
    if (m_pPos + 1 + n > m_pEnd) {
      m_bOverflow = true;
      return;
    }
    *m_pPos++ = opcode;
    memcpy(m_pPos, f.bytes, n);
    m_pPos += n;
  }

  uint8_t *m_pStart, *m_pPos, *m_pEnd;
  bool m_bOverflow;
};

} // namespace v3d

#endif
