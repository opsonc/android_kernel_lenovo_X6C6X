#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/seq_file.h>
#include <linux/mmc/mmc.h>

#define PROC_BOARD_ID_FILE "board_id"
static struct proc_dir_entry *entry = NULL;

int board_id1_gpio_value, board_id2_gpio_value, board_id3_gpio_value, board_id4_gpio_value, board_id5_gpio_value;

static bool read_pcba_config(void)
{
	int ret = 0;
	int board_id1_gpio, board_id2_gpio, board_id3_gpio, board_id4_gpio, board_id5_gpio;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;

	board_id_node = of_find_node_by_name(NULL, "board_id");
	if(board_id_node == NULL){
		pr_err("[%s] find board_id node fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);
	}

	board_id_dev = of_find_device_by_node(board_id_node);
	if(board_id_dev == NULL){
		pr_err("[%s] find board_id dev fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);
	}

	board_id1_gpio = of_get_named_gpio(board_id_node, "board_id1-gpios", 0);
	if (gpio_is_valid(board_id1_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id1_gpio, GPIOF_IN, "hw_id");
		if (!ret) {
			board_id1_gpio_value = gpio_get_value(board_id1_gpio);
			pr_err("[%s] board_id1_gpio %d value: %d\n", __func__, board_id1_gpio, board_id1_gpio_value);
		} else {
			pr_err("[%s] Can not request hw_id_gpio : %d\n", __func__, ret);
		}
	}

	board_id2_gpio = of_get_named_gpio(board_id_node, "board_id2-gpios", 0);
	if (gpio_is_valid(board_id2_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id2_gpio, GPIOF_IN, "board_id2");
		if (!ret) {
			board_id2_gpio_value = gpio_get_value(board_id2_gpio);
			pr_err("[%s] get board_id2_gpio %d value: %d\n", __func__, board_id2_gpio, board_id2_gpio_value);
		} else {
			pr_err("[%s] Can not request board_id2_gpio : %d\n", __func__, ret);
		}
	}

	board_id3_gpio = of_get_named_gpio(board_id_node, "board_id3-gpios", 0);
	if (gpio_is_valid(board_id3_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id3_gpio, GPIOF_IN, "board_id3");
		if (!ret) {
			board_id3_gpio_value = gpio_get_value(board_id3_gpio);
			pr_err("[%s] get board_id3_gpio %d value: %d\n", __func__, board_id3_gpio, board_id3_gpio_value);
		} else {
			pr_err("[%s] Can not request board_id3_gpio : %d\n", __func__, ret);
		}
	}

	board_id4_gpio = of_get_named_gpio(board_id_node, "board_id4-gpios", 0);
	if (gpio_is_valid(board_id4_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id4_gpio, GPIOF_IN, "board_id4");
		if (!ret) {
			board_id4_gpio_value = gpio_get_value(board_id4_gpio);
			pr_err("[%s] get board_id4_gpio %d value: %d\n", __func__, board_id4_gpio, board_id4_gpio_value);
		} else {
			pr_err("[%s] Can not request board_id4_gpio : %d\n", __func__, ret);
		}
	}

	board_id5_gpio = of_get_named_gpio(board_id_node, "board_id5-gpios", 0);
	if (gpio_is_valid(board_id5_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id5_gpio, GPIOF_IN, "board_id5");
		if (!ret) {
			board_id5_gpio_value = gpio_get_value(board_id5_gpio);
			pr_err("[%s] get board_id5_gpio %d value: %d\n", __func__, board_id5_gpio, board_id5_gpio_value);
		} else {
			pr_err("[%s] Can not request board_id5_gpio : %d\n", __func__, ret);
		}
	}
	return true;
}

static int board_id_proc_show(struct seq_file *file, void* data)
{
	if (0 == board_id1_gpio_value && 0 == board_id2_gpio_value && 0 == board_id3_gpio_value && 0 == board_id4_gpio_value && 0 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "Master_WIFI");
	} else if (1 == board_id1_gpio_value && 0 == board_id2_gpio_value && 0 == board_id3_gpio_value && 0 == board_id4_gpio_value && 0 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "Master_LTE");
	} else if (0 == board_id1_gpio_value && 1 == board_id2_gpio_value && 0 == board_id3_gpio_value && 0 == board_id4_gpio_value && 0 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "NA_LTE");
	} else if (0 == board_id1_gpio_value && 0 == board_id2_gpio_value && 1 == board_id3_gpio_value && 0 == board_id4_gpio_value && 0 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "No_Battery_WIFI");
	} else if (0 == board_id1_gpio_value && 0 == board_id2_gpio_value && 0 == board_id3_gpio_value && 1 == board_id4_gpio_value && 0 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "No_Battery_Master_LTE");
	} else if (0 == board_id1_gpio_value && 0 == board_id2_gpio_value && 0 == board_id3_gpio_value && 0 == board_id4_gpio_value && 1 == board_id5_gpio_value) {
		seq_printf(file, "%s\n", "No_Battery_NA_LTE");
	} else {
		seq_printf(file, "%s\n", "PCBA_UNKNOWN");
	}

	return 0;
}

static int board_id_proc_open (struct inode* inode, struct file* file)
{
	return single_open(file, board_id_proc_show, inode->i_private);
}

static const struct file_operations board_id_proc_fops =
{
	.open = board_id_proc_open,
	.read = seq_read,
};

static int board_id_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}

	read_pcba_config();

	entry = proc_create(PROC_BOARD_ID_FILE, 0644, NULL, &board_id_proc_fops);
	if (entry == NULL)
	{
		pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}

	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	pr_err("enter [%s] \n", __func__);
	return 0;
}

static const struct of_device_id boardId_of_match[] = {
	{.compatible = "mediatek,board_id",},
	{},
};

static struct platform_driver boardId_driver = {
	.probe = board_id_probe,
	.remove = board_id_remove,
	.driver = {
        .name = "board_id",
		.owner = THIS_MODULE,
		.of_match_table = boardId_of_match,
	},
};

static int __init wt_pcba_early_init(void)
{
	int ret;
	pr_err("[%s]start to register boardId driver\n", __func__);

	ret = platform_driver_register(&boardId_driver);
    if (ret) {
		pr_err("[%s]Failed to register boardId driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit wt_pcba_exit(void)
{
	platform_driver_unregister(&boardId_driver);
}

module_init(wt_pcba_early_init);
module_exit(wt_pcba_exit);

MODULE_DESCRIPTION("wt sys borad id");
MODULE_LICENSE("GPL");
