#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include "ioctl.h"

#define  DEVICE_NAME "recipedev"


static int recipe_open(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_close() is called.\n");
	return 0;
}
/* declare ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	int i;
	pr_info("recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

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
/* declare a file_operations structure */
static const struct file_operations recipe_dev_fops = {
	.owner			= THIS_MODULE,
	.open 		 	= recipe_open,
	.release 	 	= recipe_close,
	.unlocked_ioctl 	= recipe_ioctl,
};

/* declare & initialize struct miscdevice */
static struct miscdevice recipe_miscdevice = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEVICE_NAME,
		.fops = &recipe_dev_fops,
		.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
};

static int __init recipe_init(void)
{
	int ret;

	pr_info("recipe_init init\n");

	/* Register the device with the Kernel */
	ret = misc_register(&recipe_miscdevice);

	if (ret != 0) {
		pr_err("could not register the misc device ▶ 로딩 함수");
		return ret;
	}
	pr_info("The device is created correctly\n");

	return 0;
}

static void __exit recipe_exit(void)
{
	/* unregister the device with the Kernel */
	misc_deregister(&recipe_miscdevice);
	pr_info("recipe_exit exit\n");
}

module_init(recipe_init);
module_exit(recipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a miscellanenous & ioctl module ");
