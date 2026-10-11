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
extern int BMA_joystick_test;
void BMA_KeyboardInit(void);
void BMA_InputSettingsChanged(void);
int BMA_PortPad(int port);
int BMA_JoystickState(int port);
int BMA_CharToAkey(int c, int shiftctrl);

/* platform.c */
void bma800xl_main(void);
const char *BMA_SdRoot(void);
double BMA_RefreshRate(void);
void BMA_VideoSettingsChanged(void);
void bma_request_power(int power_off);

/* menu.c: returns TRUE to go on to atari800's menu. */
int BMA_Menu(void);
int BMA_MenuRequests(void);

#endif /* BMA800XL_PORT_H_ */
