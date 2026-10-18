/*
 * gamepad.h - per-port gamepad input and settings for BMA800XL.
 *
 * The host input is defined in host.h; Atari actions follow atari800's
 * platform.h and SDL controller interface.
 * This file is part of BMA800XL. See LICENSE for licensing details.
 */

#ifndef BMA800XL_PORT_GAMEPAD_H_
#define BMA800XL_PORT_GAMEPAD_H_

#include "../gamepad_profiles.h"

void BMA_GamepadInit(void);
void BMA_GamepadSettingsChanged(void);
void BMA_GamepadUpdate(void);
void BMA_GamepadMenuWatch(int enabled);
int BMA_GamepadMenuChanged(void);
void BMA_GamepadKeys(unsigned char *down);
int BMA_GamepadConsole(void);
int BMA_GamepadAction(int dispatch);
int BMA_GamepadJoystick(int port);

int BMA_PortPad(int port);
int BMA_PadBinding(int port, int button);
int BMA_PadBindingValid(int binding);
int BMA_SavePadBindings(int port, const int *bindings);
int BMA_ResetPadBindings(int port);
void BMA_DefaultPadBindings(int port, int *bindings);
const char *BMA_PadButtonName(int button);
void BMA_PadButtonLabel(int pad, int button, char *text, unsigned size);
int BMA_PadButtonIndex(int pad, int position);
const char *BMA_PadActionName(int action);
int BMA_CapturePadButton(int port);

#endif
