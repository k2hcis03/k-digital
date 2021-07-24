#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>

#include "ioctl.h"

#define DEVICE_NAME 		"recipedev"
#define MS_TO_NS(x) 		(x * 1000000L)
#define MMAPSIZE			2048

struct recipe10{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	char *mmap_buf;
};

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe10 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe10, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe10 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe10, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe10 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data, struct recipe10, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			dev_info(dev, "SET_DATA and user mmap write data are %s\n", recipe_private->mmap_buf);
			memcpy(recipe_private->mmap_buf, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 27);
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			dev_info(dev, "User data is %d\n", info.data);
			break;
		case GET_DATA:
			dev_info(dev, "GET_DATA\n");

			if (copy_to_user((void __user *)arg, &info, sizeof(info))) {
				return -EFAULT;
			}
			break;
		default:
			dev_info(dev, "invalid command %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}

static int recipe_mmap(struct file *file, struct vm_area_struct *vma) {

	struct recipe10 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data,  struct recipe10,
			recipe_miscdevice);

	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_mmap() called\n");
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys(recipe_private->mmap_buf) >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.unlocked_ioctl = recipe_ioctl,
	.mmap = recipe_mmap,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe10 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe10), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation\n");
		return -ENOMEM;
	}

	recipe_private->pdev = pdev;

	recipe_private->mmap_buf = kzalloc(MMAPSIZE, GFP_KERNEL);
	if(!recipe_private->mmap_buf){
		dev_err(dev, "failed memory allocation\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret){
		return ret; /* misc_register returns 0 if success */
	}
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe10 *recipe_private = platform_get_drvdata(pdev);

	dev_info(dev, "recipe_remove() function is called.\n");

	kfree(recipe_private->mmap_buf);

	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe10"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe10_mmap",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & mmap. module ");
