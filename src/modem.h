//
// modem.h
//
// BMA800XL - A bare metal Atari 800XL emulator for Raspberry Pi
// Copyright (C) 2026 aminch <https://github.com/aminch>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// The TCP connection behind the R: device (src/port/modem.c): a Circle
// task that dials, sends and receives, and the host_modem_* functions
// (src/host.h) the emulator uses to talk to it through two ring buffers.
//

#ifndef BMA800XL_MODEM_H_
#define BMA800XL_MODEM_H_

class CNetSubSystem;

// Starts the modem task. Call once, on core 0, when the network starts.
void StartModem(CNetSubSystem *network);

#endif
