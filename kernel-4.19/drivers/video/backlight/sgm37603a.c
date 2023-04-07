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

#include <linux/platform_data/sgm37603a_bl.h>

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *lcd_bl_i2c_client;
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
static bool main_bl;
void lcd_bl_dump_reg(void)
{
	LCD_BL_PRINT("[LCD][BL] hyper dump reg:0x1A val:0x%x\n", lcd_bl_read_byte(0x1A));
	LCD_BL_PRINT("[LCD][BL] hyper dump reg:0x1E val:0x%x\n", lcd_bl_read_byte(0x1E));
	LCD_BL_PRINT("[LCD][BL] hyper dump reg:0x10 val:0x%x\n", lcd_bl_read_byte(0x10));
	if (lcd_bl_read_byte(0x1A) == 0xFF)
		main_bl = true;
	else
		main_bl = false;
}

int lcd_bl_set_mode(enum bl_mode mode)
{
    int value;

	value = lcd_bl_read_byte(BL_CONTROL_MODE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl mode value +++ = 0x%x\n", value);

	switch (mode) {
	case BL_BRIGHTNESS_REGISTER:
		LCD_BL_PRINT("[LCD][BL] BL_BRIGHTNESS_REGISTER mode\n");
		value = value&(~BL_CONTROL_MODE_BIT6)&(~BL_CONTROL_MODE_BIT5);
		lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, value);
		break;

	case BL_PWM_DUTY_CYCLE:
		LCD_BL_PRINT("[LCD][BL] BL_PWM_DUTY_CYCLE mode\n");
		//lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, reg_value);
		break;

	case BL_REGISTER_PWM_COMBINED:
		LCD_BL_PRINT("[LCD][BL] BL_REGISTER_PWM_COMBINED mode\n");
		//lcd_bl_write_byte(BL_CONTROL_MODE_ADDRESS, reg_value);
		break;

	default:
		LCD_BL_PRINT("[LCD][BL] unknown mode\n");
		break;
	}

	value = lcd_bl_read_byte(0x11);
	LCD_BL_PRINT("[LCD][BL]bl mode value --- = 0x%x\n", value);

    return 0;
}

EXPORT_SYMBOL(lcd_bl_set_mode);

int lcd_bl_set_led_enable(void)//for led1 led2 on
{
    int value;

	value = lcd_bl_read_byte(BL_LED_ENABLE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl led enable value +++ = 0x%x\n", value);

	value = (value&(~BL_LED_ENABLE_BIT3))|BL_LED_ENABLE_BIT2|BL_LED_ENABLE_BIT1;
	lcd_bl_write_byte(BL_LED_ENABLE_ADDRESS, value);

	value = lcd_bl_read_byte(BL_LED_ENABLE_ADDRESS);
	LCD_BL_PRINT("[LCD][BL]bl led enable value --- = 0x%x\n", value);

    return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_enable);

int lcd_bl_set_led_brightness(int value)//for led1 led2 on
{
	if(printk_ratelimit())
		printk("%s:hyper bl = %d\n", __func__, value);
	value = (((1 << 12) - 1) * value + 127) / 255;
	if (value < 0) {
		printk("%d %s chenwenmin invalid value=%d\n", __LINE__, __func__, value);
		return 0;
	}

	if (value > 0) {
		lcd_bl_write_byte(0x1A, value & 0x0F);// lsb
		lcd_bl_write_byte(0x19, (value >> 4) & 0xFF);// msb
	} else {
		lcd_bl_write_byte(0x1A, 0x00);// lsb
		lcd_bl_write_byte(0x19, 0x00);// msb
	}

	return 0;
}

EXPORT_SYMBOL(lcd_bl_set_led_brightness);


#ifdef CONFIG_OF
static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,sgm37603a_bl", },
    {},
};
#endif

static const struct i2c_device_id lcd_bl_i2c_id[] = {
    {LCD_BL_I2C_ID_NAME, 0},
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
	.name = LCD_BL_I2C_ID_NAME,
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
	lcd_bl_dump_reg();
	printk("--lyd, i2c led\n");
	if(main_bl){
		pr_err("hyper backlight ic is sgm37603 !\n");
		//ret = lcd_bl_write_byte(0x11, 0x35);
	}else{
		pr_err("hyper backlight ic is ti_lm36823 !\n");
		ret = lcd_bl_write_byte(0x10, 0x07);
		//ret = lcd_bl_write_byte(0x11, 0xF5);
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
	printk("lcd_bl_init\n");    
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

