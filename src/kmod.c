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
#include <linux/uaccess.h>

#define INFO(...) printk(KERN_INFO "Heartbeat: " __VA_ARGS__)

// #define HEARTBEAT_DEBUG

#ifdef HEARTBEAT_DEBUG
#define DEBUG(...) printk(KERN_INFO "[dbg] Heartbeat: " __VA_ARGS__)
#else
#define DEBUG(...)
#endif
#define DEV_NAME "heartbeat"

static int major = -1;
static struct class *deviceclass = NULL;
static struct device *device = NULL;


/**
 * struct hb_priv
 *
 * The private state for a hearbeat instance.
 */
struct hb_priv {
  struct hrtimer timer;
  struct task_struct *owner;
  off_t return_address;
  uint64_t interval_us;
  int repeat;
  int core;
  int done;

  unsigned nrfs;
  struct hb_rollforward *rfs;

  unsigned long heartbeat_count;
};


static void hb_timer_dispatch(void *arg) {
  struct hb_priv *hb;
  struct pt_regs *regs;
  hb = arg;


  if (current == NULL) return;
  if (current->tgid != hb->owner->tgid) return;

  // Grab the register file using the ptrace interface - this may
  // seem like a bit of a hack, but it seems safe to use based on
  // it's usage elsewhere in the kernel for things like bactrace
  // generation and the likes.
  regs = task_pt_regs(current);


  if (hb->nrfs != 0) {
    if (hb->rfs) {
      size_t n;
      off_t src;
      int64_t k, not_found;

      n = hb->nrfs;
      src = regs->ip;
      not_found = -1;

      // Binary search over the rollforwards
      {
        int64_t i = 0, j = (int64_t)n - 1;

        while (i <= j) {
          k = i + ((j - i) / 2);
          if ((off_t)hb->rfs[k].from == (off_t)src) {
            regs->ip = (off_t)hb->rfs[k].to;
            if (!hb->repeat) {
              return;
            }
            break;
          } else if ((off_t)hb->rfs[k].from < (off_t)src) {
            i = k + 1;
          } else {
            j = k - 1;
          }
        }
        k = not_found;
      }
    }
  }
}

/**
 * hb_timer_handler
 *
 * This function acts as the callback function for the heartbeat timer.
 * It deliveres heartbeats by directly editing the pt_regs structure of
 * the 'owner' thread (the thread which initialized the ioctl) to point
 * to a configured return address.
 *
 * We can only safely change the return address iff current == owner,
 * otherwise we run into race conditions. The only way to solve those
 * race conditions would be to use IPIs, which would be an enormous
 * latency hit when targeting ~10us heartbeats.
 */
static enum hrtimer_restart hb_timer_handler(struct hrtimer *timer) {
  struct hb_priv *hb;
  //  struct pt_regs *regs;
  //  uint64_t old_sp;
  //  int i;


  // Grab the heartbeat private data from the timer itself
  hb = container_of(timer, struct hb_priv, timer);
  // If the thread is being torn down, or the file is being closed,
  // hb->done will have a 1 written to it. This is intended to abort
  // any heartbeat ASAP without corrupting the kernel state.
  if (hb->done) {
    INFO("done\n");
    return HRTIMER_NORESTART;
  }

  hb->heartbeat_count++;

	// dispatch the heartbeat to the other cores
  on_each_cpu(hb_timer_dispatch, (void *)hb, 1);

  if (!hb->repeat) {
    return HRTIMER_NORESTART;
  }
  // When repeating, we must forward the timer to a time in the future.
  // Otherwise the apic or hpet will just interrupt storm the kernel and
  // lock everything up. Not very fun.
  hrtimer_forward_now(timer, ns_to_ktime(hb->interval_us * 1000));
  return HRTIMER_RESTART;
}


/**
 * hb_cleanup_on_core
 *
 * This method is invoked via an ipi request on release to ensure
 * that the hrtimer is cancelled from the core that created it
 */
static void hb_cleanup_on_core(void *arg) {
  struct hb_priv *hb;
  hb = arg;
  // Set the `done` flag to one on this core. We don't have to worry
  // about TSO or atomics here as we are on the core that will read
  hb->done = 1;
  DEBUG("cleanup on core %d\n", hb->core);
  // Cancel the timer. The docs claim this function will wait for any
  // outstanding ticks to finish, but I don't quite trust that :^)
  hrtimer_cancel(&hb->timer);
  // And finally, free the hb private structure.
  kfree(hb);
}




/**
 * hb_dev_open
 *
 * When `/dev/heartbeat` is opened by a userspace process, this function
 * is invoked and the private data is allocated and attached to the file
 * object. This private data mainly contains the `hrtimer` structure which
 * is the backbone of heartbeat in the Linux kernel
 */
static int hb_dev_open(struct inode *inodep, struct file *filep) {
  struct hb_priv *hb;
  // Allocate the private data and ensure that it is zeroed
  hb = filep->private_data = kmalloc(sizeof(struct hb_priv), GFP_KERNEL);
  memset(filep->private_data, 0, sizeof(struct hb_priv));
  DEBUG("Private data after: %llx\n", (long long unsigned int)filep->private_data);
  // Configure the owner `task_struct` for this hb_priv.
  hb->owner = current;
  hb->rfs = NULL;
  hb->nrfs = 0;
  hb->return_address = (off_t)NULL;
  hb->heartbeat_count = 0;
  // And finally initialize the hrtimer in the private data
  // with our callback function.
  hrtimer_init(&hb->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hb->timer.function = hb_timer_handler;
  return 0;
}

static int hb_dev_release(struct inode *inodep, struct file *filep) {
  struct hb_priv *hb;

  hb = filep->private_data;
  INFO("Closed heartbeat device %llx with %lu heartbeats\n", (long long unsigned int)filep->private_data, (long unsigned int)hb->heartbeat_count);


  // call on the owner core, and wait
  smp_call_function_single(hb->core, hb_cleanup_on_core, hb, 1);
  filep->private_data = NULL;
  return 0;
}

static volatile int glob = 0;


/**
 * hb_ioctl
 *
 * The main interface for the userspace to configure a heartbeat.
 */
static long hb_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  struct hb_priv *hb;
  //  int i;
  uint64_t ns;

  // Grab the private data from the file like in other methods
  hb = file->private_data;

  DEBUG("%d Heartbeat ioctl %llx from current=%llx\n", smp_processor_id(), (long long unsigned int)hb, (long long unsigned int)current);

  // sanity check heartbeat state.
  if (hb == NULL) return -EINVAL;

  // If the user requests a heartbeat to be scheduled...
  if (cmd == HB_SCHEDULE) {
    // Copy their request configuration into the kernel...
    struct hb_configuration config;
    if (copy_from_user(&config, (struct hb_configuration *)arg, sizeof(struct hb_configuration))) {
      return -EINVAL;
    }

    // Update the `hb` structure with the required info
    ns = config.interval * 1000;
    hb->interval_us = config.interval;
    hb->owner = current;
    hb->return_address = config.handler_address;
    hb->core = smp_processor_id();
    hb->repeat = config.repeat;
    // and schedule the hrtimer
    hrtimer_forward_now(&hb->timer, ns_to_ktime(ns));
    hrtimer_start(&hb->timer, ns_to_ktime(ns), HRTIMER_MODE_REL);
    return 0;
  }

  if (cmd == HB_SET_ROLLFORWARDS) {
    struct hb_set_rollforwards_arg rfarg;

    // hmm... should this be allowed?
    if (hb->rfs != NULL) return -EEXIST;
    if (copy_from_user(&rfarg, (struct hb_set_rollforwards_arg *)arg, sizeof(struct hb_set_rollforwards_arg))) {
      return -EINVAL;
    }

    // INFO("SET ROLLFORWARDS\n");
    hb->rfs = kmalloc(sizeof(struct hb_rollforward) * rfarg.count, GFP_KERNEL);
    hb->nrfs = rfarg.count;

    if (copy_from_user(hb->rfs, rfarg.rfs, sizeof(struct hb_rollforward) * rfarg.count)) {
      return -EINVAL;
    }
    return 0;
  }

  // Invalid command!
  return -EINVAL;
}


/**
 * The file operations structure for /dev/heartbeat.
 * We only care about open, ioctl, and release (close)
 */
static struct file_operations fops = {
    .open = hb_dev_open,
    .unlocked_ioctl = hb_ioctl,
    .release = hb_dev_release,
};


/**
 * heartbeat_init
 *
 * Initialize the kernel module by creating /dev/heartbeat
 * and registering various functions. Most of this function
 * is gross boilerplate. Don't look into it too much or it
 * might break
 */
static int __init heartbeat_init(void) {
  deviceclass = NULL;
  device = NULL;
  // first, we create the device node in Linux
  major = register_chrdev(0, DEV_NAME, &fops);
  if (major < 0) {
    printk(KERN_ALERT "fast timer failed to register a major number\n");
    return major;
  }
  // Then, create the deviceclass with the right name
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

  INFO("Finished initialization\n");

  return 0;
}


/**
 * heartbeat_exit
 *
 * Teardown all the state and driver information for heartbeat
 */
static void __exit heartbeat_exit(void) {
  if (deviceclass) {
    device_destroy(deviceclass, MKDEV(major, 0));  // remove the device
    class_unregister(deviceclass);                 // unregister the device class
    class_destroy(deviceclass);                    // remove the device class
  }
  unregister_chrdev(major, DEV_NAME);  // unregister the major number
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
