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
#include <linux/spi/spi.h>
#include "ioctl.h"

#define DEVICE_NAME 		"recipedev"
#define US_TO_NS(x) 		(x * 1000L)
#define MMAP_SIZE			4096		//256 * 4 byte
#define MMAP_CNT			512

struct recipe12{
	struct platform_device *pdev;
	struct spi_device *spi;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe_hr_timer;
	int timer_period;
	struct task_struct *recipe_thread;
	struct completion recipe_complete_ok;
	char *mmap_buf;
	int cnt;
	u8 tx_buf;
	u8 rx_buf[2];
};

int recipe_thread(void *priv)
{
	struct recipe12 * recipe_private = (struct recipe12 *)priv;
	struct device *dev = &recipe_private->spi->dev;
	int index = 0;
	int status;
	int start_bit = 1;
	int differential = 0;
	int channel = 0;

	dev_info(dev, "recipe_thread() called\n");

	while(!kthread_should_stop()) {
    	if(recipe_private->cnt == MMAP_CNT){
    		dev_info(dev, "data count is %d.\n", recipe_private->cnt);
    		recipe_private->cnt = 0;
    		sysfs_notify(&dev->kobj, "recipe_sys", "notify");
    		dev_info(dev, "notify_write is called.\n");
    	}else{
    		wait_for_completion_interruptible(&recipe_private->recipe_complete_ok);
        	recipe_private->tx_buf = ((start_bit << 6) | (!differential << 5) | (channel << 2));
        	status = spi_write_then_read(recipe_private->spi, &recipe_private->tx_buf, 1, recipe_private->rx_buf, 2);
        	if(status < 0){
        		dev_info(dev, "error reading ADC\n");
        	}
		reinit_completion(&recipe_private->recipe_complete_ok);
		index = recipe_private->cnt++;
		*((int *)(recipe_private->mmap_buf)+index) = ((recipe_private->rx_buf[0] << 4) | (recipe_private->rx_buf[1] >> 4));
        	dev_info(dev, "recipe_thread is wakened from hrtimer %d and %d  %d\n", index,
        			recipe_private->rx_buf[0] << 4 , recipe_private->rx_buf[1] >> 4);
    	}
    }
    return 0;
}

enum hrtimer_restart recipe_hr_timer(struct hrtimer *t)
{
	struct recipe12 *recipe_private = from_timer(recipe_private, t, recipe_hr_timer);
	struct device *dev;
	ktime_t ktime;

	dev = &recipe_private->spi->dev;
	//dev_info(dev, "recipe12_hr_timer enter %d\n", recipe_private->timer_period);

	complete(&recipe_private->recipe_complete_ok);

	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe_hr_timer,
		hrtimer_cb_get_time(&recipe_private->recipe_hr_timer), ktime);

	return HRTIMER_RESTART;
//	return HRTIMER_NORESTART;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe12 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe12, recipe_miscdevice);
	dev = &recipe_private->spi->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe12 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe12, recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static int recipe_mmap(struct file *file, struct vm_area_struct *vma) {

	struct recipe12 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data,  struct recipe12,
			recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_mmap() called\n");
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys(recipe_private->mmap_buf) >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		return -EAGAIN;
	}
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe12 * recipe_private;
	struct device *dev;
	ktime_t ktime;
	int ret;

	recipe_private = container_of(file->private_data, struct recipe12, recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			dev_info(dev, "SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			dev_info(dev, "User data is %d\n", info.data);
			if(info.data == 0){
				ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
				if(ret){
					dev_info(dev, "The timer was still in use.");
				}
			}else if(info.data == 1){
				recipe_private->cnt = 0;
				ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
				hrtimer_start(&recipe_private->recipe_hr_timer, ktime, HRTIMER_MODE_REL );
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
	.mmap = recipe_mmap,
	.release = recipe_close,
};

static ssize_t notify_write(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int period;
	struct recipe12 *recipe_private = dev_get_drvdata(dev);

	sscanf(buf, "%d", &period);
	dev_info(dev, "notify_write is called.%d\n", recipe_private->timer_period);


	//sysfs_notify(&dev->kobj, "recipe_sys", "notify");

	return count;
}

static ssize_t notify_read(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct recipe12 *recipe_private = dev_get_drvdata(dev);

	dev_info(dev, "notify_read is called\n");

	return scnprintf(buf, sizeof(int)+1, "%d\n", recipe_private->timer_period);
}

static DEVICE_ATTR(notify, 0664, notify_read, notify_write);

static struct attribute *recipe_attrs[] = {
	&dev_attr_notify.attr,
	NULL,
};

static struct attribute_group recipe_sys_group = {
	.name = "recipe_sys",
	.attrs = recipe_attrs,
};

static int recipe_probe(struct spi_device *spi)
{
	int ret;
	struct device *dev = &spi->dev;
	struct recipe12 *recipe_private;
	ktime_t ktime;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&spi->dev, sizeof(struct recipe12), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->spi = spi;
	recipe_private->timer_period = 1000;
	recipe_private->cnt = 0;

	recipe_private->mmap_buf = kzalloc(MMAP_SIZE, GFP_KERNEL);
	if(!recipe_private->mmap_buf){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	init_completion(&recipe_private->recipe_complete_ok);

	recipe_private->recipe_thread = kthread_run(recipe_thread,recipe_private,"recipe Thread");
	if(recipe_private->recipe_thread){
		dev_info(dev, "Kthread Created Successfully.\n");
	} else {
		dev_err(dev, "Cannot create kthread\n");
		return -ENOMEM;
	}
//
	ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	recipe_private->recipe_hr_timer.function = &recipe_hr_timer;

	/* Register sysfs call back */
	ret = sysfs_create_group(&dev->kobj, &recipe_sys_group);
	if (ret < 0) {
		dev_err(dev, "could not register sysfs group\n");
		return ret;
	}
	
	/* Attach the SPI device to the private structure */
	spi_set_drvdata(spi, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret){
		sysfs_remove_group(&dev->kobj, &recipe_sys_group);
		return ret; /* misc_register returns 0 if success */
	}
	dev_info(dev, "recipe_probe() function is completed.\n");
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct recipe12 *recipe_private = spi_get_drvdata(spi);
	int ret;

	dev_info(dev, "recipe_remove() function is called1.\n");

	kthread_stop(recipe_private->recipe_thread);
	dev_info(dev, "recipe_remove() function is called2.\n");
	complete(&recipe_private->recipe_complete_ok);
	dev_info(dev, "recipe_remove() function is called3.\n");
	kfree(recipe_private->mmap_buf);
	dev_info(dev, "recipe_remove() function is called4.\n");
	ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}
	sysfs_remove_group(&dev->kobj, &recipe_sys_group);
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe12", },
	{ }
};
MODULE_DEVICE_TABLE(of, recipe_of_ids);

static const struct spi_device_id recipe_id[] = {
	{ .name = "MCP3204", },
	{ }
};
MODULE_DEVICE_TABLE(spi, recipe_id);

static struct spi_driver recipe_platform_driver = {
	.driver = {
		.name = "recipe12_spi",
		.owner = THIS_MODULE,
		.of_match_table = recipe_of_ids,
	},
	.probe   = recipe_probe,
	.remove  = recipe_remove,
	.id_table	= recipe_id,
};
/* Register our platform driver */
module_spi_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & spi. module ");
