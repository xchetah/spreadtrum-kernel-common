/* drivers/staging/android/rtcc.c
 *
 * RunTime CompCache v3 main file
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/cpu.h>
#include <linux/swap.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/vmstat.h>

#include <asm/atomic.h>

/*
 * RTCC reclaim entry, defined in vmscan.c
 */
extern unsigned long rtcc_reclaim_pages(unsigned long nr_to_reclaim,
	int swappiness, unsigned long *nr_swapped);
extern long nr_kswapd_swapped;

static long nr_krtccd_swapped;
static atomic_t krtccd_running;
static atomic_t need_to_reclaim;
static struct task_struct *krtccd;
static unsigned long prev_jiffy;
static unsigned long boost_end_jiffy;

#define BOOSTMODE_TIMEOUT		(60*HZ)
#define DEF_RECLAIM_INTERVAL	(10*HZ)
#define RTCC_MSG_ASYNC			1
#define RTCC_MSG_SYNC			2
#define RTCC_GRADE_NUM 			5
#define RTCC_GRADE_LIMIT		2

#ifdef CONFIG_SMP
#define RTCC_GRADE_MULTI		CONFIG_NR_CPUS
#else
#define RTCC_GRADE_MULTI		4
#endif

#define RTCC_DBG			1

int get_rtcc_status(void)
{
	return atomic_read(&krtccd_running);
}

static long swap_toplimit;

static int rtcc_boost_mode = 1;
static int rtcc_reclaim_interval = DEF_RECLAIM_INTERVAL;
static int rtcc_grade_size = RTCC_GRADE_NUM;
static int rtcc_grade[RTCC_GRADE_NUM] = {
	256 * RTCC_GRADE_MULTI,
	384 * RTCC_GRADE_MULTI,
	512 * RTCC_GRADE_MULTI,
	1024 * RTCC_GRADE_MULTI,
	4096 * RTCC_GRADE_MULTI,
};
// These values will be changed when system is booting up
static int rtcc_minfree[RTCC_GRADE_NUM] = {
        36 * 1024, // 144MB
        31 * 1024, // 124MB
        26 * 1024, // 104MB
        21 * 1024, // 84MB
        16 * 1024, // 64MB
};

static inline unsigned long get_swapped_pages(void)
{
	return (total_swap_pages - get_nr_swap_pages());
}

static inline unsigned long get_anon_pages(void)
{
	return global_page_state(NR_INACTIVE_ANON) + global_page_state(NR_ACTIVE_ANON);
}

static inline unsigned long get_file_pages(void)
{
	return global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM);
}

/*
 * Decide the rtcc grade based on free memory and free swap
 */
static int get_rtcc_grade(void)
{
	int free, i;

	// In boost mode, we will do reclaim in max speed
	if (unlikely(rtcc_boost_mode))
		return RTCC_GRADE_NUM - 1;

	// In other case, choose the grade by free memory level.
	free = global_page_state(NR_FREE_PAGES);
	for (i = 0; i <= RTCC_GRADE_LIMIT; i++) {
		if (free >= rtcc_minfree[i])
			break;
	}

	return i;
}

/*
 * Decide reclaim pages a time and the time interval based on rtcc grade
 */
static int get_reclaim_count(void)
{
	int grade, times;

	grade = get_rtcc_grade();

#if RTCC_DBG
	printk("rtcc grade = %d, swap_top = %ld\n", grade, swap_toplimit);
#endif

	if (unlikely(rtcc_boost_mode))
		return rtcc_grade[grade];

	// Divide a large reclaim into several smaller one
	times = rtcc_grade[grade] / rtcc_grade[RTCC_GRADE_LIMIT];
	if (likely(grade < RTCC_GRADE_LIMIT))
		times = 1;
	else
		grade = RTCC_GRADE_LIMIT;

	rtcc_reclaim_interval = DEF_RECLAIM_INTERVAL / times;

	return rtcc_grade[grade];
}

/*
 * Decide the ratio of anon and file pages in one reclaim
 */
static int get_reclaim_swappiness(void)
{
	int index;

	if(totalram_pages > (256 << 20) >> PAGE_SHIFT)
		index = RTCC_GRADE_NUM - 2;
	else
		index = RTCC_GRADE_NUM - 3;

	// The swap space in risk of using up, disable swap
	if (unlikely((swap_toplimit - get_swapped_pages()) <= rtcc_grade[index]))
		return 1;

	// File pages is too few, only do swap. We need keep the file cache
	// at a rational level
	if (unlikely(get_file_pages() <= rtcc_minfree[RTCC_GRADE_NUM - 2]))
		return 200;

	// Both of them are available, return the calculated swappiness value
	// To use ZRAM as more as possible, swappiness always greater than 60
	return 100 + 100 * get_anon_pages() / swap_toplimit / 2;
}

/*
 * RTCC thread entry
 */
static int rtcc_thread(void * nothing)
{
	unsigned long nr_to_reclaim, nr_reclaimed, nr_swapped;
	int swappiness;
#if RTCC_DBG
	unsigned long dt;
	struct timeval tv1, tv2;
#endif

	set_freezable();

	for ( ; ; ) {
		try_to_freeze();
		if (kthread_should_stop())
			break;

		if (likely(atomic_read(&krtccd_running) == 1)) {
#if RTCC_DBG
			do_gettimeofday(&tv1);
#endif
			swap_toplimit = get_swapped_pages() + get_anon_pages() / 2;
			swap_toplimit = min(swap_toplimit, total_swap_pages);

			nr_to_reclaim = get_reclaim_count();
			swappiness = get_reclaim_swappiness();
			nr_swapped = 0;

			nr_reclaimed = rtcc_reclaim_pages(nr_to_reclaim, swappiness, &nr_swapped);
			nr_krtccd_swapped += nr_swapped;

			printk("%s: reclaimed %ld (swapped %ld) pages.\n", __func__, nr_reclaimed, nr_swapped);

			if (likely(rtcc_boost_mode == 0)) {
				if (get_rtcc_grade() <= 0) {
					// If free memory is enough, cancel reclaim
					atomic_set(&need_to_reclaim, 0);
				} else if (swappiness <= 1 && get_file_pages() <= rtcc_minfree[RTCC_GRADE_NUM-2]) {
					// If swap space is full and file pages is few, also cancel reclaim
					atomic_set(&need_to_reclaim, 0);
				}
			} else if (get_anon_pages() < swap_toplimit / 4) {
				rtcc_boost_mode = 0;
				atomic_set(&need_to_reclaim, 0);
				printk("%s: swapped %ldMB enough, exit boost mode.\n", __func__, get_swapped_pages()/256);
			} else if (time_after(jiffies, boost_end_jiffy)) {
				rtcc_boost_mode = 0;
				printk("%s: time out, swapped %ldMB, exit boost mode.\n", __func__, get_swapped_pages()/256);
			}

			atomic_set(&krtccd_running, 0);

#if RTCC_DBG
			do_gettimeofday(&tv2);
			dt = tv2.tv_sec*1000000 + tv2.tv_usec - tv1.tv_sec*1000000 - tv1.tv_usec;
			printk("%s: cost %ldms, %ldus one page, ", __func__, dt/1000, dt/nr_reclaimed);
			printk("expect=%ld, swappiness=%d \n", (nr_reclaimed*swappiness/200), swappiness);
#endif
		}

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}

/*
 * Dump some RTCC status
 */
static void rtcc_dump(void)
{
	printk("\nneed_to_reclaim = %d\n", atomic_read(&need_to_reclaim));
	printk("krtccd_running = %d\n", atomic_read(&krtccd_running));
}

/*
 * RTCC triggered by framework code
 */
static ssize_t rtcc_trigger_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int val;

	sscanf(buf, "%d", &val);

	if (likely(val == RTCC_MSG_ASYNC)) {
		pr_info("%s: wake up krtccd async\n", __func__);
		atomic_set(&need_to_reclaim, 1);
	} else if (val == RTCC_MSG_SYNC) {
		if (atomic_read(&krtccd_running) == 0) {
			pr_info("%s: wake up krtccd sync\n", __func__);
			atomic_set(&krtccd_running, 1);
			wake_up_process(krtccd);
			prev_jiffy = jiffies;
		}
	} else {
		rtcc_dump();
	}

	return count;
}

static CLASS_ATTR(rtcc_trigger, 0220, NULL, rtcc_trigger_store);
static struct class *rtcc_class;

/*
 * RTCC idle handler, called when CPU is idle
 */
static int rtcc_idle_handler(struct notifier_block *nb, unsigned long val, void *data)
{
	int cpu;
	struct pglist_data *pgdat;
	pgdat = NODE_DATA(numa_node_id());

	if (likely(atomic_read(&need_to_reclaim) == 0))
		return 0;

	// To prevent RTCC from running too frequently
	if (likely(time_before(jiffies, prev_jiffy + rtcc_reclaim_interval)))
		return 0;
	/*make sure kswapd is not running*/
	if (unlikely(!waitqueue_active(&pgdat->kswapd_wait)))
		return 0;

#ifdef CONFIG_SMP
	if ((num_online_cpus() == 1) || rtcc_boost_mode)
#else
	if (unlikely(idle_cpu(task_cpu(krtccd)) && this_cpu_loadx(3) == 0) || rtcc_boost_mode)
#endif
	{
			if (likely(atomic_read(&krtccd_running) == 0)) {
				pr_info("%s: wake up krtccd!!!!!!\n", __func__);
				atomic_set(&krtccd_running, 1);
				wake_up_process(krtccd);
				prev_jiffy = jiffies;
			}
	}

	return 0;
}

static struct notifier_block rtcc_idle_nb = {
	.notifier_call = rtcc_idle_handler,
};

#ifdef CONFIG_KSM_ANDROID
void enable_rtcc(void)
{
	idle_notifier_register(&rtcc_idle_nb);
}
#endif

static int __init rtcc_init(void)
{
	krtccd = kthread_run(rtcc_thread, NULL, "krtccd");
	if (IS_ERR(krtccd)) {
		/* Failure at boot is fatal */
		BUG_ON(system_state == SYSTEM_BOOTING);
	}

	set_user_nice(krtccd, 5);
	atomic_set(&need_to_reclaim, 1);
	atomic_set(&krtccd_running, 0);
	prev_jiffy = jiffies;
	boost_end_jiffy = jiffies + BOOSTMODE_TIMEOUT;

#ifndef CONFIG_KSM_ANDROID
	idle_notifier_register(&rtcc_idle_nb);
#endif

	rtcc_class = class_create(THIS_MODULE, "rtcc");
	if (IS_ERR(rtcc_class)) {
		pr_err("%s: couldn't create rtcc class.\n", __func__);
		return 0;
	}

	if (class_create_file(rtcc_class, &class_attr_rtcc_trigger) < 0) {
		pr_err("%s: couldn't create rtcc trigger file in sysfs.\n", __func__);
		class_destroy(rtcc_class);
	}

	return 0;
}

static void __exit rtcc_exit(void)
{
	idle_notifier_unregister(&rtcc_idle_nb);
	if (krtccd) {
		atomic_set(&need_to_reclaim, 0);
		kthread_stop(krtccd);
		krtccd = NULL;
	}

	if (rtcc_class) {
		class_remove_file(rtcc_class, &class_attr_rtcc_trigger);
		class_destroy(rtcc_class);
	}
}

module_param_array_named(grade, rtcc_grade, uint, &rtcc_grade_size, S_IRUGO | S_IWUSR);
module_param_array_named(minfree, rtcc_minfree, uint, &rtcc_grade_size, S_IRUGO | S_IWUSR);

module_init(rtcc_init);
module_exit(rtcc_exit);

MODULE_LICENSE("GPL");
