#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static char *who   = "world";
static int   times = 1;

module_param(who, charp, 0644);
MODULE_PARM_DESC(who, "who to greet");
module_param(times, int, 0644);
MODULE_PARM_DESC(times, "number of greetings");

static int __init hello_init(void)
{
    int i;
    for (i = 0; i < times; i++)
        pr_info("hello, %s (%d)\n", who, i);
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("goodbye, %s\n", who);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("textbook");
MODULE_DESCRIPTION("hello world kernel module");
MODULE_VERSION("0.1");
