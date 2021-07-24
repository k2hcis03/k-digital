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

#include "ioctl.h"

#define DEVICE_NAME 		"recipedev"
#define GPIO_17			17
#define GPIO_27			27
#define GPIO_22			22

struct recipe6_1{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	const char *led_name;
	char led_red_value;
	char led_green_value;
	char led_blue_value;
	struct gpio_desc *led_red, *led_green, *led_blue;
};

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe6_1 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe6_1, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe6_1 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe6_1, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe6_1 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data, struct recipe6_1, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			dev_info(dev, "SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			dev_info(dev, "User data is %d, %d, %d\n", info.led_red, info.led_green, info.led_blue);
			if(info.led_red == 1){
				gpiod_set_value(recipe_private->led_red, 1);
			}else if(info.led_red == 0){
				gpiod_set_value(recipe_private->led_red, 0);
			}
			if(info.led_green == 1){
				gpiod_set_value(recipe_private->led_green, 1);
			}else if(info.led_red == 0){
				gpiod_set_value(recipe_private->led_green, 0);
			}
			if(info.led_blue == 1){
				gpiod_set_value(recipe_private->led_blue, 1);
			}else if(info.led_blue == 0){
				gpiod_set_value(recipe_private->led_blue, 0);
			}
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

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.unlocked_ioctl = recipe_ioctl,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret, count;
	struct device *dev = &pdev->dev;
	struct recipe6_1 *recipe_private;
	struct device_node *child;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe6_1), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->pdev = pdev;

	count = device_get_child_node_count(dev);
	if (!count)
		return -ENODEV;
	dev_info(dev, "device child node count is %d\n", count);

	for_each_child_of_node(dev->of_node, child){
		const char *label_name, *colour_name;

		of_property_read_string(child, "label", &label_name);

		if (!strcmp(label_name,"led")) {
			of_property_read_string(child, "colour", &colour_name);
			dev_info(dev, "led label is %s", label_name);
			if(!strcmp(colour_name, "red")){
				dev_info(dev, "led colour is %s", colour_name);
				recipe_private->led_red = devm_gpiod_get_from_of_node(dev, child,
						"gpios", 0, GPIOD_ASIS, label_name);
				if (IS_ERR(recipe_private->led_red)) {
					ret = PTR_ERR(recipe_private->led_red);
					return ret;
				}
			}else if(!strcmp(colour_name, "green")){
				dev_info(dev, "led colour is %s", colour_name);
				recipe_private->led_green = devm_gpiod_get_from_of_node(dev, child,
						"gpios", 0, GPIOD_ASIS, label_name);
				if (IS_ERR(recipe_private->led_green)) {
					ret = PTR_ERR(recipe_private->led_green);
					return ret;
				}
			}else if(!strcmp(colour_name, "blue")){
				dev_info(dev, "led colour is %s", colour_name);
				recipe_private->led_blue = devm_gpiod_get_from_of_node(dev, child,
						"gpios", 0, GPIOD_ASIS, label_name);
				if (IS_ERR(recipe_private->led_blue)) {
					ret = PTR_ERR(recipe_private->led_blue);
					return ret;
				}
			}
		}
	}
	gpiod_direction_output(recipe_private->led_red, 1);
	gpiod_set_value(recipe_private->led_red, 1);
	gpiod_direction_output(recipe_private->led_green, 1);
	gpiod_set_value(recipe_private->led_green, 1);
	gpiod_direction_output(recipe_private->led_blue, 1);
	gpiod_set_value(recipe_private->led_blue, 1);

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
	struct recipe6_1 *recipe_private = platform_get_drvdata(pdev);

	dev_info(dev, "recipe_remove() function is called.\n");
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe6-1"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe6-1",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & ioctl & memory module ");
