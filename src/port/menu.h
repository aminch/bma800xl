/*
 * menu.h - internal interfaces for BMA800XL menu screens.
 *
 * Shared helpers use atari800's ui.h menu interface.
 * This file is part of BMA800XL. See LICENSE for licensing details.
 */

#ifndef BMA800XL_PORT_MENU_H_
#define BMA800XL_PORT_MENU_H_

#include "atari.h"

void BMA_MenuSaveSetting(const char *name, const char *value);
void BMA_MenuSaveSettingInt(const char *name, int value);
int BMA_MenuSettingOn(const char *name, int default_value);
UWORD BMA_MenuCheckFlags(int on);
int BMA_MenuChoose(const char *title, const char *const choices[], int current);
int BMA_MenuValueIndex(const char *const values[], const char *value);

/* Returns TRUE if a controller setting requires a reboot. */
int BMA_MenuControllers(void);

#endif
