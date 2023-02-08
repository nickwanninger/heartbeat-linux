#include "./heartbeat_kmod.h"

#include <asm/apic.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/uaccess.h>

#include <linux/delay.h>

#define INFO(...) printk(KERN_INFO "Heartbeat: " __VA_ARGS__)

// #define HEARTBEAT_DEBUG

#ifdef HEARTBEAT_DEBUG
#define DEBUG(...) printk(KERN_INFO "[dbg] Heartbeat: " __VA_ARGS__)
#else
#define DEBUG(...)
#endif
#define DEV_NAME "heartbeat"

#define CR0_WP (1u << 16)
#define HB_VECTOR 0xda

static int major = -1;
static struct class *deviceclass = NULL;
static struct device *device = NULL;

static int hb_vector = -1;
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
  size_t n;
  off_t src;
  int64_t k, not_found;
  hb = arg;

  if (current == NULL)
    return;
  if (current->tgid != hb->owner->tgid)
    return;

  // Grab the register file using the ptrace interface - this may
  // seem like a bit of a hack, but it seems safe to use based on
  // it's usage elsewhere in the kernel for things like bactrace
  // generation and the likes.
  regs = task_pt_regs(current);

  if (hb->nrfs != 0) {
    if (hb->rfs) {

      // INFO("Dispatch rip=%lx\n", regs->ip);

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

static int hist[32];
static int next_hist = 0;

static enum hrtimer_restart hb_timer_handler(struct hrtimer *timer) {
  struct hb_priv *hb;

  // Grab the heartbeat private data from the timer itself
  hb = container_of(timer, struct hb_priv, timer);
  // If the thread is being torn down, or the file is being closed,
  // hb->done will have a 1 written to it. This is intended to abort
  // any heartbeat ASAP without corrupting the kernel state.
  if (hb->done) {
    // INFO("done\n");
    return HRTIMER_NORESTART;
  }

  hb->heartbeat_count++;

  // dispatch the heartbeat to the other cores
  // on_each_cpu(hb_timer_dispatch, (void *)hb, 1);

  if (hb_vector != -1) {
    // printk("About to send apic %llx!\n", apic);
    apic->send_IPI_all(hb_vector);
  }
  if (!hb->repeat) {
    return HRTIMER_NORESTART;
  }

restart:
  // When repeating, we must forward the timer to a time in the future.
  // Otherwise the apic or hpet will just interrupt storm the kernel and
  // lock everything up. Not very fun.
  hrtimer_forward_now(timer, ns_to_ktime(hb->interval_us * 1000));
  return HRTIMER_RESTART;
}

static void hb_cleanup_on_core(void *arg) {
  struct hb_priv *hb;
  hb = arg;

  DEBUG("cleanup on core %d\n", hb->core);

  // Set the `done` flag to one on this core. We don't have to worry
  // about TSO or atomics here as we are on the core that will read
  hb->done = 1;
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
  INFO("Open\n");

  struct hb_priv *hb;
  // Allocate the private data and ensure that it is zeroed
  hb = filep->private_data = kmalloc(sizeof(struct hb_priv), GFP_KERNEL);
  memset(filep->private_data, 0, sizeof(struct hb_priv));
  INFO("Private data after: %llx\n",
       (long long unsigned int)filep->private_data);

  get_cpu();
  // Configure the owner `task_struct` for this hb_priv.
  hb->owner = current;
  hb->rfs = NULL;
  hb->nrfs = 0;
  hb->return_address = (off_t)NULL;
  hb->heartbeat_count = 0;

  INFO("open on core %d\n", smp_processor_id());
  // And finally initialize the hrtimer in the private data
  // with our callback function.
  hrtimer_init(&hb->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hb->timer.function = hb_timer_handler;
  put_cpu();
  return 0;
}

static int hb_dev_release(struct inode *inodep, struct file *filep) {
  struct hb_priv *hb;
  INFO("Release\n");

  hb = filep->private_data;
  INFO("Closed heartbeat device %llx with %lu heartbeats\n",
       (long long unsigned int)filep->private_data,
       (long unsigned int)hb->heartbeat_count);

  // call on the owner core, and wait
  smp_call_function_single(hb->core, hb_cleanup_on_core, hb, 1);
  filep->private_data = NULL;
  return 0;
}

/**
 * hb_ioctl
 *
 * The main interface for the userspace to configure a heartbeat.
 */
static long hb_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  struct hb_priv *hb;
  INFO("ioctl %lu %lu\n", cmd, arg);
  //  int i;
  uint64_t ns;

  // Grab the private data from the file like in other methods
  hb = file->private_data;

  // sanity check heartbeat state.
  if (hb == NULL)
    return -EINVAL;

  // If the user requests a heartbeat to be scheduled...
  if (cmd == HB_SCHEDULE) {
    INFO("Schedule\n");
    // Copy their request configuration into the kernel...
    struct hb_configuration config;
    if (copy_from_user(&config, (struct hb_configuration *)arg,
                       sizeof(struct hb_configuration))) {
      return -EINVAL;
    }

    get_cpu();

    // Update the `hb` structure with the required info
    ns = config.interval * 1000;
    hb->interval_us = config.interval;
    hb->owner = current;
    hb->return_address = config.handler_address;
    hb->core = smp_processor_id();
    hb->repeat = config.repeat;

    INFO("schedule on core %d\n", smp_processor_id());
    // and schedule the hrtimer
    hrtimer_forward_now(&hb->timer, ns_to_ktime(ns));
    hrtimer_start(&hb->timer, ns_to_ktime(ns), HRTIMER_MODE_REL);

    put_cpu();
    return 0;
  }

  if (cmd == HB_SET_ROLLFORWARDS) {
    INFO("Set rollforwards\n");

    struct hb_set_rollforwards_arg rfarg;

    // hmm... should this be allowed?
    if (hb->rfs != NULL)
      return -EEXIST;
    if (copy_from_user(&rfarg, (struct hb_set_rollforwards_arg *)arg,
                       sizeof(struct hb_set_rollforwards_arg))) {
      return -EINVAL;
    }

    hb->rfs = kmalloc(sizeof(struct hb_rollforward) * rfarg.count, GFP_KERNEL);
    hb->nrfs = rfarg.count;

    if (copy_from_user(hb->rfs, rfarg.rfs,
                       sizeof(struct hb_rollforward) * rfarg.count)) {
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

/* ---- Begin modifications ---- */

unsigned long hb_error_entry;
module_param_named(hb_error_entry, hb_error_entry, ulong, 0);
MODULE_PARM_DESC(hb_error_entry, "Address of the `hb_error_entry` symbol");

unsigned long hb_error_return;
module_param_named(hb_error_return, hb_error_return, ulong, 0);
MODULE_PARM_DESC(hb_error_return, "Address of the `hb_error_return` symbol");

uint64_t original_HB_handler;
extern void * _hb_idt_entry;

int cpu_checkin[8] = {0};

static void force_write_cr0(unsigned long new_val) {
    asm __volatile__ (
        "mov %0, %%rdi;"
        "mov %%rdi, %%cr0;"
        ::"m"(new_val)
    );
}

static unsigned long force_read_cr0(void) { 
    unsigned long cr0_val;
    asm __volatile__ (
        "mov %%cr0, %%rdi;"
        "mov %%rdi, %0;"
        :"=m"(cr0_val)
    );
    return cr0_val;
}

static struct desc_ptr get_idtr(void) {
    struct desc_ptr idtr;
    asm __volatile__("sidt %0" : "=m"(idtr));
    return idtr;
}

static uint64_t extract_handler_address(gate_desc * gd) {
    uint64_t handler;
	handler = (uint64_t) gd->offset_low;
	handler += (uint64_t) gd->offset_middle << 16;
	handler += (uint64_t) gd->offset_high << 32;
    return handler;
}

static void write_handler_address_to_gd(gate_desc * gd, unsigned long handler) {
    uint16_t low = (uint16_t) (handler);
    uint16_t middle =(uint16_t) (handler >> 16);
    uint32_t high = (uint32_t) (handler >> 32);
    gd->offset_low = low;
    gd->offset_middle = middle;
    gd->offset_high = high;
}

static void modify_idt(void) {
    struct desc_ptr IDTR;
    gate_desc * idt;
    gate_desc * HB_gate_desc;
    uint64_t new_HB_handler;
		uint64_t debug_handler;

    // Get IDTR to find IDT base
    IDTR = get_idtr();
    idt = (gate_desc *) IDTR.address;
    printk("[*] IDT @ 0x%px\n", idt);

		// DEBUG
		debug_handler = extract_handler_address(idt + 0xf7);
		printk("DEBUG HANDLER %llx\n", debug_handler);

    // Offset into IDT to #XF Gate Desc
    HB_gate_desc = idt + HB_VECTOR;

    // Save the old gate descriptor info in case
    original_HB_handler = extract_handler_address(HB_gate_desc); 

    // Disable write protections
    force_write_cr0(force_read_cr0() & ~(CR0_WP));

    // Put our fake gate descriptor in
    write_handler_address_to_gd(HB_gate_desc, (unsigned long) &_hb_idt_entry);
    new_HB_handler = extract_handler_address(HB_gate_desc); 

    // Enable write protections
    force_write_cr0(force_read_cr0() | CR0_WP);

    // Profit
    printk("[!] Modified IDT: 0x%llx -> 0x%llx\n", original_HB_handler, new_HB_handler);
}

void hb_irq_handler(struct pt_regs *regs) {
	asm __volatile__("cli");
	int cpu = smp_processor_id();
	cpu_checkin[cpu] = 1;
  printk("[+] CPU 0x%x: Hit hb irq handler\n", cpu);
	apic_eoi();
	asm __volatile__("sti");
	return;
}

EXPORT_SYMBOL(hb_irq_handler);

struct user_process_info {
  void (*user_handler)(void);
  void * state_save_area;
} user_proc_info;

static void interrupt_setup(void) {
  hb_vector = HB_VECTOR;

  modify_idt();

	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	asm __volatile__("mfence":::"memory");

  // send the IPI to that vector
  if (hb_vector != -1) apic->send_IPI_allbutself(hb_vector);
	//if (hb_vector != -1) apic->send_IPI_all(hb_vector);
}


/* ---- End modifications ---- */

/**
 * heartbeat_init
 *
 * Initialize the kernel module by creating /dev/heartbeat
 * and registering various functions. Most of this function
 * is gross boilerplate. Don't look into it too much or it
 * might break
 */
static int __init heartbeat_init(void) {

  int i;
  int rc;

	unsigned long flags;
	local_irq_save(flags);
  
	interrupt_setup();

  deviceclass = NULL;
  device = NULL;
  // first, we create the device node in Linux
  major = register_chrdev(0, DEV_NAME, &fops);
  if (major < 0) {
    printk(KERN_ALERT "fast timer failed to register a major number\n");
    return major;
  }
  INFO("Major = %d\n", major);
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

  // for (hb_vector = 240; hb_vector > 0; hb_vector--) {
  //   // set_irq_flags(hb_vector, IRQF_VALID);
  //   rc = request_irq(hb_vector, (void *)hb_irq_handler, IRQF_SHARED,
  //                    "Heartbeat", &fops);
  //   if (rc == 0) {
  //     INFO("Found irq at %d\n", hb_vector);
  //     break;
  //   }
  // }
  // if (hb_vector == 240)
  //   hb_vector = -1;

  // if (hb_vector != -1) {
  //   for (i = 0; i < 10; i++) {
  //     apic->send_IPI_all(hb_vector);
  //   }
  // }

  INFO("hb_vector=%d\n", hb_vector);  
	INFO("Finished initialization\n");
	
	local_irq_restore(flags);

  return 0;
}

/**
 * heartbeat_exit
 *
 * Teardown all the state and driver information for heartbeat
 */
static void __exit heartbeat_exit(void) {
  if (deviceclass) {
    device_destroy(deviceclass, MKDEV(major, 0)); // remove the device
    class_unregister(deviceclass);                // unregister the device class
    class_destroy(deviceclass);                   // remove the device class
  }
  unregister_chrdev(major, DEV_NAME); // unregister the major number
  free_irq(hb_vector, &fops);

  return;
}

module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick Wanninger<ncw@u.northwestern.edu>");
MODULE_DESCRIPTION("Heartbeat Kernel Module");
MODULE_VERSION("0.1");
