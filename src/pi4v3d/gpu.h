//
// gpu.h
//
// The Pi 4's 3D GPU (V3D 4.2): powers it up, sets up its MMU and runs
// rendering jobs written as control lists (cl.h).
//
// Sources:
//  - Linux, drivers/gpu/drm/v3d/ (v3d_drv.c, v3d_gem.c, v3d_mmu.c,
//    v3d_sched.c), Copyright (C) 2014-2018 Broadcom, GPL 2.0 or later: the
//    start-up order, cache and MMU set-up, starting a job.
//  - Linux, drivers/pmdomain/bcm/bcm2835-power.c (GPL 2.0 or later): the
//    BCM2711 V3D reset and bus bridges.
//  - Raspberry Pi firmware mailbox: the V3D power domain and clock.
//  - Mesa, src/broadcom/vulkan/v3dvx_meta_common.c (Copyright (C) 2021
//    Raspberry Pi Ltd, MIT licence): a rendering job without binning.
//  - Circle, include/circle/memory.h (HEAP_DMA30), synchronize64.h (cache
//    maintenance).
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4V3D_GPU_H_
#define BMA800XL_PI4V3D_GPU_H_

#include <stdint.h>
#include <stddef.h>

namespace v3d {

class Gpu {
public:
  Gpu(void);

  // Powers the GPU up and sets up its MMU. False if it can't be used.
  bool Initialize(void);
  bool Ready(void) const { return m_bReady; }

  // Memory the GPU can use: zeroed, ALIGN-byte aligned, below 1 GB. Never
  // freed.
  void *Allocate(size_t bytes, size_t align = 64);

  // Runs the binning control list from START to END (GPU addresses), with
  // TILE_ALLOC (SIZE bytes) for the tile lists and TILE_STATE for the
  // tiles' state, and waits for it, at most TIMEOUT_MS. False if it didn't
  // finish.
  bool Bin(uint32_t start, uint32_t end, uint32_t tile_alloc, uint32_t size,
           uint32_t tile_state, unsigned timeout_ms);

  // Runs the rendering control list from START to END and waits for it.
  bool Render(uint32_t start, uint32_t end, unsigned timeout_ms);

  // Clears a small picture to a known colour through the GPU and checks
  // it. Logs the result.
  bool ClearTest(void);

  // The GPU address of memory from Allocate().
  static uint32_t Address(const void *p) { return (uint32_t)(uintptr_t)p; }

private:
  bool PowerUp(void);
  bool Present(void);
  bool SetUpMmu(void);
  void InvalidateCaches(void);
  void LogState(const char *when);
  bool Wait(uint32_t counter, uint32_t before, unsigned timeout_ms, const char *what);

  uint32_t Hub(uint32_t reg) const;
  void SetHub(uint32_t reg, uint32_t value) const;
  uint32_t Core(uint32_t reg) const;
  void SetCore(uint32_t reg, uint32_t value) const;

  bool m_bReady;
  uint32_t *m_pPageTable;
  uint8_t *m_pScratch;
};

} // namespace v3d

#endif
