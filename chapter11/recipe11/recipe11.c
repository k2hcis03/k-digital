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

#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include <linux/sysfs.h>

#include <linux/kthread.h>             //kernel threads
#include <linux/delay.h>

#include "ioctl.h"

#define US_TO_NS(x) 		(x * 1000L)
#define DEVICE_NAME 		"recipedev"

struct recipe11 {
	struct platform_device* pdev;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe_hr_timer;
	int timer_period;
	struct task_struct* recipe_thread;
	struct completion recipe_complete_ok;
};

int recipe_thread(void* priv)
{
	struct recipe11* recipe_private = (struct recipe11*)priv;
	struct device* dev = &recipe_private->pdev->dev;


	dev_info(dev, "recipe_thread() called\n");

	while (!kthread_should_stop()) {
		wait_for_completion_interruptible(&recipe_private->recipe_complete_ok);
		reinit_completion(&recipe_private->recipe_complete_ok);

		dev_info(dev, "recipe_thread is wakened from hrtimer\n");
	}
	return 0;
}

enum hrtimer_restart recipe_hr_timer(struct hrtimer* t)
{
	struct recipe11* recipe_private = from_timer(recipe_private, t, recipe_hr_timer);
	struct device* dev;
	ktime_t ktime;

	dev = &recipe_private->pdev->dev;
	//dev_info(dev, "recipe11_hr_timer enter %d\n", recipe_private->timer_period);

	complete(&recipe_private->recipe_complete_ok);

	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe_hr_timer,
		hrtimer_cb_get_time(&recipe_private->recipe_hr_timer), ktime);

	return HRTIMER_RESTART;
	//	return HRTIMER_NORESTART;
}

static int recipe_open(struct inode* inode, struct file* file)
{
	struct recipe11* recipe_private;
	struct device* dev;
	recipe_private = container_of(file->private_data, struct recipe11, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode* inode, struct file* file)
{
	struct recipe11* recipe_private;
	struct device* dev;
	recipe_private = container_of(file->private_data, struct recipe11, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe11* recipe_private;
	struct device* dev;
	ktime_t ktime;
	int ret;

	recipe_private = container_of(file->private_data, struct recipe11, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
	case SET_DATA:
		dev_info(dev, "SET_DATA\n");
		if (copy_from_user(&info, (void __user*)arg, sizeof(info))) {
			return -EFAULT;
		}
		dev_info(dev, "User data is %d\n", info.data);
		if (info.data == 0) {
			ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
			if (ret) {
				dev_info(dev, "The timer was still in use.");
			}
		}
		else if (info.data == 1) {
			ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
			hrtimer_start(&recipe_private->recipe_hr_timer, ktime, HRTIMER_MODE_REL);
		}
		break;
	case GET_DATA:
		dev_info(dev, "GET_DATA\n");

		if (copy_to_user((void __user*)arg, &info, sizeof(info))) {
			return -EFAULT;
		}
		break;
	default:
		dev_info(dev, "invalid command %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.unlocked_ioctl = recipe_ioctl,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device* pdev)
{
	int ret;
	struct device* dev = &pdev->dev;
	struct recipe11* recipe_private;
	ktime_t ktime;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe11), GFP_KERNEL);
	if (!recipe_private) {
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->pdev = pdev;
	recipe_private->timer_period = 500000;

	init_completion(&recipe_private->recipe_complete_ok);

	recipe_private->recipe_thread = kthread_run(recipe_thread, recipe_private, "recipe Thread");
	if (recipe_private->recipe_thread) {
		dev_info(dev, "Kthread Created Successfully.\n");
	}
	else {
		dev_err(dev, "Cannot create kthread\n");
		return -ENOMEM;
	}
	//
	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	recipe_private->recipe_hr_timer.function = &recipe_hr_timer;

	platform_set_drvdata(pdev, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret) {
		return ret; /* misc_register returns 0 if success */
	}
	dev_info(dev, "recipe_probe() function is completed.\n");
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device* pdev)
{
	struct device* dev = &pdev->dev;
	struct recipe11* recipe_private = platform_get_drvdata(pdev);
	int ret;

	dev_info(dev, "recipe_remove() function is called.\n");

	complete(&recipe_private->recipe_complete_ok);
	kthread_stop(recipe_private->recipe_thread);
	//complete(&recipe_private->recipe_complete_ok);

	ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
	if (ret) {
		dev_info(dev, "The timer was still in use.");
	}
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{.compatible = "brcm,recipe11"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe11_thread",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & mmap. module ");
