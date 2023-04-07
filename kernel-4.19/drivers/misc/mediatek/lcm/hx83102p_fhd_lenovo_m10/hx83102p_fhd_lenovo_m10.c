/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#ifdef BUILD_LK
#define GPIO_LCM_ID0    GPIO21
#define GPIO_LCM_ID1    GPIO29
#endif
extern bool himax_gesture_flag;
static unsigned int ENP = 496; //gpio167
static unsigned int ENN = 497; //GPIO168
static unsigned int RST = 374; //gpio45
//static unsigned int PWM = 372; //gpio43
static unsigned int LCD_TP_RST = 501;  //gpio172
//static unsigned int BKL = 483; //gpio154
static unsigned int BOOST = 506; //GPIO177

#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN
#define GPIO_LCD_RST        RST
#define GPIO_LCD_TP_RST      LCD_TP_RST
//#define GPIO_BLK_EN         BKL
#define GPIO_BOOST_EN         BOOST
//#define GPIO_PWM_EN         PWM

#define IC_id_addr			0xF7
#define IC_id				0x46

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;


#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1200)
#define FRAME_HEIGHT (1920)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH	(138780)
#define LCM_PHYSICAL_HEIGHT	(222048)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 30, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0xB9, 3, {0x83, 0x10, 0x2e} },
	{0xB1, 1, {0x21} },
	{REGFLAG_DELAY, 10, {} }
};
static struct LCM_setting_table lcm_suspend_setting_gesture[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 30, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{ 0xB9, 3, { 0x83, 0x10, 0x2E } },
	{ 0xD1, 4, { 0x67, 0x0C, 0xFF, 0x05 } },
	{ 0xB1, 17, { 0x10, 0xFA, 0xAF, 0xAF, 0x2B, 0x2B, 0xC1, 0x75, 0x39, 0x36, 0x36, 0x36, 0x36, 0x22, 0x21, 0x15, 0x00 } },
	{ 0xB2, 16, { 0x00, 0xB0, 0x47, 0x80, 0x00, 0x22, 0x62, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x15, 0x20, 0xD7, 0x00 } },
	{ 0xB4, 16, { 0x7F, 0x64, 0x7F, 0x64, 0x89, 0x6C, 0x68, 0x50, 0x01, 0x9C, 0x01, 0x58, 0x00, 0xFF, 0x00, 0xFF } },
	{ 0xBF, 3, { 0xFC, 0x85, 0x80 } },
	{ 0xD2, 2, { 0x2B, 0x2B } },
	{ 0xD3, 43, { 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x14, 0x0C, 0x27, 0x27, 0x22, 0x2F, 0x1F, 0x1F, 0x04, 0x04, 0x32, 0x10, 0x1D, 0x00, 0x1D, 0x32, 0x17, 0xB6, 0x07, 0xB6, 0x32, 0x10, 0x1A, 0x00, 0x1A, 0x00, 0x00, 0x1A, 0x3A, 0x01, 0x55, 0x1A, 0x39, 0x60, 0x76, 0x0F } },
	{ 0xE0, 46, { 0x00, 0x03, 0x0B, 0x11, 0x17, 0x24, 0x3B, 0x44, 0x4C, 0x4A, 0x68, 0x71, 0x79, 0x8B, 0x8A, 0x94, 0x9E, 0xB1, 0xB0, 0x57, 0x5E, 0x69, 0x73, 0x00, 0x03, 0x0B, 0x11, 0x17, 0x24, 0x3B, 0x44, 0x4C, 0x4A, 0x68, 0x71, 0x79, 0x8B, 0x8A, 0x94, 0x9E, 0xB1, 0xB0, 0x57, 0x5E, 0x69, 0x73 } },
	{ 0xBD, 1, { 0x01 } },
	{ 0xB1, 4, { 0x01, 0x9B, 0x01, 0x31 } },
	{ 0xCB, 10, { 0xF4, 0x36, 0x12, 0x16, 0xC0, 0x28, 0x6C, 0x85, 0x3F, 0x04 } },
	{ 0xD3, 11, { 0x01, 0x00, 0xFC, 0x00, 0x00, 0x11, 0x10, 0x00, 0x0E, 0x00, 0x01 } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xB4, 6, { 0x4E, 0x00, 0x33, 0x11, 0x33, 0x88 } },
	{ 0xBF, 3, { 0xF2, 0x00, 0x02 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xC0, 14, { 0x23, 0x23, 0x22, 0x11, 0xA2, 0x17, 0x00, 0x80, 0x00, 0x00, 0x08, 0x00, 0x63, 0x63 } },
	{ 0xC6, 1, { 0xF9 } },
	{ 0xC7, 1, { 0x30 } },
	{ 0xC8, 8, { 0x00, 0x04, 0x04, 0x00, 0x00, 0x82, 0x13, 0xFF } },
	{ 0xD0, 3, { 0x07, 0x04, 0x05 } },
	{ 0xD5, 44, { 0x2D, 0x2D, 0x2D, 0x2D, 0x18, 0x18, 0x18, 0x18, 0x21, 0x20, 0x21, 0x20, 0x25, 0x24, 0x25, 0x24, 0x18, 0x18, 0x18, 0x18, 0x39, 0x39, 0x39, 0x39, 0x2E, 0x2E, 0x2E, 0x2E, 0x03, 0x02, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00, 0x07, 0x06, 0x07, 0x06, 0x05, 0x04, 0x05, 0x04 } },
	{ 0xD6, 44, { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x20, 0x21, 0x20, 0x21, 0x24, 0x25, 0x24, 0x25, 0x2D, 0x2D, 0x2D, 0x2D, 0x39, 0x39, 0x39, 0x39, 0x2E, 0x2E, 0x2E, 0x2E, 0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07, 0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03 } },
	{ 0xE7, 23, { 0x12, 0x13, 0x02, 0x02, 0x48, 0x48, 0x0E, 0x0E, 0x18, 0x23, 0x26, 0x78, 0x25, 0x7F, 0x01, 0x27, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x68 } },
	{ 0xBD, 1, { 0x01 } },
	{ 0xE7, 7, { 0x02, 0x37, 0x01, 0x8E, 0x0D, 0xD3, 0x0E } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xE7, 28, { 0xFF, 0x01, 0xFD, 0x01, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x02, 0x40 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xBA, 8, { 0x70, 0x03, 0xA8, 0x85, 0xF2, 0x00, 0xC0, 0x0D } },
	{ 0xBD, 1, { 0x02 } },
	{ 0xD8, 12, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0 } },
	{ 0xBD, 1, { 0x03 } },
	{ 0xD8, 24, { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0x55, 0x55, 0x55, 0x55, 0x55, 0x50, 0x55, 0x55, 0x55, 0x55, 0x55, 0x50 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xCC, 1, { 0x02 } },
	{ 0xBD, 1, { 0x03 } },
	{ 0xB2, 1, { 0x80 } },
	{ 0xBD, 1, { 0x00 } },
	{ 0xE1, 2, { 0x02, 0x04 } },
	{ 0x35, 1, { 0x00 } },
	{0x51, 2, {0x00, 0x00} },
	{0x53, 1, {0x2C} },
	{0x55, 1, {0x00} },
	{REGFLAG_DELAY, 5, {} },
	{0xB9, 3, {0x83, 0x10, 0x2e} },
	{0xC9, 4, {0x42, 0x05, 0x93, 0x01} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 60, {} },
	{0xB2, 16, {0x00, 0xB0, 0x47, 0x80, 0x00, 0x22, 0x62, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x15, 0x20, 0xD7, 0x00} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} }
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x0F, 0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table ata_check[] = {
	{0xFF, 1, {0x21} },
	{0xFB, 1, {0x01} }
};

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	int ret;

	ret = gpio_request(GPIO, "GPIO");
	if (ret < 0)
		pr_err("[%s]: GPIO requset fail!\n", __func__);

	if (gpio_is_valid(GPIO)) {
		ret = gpio_direction_output(GPIO, output);
			if (ret < 0)
				pr_err("[%s]: failed to set output", __func__);
	}

	gpio_free(GPIO);
}

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	//params->physical_width_um = LCM_PHYSICAL_WIDTH;
	//params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	//params->density = LCM_DENSITY;

	params->dsi.mode = BURST_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode_enable = 1;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 28;
	params->dsi.vertical_frontporch = 98;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 14;
	params->dsi.horizontal_backporch = 94;
	params->dsi.horizontal_frontporch = 95;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

	params->dsi.PLL_CLOCK = 550;//550;

#if 0  /*non-continuous clk*/
	params->dsi.cont_clock = 0;
	params->dsi.clk_lp_per_line_enable = 1;
#else /*continuous clk*/
	params->dsi.cont_clock = 1;
#endif
  //  params->max_refresh_rate=60;
	//params->min_refresh_rate=60;
	  //params->=60;


	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;

}

static void lcm_init_power(void)
{
	LCM_LOGI("[hyper] hyper %s enter\n", __func__);

	/* enable backlight*/
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(40);
	//lcm_set_gpio_output(GPIO_BLK_EN, 1);

    lcm_set_gpio_output(GPIO_LCD_TP_RST,0);
	MDELAY(5);
 	lcm_set_gpio_output(GPIO_LCD_TP_RST,1);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(156);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}


static void lcm_suspend_power(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

	if(!himax_gesture_flag){
	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_TP_RST,0);
	MDELAY(5);
	}

	if(!himax_gesture_flag){
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
	}
	/* disable backlight*/
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
	MDELAY(5);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_resume_power(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

  	lcm_set_gpio_output(GPIO_LCD_TP_RST,0);
	MDELAY(5);
 	lcm_set_gpio_output(GPIO_LCD_TP_RST,1);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_init(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
	/* enable backlight*/
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(40);
	//lcm_set_gpio_output(GPIO_BLK_EN, 1);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);
	
	if(!himax_gesture_flag){
	
	push_table(NULL, lcm_suspend_setting,
	sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	}
	else{
		
	push_table(NULL, lcm_suspend_setting_gesture,
	sizeof(lcm_suspend_setting_gesture) / sizeof(struct LCM_setting_table), 1);
	}
	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_resume(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(156);

	push_table(NULL, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);

	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
  	MDELAY(1);
  	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(40);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
		       unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ERROR] [0x9C]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x9C]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned char ata_id = 0xFF;
	unsigned char buffer_ata[4];
	unsigned int array[16];

	push_table(NULL, ata_check,
	sizeof(ata_check) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00013700;/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(IC_id_addr, buffer_ata, 1);
	ata_id = buffer_ata[0];
	LCM_LOGI("%s, ata_check, hyper ata_id = 0x%x\n", __func__, ata_id);

	if (ata_id == IC_id) {
		LCM_LOGI("%s, hyper ata_id compare success\n", __func__);
		return 1;
	}

	return 0;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("hyper %s,hyper backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}


struct LCM_DRIVER hx83102p_fhd_lenovo_m10_lcm_drv = {
	.name = "hx83102p_fhd_lenovo_k10_330nit",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};
