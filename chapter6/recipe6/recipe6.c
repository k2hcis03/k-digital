#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define  DEVICE_NAME "recipedev"

struct recipe6 {
	struct platform_device *pdev;
	char *name;
	int number;
	struct miscdevice recipe_miscdevice;
};

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe6* recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe6, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_open() is called.\n");
	dev_info(dev, "private data name is %s and data is %d.\n", recipe_private->name, recipe_private->number);
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe6 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe6, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_close() is called.\n");
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
	struct recipe6 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(dev, sizeof(struct recipe6), GFP_KERNEL);
	if(!recipe_private){
		dev_info(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->pdev = pdev;

	recipe_private->name = devm_kzalloc(dev, sizeof(100), GFP_KERNEL);
	if(!recipe_private->name){
		dev_info(dev, "failed memory allocation");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, recipe_private);
	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
	recipe_private->number = 100;
	strcpy(recipe_private->name, "devm_kzalloc() sample");

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret != 0) {
		dev_info(dev, "could not register the misc device recipe_dev");
		return ret;
	}
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe6 *recipe_private = platform_get_drvdata(pdev);
	dev_info(dev, "recipe_remove() function is called.\n");
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe6"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe6",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & ioctl & memory module ");
