/*
 * File:   hall.c
 * Company: hoperun
 *
 * Created on January 8, 2020, 11:07 AM
 */

#define DEBUG

/* Standard Linux includes */
#include <linux/init.h>				// __init, __initdata, etc
#include <linux/module.h>			// Needed to be a module
#include <linux/kernel.h>			// Needed to be a kernel module
#include <linux/i2c.h>				// I2C functionality
#include <linux/slab.h>				// devm_kzalloc
#include <linux/types.h>			// Kernel datatypes
#include <linux/errno.h>			// EINVAL, ERANGE, etc
#include <linux/of_device.h>			// Device tree functionality
#include <linux/interrupt.h>
#include "hall.h"
#include <linux/regulator/consumer.h>
#include <linux/delay.h>




/******************************************************************************
* Driver functions
******************************************************************************/


int hall_int_gpio;
static int hall_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	hall_int_gpio = of_get_named_gpio(np, "hall,irq-gpio", 0);
	if (hall_int_gpio == 7) {
		printk("parse irq gpio correctly\n ");
		return 0;
	} else {
		printk("parse irq gpio incorrectly\n ");
		return -ENOMEM;
	}
}

int input_data = -1;
static irqreturn_t hall_irq_handler(int irq, void *dev_id)
{
	struct hall_data *sdata = dev_id;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&sdata->irq_lock, irqflags);
	//hall_irq_disable(sdata);
	input_data = gpio_get_value(hall_int_gpio);


	input_report_key(sdata->input_dev, gpio_key.code, 1);
	input_report_key(sdata->input_dev, gpio_key.code, 0);
	printk("input_report_key ,status :%d\n", input_data);
	input_sync(sdata->input_dev);

	spin_unlock_irqrestore(&sdata->irq_lock, irqflags);
	//hall_irq_enable(sdata);

	printk("gpio_get_value : %d\n", input_data);

	return IRQ_HANDLED;
}

static s8 hall_request_irq(struct hall_data *sdata)
{
	s32 ret = -1;
	const u8 irq_table[] = HALL_IRQ_TAB;

	ret  = request_irq(sdata->client->irq, hall_irq_handler, irq_table[sdata->int_trigger_type], sdata->client->name, sdata);
	if (ret) {
		printk("Request IRQ failed!ERRNO:%d.", ret);
	} else {
		irq = sdata->client->irq;
		return 0;
	}
	return -ENOMEM;
}

static s8 hall_request_io_port(struct hall_data *sdata)
{
	s32 ret = 0;

	ret = gpio_request(hall_int_gpio, "HALL_INT_IRQ");
	if (ret) {
		printk("[GPIO]irq gpio request failed");
	}

	ret = gpio_direction_input(hall_int_gpio);
	if (ret) {
		printk("[GPIO]set_direction for irq gpio failed");
		goto err_irq_gpio_dir;
	}
	return 0;

err_irq_gpio_dir:
	gpio_free(hall_int_gpio);

	return ret;
}

static s8 hall_request_input_dev(struct hall_data *sdata)
{
	s8 ret = -1;

	sdata->input_dev = input_allocate_device();
	if (sdata->input_dev == NULL) {
		printk("Failed to allocate input device.");
		return -ENOMEM;
	}

	__set_bit(EV_SYN, sdata->input_dev->evbit);
	__set_bit(EV_ABS, sdata->input_dev->evbit);
	__set_bit(EV_KEY, sdata->input_dev->evbit);
	__set_bit(EV_REP, sdata->input_dev->evbit);


	sdata->input_dev->name = hall_name;
	sdata->input_dev->phys = hall_input_phys;
	sdata->input_dev->id.bustype = BUS_I2C;

	input_set_abs_params(sdata->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255
	input_set_abs_params(sdata->input_dev, ABS_MT_POSITION_X, 0, 1080 - 1, 0, 0);
	input_set_abs_params(sdata->input_dev, ABS_MT_POSITION_Y, 0, 2340 - 1, 0, 0);

	ret = input_register_device(sdata->input_dev);
	if (ret) {
		printk("Register %s input device failed", sdata->input_dev->name);
		return -ENODEV;
	}
	input_set_capability(sdata->input_dev, EV_KEY, gpio_key.code);

	return 0;
}
static struct proc_dir_entry *gpio_status;
#define GOIP_STATUS "hall_status"

static int gpio_proc_show(struct seq_file *file, void *data)
{

	input_data = gpio_get_value(hall_int_gpio);
	seq_printf(file, "%d\n", input_data);

	return 0;
}


static int gpio_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, gpio_proc_show, inode->i_private);
}


static const struct file_operations gpio_status_ops = {
	.open = gpio_proc_open,
	.read = seq_read,
};

static int hall_suspend(struct device *dev)
{
	int ret;

	ret = enable_irq_wake(irq);
	if (ret) {
		printk("hall_suspend enable_irq_wake failed!\n");
		return -ENODEV;
	}

	return 0;
}

static int hall_resume(struct device *dev)
{
	int ret;

	ret = disable_irq_wake(irq);
	if (ret) {
		printk("hall_resume disable_irq_wake failed!\n");
		return -ENODEV;
	}

	return 0;
}

static int hall_probe (struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct hall_data *sdata;

        printk("HALL %s enter ===>>", __func__);
	if (!client) {
		pr_err("HALL  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);

	/* Make sure probe was called on a compatible device */
	if (!of_match_device(hall_dt_match, &client->dev)) {
		dev_err(&client->dev, "HALL  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
	printk("HALL  %s - Device tree matched!\n", __func__);

	sdata = kzalloc(sizeof(*sdata), GFP_KERNEL);
	if (sdata == NULL) {
		printk("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	//parse dt for irq
	if (client->dev.of_node) {
		ret = hall_parse_dt(&client->dev);
		if (!ret)
			printk("hall_parse_dt success\n");
	}
	//set irq Trigger mode
	sdata->int_trigger_type = HALL_INT_TRIGGER;

	sdata->client = client;
	spin_lock_init(&sdata->irq_lock);

	//request input dev
	ret = hall_request_input_dev(sdata);
	if (ret < 0) {
		printk("HALL request input dev failed");
	}

	//request io port
	if (gpio_is_valid(hall_int_gpio)) {
		ret = hall_request_io_port(sdata);
		if (ret < 0) {
			printk("HALL %s -request io port fail\n", __func__);
			return -ENOMEM;
		}
	} else {
		printk("HALL %s -gpio is not valid\n", __func__);
		return -ENOMEM;
	}


	//request irq
	ret = hall_request_irq(sdata);
	if (ret < 0) {
		printk("HALL %s -request irq fail\n", __func__);
	}

	__set_bit(EV_REP, sdata->input_dev->evbit);

	//enable irq
	printk("after hall_irq_enable,probe end \n");

	gpio_status = proc_create(GOIP_STATUS, 0644, NULL, &gpio_status_ops);
	if (gpio_status == NULL) {
		printk("tpd, create_proc_entry gpio_status_ops failed\n");
	}

	return 0;
}


static int __init hall_init(void)
{
	int ret = 0;
	printk("HALL  %s - Start driver initialization...\n", __func__);

	ret = i2c_add_driver(&hall_driver);
	printk("ret : %d\n", ret);
	return ret;
}

static void __exit hall_exit(void)
{
	i2c_del_driver(&hall_driver);
	pr_debug("HALL  %s - Driver deleted...\n", __func__);
}


/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(hall_init);			// Defines the module's entrance function
module_exit(hall_exit);			// Defines the module's exit function

MODULE_LICENSE("GPL");				// Exposed on call to modinfo
MODULE_DESCRIPTION("Hall Driver");	// Exposed on call to modinfo
MODULE_AUTHOR("Hall");			// Exposed on call to modinfo

