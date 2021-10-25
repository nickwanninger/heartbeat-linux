#include <linux/device.h>  // Header to support the kernel Driver Model
#include <linux/errno.h>
#include <linux/fs.h>  // Header for the Linux file system support
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/smp.h>

#define INTERRUPT_US 10
#define INTERRUPT_NS (INTERRUPT_US * 1000)

#define BUFFER_SIZE 50000
#define INFO()

#define DEV_NAME "heartbeat"


unsigned long hb_cycles(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}



// The high resolution timer object
static struct hrtimer timer;

#define MEASUREMENT_COUNT 50000
static long nmeasurements = 0;
static size_t measurements_size = MEASUREMENT_COUNT * sizeof(uint64_t);
static uint64_t *measurements;
static uint64_t last_time = 0;

static void hb_process_data(void) {
  uint64_t total = 0;
  int i;
  for (i = 0; i < MEASUREMENT_COUNT; i++) {
    total += measurements[i];
    // printk(KERN_INFO " meas[%d] = %lluus\n", i, measurements[i] / 1000);
  }
  printk(KERN_INFO "Average interval: %llu\n", (total / MEASUREMENT_COUNT));
  return;
}

static enum hrtimer_restart hb_timer_handler(struct hrtimer *timer) {
  // 1. get the ptregs from the process
  // 2. setup the stack of the process to emulate a function call
  // 3. change the instruction pointer to a handler function
  //    that is given by the process through the ioctl interface
  // 4. return from the handler
  //
  // some problems:
  // 1. This module may not be run "on top" of the process (in an
  //    irq) or even on the same CPU core, so we may need to use
  //    a fast IPI mechanism to alert another core.
  // 2. We may not have access to `current` as this handler may be
  //    independent from interrupts/scheduling

  // printk(KERN_INFO "core: %d %p\n", smp_processor_id(), get_current());
  uint64_t now, dt;
  hrtimer_forward_now(timer, ns_to_ktime(INTERRUPT_NS));

  // return HRTIMER_RESTART;

  if (nmeasurements >= MEASUREMENT_COUNT) {
    hb_process_data();
    return HRTIMER_NORESTART;
  }

  // now = ktime_get_ns();
	now = hb_cycles();
  dt = now - last_time;
  last_time = now;

  measurements[nmeasurements++] = dt;

  return HRTIMER_RESTART;
}

static int major;
static int numberOpens = 0;

static struct class *deviceclass = NULL;
static struct device *device = NULL;

static int hb_dev_open(struct inode *inodep, struct file *filep) {
  numberOpens++;
  printk(KERN_INFO "Device has been opened %d time(s)\n", numberOpens);
  return 0;
}

static ssize_t hb_dev_read(struct file *filep, char *buffer, size_t len,
			loff_t *offset) {
  return 0;
}

static ssize_t hb_dev_write(struct file *filep, const char *buffer, size_t len,
			 loff_t *offset) {
  return 0;
}

static int hb_dev_release(struct inode *inodep, struct file *filep) {
  printk(KERN_INFO "Device successfully closed\n");
  return 0;
}

static struct file_operations fops = {
    .open = hb_dev_open,
    .read = hb_dev_read,
    .write = hb_dev_write,
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


  measurements = (uint64_t *)kmalloc(measurements_size, GFP_KERNEL);
  if (!measurements) {
    printk(KERN_ERR "Failed to allocate measurements array\n");
    return 1;
  }

  hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  timer.function = &hb_timer_handler;

  // initialize last_time
  last_time = hb_cycles();

  hrtimer_start(&timer, ns_to_ktime(INTERRUPT_NS), INTERRUPT_NS);
  printk(KERN_INFO "started timer_module!\n");

  return 0;
}

static void __exit heartbeat_exit(void) {
  kfree(measurements);
  hrtimer_cancel(&timer);
  device_destroy(deviceclass, MKDEV(major, 0));	 // remove the device
  class_unregister(deviceclass);		 // unregister the device class
  class_destroy(deviceclass);			 // remove the device class
  unregister_chrdev(major, DEV_NAME);	 // unregister the major number
  return;
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick Wanninger<ncw@u.northwestern.edu>, Deniz Ulusel<ulusel@u.northwestern.edu>");
MODULE_DESCRIPTION("Heartbeat Kernel Module");
MODULE_VERSION("0.1");
