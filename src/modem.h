//
// modem.h
//
// The TCP connection behind the R: device (src/port/modem.c): a Circle
// task that dials, sends and receives, and the host_modem_* functions
// (src/host.h) the emulator uses to talk to it through two ring buffers.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_MODEM_H_
#define BMA800XL_MODEM_H_

class CNetSubSystem;

// Starts the modem task. Call once, on core 0, when the network starts.
void StartModem(CNetSubSystem *network);

#endif
