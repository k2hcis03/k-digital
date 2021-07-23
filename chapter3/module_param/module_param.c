/*
 * my_first_module.c
 *
 *  Created on: 2021. 1. 18.
 *      Author: k2h
 */

#include <linux/module.h>

static int param = 100;
module_param(param, int, S_IRUGO);

static int __init hello_init(void)
{
	pr_info("Hello world init\n");
	pr_info("parameter is %d\n", param);
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("Hello world exit\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a simple init/exit module test");
