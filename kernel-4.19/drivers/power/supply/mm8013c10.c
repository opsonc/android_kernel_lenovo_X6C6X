// SPDX-License-Identifier: GPL-2.0

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/workqueue.h>
#include "mm8013c10.h"


#define MM8013C10_TEMPERATURE_REG 0x06
#define MM8013C10_BAT_CC_REG      0x2a
#define MM8013C10_BAT_SOC_REG     0x2c
#define MM8013C10_BAT_SOH_REG     0x2e

#define MM8013C10_CONTROL_READ_REG           0x00
#define MM8013C10_CONTROL_WRITE_REG          0x01
#define MM8013C10_CONTROL_PARAMETER_CHEM_ID  0x0008

#define STATUS_CHECK_INTERVAL_MS 5000

#define BATTERY_ID_SCUD      0x0102
#define BATTERY_ID_SUNWODA   0x0101
#define BATTERY_NAME_SCUD    "K10_SCUD_4V43_7500mah"
#define BATTERY_NAME_SUNWODA "K10_SUNWODA_4V43_7500mah"


static const struct i2c_device_id mm8013c10_i2c_id[] = { {"mm8013c10", 0}, {} };
static DEFINE_MUTEX(mm8013c10_i2c_access);
static struct i2c_client *new_client;

struct mm8013c10_info {
	struct device *dev;
	struct delayed_work update_work;
	u16 bat_soc; /* state of charge */
	u16 bat_soh; /* state of health */
	u16 bat_cc;  /* cycle count */
	u16 bat_id;
	int internal_temp;
	const char *bat_name;
	int irq;
};

static struct mm8013c10_info *g_info = NULL;

int mm8013c10_get_bat_info(enum mm8013c10_bat_info bat_info)
{
	if (!g_info)
		return -1;

	switch (bat_info) {
	case MM8013C10_BAT_SOC:
		return g_info->bat_soc;
	case MM8013C10_BAT_CC:
		return g_info->bat_cc;
	case MM8013C10_BAT_SOH:
		return g_info->bat_soh;
	case MM8013C10_BAT_ID:
		return g_info->bat_id;
	default:
		return -1;
	}
}
EXPORT_SYMBOL_GPL(mm8013c10_get_bat_info);

const char *mm8013c10_get_battery_name()
{
	if (!g_info)
		return "";

	return g_info->bat_name;
}
EXPORT_SYMBOL_GPL(mm8013c10_get_battery_name);

static int mm8013c10_read_word(u8 cmd, u16 *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 3;
	u8 buf[2] = {};
	struct i2c_msg msgs[2] = {
		{
			.addr = new_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = new_client->addr,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = buf,
		}
	};

	mutex_lock(&mm8013c10_i2c_access);
	do {
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
		if (ret != xfers)
			mdelay(10);
	} while (ret != xfers && --retries);
	mutex_unlock(&mm8013c10_i2c_access);

	if (ret == xfers)
		*returnData = *((u16 *)buf);

	return ret;
}

static int mm8013c10_control_read(u16 parameter, u16 *returnData)
{
	int ret, retries = 3, xfers = 2;
	u8 command[8] = {};
	u8 buf[2] = {};
	struct i2c_msg msgs[2] = {
		{
			.addr = new_client->addr,
			.flags = 0,
			.len = 3,
			.buf = command,
		},
		{
			.addr = new_client->addr,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = buf,
		}
	};

	command[0] = MM8013C10_CONTROL_READ_REG;
	command[1] = *((u8 *)&parameter);
	command[2] = *((u8 *)&parameter + 1);

	mutex_lock(&mm8013c10_i2c_access);
	do {
		ret = i2c_transfer(new_client->adapter, msgs, xfers);
		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n", new_client->adapter->name);
			break;
		}
		if (ret != xfers)
			mdelay(10);
	} while (ret != xfers && --retries);
	mutex_unlock(&mm8013c10_i2c_access);

	if (ret == xfers)
		*returnData = *((u16 *)buf);

	return ret;
}

static void notify_battery_power_supply(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	if (psy)
		power_supply_changed(psy);
	return;
}

static void mm8013c10_update_work(struct work_struct *work)
{
	u16 val;
	int ret, stat_changed = 0, temp;
	struct mm8013c10_info *info = container_of(work, struct mm8013c10_info, update_work.work);

	ret = mm8013c10_read_word(MM8013C10_BAT_SOC_REG, &val);
	if (2 == ret) {
		if (val != info->bat_soc) {
			pr_info("%s: battery soc changed, now is %u, previous is %u\n", __func__, val, info->bat_soc);
			info->bat_soc = val;
			++stat_changed;
		}
	}
	else
		pr_err("%s: read battery soc error, ret = %d\n", __func__, ret);
	ret = mm8013c10_read_word(MM8013C10_BAT_CC_REG, &val);
	if (2 == ret) {
		if (val != info->bat_cc) {
			pr_info("%s: battery cycle count changed, now is %u, previous is %u\n", __func__, val, info->bat_cc);
			info->bat_cc = val;
			++stat_changed;
		}
	}
	else
		pr_err("%s: read battery cycle count error, ret = %d\n", __func__, ret);
	ret = mm8013c10_read_word(MM8013C10_BAT_SOH_REG, &val);
	if (2 == ret) {
		if (val != info->bat_soh) {
			pr_info("%s: battery soh changed, now is %u, previous is %u\n", __func__, val, info->bat_soh);
			info->bat_soh = val;
			++stat_changed;
		}
	}
	else
		pr_err("%s: read battery soh error, ret = %d\n", __func__, ret);
	ret = mm8013c10_read_word(MM8013C10_TEMPERATURE_REG, &val);
	if (2 == ret) {
		temp = (int)val/10 - 273;
		if (temp != info->internal_temp) {
			//pr_info("%s: internal temperature changed, now is %d, previous is %d\n", __func__, temp, info->internal_temp);
			info->internal_temp = temp;
		}
	}
	else
		pr_err("%s: read internal temperature error, ret = %d\n", __func__, ret);
	pr_info("%s: soc = %u, cc = %u, soh = %u, internal_temp = %d\n", __func__, info->bat_soc, info->bat_cc, info->bat_soh, info->internal_temp);
	if (stat_changed)
		notify_battery_power_supply();
	schedule_delayed_work(&info->update_work, msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
}

static int mm8013c10_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mm8013c10_info *info = NULL;
	int ret;
	u16 temp;

	pr_info("[%s]\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct mm8013c10_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	new_client = client;
	info->dev = &client->dev;

	ret = mm8013c10_control_read(MM8013C10_CONTROL_PARAMETER_CHEM_ID, &(info->bat_id));
	pr_info("%s: read battery id, ret = %d, bat_id = 0x%04x\n", __func__, ret, info->bat_id);
	if (info->bat_id == BATTERY_ID_SCUD)
		info->bat_name = BATTERY_NAME_SCUD;
	else if (info->bat_id == BATTERY_ID_SUNWODA)
		info->bat_name = BATTERY_NAME_SUNWODA;
	else
		info->bat_name = "Unknown Battery";

	ret = mm8013c10_read_word(MM8013C10_BAT_SOC_REG, &(info->bat_soc));
	pr_info("%s: read battery soc, ret = %d, soc = %d\n", __func__, ret, info->bat_soc);

	ret = mm8013c10_read_word(MM8013C10_BAT_CC_REG, &(info->bat_cc));
	pr_info("%s: read battery cycle count, ret = %d, cc = %d\n", __func__, ret, info->bat_cc);

	ret = mm8013c10_read_word(MM8013C10_BAT_SOH_REG, &(info->bat_soh));
	pr_info("%s: read battery soh, ret = %d, soh = %d\n", __func__, ret, info->bat_soh);

	ret = mm8013c10_read_word(MM8013C10_TEMPERATURE_REG, &temp);
	if (ret == 2)
		info->internal_temp = (int)temp/10 - 273;
	pr_info("%s: read internal temperature, ret = %d, internal_temp = %d\n", __func__, ret, info->internal_temp);

	g_info = info;
	INIT_DELAYED_WORK(&info->update_work, mm8013c10_update_work);
	schedule_delayed_work(&info->update_work, msecs_to_jiffies(STATUS_CHECK_INTERVAL_MS));
	return 0;
}

static int mm8013c10_driver_remove(struct i2c_client *client)
{
	struct mm8013c10_info *info = i2c_get_clientdata(client);

	if (info) {
		cancel_delayed_work_sync(&info->update_work);
		return 0;
	}
	return -1;
}

#ifdef CONFIG_OF
static const struct of_device_id mm8013c10_of_match[] = {
	{.compatible = "mediatek,mm8013c10"},
	{},
};
#else
static struct i2c_board_info i2c_mm8013c10 __initdata = {
	I2C_BOARD_INFO("mm8013c10", (mm8013c10_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver mm8013c10_driver = {
	.driver = {
		.name = "mm8013c10",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mm8013c10_of_match,
#endif
	},
	.probe = mm8013c10_driver_probe,
	.remove = mm8013c10_driver_remove,
	.id_table = mm8013c10_i2c_id,
};

static int __init mm8013c10_init(void)
{
	pr_info("[%s] init start with i2c DTS", __func__);

	if (i2c_add_driver(&mm8013c10_driver) != 0) {
		pr_info("[%s] failed to register mm8013c10 i2c driver.\n", __func__);
	} else {
		pr_info("[%s] Success to register mm8013c10 i2c driver.\n", __func__);
	}
	return 0;
}

static void __exit mm8013c10_exit(void)
{
	i2c_del_driver(&mm8013c10_driver);
}

module_init(mm8013c10_init);
module_exit(mm8013c10_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C mm8013c10 Driver");
MODULE_AUTHOR("Wangyang");
