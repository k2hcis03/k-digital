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

#define DEVICE_NAME 		"recipedev"
#define US_TO_NS(x) 		(x * 1000L)

struct recipe8_1{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe8_1_timer;
	int timer_period;
	struct gpio_desc *gpio;
};

enum hrtimer_restart recipe8_1_hr_timer(struct hrtimer *t)
{
	static int toggle = 0;

	struct recipe8_1 *recipe_private = from_timer(recipe_private, t, recipe8_1_timer);
	struct device *dev;
	ktime_t ktime;

	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe8_1_hr_timer enter %d\n", recipe_private->timer_period);

	if(toggle == 0){
		gpiod_set_value(recipe_private->gpio, 1);
		toggle = 1;
	}else{
		gpiod_set_value(recipe_private->gpio, 0);
		toggle = 0;
	}

	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe8_1_timer,
			hrtimer_cb_get_time(&recipe_private->recipe8_1_timer), ktime);

	return HRTIMER_RESTART;
//	return HRTIMER_NORESTART;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe8_1 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe8_1, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe8_1 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe8_1, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
//	.read = recipe_read,
//	.unlocked_ioctl = recipe_ioctl,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe8_1 *recipe_private;
	ktime_t ktime;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe8_1), GFP_KERNEL);
	recipe_private->pdev = pdev;
	recipe_private->timer_period = 200;

	ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe8_1_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	recipe_private->recipe8_1_timer.function = &recipe8_1_hr_timer;

	platform_set_drvdata(pdev, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	recipe_private->gpio = devm_gpiod_get(dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(recipe_private->gpio)) {
			dev_err(dev, "gpio get failed\n");
			return PTR_ERR(recipe_private->gpio);
	}
	gpiod_direction_output(recipe_private->gpio, 0);
	gpiod_set_value(recipe_private->gpio, 1);

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret) return ret; /* misc_register returns 0 if success */

	hrtimer_start(&recipe_private->recipe8_1_timer, ktime, HRTIMER_MODE_REL );

	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe8_1 *recipe_private = platform_get_drvdata(pdev);
	int ret;

	dev_info(dev, "recipe_remove() function is called.\n");

	ret = hrtimer_cancel(&recipe_private->recipe8_1_timer);
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe8_1"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe8_1",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & high resolution timer module ");
