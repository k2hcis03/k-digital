#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/property.h>
#include <linux/interrupt.h>

#include <linux/timer.h>
#define DEVICE_NAME 		"recipedev"

struct recipe8{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct timer_list recipe8_timer;
	int timer_period;
};

static void recipe8_timer(struct timer_list *t)
{
	struct recipe8 *recipe_private = from_timer(recipe_private, t, recipe8_timer);
	struct device *dev;

	dev = &recipe_private->pdev->dev;
	mod_timer(&recipe_private->recipe8_timer, jiffies + msecs_to_jiffies(recipe_private->timer_period));
	dev_info(dev, "recipe8_timer enter\n");
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe8 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe8, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe8 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe8, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe8 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe8), GFP_KERNEL);
	recipe_private->pdev = pdev;
	recipe_private->timer_period = 1000;

	timer_setup (&recipe_private->recipe8_timer, recipe8_timer, 0);
	mod_timer(&recipe_private->recipe8_timer, jiffies + msecs_to_jiffies(recipe_private->timer_period));

	platform_set_drvdata(pdev, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret) return ret; /* misc_register returns 0 if success */

	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe8 *recipe_private = platform_get_drvdata(pdev);
	dev_info(dev, "recipe_remove() function is called.\n");
	del_timer(&recipe_private->recipe8_timer);
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe8"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe8",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & timer module ");
