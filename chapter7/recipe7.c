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

#define DEVICE_NAME 		"recipedev"

struct recipe7{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct gpio_desc *key_int;
	int irq;
};
static char *KEYS_NAME = "recipe7_INT";

static irqreturn_t recipe7_isr(int irq, void *data)
{
	struct recipe7 *priv = data;
	struct device *dev;

	dev = &priv->pdev->dev;
	dev_info(dev, "recipe7_isr is called & irq number is %d\n", priv->irq);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t recipe7_thread_isr(int irq, void *data)
{
	struct recipe7 *priv = data;
	struct device *dev;

	dev = &priv->pdev->dev;
	dev_info(dev, "recipe7_thread_isr is called & irq number is %d\n", priv->irq);
	return IRQ_HANDLED;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe7 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe7, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe7 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe7, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
//	.unlocked_ioctl = recipe_ioctl,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe7 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe7), GFP_KERNEL);
	recipe_private->pdev = pdev;

	recipe_private->key_int = devm_gpiod_get(dev, NULL, GPIOD_IN);
	if (IS_ERR(recipe_private->key_int)) {
			dev_err(dev, "gpio get failed\n");
			return PTR_ERR(recipe_private->key_int);
	}
	recipe_private->irq = gpiod_to_irq(recipe_private->key_int);
	if (recipe_private->irq < 0)
		return recipe_private->irq;

	dev_info(dev, "The IRQ number is: %d\n", recipe_private->irq);

	ret = devm_request_threaded_irq(dev, recipe_private->irq, recipe7_isr,
						recipe7_thread_isr, IRQF_TRIGGER_FALLING | IRQF_SHARED,
						KEYS_NAME, recipe_private);
	if (ret) {
		dev_err(dev, "Failed to request interrupt %d, error %d\n", recipe_private->irq, ret);
		return ret;
	}

	dev_info(dev, "gpio initial is completed\n");
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
	struct recipe7 *recipe_private = platform_get_drvdata(pdev);
	dev_info(dev, "recipe_remove() function is called.\n");
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe7"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe7",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & interrupt module ");
