/*
 * Copyright (C) 2012 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __I2C_BL_H
#define __I2C_BL_H

#if 0 def CONFIG_BACKLIGHT_I2C_BL
void lm3530_lcd_backlight_set_level(int level);
void lm3530_lcd_backlight_pwm_disable(void);
int lm3530_lcd_backlight_on_status(void);
#endif

struct i2c_bl_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
};

#endif
