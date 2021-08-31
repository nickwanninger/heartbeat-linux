#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#define INTERRUPT_NS 100000 // 100 us
#define BUFFER_SIZE 500000
#define INFO()
static struct hrtimer timer;
static uint64_t inc_count = 0;
static uint64_t agg_interval_sum = 0;
static uint64_t *intervals;

static void process_data()
{

        uint64_t avg_interval = agg_interval_sum / inc_count;
        printk(KERN_INFO "Average interval size: %" PRIu64 "\n", avg_interval);
        return;
}
static inline uint64_t record_time()
{
        uint64_t ts = ktime_get_ns();
        intervals[inc_count] = ts;
        inc_count++;
        return ts;
}
static uint64_t *init_intervals_arr()
{
        intervals = (uint64_t *)kmalloc(sizeof(uint64_t) * BUFFER_SIZE);
        if (!intervals)
        {
                return NULL;
        }
        memset(intervals, 0, sizeof(uint64_t) * BUFFER_SIZE);
        return intervals;
}

static enum hrtimer_restart timer_handler(struct hrtimer *timer)
{
        hrtimer_forward_now(&timer, ns_to_ktime(INTERRUPT_NS));
        if (inc_count < BUFFER_SIZE)
        {
                uint64_t cur = record_time();
                agg_interval_sum += cur - intervals[inc_count - 1];
                return HRTIMER_RESTART;
        }
        printk(KERN_INFO "finished timer iteration.\n");
        return HRTIMER_NORESTART;
}

static int __init timer_init(void)
{
        intervals = init_intervals_arr();
        if (!intervals)
        {
                printk(KERN_ERROR "Failed to allocate intervals array\n");
                return;
        }
        hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        timer.function = &timer_handler;
        record_time(); //adds first time and increments inc_count
        hrtimer_start(&timer, ns_to_ktime(INTERRUPT_NS), INTERRUPT_NS);
        printk(KERN_INFO "started timer_module!\n");

        return 0;
}

static void __exit timer_exit(void)
{
        hrtimer_cancel(&timer);
        printk(KERN_INFO "exiting timer_module!\n");
        return;
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luke Arnold");
MODULE_DESCRIPTION("Kernel Timer Module");
MODULE_VERSION("0.0");