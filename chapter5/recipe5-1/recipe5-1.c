#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "ioctl.h"

#define  DEVICE_NAME "recipedev"

static int recipe_dev_open(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_open is called.\n");
	return 0;
}

static int recipe_dev_close(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_close is called.\n");
	return 0;
}

static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	int i;
	pr_info("recipe_dev_ioctl is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			pr_info("SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			for(i = 0; i < info.size; i++){
				pr_info("ioctl size : %d, ioctl buffer : %d",info.size, info.buffer[i]);
			}
			break;
		case GET_DATA:
			pr_info("GET_DATA\n");
			pr_info("ioctl size : %d, ioctl buf : %d\n", info.size, info.buffer[0]);
			info.data = 100;
			info.size = sizeof(info.buffer);
			if (copy_to_user((void __user *)arg, &info, sizeof(info))) {
				return -EFAULT;
			}
			break;
		default:
			pr_info("invalid command %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}

static const struct file_operations recipe_dev_fops = {
	.owner = THIS_MODULE,
	.open = recipe_dev_open,
	.release = recipe_dev_close,
	.unlocked_ioctl = recipe_ioctl,
};

static struct miscdevice recipe_miscdevice = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEVICE_NAME,
		.fops = &recipe_dev_fops,
		.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
};

/* Add probe() function */
static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	pr_info("recipe_probe function is called.\n");
	ret = misc_register(&recipe_miscdevice);

	if (ret != 0) {
		pr_err("could not register the misc device recipe_dev");
		return ret;
	}
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	pr_info("recipe_remove function is called.\n");
	misc_deregister(&recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe5_1"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe5_1",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & ioctl module ");
