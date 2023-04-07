/*
 * Copyright (C) 2018 HUAQIN Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

enum bl_mode {
	BL_BRIGHTNESS_REGISTER = 0,
	BL_PWM_DUTY_CYCLE = 1,
	BL_REGISTER_PWM_COMBINED = 2
};

#define BL_I2C_ADDRESS			  0x11
#define BL_LED_ENABLE_ADDRESS	  0x08
#define BL_LED_ENABLE_BIT4      (1<<4)
#define BL_LED_ENABLE_BIT3      (1<<3)
#define BL_LED_ENABLE_BIT2      (1<<2)
#define BL_LED_ENABLE_BIT1      (1<<1)
#define BL_LED_ENABLE_BIT0      (1<<0)

#define BL_CONTROL_MODE_ADDRESS	  0x11
#define BL_CONTROL_MODE_BIT5      (1<<5)
#define BL_CONTROL_MODE_BIT6      (1<<6)


#define LCD_BL_PRINT printk

#define LCD_BL_I2C_ID_NAME_KTZ8864A "lcd_bl_ktz8864a"

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/

