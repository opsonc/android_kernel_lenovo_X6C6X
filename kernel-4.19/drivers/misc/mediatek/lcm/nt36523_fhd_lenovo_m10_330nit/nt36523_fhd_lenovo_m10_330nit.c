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

static unsigned int ENP = 496; //gpio167
static unsigned int ENN = 497; //GPIO168
static unsigned int RST = 374; //gpio45
//static unsigned int BKL = 483; //gpio154
static unsigned int BOOST = 506; //GPIO177
static unsigned int LCD_TP_RST = 501;  //gpio172

#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN
#define GPIO_LCD_RST        RST
//#define GPIO_BLK_EN         BKL
#define GPIO_BOOST_EN         BOOST
#define GPIO_LCD_TP_RST      LCD_TP_RST

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

#define LCM_DENSITY (216)

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
extern bool nvt_gesture_flag;

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
  	{ 0xF0,1,{0xAA}},
	{ 0xF1,1,{0x55}},
	{ 0xF2,1,{0x99}},
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xFF,1,{0x26}},
	
	{ 0xFB,1,{0x01}},
	{ 0xA7,1,{0x80}},
	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x05, 1, {0xD1} },
	{0x07, 1, {0x87} },
	{0x08, 1, {0x4B} },
	{0x0E, 1, {0x91} },
	{0x0F, 1, {0x69} },
	//{0x1F, 1, {0x00} },
	//{0x69, 1, {0xA9} },
	//{0x6D, 1, {0x33} },

	{0x95, 1, {0xF5} },
	{0x96, 1, {0xF5} },
	{0x9D, 1, {0x14} },
	{0x9E, 1, {0x14} },
	
	{ 0x6D,1,{0x33}},
	{ 0x69,1,{0x98}},
	{0x75, 1, {0xA2} },
	{ 0x77,1,{0xB3}},
	//test mode}},
	//{ 0x58,1,{0x40}},
	{ 0x0D,1,{0x63}},
	
	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0x91, 1, {0x44} },
	{0x92, 1, {0x7B} },
	{0x93, 1, {0x1A} },
	{0x94, 1, {0x5B} },
	//1200x1920}},
	{0x60, 1, {0x96} },
	{0x61, 1, {0x80} },
	{0x63, 1, {0x70} },
	{0xC2, 1, {0xCC} },
	//{0x9B, 1, {0x0F} },
	//{0x9A, 1, {0x08} },
	//{0xA5, 1, {0x00} },
	//{0xA6, 1, {0x41} },
	{0x00, 1, {0x03} },
	{0x01, 1, {0x03} },
	{0x02, 1, {0x03} },
	{0x03, 1, {0x03} },
	{0x04, 1, {0x03} },
	{0x05, 1, {0x03} },
	{0x06, 1, {0x03} },
	{0x07, 1, {0x03} },
	{ 0x08,1,{0x22}},
	{0x09, 1, {0x06} },
	{0x0A, 1, {0x05} },
	{0x0B, 1, {0x1D} },
	{0x0C, 1, {0x1C} },
	{0x0D, 1, {0x11} },
	{0x0E, 1, {0x10} },
	{0x0F, 1, {0x0F} },
	{0x10, 1, {0x0E} },
	{0x11, 1, {0x0D} },
	{0x12, 1, {0x0C} },
	{0x13, 1, {0x04} },
	{0x14, 1, {0x03} },
	{0x15, 1, {0x03} },
	{0x16, 1, {0x03} },
	{0x17, 1, {0x03} },
	{0x18, 1, {0x03} },
	{0x19, 1, {0x03} },
	{0x1A, 1, {0x03} },
	{0x1B, 1, {0x03} },
	{0x1C, 1, {0x03} },
	{0x1D, 1, {0x03} },
	{ 0x1E,1,{0x22}},
	{0x1F, 1, {0x06} },
	{0x20, 1, {0x05} },
	{0x21, 1, {0x1D} },
	{0x22, 1, {0x1C} },
	{0x23, 1, {0x11} },
	{0x24, 1, {0x10} },
	{0x25, 1, {0x0F} },
	{0x26, 1, {0x0E} },
	{0x27, 1, {0x0D} },
	{0x28, 1, {0x0C} },
	{0x29, 1, {0x04} },
	{0x2A, 1, {0x03} },
	{0x2B, 1, {0x03} },
	//STV 
	{ 0x2F,1,{0x04}},
	{ 0x30,1,{0x32}},
	{ 0x31,1,{0x41}},
	{ 0x33,1,{0x32}},
	{ 0x34,1,{0x04}},
	{ 0x35,1,{0x41}},
	{ 0x37,1,{0x44}},
	{ 0x38,1,{0x40}},
	{0x39, 1, {0x00} },
	{ 0x3A,1,{0x01}},
	{0x3B, 1, {0x43} },
	{0x3D, 1, {0x93} },
	{ 0xAB,1,{0x44}},
	{ 0xAC,1,{0x40}},
	//GCK 
	{0x4D, 1, {0x21} },
	{0x4E, 1, {0x43} },
	{0x4F, 1, {0x65} },
	{0x51, 1, {0x56} },
	{0x52, 1, {0x34} },
	{0x53, 1, {0x12} },
	{ 0x55,2,{0x83,0x03}},
	{0x56, 1, {0x06} },
	{0x58, 1, {0x21} },
	{ 0x59,1,{0x40}},
	{ 0x5A,1,{0x01}},
	{0x5B, 1, {0x43} },
	{0x5E, 2, {0x00, 0x0C} },
	{0x5F, 1, {0x00} },
	//POL 
	{0x7A, 1, {0x00} },
	{0x7B, 1, {0x00} },
	{0x7C, 1, {0x00} },
	{0x7D, 1, {0x00} },
	{0x7E, 1, {0x20} },
	{0x7F, 1, {0x3C} },
	{0x80, 1, {0x00} },
	{0x81, 1, {0x00} },
	
	{0x82, 1, {0x08} },
	
	//{0x83, 1, {0x1B} },
	{0x97, 1, {0x02} },
	{ 0xC5,1,{0x10}},
	//SOE 
	{0xD7, 1, {0x55} },
	{0xD8, 1, {0x55} },
	{0xD9, 1, {0x23} },
	{0xDA, 1, {0x05} },
	{0xDB, 1, {0x01} },
	{0xDC, 1, {0x7B} },
	{0xDD, 1, {0x55} },
	{0xDE, 1, {0x27} },
	{0xDF, 1, {0x01} },
	{0xE0, 1, {0x7B} },
	{0xE1, 1, {0x01} },
	{0xE2, 1, {0x7B} },
	{0xE3, 1, {0x01} },
	{0xE4, 1, {0x7B} },
	{0xE5, 1, {0x01} },
	{0xE6, 1, {0x7B} },
	{0xE7, 1, {0x00} },
	{0xE8, 1, {0x00} },
	{0xE9, 1, {0x01} },
	{0xEA, 1, {0x7B} },
	{0xEB, 1, {0x01} },
	{0xEE, 1, {0x7B} },
	{0xEF, 1, {0x01} },
	{0xF0, 1, {0x7B} },
	
	{ 0xB6,12,{0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x00}},
	
	{ 0xFF,1,{0x25}},
	{ 0xFB,1,{0x01}},
	//Auto porch }},
	//{ 0x05,1,{0x00}},
	//LINE_N }},
	{ 0x1E,1,{0x00}},
	{ 0x1F,1,{0x01}},
	{ 0x20,1,{0x43}},
	//LINE_N+1 }},
	{ 0x25,1,{0x00}},
	{ 0x26,1,{0x01}},
	{ 0x27,1,{0x43}},
	//TP3  }},
	{ 0x3F,1,{0x80}},
	{ 0x40,1,{0x00}},
	{ 0x43,1,{0x00}},
	//STV_TP3}},
	{ 0x44,1,{0x01}},
	{ 0x45,1,{0x43}},
	//GCK_TP3}},
	{ 0x48,1,{0x01}},
	{ 0x49,1,{0x43}},
	
	//LSTP0 }},
	{ 0x5B,1,{0x80}},
	{ 0x5C,1,{0x00}},
	//STV_LSTP0}},
	{ 0x5D,1,{0x01}},
	{ 0x5E,1,{0x43}},
	//GCK_LSTP0}},
	{ 0x61,1,{0x01}},
	{ 0x62,1,{0x43}},
	{ 0x68,1,{0x0C}},
	
	{ 0xFF,1,{0x26}},
	{ 0xFB,1,{0x01}},
	
	{ 0x00,1,{0xA1}},
	{ 0x02,1,{0x31}},
	{ 0x04,1,{0x28}},
	{ 0x0A,1,{0xF2}},
	{ 0x06,1,{0x20}},
	{ 0x0C,1,{0x13}},
	{ 0x0D,1,{0x0A}},
	{ 0x0F,1,{0x0A}},
	{ 0x11,1,{0x00}},
	{ 0x12,1,{0x50}},
	{ 0x13,1,{0x4D}},
	{ 0x14,1,{0x61}},
	{ 0x15,1,{0x00}},
	{ 0x16,1,{0x10}},
	{ 0x17,1,{0xA0}},
	{ 0x18,1,{0x86}},
	
	{ 0x19,1,{0x0C}},
	{ 0x1A,1,{0x00}},
	{ 0x1B,1,{0x0C}},
	{ 0x1C,1,{0x00}},
	{ 0x2A,1,{0x0C}},
	{ 0x2B,1,{0x00}},
	
	//RTNA_LINE N/ N+1 }},
	{ 0x1D,1,{0x00}},
	{ 0x1E,1,{0x7D}},
	{ 0x1F,1,{0x7D}},
	
	//RTNA_TP3 }},
	{ 0x2F,1,{0x05}},
	{ 0x30,1,{0x7D}},
	{ 0x31,1,{0x02}},
	{ 0x32,1,{0x77}},
	{ 0x33,1,{0x91}},
	{ 0x34,1,{0x78}},
	{ 0x35,1,{0x16}},
	//LAST_TP0 }},
	{ 0x39,1,{0x0C}},
	{ 0x3A,1,{0x7E}},
	{ 0x3B,1,{0x06}},
	
	//Dynamic Long H }},
	{ 0xC8,1,{0x04}},
	{ 0xC9,1,{0x84}},
	{ 0xCA,1,{0x4E}},
	{ 0xCB,1,{0x00}},
	{ 0xA9,1,{0x4C}},
	{ 0xAA,1,{0x4A}},
	{ 0xAB,1,{0x49}},
	{ 0xAC,1,{0x47}},
	{ 0xAD,1,{0x45}},
	{ 0xAE,1,{0x43}},
	
	{ 0xFF,1,{0x27}},
	{ 0xFB,1,{0x01}},
	{ 0xD1,1,{0x24}},
	{ 0xD2,1,{0x30}},
	//{ 0xD1,1,{0x45}},
	//{ 0xD2,1,{0x67}},
	//LongV mode}},
	{ 0xC0,1,{0x18}},
	{ 0xC1,1,{0x00}},
	{ 0xC2,1,{0x00}},
	
	{ 0x56,1,{0x06}},
	//FR0 }},
	{ 0x58,1,{0x80}},
	{ 0x59,1,{0x50}},
	{ 0x5A,1,{0x00}},
	{ 0x5B,1,{0x14}},
	{ 0x5C,1,{0x00}},
	{ 0x5D,1,{0x01}},
	{ 0x5E,1,{0x20}},
	{ 0x5F,1,{0x10}},
	{ 0x60,1,{0x00}},
	{ 0x61,1,{0x1B}},
	{ 0x62,1,{0x00}},
	{ 0x63,1,{0x01}},
	{ 0x64,1,{0x22}},
	{ 0x65,1,{0x1A}},
	{ 0x66,1,{0x00}},
	{ 0x67,1,{0x01}},
	{ 0x68,1,{0x23}},
	//TC}},
	//HT:VGHO->18V}},
	//LT:VGHO->9.7V}},
	{ 0x98,1,{0x01}},
	{ 0xB4,1,{0x03}},
	{ 0x9B,1,{0xBD}},
	{ 0xA0,1,{0x90}},
	{ 0xAB,1,{0x14}},
	{ 0xBC,1,{0x0C}},
	{ 0xBD,1,{0x28}},
	
	{ 0xFF,1,{0x2A}},
	{ 0xFB,1,{0x01}},
	//PEN_EN=1,{1,{UL_FREQ=0 }},
	{ 0x22,1,{0x2F}},
	{ 0x23,1,{0x08}},
	//FR0 (60Hz,1,{IVE4 VFP=26) }},
	{ 0x24,1,{0x00}},
	{ 0x25,1,{0x7B}},
	{ 0x26,1,{0xF8}},
	{ 0x27,1,{0x00}},
	{ 0x28,1,{0x1A}},
	{ 0x29,1,{0x00}},
	{ 0x2A,1,{0x1A}},
	{ 0x2B,1,{0x00}},
	{ 0x2D,1,{0x1A}},
	//power sequence}},
	{ 0x64,1,{0x96}},
	{ 0x65,1,{0x00}},
	{ 0x66,1,{0x00}},
	{ 0x6A,1,{0x96}},
	{ 0x6B,1,{0x00}},
	{ 0x6C,1,{0x00}},
	{ 0x70,1,{0x92}},
	{ 0x71,1,{0x00}},
	{ 0x72,1,{0x00}},
	//abnormal off }},
	{ 0xA2,1,{0x33}},
	{ 0xA3,1,{0x30}},
	{ 0xA4,1,{0xC0}},
	
	{ 0xFF,1,{0xF0}},
	{ 0xFB,1,{0x01}},
	{ 0x3A,1,{0x08}},
	
	{ 0xFF,1,{0x10}},

	{ 0xB9,1,{0x01}},
	
	{ 0xFF,1,{0x20}},

	{ 0x18,1,{0x40}},
	
	{ 0xFF,1,{0x10}},

	{ 0xB9,1,{0x02}},


		

	{ 0xFF,1,{0x2A}},
	{ 0xFB,1,{0x01}},
	{ 0xF1,1,{0x03}},
	
	//CABC}},
	{ 0xFF,1,{0x23}},
	{ 0xFB,1,{0x01}},
	{ 0x07,1,{0x00}},
	{ 0x08,1,{0x08}},
	{ 0x09,1,{0x00}},
	{ 0xFF,1,{0xD0}},
	{ 0xFB,1,{0x01}},
	{ 0x02,1,{0x77}},
	{ 0x09,1,{0xBF}},

	{0xFF,1,{0x27}},
	{0xFB,1,{0x01}},
	{0xDE,1,{0xFF}},
	{0xDF,1,{0xFD}},

	{ 0xFF,1,{0x10}},
	
	{ 0xFB,1,{0x01}},
	{ 0xBB,1,{0x13}},
	{ 0x53,1,{0x2C}},
	{ 0x3B,5,{0x03,0x5B,0x1A,0x04,0x04}},
	{ 0x35,1,{0x00}},
	{ 0x11, 0, {} },
	{REGFLAG_DELAY, 100, {} },
	{ 0x29, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{ 0xF0,1,{0x55}},
	{ 0xF1,1,{0xAA}},
	{ 0xF2,1,{0x66}}
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
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
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

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 89;
	params->dsi.vertical_frontporch = 26;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 2;
	params->dsi.horizontal_backporch = 12;
	params->dsi.horizontal_frontporch = 12;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

	params->dsi.PLL_CLOCK = 550;//550;

#if 1  /*non-continuous clk*/
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

	lcm_set_gpio_output(GPIO_BOOST_EN, 1);
	MDELAY(1);
	//lcm_set_gpio_output(GPIO_BLK_EN, 1);
	//MDELAY(1);
	/* enable backlight*/
	//lcm_set_gpio_output(GPIO_PWM_EN, 1);
	lcm_set_gpio_output(GPIO_LCD_TP_RST,0);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_TP_RST,1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);


	LCM_LOGI("[hyper] %s exit\n", __func__);
}


static void lcm_suspend_power(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

	if(!nvt_gesture_flag) {
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		MDELAY(10);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
	}
	/* disable backlight*/
	lcm_set_gpio_output(GPIO_BOOST_EN, 0);
	MDELAY(1);

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
	MDELAY(10);

	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_init(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);
	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

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
	//lcm_set_gpio_output(GPIO_BLK_EN, 1);


	LCM_LOGI("[hyper] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	LCM_LOGI("[hyper] %s enter\n", __func__);

	push_table(NULL, lcm_suspend_setting,
	sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

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
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_RST, 1);
	MDELAY(10);

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


struct LCM_DRIVER nt36523_fhd_lenovo_m10_330nit_lcm_drv = {
	.name = "nt36523_fhd_lenovo_k10_330nit",
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

