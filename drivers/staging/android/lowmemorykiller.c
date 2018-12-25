/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/sched/rt.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

/*These 3 values should be exactly the same as userspace daemon*/
#define LMK_NETLINK_PROTO NETLINK_USERSOCK
#define LMK_NETLINK_GROUP 21
#define LMK_NETLINK_MAX_NAME_LENGTH 100

#ifdef CONFIG_HIGHMEM
#define _ZONE ZONE_HIGHMEM
#else
#define _ZONE ZONE_NORMAL
#endif

#ifdef CONFIG_OOM_NOTIFIER
#define MULTIPLE_OOM_KILLER
#endif

#ifdef MULTIPLE_OOM_KILLER
#define OOM_DEPTH 3
#endif

struct sock *nl_sk = NULL;

static uint32_t lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
static int lmk_fast_run = 1;

#ifdef CONFIG_E_SHOW_MEM
/*
 * when some process is killed by lmk, show system memory information
 */
static char *lowmem_proc_name;
#endif

static unsigned long lowmem_deathpending_timeout;
static unsigned long oom_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

struct app_info{
	int uid;
	int pid;
	int adj;
};

static void send_killing_app_info_to_user(int uid,int pid,int adj){
	struct sk_buff* skb;
	struct nlmsghdr* nlh;
	struct app_info app;
	int msg_size=sizeof(struct app_info);
	int res=0;

	app.uid=uid;
	app.pid=pid;
	app.adj=adj;

	skb=nlmsg_new(msg_size,0);
	if(!skb){
		printk(KERN_ERR "allocation failure\n");
		return;
	}
	nlh=nlmsg_put(skb,0,1,NLMSG_DONE,msg_size,GFP_KERNEL);
	memcpy(nlmsg_data(nlh),&app,msg_size);
	res=nlmsg_multicast(nl_sk,skb,0,LMK_NETLINK_GROUP,GFP_KERNEL);
	if(res<0){
		printk(KERN_ERR "nlmsg_multicast error:%d\n",res);
	}
}
/*for testing*/
static void lmk_nl_recv_msg(struct sk_buff* skb){
	send_killing_app_info_to_user(100,100,100);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj);
static int lowmem_oom_score_adj_to_oom_adj(int oom_score_adj);
#define OOM_SCORE_ADJ_TO_OOM_ADJ(__SCORE_ADJ__)   lowmem_oom_score_adj_to_oom_adj(__SCORE_ADJ__)
#define OOM_ADJ_TO_OOM_SCORE_ADJ(__ADJ__)   lowmem_oom_adj_to_oom_score_adj(__ADJ__)
#else
#define OOM_SCORE_ADJ_TO_OOM_ADJ(__SCORE_ADJ__)  (__SCORE_ADJ__)
#define OOM_ADJ_TO_OOM_SCORE_ADJ(__ADJ__)    (__ADJ__)
#endif

#if defined(CONFIG_SEC_DEBUG_LMK_MEMINFO) || defined(CONFIG_E_SHOW_MEM)
static void dump_tasks_info(void)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("[ pid ]   uid  tgid total_vm      rss   swap cpu"
		" oom_score_adj name\n");
	for_each_process(p) {
		/* check unkillable tasks */
		if (is_global_init(p))
			continue;
		if (p->flags & PF_KTHREAD)
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			* This is a kthread or all of p's threads have already
			* detached their mm's.  There's no need to report
			* them; they can't be oom killed anyway.
			*/
			continue;
		}

		pr_info("[%5d] %5d %5d %8lu %8lu %6lu %3u         %5d %s\n",
		task->pid, task_uid(task), task->tgid,
		task->mm->total_vm, get_mm_rss(task->mm),
		get_mm_counter(task->mm, MM_SWAPENTS),
		task_cpu(task),
		task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
}
#endif

#ifdef CONFIG_ZRAM
extern ssize_t  zram_mem_usage(void);
extern ssize_t zram_mem_free_percent(void);
static uint lmk_lowmem_threshold_adj = 2;
module_param_named(lmk_lowmem_threshold_adj, lmk_lowmem_threshold_adj, uint, S_IRUGO | S_IWUSR);

static uint zone_wmark_ok_safe_gap = 256;
module_param_named(zone_wmark_ok_safe_gap, zone_wmark_ok_safe_gap, uint, S_IRUGO | S_IWUSR);
short cacl_zram_score_adj(void)
{
	struct sysinfo swap_info;
	ssize_t  swap_free_percent = 0;
	ssize_t zram_free_percent = 0;
	ssize_t  ret = 0;

	si_swapinfo(&swap_info);
	if(!swap_info.totalswap)
	{
		return  -1;
	}

	swap_free_percent =  swap_info.freeswap * 100/swap_info.totalswap;
	zram_free_percent =  zram_mem_free_percent();
	if(zram_free_percent < 0)
		return -1;

	ret = (swap_free_percent <  zram_free_percent) ?  swap_free_percent :  zram_free_percent;

	return OOM_ADJ_TO_OOM_SCORE_ADJ(ret*OOM_ADJUST_MAX/100);
}
#endif

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t = p;

	do {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	} while_each_thread(p, t);

	return 0;
}

static DEFINE_MUTEX(scan_mutex);

int can_use_cma_pages(gfp_t gfp_mask)
{
	int can_use = 0;
	int mtype = allocflags_to_migratetype(gfp_mask);
	int i = 0;
	int *mtype_fallbacks = get_migratetype_fallbacks(mtype);

	if (is_migrate_cma(mtype)) {
		can_use = 1;
	} else {
		for (i = 0;; i++) {
			int fallbacktype = mtype_fallbacks[i];

			if (is_migrate_cma(fallbacktype)) {
				can_use = 1;
				break;
			}

			if (fallbacktype == MIGRATE_RESERVE)
				break;
		}
	}
	return can_use;
}

void tune_lmk_zone_param(struct zonelist *zonelist, int classzone_idx,
					int *other_free, int *other_file,
					int use_cma_pages)
{
	struct zone *zone;
	struct zoneref *zoneref;
	int zone_idx;

	for_each_zone_zonelist(zone, zoneref, zonelist, MAX_NR_ZONES) {
		zone_idx = zonelist_zone_idx(zoneref);
		if (zone_idx == ZONE_MOVABLE) {
			if (!use_cma_pages)
				*other_free -=
				    zone_page_state(zone, NR_FREE_CMA_PAGES);
			continue;
		}

		if (zone_idx > classzone_idx) {
			if (other_free != NULL)
				*other_free -= zone_page_state(zone,
							       NR_FREE_PAGES);
			if (other_file != NULL)
				*other_file -= zone_page_state(zone,
							       NR_FILE_PAGES)
					      - zone_page_state(zone, NR_SHMEM);
		} else if (zone_idx < classzone_idx) {
			if (zone_watermark_ok(zone, 0, 0, classzone_idx, 0)) {
				if (!use_cma_pages) {
					*other_free -= min(
					  zone->lowmem_reserve[classzone_idx] +
					  zone_page_state(
					    zone, NR_FREE_CMA_PAGES),
					  zone_page_state(
					    zone, NR_FREE_PAGES));
				} else {
					*other_free -=
					  zone->lowmem_reserve[classzone_idx];
				}
			} else {
				*other_free -=
					   zone_page_state(zone, NR_FREE_PAGES);
			}
		}
	}
}

void tune_lmk_param(int *other_free, int *other_file, struct shrink_control *sc)
{
	gfp_t gfp_mask;
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx, classzone_idx;
	unsigned long balance_gap;
	int use_cma_pages;

	gfp_mask = sc->gfp_mask;
	zonelist = node_zonelist(0, gfp_mask);
	high_zoneidx = gfp_zone(gfp_mask);
	first_zones_zonelist(zonelist, high_zoneidx, NULL, &preferred_zone);
	classzone_idx = zone_idx(preferred_zone);
	use_cma_pages = can_use_cma_pages(gfp_mask);

	balance_gap = min(low_wmark_pages(preferred_zone),
			  (preferred_zone->present_pages +
			   KSWAPD_ZONE_BALANCE_GAP_RATIO-1) /
			   KSWAPD_ZONE_BALANCE_GAP_RATIO);

	if (likely(current_is_kswapd() && zone_watermark_ok(preferred_zone, 0,
			  high_wmark_pages(preferred_zone) + SWAP_CLUSTER_MAX +
			  balance_gap, 0, 0))) {
		if (lmk_fast_run)
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       other_file, use_cma_pages);
		else
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       NULL, use_cma_pages);

		if (zone_watermark_ok(preferred_zone, 0, 0, _ZONE, 0)) {
			if (!use_cma_pages) {
				*other_free -= min(
				  preferred_zone->lowmem_reserve[_ZONE]
				  + zone_page_state(
				    preferred_zone, NR_FREE_CMA_PAGES),
				  zone_page_state(
				    preferred_zone, NR_FREE_PAGES));
			} else {
				*other_free -=
				  preferred_zone->lowmem_reserve[_ZONE];
			}
		} else {
			*other_free -= zone_page_state(preferred_zone,
						      NR_FREE_PAGES);
		}

		lowmem_print(4, "lowmem_shrink of kswapd tunning for highmem "
			     "ofree %d, %d\n", *other_free, *other_file);
	} else {
		tune_lmk_zone_param(zonelist, classzone_idx, other_free,
			       other_file, use_cma_pages);

		if (!use_cma_pages) {
			*other_free -=
			  zone_page_state(preferred_zone, NR_FREE_CMA_PAGES);
		}

		lowmem_print(4, "lowmem_shrink tunning for others ofree %d, "
			     "%d\n", *other_free, *other_file);
	}
}

#ifdef CONFIG_E_SHOW_MEM
static int process_need_show_memory(char *cmdline)
{
	if (cmdline == NULL)
		return false;

	lowmem_print(4, "need show memory process name:%s\n", lowmem_proc_name);

	if (lowmem_proc_name && strlen(lowmem_proc_name) > 0)
		if (strstr(lowmem_proc_name, cmdline))
			return true;

	return false;
}
#endif
/*
  * It's reasonable to grant the dying task an even higher priority to
  * be sure it will be scheduled sooner and free the desired pmem.
  * It was suggested using SCHED_FIFO:1 (the lowest RT priority),
  * so that this task won't interfere with any running RT task.
  */
static void boost_dying_task_prio(struct task_struct *p)
{
         if (!rt_task(p)) {
                 struct sched_param param;
                 param.sched_priority = 1;
                 sched_setscheduler_nocheck(p, SCHED_FIFO, &param);
         }
}


typedef struct lmk_debug_info
{
	short min_score_adj;
	short zram_score_adj;
	ssize_t zram_free_percent;
	ssize_t zram_mem_usage;
}lmk_debug_info;

static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	lmk_debug_info  lmk_info = {0};
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int selected_process_uid;
	int selected_process_pid;
	int selected_process_adj;
	int rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;
	bool is_need_lmk_kill = true;
	unsigned long nr_to_scan = sc->nr_to_scan;
	struct sysinfo si;
#ifdef CONFIG_ZRAM
	short zram_score_adj = 0;
#endif

#ifdef CONFIG_SEC_DEBUG_LMK_MEMINFO
	static DEFINE_RATELIMIT_STATE(lmk_rs, DEFAULT_RATELIMIT_INTERVAL, 1);
#endif

#ifdef CONFIG_E_SHOW_MEM
	/* 600s */
	static DEFINE_RATELIMIT_STATE(lmk_mem_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12 * 10, 1);
	static DEFINE_RATELIMIT_STATE(lmk_meminfo_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12, 1);
#endif

	if (nr_to_scan > 0) {
		if (mutex_lock_interruptible(&scan_mutex) < 0)
			return 0;
	}

	other_free = global_page_state(NR_FREE_PAGES);
	if (global_page_state(NR_SHMEM) + total_swapcache_pages() <
		global_page_state(NR_FILE_PAGES))
		other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM) -
						total_swapcache_pages();
	else
		other_file = 0;

	tune_lmk_param(&other_free, &other_file, sc);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;

	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	if (nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %hd\n",
				nr_to_scan, sc->gfp_mask, other_free,
				other_file, min_score_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (nr_to_scan <= 0 || min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     nr_to_scan, sc->gfp_mask, rem);

		if (nr_to_scan > 0)
			mutex_unlock(&scan_mutex);

		return rem;
	}

#if 0
	zram_score_adj = cacl_zram_score_adj();
	if(zram_score_adj  < 0 )
	{
		zram_score_adj = min_score_adj;
	}
	else
	{
		lmk_info.zram_mem_usage = zram_mem_usage();
		lmk_info.zram_free_percent = zram_mem_free_percent();
	}

	lmk_info.min_score_adj = min_score_adj;
	lmk_info.zram_score_adj = zram_score_adj;

	if(min_score_adj < zram_score_adj)
	{
		gfp_t gfp_mask;
		struct zone *preferred_zone;
		struct zonelist *zonelist;
		enum zone_type high_zoneidx;
		gfp_mask = sc->gfp_mask;
		zonelist = node_zonelist(0, gfp_mask);
		high_zoneidx = gfp_zone(gfp_mask);
		first_zones_zonelist(zonelist, high_zoneidx, NULL, &preferred_zone);
		if (zram_score_adj <= OOM_ADJ_TO_OOM_SCORE_ADJ(lmk_lowmem_threshold_adj))
		{
			printk("%s:min:%d, zram:%d, threshold:%d\r\n", __func__, min_score_adj,zram_score_adj, OOM_ADJ_TO_OOM_SCORE_ADJ(lmk_lowmem_threshold_adj));
			if(!min_score_adj)
				is_need_lmk_kill = false;

			zram_score_adj = min_score_adj;
		}
		else if (!zone_watermark_ok_safe(preferred_zone, 0, min_wmark_pages(preferred_zone)  + zone_wmark_ok_safe_gap, 0, 0))
		{
			zram_score_adj =  (min_score_adj + zram_score_adj)/2;
		}
		else
		{
			lowmem_print(2, "ZRAM: return min_score_adj:%d, zram_score_adj:%d\r\n", min_score_adj, zram_score_adj);
			if (nr_to_scan > 0)
				mutex_unlock(&scan_mutex);
			return rem;
		}
	}
	lowmem_print(2, "ZRAM: min_score_adj:%d, zram_score_adj:%d\r\n", min_score_adj, zram_score_adj);
	min_score_adj = zram_score_adj;
#endif

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (test_task_flag(tsk, TIF_MEMDIE)) {
			if (time_before_eq(jiffies, lowmem_deathpending_timeout)) {
				rcu_read_unlock();
				/* give the system time to free up the memory */
				msleep_interruptible(20);
				mutex_unlock(&scan_mutex);
				return 0;
			}
			continue;
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
#ifdef CONFIG_ZRAM
		tasksize = get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS);
#else
		tasksize = get_mm_rss(p->mm);
#endif
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, OOM_SCORE_ADJ_TO_OOM_ADJ(oom_score_adj), tasksize);
	}
	if (selected && (selected_oom_score_adj || is_need_lmk_kill)) {
			si_swapinfo(&si);
			lowmem_print(1, "Killing '%s' (%d), adj %hd,\n" \
				"   to free %ldkB on behalf of '%s' (%d) because\n" \
				"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n" \
				"   Free memory is %ldkB above reserved\n"	\
				"   swaptotal is %ldkB, swapfree is %ldkB\n",
			     selected->comm, selected->pid,
			     OOM_SCORE_ADJ_TO_OOM_ADJ(selected_oom_score_adj),
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     other_file * (long)(PAGE_SIZE / 1024),
			     minfree * (long)(PAGE_SIZE / 1024),
			     OOM_SCORE_ADJ_TO_OOM_ADJ(min_score_adj),
			     other_free * (long)(PAGE_SIZE / 1024),
			     si.totalswap * (long)(PAGE_SIZE / 1024),
			     si.freeswap * (long)(PAGE_SIZE / 1024));

			selected_process_uid = selected->cred->uid;
			selected_process_pid = selected->pid;
			selected_process_adj = OOM_SCORE_ADJ_TO_OOM_ADJ(selected_oom_score_adj);

			lowmem_deathpending_timeout = jiffies + HZ;

			//Improve the priority of killed process can accelerate the process to die,
			//and the process memory would be released quickly
			boost_dying_task_prio(selected);

			send_sig(SIGKILL, selected, 0);
			set_tsk_thread_flag(selected, TIF_MEMDIE);
			rem -= selected_tasksize;
			rcu_read_unlock();
			send_killing_app_info_to_user(selected_process_uid,selected_process_pid,selected_process_adj);
#ifdef CONFIG_SEC_DEBUG_LMK_MEMINFO
		if (__ratelimit(&lmk_rs)) {
			dump_tasks_info();
		}
#endif
#ifdef CONFIG_E_SHOW_MEM
		if ((0 == min_score_adj)
			&& (__ratelimit(&lmk_meminfo_rs))) {
			enhanced_show_mem(E_SHOW_MEM_ALL);
		} else if (__ratelimit(&lmk_mem_rs)) {
			if ((!si.freeswap)
				|| ((si.totalswap / (si.freeswap + 1)) >= 10))
				enhanced_show_mem(E_SHOW_MEM_CLASSIC);
			else
				enhanced_show_mem(E_SHOW_MEM_BASIC);
		} else if (process_need_show_memory(selected->comm)) {
			enhanced_show_mem(E_SHOW_MEM_ALL);
		}
#endif
		/* give the system time to free up the memory */
		msleep_interruptible(20);
	} else
		rcu_read_unlock();

	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     nr_to_scan, sc->gfp_mask, rem);
	mutex_unlock(&scan_mutex);
	return rem;
}


/*
 * CONFIG_OOM_NOTIFIER
 *
 * The way to select victim by oom-killer provided by
 * linux kernel is totally different from android policy.
 * Hence, it makes more sense that we select the oom victim
 * as android does when LMK is invoked.
 *
*/
#ifdef CONFIG_OOM_NOTIFIER

static int android_oom_handler(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct task_struct *tsk;
#ifdef MULTIPLE_OOM_KILLER
	struct task_struct *selected[OOM_DEPTH] = {NULL,};
#else
	struct task_struct *selected = NULL;
#endif
	int rem = 0;
	int tasksize;
	int i;
	int min_score_adj = OOM_SCORE_ADJ_MAX + 1;
#ifdef MULTIPLE_OOM_KILLER
	int selected_tasksize[OOM_DEPTH] = {0,};
	int selected_oom_score_adj[OOM_DEPTH] = {OOM_ADJUST_MAX,};
	int all_selected_oom = 0;
	int max_selected_oom_idx = 0;
#else
	int selected_tasksize = 0;
	int selected_oom_score_adj;
#endif
#ifdef CONFIG_SEC_DEBUG_LMK_MEMINFO
	static DEFINE_RATELIMIT_STATE(oom_rs,
		DEFAULT_RATELIMIT_INTERVAL/5, 1);
#endif
#ifdef CONFIG_E_SHOW_MEM
	/* 600s */
	static DEFINE_RATELIMIT_STATE(oom_mem_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12 * 10, 1);
	/* 60s */
	static DEFINE_RATELIMIT_STATE(oom_meminfo_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12, 1);
#endif

	unsigned long *freed = data;

	lowmem_print(1, "enter android_oom_handler\n");

	/* show status */
	pr_warning("%s invoked Android-oom-killer: "
		" oom_score_adj=%d\n",
		current->comm, current->signal->oom_score_adj);
#ifdef CONFIG_SEC_DEBUG_LMK_MEMINFO
	dump_stack();
	show_mem(SHOW_MEM_FILTER_NODES);
	if (__ratelimit(&oom_rs))
		dump_tasks_info();
#endif

	min_score_adj = 0;
#ifdef MULTIPLE_OOM_KILLER
	for (i = 0; i < OOM_DEPTH; i++)
		selected_oom_score_adj[i] = min_score_adj;
#else
	selected_oom_score_adj = min_score_adj;
#endif

	read_lock(&tasklist_lock);
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;
#ifdef MULTIPLE_OOM_KILLER
		int is_exist_oom_task = 0;
#endif

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (test_task_flag(tsk, TIF_MEMDIE)) {
			if (time_before_eq(jiffies, oom_deathpending_timeout)) {
				read_unlock(&tasklist_lock);
				/* give the system time to free up the memory */
				msleep_interruptible(20);
				return 0;
			}
			continue;
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;

		lowmem_print(2, "oom: ------ %d (%s), adj %d, size %d\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
#ifdef MULTIPLE_OOM_KILLER
		if (all_selected_oom < OOM_DEPTH) {
			for (i = 0; i < OOM_DEPTH; i++) {
				if (!selected[i]) {
					is_exist_oom_task = 1;
					max_selected_oom_idx = i;
					break;
				}
			}
		} else if (selected_oom_score_adj[max_selected_oom_idx] < oom_score_adj ||
			(selected_oom_score_adj[max_selected_oom_idx] == oom_score_adj &&
			selected_tasksize[max_selected_oom_idx] < tasksize)) {
			is_exist_oom_task = 1;
		}

		if (is_exist_oom_task) {
			selected[max_selected_oom_idx] = p;
			selected_tasksize[max_selected_oom_idx] = tasksize;
			selected_oom_score_adj[max_selected_oom_idx] = oom_score_adj;

			if (all_selected_oom < OOM_DEPTH)
				all_selected_oom++;

			if (all_selected_oom == OOM_DEPTH) {
				for (i = 0; i < OOM_DEPTH; i++) {
					if (selected_oom_score_adj[i] < selected_oom_score_adj[max_selected_oom_idx])
						max_selected_oom_idx = i;
					else if (selected_oom_score_adj[i] == selected_oom_score_adj[max_selected_oom_idx] &&
						selected_tasksize[i] < selected_tasksize[max_selected_oom_idx])
						max_selected_oom_idx = i;
				}
			}

			lowmem_print(2, "oom: max_selected_oom_idx(%d) select %d (%s), adj %d, \
					size %d, to kill\n",
				max_selected_oom_idx, p->pid, p->comm, oom_score_adj, tasksize);
		}
#else
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "oom: select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
#endif
	}
	min_score_adj = 1000;
#ifdef MULTIPLE_OOM_KILLER
	for (i = 0; i < OOM_DEPTH; i++) {
		if (selected[i]) {
			lowmem_print(1, "oom: send sigkill to %d (%s), adj %d,\
				     size %d\n",
				     selected[i]->pid, selected[i]->comm,
				     selected_oom_score_adj[i],
				     selected_tasksize[i]);
			if (min_score_adj > selected_oom_score_adj[i])
				min_score_adj = selected_oom_score_adj[i];
			send_sig(SIGKILL, selected[i], 0);
			set_tsk_thread_flag(selected[i], TIF_MEMDIE);
			rem -= selected_tasksize[i];
			*freed += (unsigned long)selected_tasksize[i];
#ifdef OOM_COUNT_READ
			oom_count++;
#endif

		}
	}
	if(selected[0])
		oom_deathpending_timeout = jiffies + HZ;
#else
	if (selected) {
		lowmem_print(1, "oom: send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_score_adj, selected_tasksize);
		oom_deathpending_timeout = jiffies + HZ;
		min_score_adj = selected_oom_score_adj;
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		send_sig(SIGKILL, selected, 0);
		rem -= selected_tasksize;
		*freed += (unsigned long)selected_tasksize;
#ifdef OOM_COUNT_READ
		oom_count++;
#endif
	}
#endif
	read_unlock(&tasklist_lock);

#ifdef CONFIG_E_SHOW_MEM
	if (!min_score_adj && __ratelimit(&oom_meminfo_rs))
		enhanced_show_mem(E_SHOW_MEM_CLASSIC);
	else if (__ratelimit(&oom_mem_rs))
		enhanced_show_mem(E_SHOW_MEM_BASIC);
#endif

	/* give the system time to free up the memory */
	msleep_interruptible(20);

	lowmem_print(2, "oom: get memory %lu", *freed);
	return rem;
}

static struct notifier_block android_oom_notifier = {
	.notifier_call = android_oom_handler,
};
#endif /* CONFIG_OOM_NOTIFIER */

#ifdef CONFIG_E_SHOW_MEM
static int tasks_e_show_mem_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
	enum e_show_mem_type type = val;
	struct sysinfo i;

	si_swapinfo(&i);
	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("Enhanced Mem-info :TASK\n");
	printk("Detail:\n");
	if (E_SHOW_MEM_CLASSIC == type || E_SHOW_MEM_ALL == type)
		dump_tasks_info();
	printk("Total used:\n");
	printk("        anon: %lu kB\n", ((global_page_state(NR_ACTIVE_ANON)
		+ global_page_state(NR_INACTIVE_ANON)) << PAGE_SHIFT) / 1024);
	printk("        swaped: %lu kB\n", ((i.totalswap - i.freeswap)
		<< PAGE_SHIFT) / 1024);
	return 0;
}

static struct notifier_block tasks_e_show_mem_notifier = {
	.notifier_call = tasks_e_show_mem_handler,
};
#endif

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static struct netlink_kernel_cfg cfg  = {
	.input = lmk_nl_recv_msg,
};

static int __init lowmem_init(void)
{
	register_shrinker(&lowmem_shrinker);
#ifdef CONFIG_OOM_NOTIFIER
	register_oom_notifier(&android_oom_notifier);
#endif
#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&tasks_e_show_mem_notifier);
#endif
	printk("entering:%s\n",__FUNCTION__);
	nl_sk = netlink_kernel_create(&init_net,LMK_NETLINK_PROTO,&cfg);
	if(!nl_sk) {
		printk(KERN_ALERT "error createing nl socket.\n");
	}
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
#ifdef CONFIG_E_SHOW_MEM
	unregister_e_show_mem_notifier(&tasks_e_show_mem_notifier);
#endif
	netlink_kernel_release(nl_sk);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_oom_score_adj_to_oom_adj(int oom_score_adj)
{
	if (oom_score_adj == OOM_SCORE_ADJ_MAX)
		return OOM_ADJUST_MAX;
	else
		return  (oom_score_adj * (-OOM_DISABLE) + OOM_SCORE_ADJ_MAX - 1) /  OOM_SCORE_ADJ_MAX;
}

static uint32_t oom_score_to_oom_enable = 1;

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	if(oom_score_to_oom_enable)
	{
		int oom_adj[6] = {0};
		int t = 0;
		for(t = 0; t < 6; t++)
		{
			oom_adj[t] = lowmem_oom_score_adj_to_oom_adj(lowmem_adj[t]);
		}
		return sprintf(buffer, "%d,%d,%d,%d,%d,%d", oom_adj[0], oom_adj[1], oom_adj[2], oom_adj[3], oom_adj[4], oom_adj[5]);
	}

	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};


module_param_array_named(oom_score_adj, lowmem_adj, short, &lowmem_adj_size, S_IRUGO | S_IWUSR);

module_param_named(oom_score_to_oom_enable, oom_score_to_oom_enable, uint, S_IRUGO | S_IWUSR);

#endif

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
__module_param_call(MODULE_PARAM_PREFIX, adj,
		    &lowmem_adj_array_ops,
		    .arr = &__param_arr_adj,
		    S_IRUGO | S_IWUSR, -1);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(lmk_fast_run, lmk_fast_run, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_E_SHOW_MEM
module_param_named(proc_name, lowmem_proc_name, charp, S_IRUGO | S_IWUSR);
#endif

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

