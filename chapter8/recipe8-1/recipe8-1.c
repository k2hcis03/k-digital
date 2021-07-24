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

struct recipe10{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct gpio_desc *key_int;
	int irq;
	struct tasklet_struct tlet;
	wait_queue_head_t wait_queue;
	int flag;
	unsigned char value;
};
static char *KEYS_NAME = "RECIPE10_INT";

static irqreturn_t recipe10_isr(int irq, void *data)
{
	struct recipe10 *priv = data;
	struct device *dev;

	dev = &priv->pdev->dev;
	dev_info(dev, "recipe10_isr is called & irq number is %d\n", priv->irq);

	tasklet_hi_schedule(&priv->tlet);

	return IRQ_HANDLED;
}

void recipe10_tasklet_fn(unsigned long data)
{
	struct recipe10 *priv = (struct recipe10 *)data;
	struct device *dev;
	dev = &priv->pdev->dev;
	dev_info(dev, "recipe10_tasklet_fn is called\n");

	if(priv->flag == 0){
		priv->flag = 1;
		wake_up_interruptible(&priv->wait_queue);
	}
}

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


static int recipe_read(struct file *file, char __user *buff,
	               size_t count, loff_t *off)
{
	struct recipe10 * recipe_private;
	struct device *dev;
	int ret;
	char *data;

	recipe_private = container_of(file->private_data, struct recipe10,
			recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_read entered\n");

	recipe_private->flag = 0;
	ret = wait_event_interruptible(recipe_private->wait_queue, recipe_private->flag != 0);
	if(ret)
		return ret;
	dev_info(dev, "recipe_read is wakened\n");

	data = devm_kzalloc(dev, count, GFP_KERNEL);
	if(!data){
		dev_info(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->value += 1;

	memset(data, recipe_private->value, count);
	if(copy_to_user(buff, data, count)){
		return -EFAULT;
	}
	*off+=count;

	return count;
}
static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.read = recipe_read,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe10 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe10), GFP_KERNEL);
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

	ret = devm_request_threaded_irq(dev, recipe_private->irq, recipe10_isr,
						NULL, IRQF_TRIGGER_FALLING | IRQF_SHARED,
						KEYS_NAME, recipe_private);
	if (ret) {
		dev_err(dev, "Failed to request interrupt %d, error %d\n", recipe_private->irq, ret);
		return ret;
	}
	/* Init the wait queue head */
	init_waitqueue_head(&recipe_private->wait_queue);
	recipe_private->flag = 0;
	recipe_private->value = 0;

	/* Init tasklet */
	tasklet_init(&recipe_private->tlet, recipe10_tasklet_fn, (unsigned long)recipe_private);

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
	struct recipe10 *recipe_private = platform_get_drvdata(pdev);
	dev_info(dev, "recipe_remove() function is called.\n");
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
		.name = "recipe10",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & interrupt & tasklet module ");
