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

#define INFO(...) printk(KERN_INFO "Heartbeat: " __VA_ARGS__)
#define DEV_NAME "heartbeat"

#undef INFO
#define INFO(...)
static int major;
static struct class *deviceclass = NULL;
static struct device *device = NULL;


struct hb_priv {
  struct hrtimer timer;
  struct task_struct *owner;
  off_t return_address;
	uint64_t interval_us;
	int repeat;
};

static enum hrtimer_restart hb_timer_handler(struct hrtimer *timer) {
  struct hb_priv *hb;
  struct pt_regs *regs;
  hb = container_of(timer, struct hb_priv, timer);


    // printk("hmm..\n");
	hrtimer_forward_now(timer, ns_to_ktime(hb->interval_us * 1000));

  // If the target thread is not running, schedule it again
	// at the same interval, hoping it schedules
  if (current != hb->owner) {
    // printk("hmm..\n");
  	// return HRTIMER_NORESTART;
    return HRTIMER_RESTART;
  }

  regs = task_pt_regs(hb->owner);

	if (regs == NULL) {
		printk("huhh!\n");
	}
  // printk("pc=%llx, sp=%llx. We need to swap to %llx\n", regs->ip, regs->sp, hb->return_address);


	uint64_t old_sp = regs->sp;

  // redzone is bad.
  regs->sp -= 128;
  copy_to_user((void *)regs->sp, &regs->ip, sizeof(regs->ip));
  regs->sp -= 8;
	// store the old sp on the stack so we can restore to it (redzone is bad.)
  copy_to_user((void *)regs->sp, &old_sp, sizeof(regs->ip));
  regs->ip = hb->return_address;

	if (hb->repeat) {
		hrtimer_forward_now(timer, ns_to_ktime(hb->interval_us * 1000));
		return HRTIMER_RESTART;
	}
  return HRTIMER_NORESTART;
}



static int hb_dev_open(struct inode *inodep, struct file *filep) {
  struct hb_priv *hb;

  hb = filep->private_data = kmalloc(sizeof(struct hb_priv), GFP_KERNEL);


  hb->owner = current;

  memset(filep->private_data, 0, sizeof(struct hb_priv));
  INFO("Private data after: %p\n", filep->private_data);


  hrtimer_init(&hb->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hb->timer.function = hb_timer_handler;

  return 0;
}

static int hb_dev_release(struct inode *inodep, struct file *filep) {
  struct hb_priv *hb;

  hb = filep->private_data;
  INFO("Closed heartbeat device %p\n", (off_t)filep->private_data);

  hrtimer_cancel(&hb->timer);
  kfree(filep->private_data);
	filep->private_data = NULL;
  return 0;
}

static long hb_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  struct hb_priv *hb;
  hb = file->private_data;

  INFO("%d Heartbeat IOCTL %llx from current=%llx\n", smp_processor_id(), hb, current);

  // sanity check.
  if (hb == NULL) return -EINVAL;

  if (cmd == HB_SCHEDULE) {
    struct hb_configuration config;
    if (copy_from_user(&config, (struct hb_configuration *)arg, sizeof(struct hb_configuration))) {
      return -EINVAL;
    }
    uint64_t ns = config.interval * 1000;
		hb->interval_us = config.interval;
    hb->owner = current;
    hb->return_address = config.handler_address;
		hb->repeat = config.repeat;

  	hrtimer_forward_now(&hb->timer, ns_to_ktime(ns));
    hrtimer_start(&hb->timer, ns_to_ktime(ns), HRTIMER_MODE_REL);
    return 0;
  }
  return -EINVAL;
}

static struct file_operations fops = {
    .open = hb_dev_open,
    .unlocked_ioctl = hb_ioctl,
    .release = hb_dev_release,
};

static int __init heartbeat_init(void) {
  deviceclass = NULL;
  device = NULL;
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
