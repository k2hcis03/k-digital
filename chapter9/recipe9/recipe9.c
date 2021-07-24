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

struct recipe9{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe9_timer;
	int timer_period;
	char name[100];
	struct gpio_desc *gpio;
};

enum hrtimer_restart recipe9_hr_timer(struct hrtimer *t)
{
	static int toggle = 0;

	struct recipe9 *recipe_private = from_timer(recipe_private, t, recipe9_timer);
	struct device *dev;
	ktime_t ktime;

	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe9_hr_timer enter %d\n", recipe_private->timer_period);

	if(toggle == 0){
		gpiod_set_value(recipe_private->gpio, 1);
		toggle = 1;
	}else{
		gpiod_set_value(recipe_private->gpio, 0);
		toggle = 0;
	}

	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe9_timer,
			hrtimer_cb_get_time(&recipe_private->recipe9_timer), ktime);

	return HRTIMER_RESTART;
//	return HRTIMER_NORESTART;
}

static ssize_t name_write(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct recipe9 *recipe_private = dev_get_drvdata(dev);

	sscanf(buf, "%s", recipe_private->name);
	dev_info(dev, "period_write period is %s\n", recipe_private->name);

	return count;
}

static ssize_t name_read(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct recipe9 *recipe_private = dev_get_drvdata(dev);

	dev_info(dev, "name_read name  is %s\n", recipe_private->name);

	return scnprintf(buf, sizeof(recipe_private->name), "%s\n", recipe_private->name);
}
static DEVICE_ATTR(name, 0664, name_read, name_write);

static ssize_t period_write(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int period;
	//struct recipe9 *recipe_private = platform_get_drvdata(pdev);
	struct recipe9 *recipe_private = dev_get_drvdata(dev);

	sscanf(buf, "%d", &period);
	dev_info(dev, "period_write period is %d\n", period);

	if(period >= 0){
		recipe_private->timer_period = period;
	}else{
		return -EINVAL;
	}

	return count;
}
static ssize_t period_read(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	//struct recipe9 *recipe_private = platform_get_drvdata(pdev);
	struct recipe9 *recipe_private = dev_get_drvdata(dev);

	dev_info(dev, "period_read period is %d\n", recipe_private->timer_period);

	return scnprintf(buf, sizeof(int)+1, "%d\n", recipe_private->timer_period);
}

static DEVICE_ATTR(period, 0664, period_read, period_write);

static struct attribute *recipe_attrs[] = {
	&dev_attr_period.attr,
	&dev_attr_name.attr,
	NULL,
};

static struct attribute_group recipe_sys_group = {
	.name = "recipe_sys",
	.attrs = recipe_attrs,
};

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe9 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe9, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe9 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe9, recipe_miscdevice);
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
	struct recipe9 *recipe_private;
	ktime_t ktime;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe9), GFP_KERNEL);
	recipe_private->pdev = pdev;
	recipe_private->timer_period = 200;

	ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe9_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	recipe_private->recipe9_timer.function = &recipe9_hr_timer;

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
	strcpy(recipe_private->name, "test");
	gpiod_direction_output(recipe_private->gpio, 0);
	gpiod_set_value(recipe_private->gpio, 1);

	/* Register sysfs call back function */
	ret = sysfs_create_group(&dev->kobj, &recipe_sys_group);
	if (ret < 0) {
		dev_err(dev, "could not register sysfs group\n");
		return ret;
	}

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret){
		sysfs_remove_group(&dev->kobj, &recipe_sys_group);
		return ret; /* misc_register returns 0 if success */
	}
	hrtimer_start(&recipe_private->recipe9_timer, ktime, HRTIMER_MODE_REL );
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe9 *recipe_private = platform_get_drvdata(pdev);
	int ret;

	dev_info(dev, "recipe_remove() function is called.\n");

	ret = hrtimer_cancel(&recipe_private->recipe9_timer);
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}
	sysfs_remove_group(&dev->kobj, &recipe_sys_group);
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe9"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe9",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & sysfs module ");
