#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <linux/sched/types.h>
#include <linux/sched/clock.h>

#define SHOW_DEBUG_INFO

struct rts5452e_chip {
	struct i2c_client *client;
	struct device *dev;
	struct semaphore suspend_lock;
};

enum {
	VENDOR_CMD_ENABLE,
	GET_IC_STATUS,
	GET_STATUS,
};

struct rts5452e_command {
	const char *name;
	u8 command;
	u8 length;
	u8 value[32];
};

struct rts5452e_command rts_cmd[] = {
	[VENDOR_CMD_ENABLE] = {"VENDOR_CMD_ENABLE", 0x01, 0x03, {0xda, 0x0b, 0x01}},
	[GET_IC_STATUS] 	= {"GET_IC_STATUS", 0x3A, 0x03, {0x00, 0x00, 0x14}},
	[GET_STATUS] 		= {"GET_STATUS", 0x09, 0x03, {0x00, 0x00, 0x0E}},
};

enum {
	NO_CONNECTER,
	DFP = 1,
	UFP,
};


#define MAX_RETRY_TIMES         5
#define COMMAND_IN_PROCESS      0
#define COMMAND_PROCESS_FAILED  0x03
#define COMMAND_PROCESS_SUCCESS 0x01
#define COMMAND_PROCESS_MASK    (BIT(0) | BIT(1))

#define MAX_READ_LENGTH         32
#define READ_COMMAND_CODE       0x80

#define FW_MAIN_VER_BYTE        4
#define FW_SUB_VER_BYTE         5
#define FW_SUB_VER2_BYTE        6

#define DATA_ROLE_BYTE          11
#define DATA_ROLE_MASK          (BIT(0) | BIT(1) | BIT(2))
#define CONNECT_STATUS_BYTE     5
#define CONNECT_STATUS_MASK     BIT(7)
#define PLUG_DIRECTION_BYTE     12
#define PLUG_DIRECTION_MASK     BIT(5)
#define PD_READY_BYTE           9
#define PD_READY_MASK           BIT(0)

static struct i2c_client *rts5452e_i2c_client;
static DEFINE_MUTEX(rts5452e_i2c_access);

static int g_data_role = -1;
int rts5452e_get_data_role()
{
	return g_data_role;
}
EXPORT_SYMBOL_GPL(rts5452e_get_data_role);

static int g_is_pd_port = -1;
int rts5452e_is_pd_port()
{
	return g_is_pd_port;
}
EXPORT_SYMBOL_GPL(rts5452e_is_pd_port);

static int rts5452e_command_set(struct i2c_client *client, struct rts5452e_command *rts)
{
	int ret, count = 0;

	mutex_lock(&rts5452e_i2c_access);
	ret = i2c_smbus_write_block_data(client, rts->command, rts->length, &(rts->value[0]));
	if (ret) {
		mutex_unlock(&rts5452e_i2c_access);
		pr_err("%s: smbus write command %s failed!\n", __func__, rts->name);
		return -1;
	}
	do {
		mdelay(1);
		ret = i2c_smbus_read_byte(client);
		pr_info("%s: command %s get ping status, ret = 0x%02x\n", __func__, rts->name, ret);
		if ((ret & COMMAND_PROCESS_MASK) == COMMAND_PROCESS_FAILED) {
			mutex_unlock(&rts5452e_i2c_access);
			pr_err("%s: command %s process failed!\n", __func__, rts->name);
			return -1;
		}
		else if ((ret & COMMAND_PROCESS_MASK) == COMMAND_PROCESS_SUCCESS) {
			mutex_unlock(&rts5452e_i2c_access);
			pr_info("%s: command %s process success\n", __func__, rts->name);
			return 0;
		}
		count++;
	} while (count <= MAX_RETRY_TIMES);
	mutex_unlock(&rts5452e_i2c_access);
	pr_err("%s: command %s process over time!\n", __func__, rts->name);
	return -1;
}

static int rts5452e_command_get(struct i2c_client *client, struct rts5452e_command *rts, u8 *recv)
{
	int ret;

	ret = rts5452e_command_set(client, rts);
	if (ret)
		return -1;
	mutex_lock(&rts5452e_i2c_access);
	ret = i2c_smbus_read_i2c_block_data(client, READ_COMMAND_CODE, MAX_READ_LENGTH, recv);
	mutex_unlock(&rts5452e_i2c_access);
	pr_err("%s: command %s get %d %s received\n", __func__, rts->name, ret, ret <= 1 ? "byte" : "bytes");
	return ret;
}


#ifdef SHOW_DEBUG_INFO
static void show_receive_info(u8 *recv, const char *cmd_name)
{
	char buf[64] = {};
	char *pos = buf;
	int i, j;

	for (i = 0; i < 4; ++i) {
		pos = buf;
		for (j = i * 8; j < (i + 1) * 8; ++j) {
			sprintf(pos, "0x%02x ", recv[j]);
			pos += 5;
		}
		pr_err("%s: %s\n", cmd_name, buf);
	}
}
#else
static void show_receive_info(u8 *recv, const char *cmd_name)
{
}
#endif

static int show_firmware_version(struct i2c_client *client)
{
	u8 buf[MAX_READ_LENGTH] = {};
	char version[16] = {};

	if (rts5452e_command_get(client, &(rts_cmd[GET_IC_STATUS]), buf) < FW_SUB_VER2_BYTE + 1) {
		pr_err("%s: get firmware version failed!\n", __func__);
		return -1;
	}
	show_receive_info(buf, rts_cmd[GET_IC_STATUS].name);
	sprintf(version, "%d.%d.%d", buf[FW_MAIN_VER_BYTE], buf[FW_SUB_VER_BYTE], buf[FW_SUB_VER2_BYTE]);
	pr_err("RTS5452E firmware version: %s\n", version);
	g_is_pd_port = buf[PD_READY_BYTE] & PD_READY_MASK;
	pr_err("RTS5452E g_is_pd_port = %d\n", g_is_pd_port);
	return 0;
}

static int get_data_role(struct i2c_client *client)
{
	u8 buf[MAX_READ_LENGTH] = {};

	if (rts5452e_command_get(client, &(rts_cmd[GET_STATUS]), buf) < DATA_ROLE_BYTE + 1) {
		pr_err("%s: get data role failed!\n", __func__);
		return -1;
	}
	show_receive_info(buf, rts_cmd[GET_STATUS].name);
	if (buf[CONNECT_STATUS_BYTE] & CONNECT_STATUS_MASK)
		return (buf[DATA_ROLE_BYTE] & DATA_ROLE_MASK);
	return NO_CONNECTER;
}

int rts5452e_get_plug_direction()
{
	u8 buf[MAX_READ_LENGTH] = {};

	if (!rts5452e_i2c_client) {
		pr_err("%s: i2c client is NULL\n", __func__);
		return -1;
	}
	if (rts5452e_command_get(rts5452e_i2c_client, &(rts_cmd[GET_STATUS]), buf) < PLUG_DIRECTION_BYTE + 1) {
		pr_err("%s: get plug direction failed!\n", __func__);
		return -1;
	}
	show_receive_info(buf, rts_cmd[GET_STATUS].name);
	if (buf[CONNECT_STATUS_BYTE] & CONNECT_STATUS_MASK) {
		if (buf[PLUG_DIRECTION_BYTE] & PLUG_DIRECTION_MASK)
			return 2;
		else
			return 1;
	}
	else
		return NO_CONNECTER;
}
EXPORT_SYMBOL_GPL(rts5452e_get_plug_direction);

extern void mt_usb_host_connect(int);
extern void mt_usb_connect(void);
static int rts5452e_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BLOCK_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	ret = rts5452e_command_set(client, &(rts_cmd[VENDOR_CMD_ENABLE]));
	if (ret) {
		pr_err("%s: enable control failed\n", __func__);
		return -1;
	}

	show_firmware_version(client);

	g_data_role = get_data_role(client);
	pr_err("%s: g_data_role = %d\n", __func__, g_data_role);
	if (g_data_role == DFP) {
		pr_info("%s: DFP connected, keep usb to device\n", __func__);
		mt_usb_connect();
	}
	else if (g_data_role == NO_CONNECTER)
		pr_info("%s: No connecter, only vbus, keep usb to device\n", __func__);
	else {
		pr_info("%s: UFP or unknown connecter, set usb to host\n", __func__);
		mt_usb_host_connect(0);
	}

	rts5452e_i2c_client = client;

	return 0;
}

static int rts5452e_i2c_remove(struct i2c_client *client)
{
	struct rts5452e_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		pr_info("%s\n", __func__);
	}
	return 0;
}

#ifdef CONFIG_PM
static int rts5452e_i2c_suspend(struct device *dev)
{
	struct rts5452e_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
	}

	return 0;
}

static int rts5452e_i2c_resume(struct device *dev)
{
	struct rts5452e_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}

static void rts5452e_shutdown(struct i2c_client *client)
{
	struct rts5452e_chip *chip = i2c_get_clientdata(client);

	if (chip)
		pr_info("%s\n", __func__);
}

#ifdef CONFIG_PM_RUNTIME
static int rts5452e_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int rts5452e_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops rts5452e_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			rts5452e_i2c_suspend,
			rts5452e_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		rts5452e_pm_suspend_runtime,
		rts5452e_pm_resume_runtime,
		NULL
	)
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define RTS5452E_PM_OPS	(&rts5452e_pm_ops)
#else
#define RTS5452E_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id rts5452e_id_table[] = {
	{},
};
MODULE_DEVICE_TABLE(i2c, rts5452e_id_table);

static const struct of_device_id rts_match_table[] = {
	{.compatible = "mediatek,usb_type_c",},
	{},
};

static struct i2c_driver rts5452e_driver = {
	.driver = {
		.name = "usb_type_c",
		.owner = THIS_MODULE,
		.of_match_table = rts_match_table,
		.pm = RTS5452E_PM_OPS,
	},
	.probe = rts5452e_i2c_probe,
	.remove = rts5452e_i2c_remove,
	.shutdown = rts5452e_shutdown,
	.id_table = rts5452e_id_table,
};

static int __init rts5452e_init(void)
{
	struct device_node *np;

	pr_info("%s: initializing...\n", __func__);
	np = of_find_node_by_name(NULL, "usb_type_c");
	if (np != NULL)
		pr_info("usb_type_c node found...\n");
	else
		pr_info("usb_type_c node not found...\n");
	return i2c_add_driver(&rts5452e_driver);
}
subsys_initcall(rts5452e_init);

static void __exit rts5452e_exit(void)
{
	i2c_del_driver(&rts5452e_driver);
}
module_exit(rts5452e_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wangyang");
MODULE_DESCRIPTION("RTS5452E SMBUS Driver");
