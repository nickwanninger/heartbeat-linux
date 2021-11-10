#include <heartbeat_kmod.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
#include <linux/smp.h>


#define INFO()

#define DEV_NAME "heartbeat"


static enum hrtimer_restart hb_timer_handler(struct hrtimer *timer) {

  return HRTIMER_NORESTART;
}

static int major;
static int numberOpens = 0;

static struct class *deviceclass = NULL;
static struct device *device = NULL;


struct hb_priv {
	struct hrtimer timer;
	off_t return_address;
};

static int hb_dev_open(struct inode *inodep, struct file *filep) {
	struct hb_priv *hb;


	filep->private_data = (struct hb_priv*)kmalloc(sizeof(struct hb_priv), GFP_KERNEL);
	hb = filep->private_data;


	hrtimer_init(&hb->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

  numberOpens++;
  printk(KERN_INFO "Device has been opened %d time(s)\n", numberOpens);
  return 0;
}

static ssize_t hb_dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) { return 0; }

static ssize_t hb_dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) { return 0; }

static int hb_dev_release(struct inode *inodep, struct file *filep) {

	struct hb_priv *hb;


	hb = filep->private_data;
  printk(KERN_INFO "Device successfully closed %lx\n", (off_t)filep->private_data);


	hrtimer_cancel(&hb->timer);
	kfree(filep->private_data);
  return 0;
}

static long hb_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct hb_priv *hb;
	hb = file->private_data;
  printk("Heartbeat IOCTL\n");
  if (cmd == HB_SCHEDULE) {
    struct hb_configuration config;

    printk("Heartbeat init %lx\n", arg);
    if (copy_from_user(&config, (struct hb_configuration *)arg, sizeof(struct hb_configuration))) {
      return -EINVAL;
    }
    printk("hb init at %lx\n", config.handler_address);
		uint64_t ns = config.interval * 1000;

		hrtimer_start(&hb->timer, ns_to_ktime(ns), ns);
    return 0;
  }
  return -EINVAL;
}

static struct file_operations fops = {
    .open = hb_dev_open,
    .read = hb_dev_read,
    .write = hb_dev_write,
    .unlocked_ioctl = hb_ioctl,
    .release = hb_dev_release,
};

static int __init heartbeat_init(void) {
  // first, we create the device node in Linux
  major = register_chrdev(0, DEV_NAME, &fops);
  if (major < 0) {
    printk(KERN_ALERT "fast timer failed to register a major number\n");
    return major;
  }

  deviceclass = class_create(THIS_MODULE, DEV_NAME);
  if (IS_ERR(deviceclass)) {
    unregister_chrdev(major, DEV_NAME);
    printk(KERN_ALERT "Failed to register device class\n");
    return PTR_ERR(deviceclass);
  }

  // Register the device driver
  device = device_create(deviceclass, NULL, MKDEV(major, 0), NULL, DEV_NAME);
  if (IS_ERR(device)) {
    class_destroy(deviceclass);
    unregister_chrdev(major, DEV_NAME);
    printk(KERN_ALERT "Failed to create the device\n");
    return PTR_ERR(device);
  }

  printk(KERN_INFO "started timer_module!\n");

  return 0;
}

static void __exit heartbeat_exit(void) {
  // kfree(measurements);
  // hrtimer_cancel(&timer);
  device_destroy(deviceclass, MKDEV(major, 0));  // remove the device
  class_unregister(deviceclass);                 // unregister the device class
  class_destroy(deviceclass);                    // remove the device class
  unregister_chrdev(major, DEV_NAME);            // unregister the major number
  return;
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(
    "Nick Wanninger<ncw@u.northwestern.edu>, Deniz "
    "Ulusel<ulusel@u.northwestern.edu>");
MODULE_DESCRIPTION("Heartbeat Kernel Module");
MODULE_VERSION("0.1");
