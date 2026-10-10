/*
 * bma800xl.h - shared declarations for the BMA800XL atari800 port.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMA800XL_PORT_H_
#define BMA800XL_PORT_H_

/* keyboard.c */
extern int BMA_keypad_joystick;
extern int BMA_arrow_joystick;
void BMA_KeyboardInit(void);
int BMA_CharToAkey(int c, int shiftctrl);

/* platform.c */
void bma800xl_main(void);

#endif /* BMA800XL_PORT_H_ */
