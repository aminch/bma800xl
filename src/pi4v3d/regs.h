//
// regs.h
//
// Addresses and registers of the Pi 4's 3D GPU (V3D 4.2), and of the power
// controls it needs.
//
// Register offsets and fields adapted from Linux,
// drivers/gpu/drm/v3d/v3d_regs.h and drivers/pmdomain/bcm/bcm2835-power.c,
// Copyright (C) 2017-2018 Broadcom, GPL 2.0 or later. Addresses from the
// BCM2711 device tree (bcm2711.dtsi, bcm2711-rpi-ds.dtsi).
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4V3D_REGS_H_
#define BMA800XL_PI4V3D_REGS_H_

#include <stdint.h>

namespace v3d {

// ARM addresses (low peripheral mode).
const uintptr_t HubBase = 0xFEC00000;
const uintptr_t CoreBase = 0xFEC04000;   // core 0
const uintptr_t PmBase = 0xFE100000;
const uintptr_t AsbBase = 0xFEC11000;    // the V3D's async bus bridges

// Firmware mailbox.
const uint32_t TagSetDomainState = 0x00038030;
const uint32_t DomainV3d = 10;
const uint32_t ClockV3d = 5;

// Hub registers.
const uint32_t HubAxiCfg = 0x00000;
const uint32_t HubIdent0 = 0x00008;
const uint32_t HubIdent1 = 0x0000C;      // 3:0 tech version, 7:4 revision, 11:8 cores
const uint32_t HubIdent2 = 0x00010;
const uint32_t MmucControl = 0x01000;
const uint32_t MmucEnable = 1u << 0;
const uint32_t MmucFlush = 1u << 1;
const uint32_t MmucFlushing = 1u << 2;
const uint32_t MmuCtl = 0x01200;
const uint32_t MmuEnable = 1u << 0;
const uint32_t MmuTlbClear = 1u << 2;
const uint32_t MmuTlbClearing = 1u << 7;
const uint32_t MmuWriteViolationInt = 1u << 10;
const uint32_t MmuWriteViolationAbort = 1u << 11;
const uint32_t MmuPtInvalidEnable = 1u << 16;
const uint32_t MmuPtInvalidInt = 1u << 18;
const uint32_t MmuPtInvalidAbort = 1u << 19;
const uint32_t MmuCapExceededInt = 1u << 25;
const uint32_t MmuCapExceededAbort = 1u << 26;
const uint32_t MmuPtPaBase = 0x01204;    // page table address >> 12
const uint32_t MmuVioId = 0x0122C;
const uint32_t MmuIllegalAddr = 0x01230;
const uint32_t MmuIllegalAddrEnable = 1u << 31;
const uint32_t MmuVioAddr = 0x01234;
const uint32_t MmuDebugInfo = 0x01238;

// MMU page table entries: 4 KB pages.
const unsigned PageShift = 12;
const uint32_t PteValid = 1u << 28;
const uint32_t PteWriteable = 1u << 29;

// Core registers.
const uint32_t CtlIdent0 = 0x00000;
const uint32_t CtlSlcActl = 0x00024;     // slice caches: write 0xF to each field to invalidate
const uint32_t CtlL2tcActl = 0x00030;
const uint32_t L2tFlush = 1u << 0;       // with mode 0 (flush) in bits 2:1
const uint32_t CtlL2tFlushStart = 0x00034;
const uint32_t CtlL2tFlushEnd = 0x00038;
const uint32_t CtlIntSts = 0x00050;
const uint32_t CleCt0Cs = 0x00100;
const uint32_t CleCt1Cs = 0x00104;
const uint32_t CleCt0Ca = 0x00110;
const uint32_t CleCt1Ca = 0x00114;
const uint32_t CleBfc = 0x00134;         // binning frames completed
const uint32_t CleRfc = 0x00138;         // rendering frames completed
const uint32_t CleCt0Qts = 0x0015C;      // binning tile state address
const uint32_t CleCt0QtsEnable = 1u << 1;
const uint32_t CleCt0Qba = 0x00160;      // binning control list start
const uint32_t CleCt1Qba = 0x00164;      // render control list start
const uint32_t CleCt0Qea = 0x00168;      // binning control list end; writing it starts the job
const uint32_t CleCt1Qea = 0x0016C;      // render control list end; writing it starts the job
const uint32_t CleCt0Qma = 0x00170;      // tile allocation memory
const uint32_t CleCt0Qms = 0x00174;      // and its size
const uint32_t PtbBpos = 0x0030C;        // binner overflow memory size
const uint32_t ErrStat = 0x00F20;

// Power manager: the GRAFX domain register; writes need the password.
const uint32_t PmGrafx = 0x10C;
const uint32_t PmPassword = 0x5A000000;
const uint32_t PmV3dResetN = 1u << 6;

// Bus bridges: slave and master control; bit 0 requests stop, bit 1
// acknowledges it.
const uint32_t AsbSlaveCtrl = 0x08;
const uint32_t AsbMasterCtrl = 0x0C;
const uint32_t AsbRequestStop = 1u << 0;
const uint32_t AsbAck = 1u << 1;

} // namespace v3d

#endif
