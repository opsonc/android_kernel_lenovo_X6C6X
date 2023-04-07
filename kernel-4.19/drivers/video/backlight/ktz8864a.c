/*
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/platform_data/ktz8864a_bl.h>

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *lcd_bl_i2c_client;
static  unsigned char  level_value_msb[256] =  {0,1,1,1,3,4,6,7,9,9,
                                                11,12,13,15,16,17,18,20,20,22,
                                                23,24,26,27,28,29,30,31,32,34,
                                                35,36,37,39,40,42,43,44,45,46,
                                                47,48,50,51,52,54,55,56,57,58,
                                                59,61,62,63,64,65,66,67,69,69,
                                                71,72,74,75,76,77,78,79,81,82,
                                                83,84,85,86,88,89,91,92,93,94,
                                                95,97,98,99,99,101,102,103,104,106,
                                                107,108,109,110,112,113,114,115,116,118,
                                                119,120,121,122,124,125,126,126,126,127,
                                                128,130,130,132,133,134,136,137,138,139,
                                                140,141,142,144,145,146,146,148,150,151,
                                                152,153,155,156,157,158,159,160,161,162,
                                                163,165,166,167,169,170,171,172,173,175,
                                                175,177,178,179,180,181,182,184,186,186,
                                                187,189,190,191,191,193,194,195,196,197,
                                                198,200,200,202,203,204,205,206,208,209,
                                                210, 211,212,213,214,216,217,218,219,221,
                                                222,223,223,225,226,227,228,230,231,232,
                                                233,234,235,237,237,239,240,241,242,244,
                                                244,245,247,248,249,250,252,252,254,255,
                                                255,255,255,255,255,255,255,255,255,255,
                                                255,255,255,255,255,255,255,255,255,255,
                                                255,255,255,255,255,255,255,255,255,255,
                                                255,255,255,255,255,255};
static DEFINE_MUTEX(read_lock);
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bl_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

static int lcd_bl_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

    if (NULL == lcd_bl_i2c_client) {
	LCD_BL_PRINT("[LCD][BL] lcd_bl_i2c_client is null!!\n");
	return -EINVAL;
    }
    ret = i2c_master_send(lcd_bl_i2c_client, write_data, 2);

    if (ret < 0)
	LCD_BL_PRINT("[LCD][BL] i2c write data fail !!\n");

    return ret;
}

static int lcd_bl_read_byte(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	if (NULL == lcd_bl_i2c_client) {
		LCD_BL_PRINT("[LCD][BL] lcd_bl_i2c_client is null!!\n");
		return -EINVAL;
	}

	mutex_lock(&read_lock);

	buffer[0] = regnum;
	res = i2c_master_send(lcd_bl_i2c_client, buffer, 0x1);
	if (res <= 0)	{
	  mutex_unlock(&read_lock);
	  LCD_BL_PRINT("read reg send res = %d\n", res);
	  return res;
	}
	res = i2c_master_recv(lcd_bl_i2c_client, reg_value, 0x1);
	if (res <= 0) {
	  mutex_unlock(&read_lock);
	  LCD_BL_PRINT("read reg recv res = %d\n", res);
	  return res;
	}
	mutex_unlock(&read_lock);

	return reg_value[0];
}
static bool main_bl = false;
int lcd_bl_dump_reg_ktz8864a(void)
{
/*
    if ((lcd_bl_read_byte(0x01)& 0x03) == 0x02)
		main_bl = true;
	else
		main_bl = false;
*/
	return main_bl;
}

EXPORT_SYMBOL(lcd_bl_dump_reg_ktz8864a);

int lcd_bl_set_led_brightness_ktz8864a(int value)//for led1 led2 on
{
	if(printk_ratelimit())
        printk("%s:hyper bl = %d\n", __func__, value);
	//value = (((1 << 11) - 1) * value + 127) / 255;   // by zhaowan
	if (value < 0) {
        printk("%d %s  invalid value=%d\n", __LINE__, __func__, value);
	 return 0;
	}

	if (value > 0) {
        lcd_bl_write_byte(0x04, 0x07);// lsb
        lcd_bl_write_byte(0x05, level_value_msb[value & 0xFF]);// msb
	} else {
        lcd_bl_write_byte(0x04, 0x00);// lsb
        lcd_bl_write_byte(0x05, 0x00);// msb
	}

//      printk("%s:chenpengbo02  hyper bl = 0x%x\n", __func__, lcd_bl_read_byte(0x02));
//      printk("%s:chenpengbo03  hyper bl = 0x%x\n", __func__, lcd_bl_read_byte(0x03));
//      printk("%s:chenpengbo05  hyper bl = 0x%x\n", __func__, lcd_bl_read_byte(0x05));
//      printk("%s:chenpengbo15  hyper bl = 0x%x\n", __func__, lcd_bl_read_byte(0x15));

	return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_brightness_ktz8864a);


#ifdef CONFIG_OF
static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,ktz8864a_bl", },
    {},
};
#endif

static const struct i2c_device_id lcd_bl_i2c_id[] = {
    {LCD_BL_I2C_ID_NAME_KTZ8864A, 0},
    {},
};

static struct i2c_driver lcd_bl_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
    .id_table = lcd_bl_i2c_id,
    .probe = lcd_bl_i2c_probe,
    .remove = lcd_bl_i2c_remove,
    .driver = {
	.owner = THIS_MODULE,
	.name = LCD_BL_I2C_ID_NAME_KTZ8864A,
#ifdef CONFIG_OF
	.of_match_table = i2c_of_match,
#endif
    },
};

static int lcd_bl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	if (NULL == client) {
         printk("[LCD][BL] i2c_client is NULL\n");
	   return -EINVAL;
	}

	lcd_bl_i2c_client = client;
	printk("--lyd, i2c led chen\n");
	//lcd_bl_dump_reg_ktz8864a();
	if ((lcd_bl_read_byte(0x01)& 0x03) == 0x02)
		main_bl = true;
	else
		main_bl = false;

	if(main_bl)
	{
           ret = lcd_bl_write_byte(0x0C, 0x28);
           ret = lcd_bl_write_byte(0x0D, 0x1E);
           ret = lcd_bl_write_byte(0x0E, 0x1E);
        //   ret = lcd_bl_write_byte(0x02, 0xE8);
           ret = lcd_bl_write_byte(0x02, 0xE8);
           ret = lcd_bl_write_byte(0x03, 0xCD); // set LED CURRENT RAMP 256ms,PWM_HYST set 0b101
           ret = lcd_bl_write_byte(0x11, 0x37);
           ret = lcd_bl_write_byte(0x09, 0x18);
           //  ret = lcd_bl_write_byte(0x15, 0xC0);
           ret = lcd_bl_write_byte(0x15, 0xB0);
           ret = lcd_bl_write_byte(0x08, 0x1F);   //
	}
	else
	{
          printk(" no support ktz8864a\n");
	}
    return 0;
}

static int lcd_bl_i2c_remove(struct i2c_client *client)
{
    lcd_bl_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}

static int __init lcd_bl_init(void)
{
    printk(KERN_ERR"lcd_bl_init\n");
    if (i2c_add_driver(&lcd_bl_i2c_driver)) {
	 LCD_BL_PRINT("[LCD][BL] Failed to register lcd_bl_i2c_driver!\n");
	 return -EINVAL;
    }

    return 0;
}

static void __exit lcd_bl_exit(void)
{
    i2c_del_driver(&lcd_bl_i2c_driver);
}

module_init(lcd_bl_init);
module_exit(lcd_bl_exit);

MODULE_AUTHOR("<zuoqiquan@mediatek.com>");
MODULE_DESCRIPTION("QCOM LCD BL I2C Driver");
MODULE_LICENSE("GPL");

