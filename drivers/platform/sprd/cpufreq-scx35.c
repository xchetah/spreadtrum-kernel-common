/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>
//#include <asm/system.h>
#include <trace/events/power.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_misc.h>
#include <linux/of_platform.h>

#if defined(CONFIG_ARCH_SC8825)
#define MHz                     (1000000)
#define GR_MPLL_REFIN_2M        (2 * MHz)
#define GR_MPLL_REFIN_4M        (4 * MHz)
#define GR_MPLL_REFIN_13M       (13 * MHz)
#define GR_MPLL_REFIN_SHIFT     16
#define GR_MPLL_REFIN_MASK      (0x3)
#define GR_MPLL_N_MASK          (0x7ff)
#define GR_MPLL_MN		(REG_GLB_M_PLL_CTL0)
#define GR_GEN1			(REG_GLB_GEN1)
#endif

#define TURNOFF_MPLL	1

#define FREQ_TABLE_SIZE 	10
#define DVFS_BOOT_TIME	(30 * HZ)
#define SHARK_TDPLL_FREQUENCY	(768000)
#define TRANSITION_LATENCY	(50 * 1000) /* ns */

static DEFINE_MUTEX(freq_lock);
struct cpufreq_freqs global_freqs;
unsigned int percpu_target[CONFIG_NR_CPUS] = {0};
static unsigned long boot_done;
static unsigned int sprd_top_frequency; /* khz */
#if defined (CONFIG_ARCH_SCX20)
extern int sci_efuse_binning_result_get(u32 *p_binning_data);
u32 p_binning_data;
#endif

struct cpufreq_conf {
	struct clk 					*clk;
	struct clk 					*mpllclk;
	struct clk 					*tdpllclk;
	struct regulator 				*regulator;
	struct cpufreq_frequency_table			*freq_tbl;
	unsigned int					*vddarm_mv;
	unsigned int					max_axi_freq;
};

struct cpufreq_table_data {
	struct cpufreq_frequency_table 		freq_tbl[FREQ_TABLE_SIZE];
	unsigned int				vddarm_mv[FREQ_TABLE_SIZE];
};

struct cpufreq_conf *sprd_cpufreq_conf = NULL;
static struct mutex cpufreq_vddarm_lock;

#if defined(CONFIG_ARCH_SC8825)
static struct cpufreq_table_data sc8825_cpufreq_table_data = {
	.freq_tbl =	{
		{0, 1000000},
		{1, 500000},
		{2, CPUFREQ_TABLE_END}
	},
	.vddarm_mv = {
		0
	},
};

struct cpufreq_conf sc8825_cpufreq_conf = {
	.clk = NULL,
	.regulator = NULL,
	.freq_tbl = sc8825_cpufreq_table_data.freq_tbl,
	.vddarm_mv = sc8825_cpufreq_table_data.vddarm_mv,
};

static void set_mcu_clk_freq(u32 mcu_freq)
{
	u32 val, rate, arm_clk_div, gr_gen1;

	rate = mcu_freq / MHz;
	switch(1000 / rate)
	{
		case 1:
			arm_clk_div = 0;
			break;
		case 2:
			arm_clk_div = 1;
			break;
		default:
			panic("set_mcu_clk_freq fault\n");
			break;
	}
	pr_debug("%s --- before, AHB_ARM_CLK: %08x, rate = %d, div = %d\n",
		__func__, __raw_readl(REG_AHB_ARM_CLK), rate, arm_clk_div);

	gr_gen1 =  __raw_readl(GR_GEN1);
	gr_gen1 |= BIT(9);
	__raw_writel(gr_gen1, GR_GEN1);

	val = __raw_readl(REG_AHB_ARM_CLK);
	val &= 0xfffffff8;
	val |= arm_clk_div;
	__raw_writel(val, REG_AHB_ARM_CLK);

	gr_gen1 &= ~BIT(9);
	__raw_writel(gr_gen1, GR_GEN1);

	pr_debug("%s --- after, AHB_ARM_CLK: %08x, rate = %d, div = %d\n",
		__func__, __raw_readl(REG_AHB_ARM_CLK), rate, arm_clk_div);

	return;
}

static unsigned int get_mcu_clk_freq(void)
{
	u32 mpll_refin, mpll_n, mpll_cfg = 0, rate, val;

	mpll_cfg = __raw_readl(GR_MPLL_MN);

	mpll_refin = (mpll_cfg >> GR_MPLL_REFIN_SHIFT) & GR_MPLL_REFIN_MASK;
	switch(mpll_refin){
		case 0:
			mpll_refin = GR_MPLL_REFIN_2M;
			break;
		case 1:
		case 2:
			mpll_refin = GR_MPLL_REFIN_4M;
			break;
		case 3:
			mpll_refin = GR_MPLL_REFIN_13M;
			break;
		default:
			pr_err("%s mpll_refin: %d\n", __FUNCTION__, mpll_refin);
	}
	mpll_n = mpll_cfg & GR_MPLL_N_MASK;
	rate = mpll_refin * mpll_n;

	/*find div */
	val = __raw_readl(REG_AHB_ARM_CLK) & 0x7;
	val += 1;
	return rate / val;
}
#endif

static struct cpufreq_table_data sc8830_cpufreq_table_data_cs = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1000000},
		{2, SHARK_TDPLL_FREQUENCY},
		{3, 600000},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1300000,
		1200000,
		1150000,
		1100000,
		1000000,
	},
};

/*
for 7715 test
*/
static struct cpufreq_table_data sc7715_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1000000},
		{1, SHARK_TDPLL_FREQUENCY},
		{2, 600000},
		{3, SHARK_TDPLL_FREQUENCY/2},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1150000,
		1100000,
		1100000,
		1000000,
	},
};


static struct cpufreq_table_data sc8830_cpufreq_table_data_es = {
	.freq_tbl = {
		{0, 1000000},
		{1, SHARK_TDPLL_FREQUENCY},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1250000,
		1200000,
		1000000,
	},
};

static struct cpufreq_table_data sc8830t_cpufreq_table_data_es = {
        .freq_tbl = {
                {0, 1300000},
                {1, 1200000},
                {2, 1000000},
                {3, SHARK_TDPLL_FREQUENCY},
                {4, CPUFREQ_TABLE_END},
        },
        .vddarm_mv = {
                1060000,
                1030000,
                960000,
                900000,
                900000,
        },
};

static struct cpufreq_table_data sc8830t_cpufreq_table_data_es_1300 = {
        .freq_tbl = {
                {0, 1300000},
                {1, 1200000},
                {2, 1000000},
                {3, SHARK_TDPLL_FREQUENCY},
                {4, CPUFREQ_TABLE_END},
        },
        .vddarm_mv = {
                1060000,
                1030000,
                960000,
                900000,
                900000,
        },
};

static struct cpufreq_table_data sc7730_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1300000},
		{1, 1200000},
		{2, 1000000},
		{3, SHARK_TDPLL_FREQUENCY},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1000000,
		970000,
		930000,
		930000,
		930000,
	},
};

#if defined (CONFIG_MACH_SP9830I_2H11_4M) || defined (CONFIG_MACH_SP9832A_2H11_4M)
static struct cpufreq_table_data sc9832_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1500000},
		{1, 1350000},
		{2, 900000},
		{3, 768000},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1100000,
		1000000,
		900000,
		900000,
		900000,
	},
};
#elif defined (CONFIG_MACH_SP9830I_J3LTE) || defined (CONFIG_MACH_SP9830I_J2LTE) 
static struct cpufreq_table_data sc9832_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1500000},
		{1, 1350000},
		{2, 1200000},
		{3, 900000},
		{4, 768000},
		{5, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1100000,
		1000000,
		1000000,
		900000,
		900000,
		900000,
	},
};
#else
static struct cpufreq_table_data sc9832_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1300000},
		{1, 900000},
		{2, 768000},
		{3, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1000000,
		900000,
		900000,
		900000,
	},
};
#endif
static struct cpufreq_table_data sc9832_cpufreq_table_data_sharklc = {
	.freq_tbl = {
		{0, 1300000},
		{1, 900000},
		{2, 768000},
		{3, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1120000,
		1050000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc9630_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1500000},
		{1, 1350000},
		{2, 900000},
		{3, 768000},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1100000,
		1000000,
		900000,
		900000,
		900000,
	},
};

static struct cpufreq_table_data sc9630_1500m_cpufreq_table_data_new = {
	.freq_tbl = {
		{0, 1500000},
		{1, 1350000},
		{2, 900000},
		{3, 768000},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1150000,
		1050000,
		900000,
		900000,
		900000,
	},
};

static struct cpufreq_table_data sc7720_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
#if TURNOFF_MPLL
                {1, 768000},
#else
		{1, 813000},
#endif
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1050000,
		1050000,
	},
};

#if defined CONFIG_MACH_PIKEB_J1MINI_3G
static struct cpufreq_table_data sc7720_bin1_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 768000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1100000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin2_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 768000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1150000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin3_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 768000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin4_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 768000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1250000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_binx_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 768000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1050000,
		1050000,
	},
};
#else
static struct cpufreq_table_data sc7720_bin1_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1100000},
		{2, 1000000},
		{3, 900000},
		{4, 813000},
		{5, 768000},
		{6, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1100000,
		1050000,
		1050000,
		1050000,
		1050000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin2_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1100000},
		{2, 1000000},
		{3, 900000},
		{4, 813000},
		{5, 768000},
		{6, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1150000,
		1100000,
		1050000,
		1050000,
		1050000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin3_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1100000},
		{2, 1000000},
		{3, 900000},
		{4, 813000},
		{5, 768000},
		{6, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1150000,
		1100000,
		1050000,
		1050000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_bin4_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1100000},
		{2, 1000000},
		{3, 900000},
		{4, 813000},
		{5, 768000},
		{6, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1250000,
		1200000,
		1150000,
		1100000,
		1050000,
		1050000,
		1050000,
	},
};
static struct cpufreq_table_data sc7720_binx_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1100000},
		{2, 1000000},
		{3, 900000},
		{4, 813000},
		{5, 768000},
		{6, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1150000,
		1100000,
		1050000,
		1050000,
		1050000,
		1050000,
	},
};
#endif
static struct cpufreq_table_data sc9631l64_cpufreq_table_data_es = {
	.freq_tbl = {
		{0, 1300000},
		{1, 1000000},
		{2, SHARK_TDPLL_FREQUENCY},
		{3, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1000000,
		900000,
		900000,
		900000,
	},
};
static struct cpufreq_table_data sc9820_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1250000},
		{1, 800000},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1200000,
		1050000,
		1050000,
	},
};
struct cpufreq_conf sc8830_cpufreq_conf = {
	.clk = NULL,
	.mpllclk = NULL,
	.tdpllclk = NULL,
	.regulator = NULL,
	.freq_tbl = NULL,
	.vddarm_mv = NULL,
};

int cpufreq_table_thermal_update(unsigned int freq, unsigned int voltage)
{
	struct cpufreq_frequency_table *freq_tbl;
	unsigned int *vddarm;
	int i;

	if (NULL == sprd_cpufreq_conf)
		return -1;
	freq_tbl = sprd_cpufreq_conf->freq_tbl;
	vddarm = sprd_cpufreq_conf->vddarm_mv;
	if (NULL == freq_tbl && NULL == vddarm)
		return -1;

	for (i = 0; freq_tbl[i].frequency != CPUFREQ_TABLE_END; ++i) {
		if (freq_tbl[i].frequency == freq)
			goto done;
	}
	pr_err(KERN_ERR "%s cpufreq %dMHz isn't find!\n", __func__, freq);
	return -1;
done:
	printk(KERN_ERR "%s: %dMHz voltage is %duV\n",
		__func__, freq, voltage);
	if (vddarm[i] == voltage)
		return 0;

	mutex_lock(&cpufreq_vddarm_lock);
	vddarm[i] = voltage;
	mutex_unlock(&cpufreq_vddarm_lock);

	return 0;
}

static unsigned int sprd_raw_get_cpufreq(void)
{
#if defined(CONFIG_ARCH_SCX35)
	return clk_get_rate(sprd_cpufreq_conf->clk) / 1000;
#elif defined(CONFIG_ARCH_SC8825)
	return get_mcu_clk_freq() / 1000;
#endif
}

unsigned int last_freq = 0;
static void dump_axi_cpu(unsigned int freq)
{
	u32 div, axi_freq;
#if !defined(CONFIG_ARCH_WHALE)
#ifndef CONFIG_ARCH_SCX35L
	div = sci_glb_read(REG_AP_AHB_CA7_CKG_CFG, -1UL);
	div &= (0x7<<8);
	div >>= 8;
	axi_freq = freq / (div + 1);
#elif defined CONFIG_ARCH_SCX35LT8
	div = sci_glb_read(REG_AP_AHB_CA7_CKG_DIV_CFG, -1UL);
	div &= (0x7<<8);
	div >>= 8;
	axi_freq = freq / (div + 1);
#else
	if (soc_is_scx9630_v0() || soc_is_scx9830i_v0() || soc_is_scx9832a_v0()) { /* for sharkL */
		div = sci_glb_read(REG_AP_AHB_CA7_CKG_DIV_CFG, -1UL);
		div &= (0x7<<8);
		div >>= 8;
		axi_freq = freq / (div + 1);
	}
#endif
	printk("%s(%d): cpu_freq %d, div %d, axi_freq %d\n", __func__, __LINE__,
		freq, div, axi_freq);
#endif
}

static void dump_clk_setting(unsigned int freq)
{
	u32 real_freq;

	if (freq == SHARK_TDPLL_FREQUENCY / 2
		|| freq == SHARK_TDPLL_FREQUENCY) {
		real_freq = clk_get_rate(sprd_cpufreq_conf->clk) / 1000;
		if (clk_get_parent(sprd_cpufreq_conf->clk) != sprd_cpufreq_conf->tdpllclk)
			pr_err("%s(%d) err: clk parent is not tdpll\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: clk parent is tdpll\n", __func__, __LINE__);
		if (real_freq != SHARK_TDPLL_FREQUENCY / 2 && real_freq != SHARK_TDPLL_FREQUENCY)
			pr_err("%s(%d) err: real clk %d is not right, TDPLL %d\n",
				__func__, __LINE__, real_freq, SHARK_TDPLL_FREQUENCY);
		else
			printk("%s(%d) ok: cpu freq is %d\n", __func__, __LINE__, real_freq);
	} else {
		if (clk_get_parent(sprd_cpufreq_conf->clk) != sprd_cpufreq_conf->mpllclk)
			pr_err("%s(%d) err: clk parent is not mpll\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: cpu freq parent is mpll\n", __func__, __LINE__);
		if (clk_get_rate(sprd_cpufreq_conf->clk) != clk_get_rate(sprd_cpufreq_conf->mpllclk))
			pr_err("%s(%d) err: cpu clk != mpll clk\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: cpu clk rate is equal to mpll clk rate\n", __func__, __LINE__);
	}
}

static inline int get_axi_div(unsigned int freq)
{
	if (freq % sprd_cpufreq_conf->max_axi_freq)
		return (freq / sprd_cpufreq_conf->max_axi_freq) + 1;
	else
		return freq / sprd_cpufreq_conf->max_axi_freq;
}

static void cpufreq_adjust_axi_clk(unsigned int freq)
{
	struct clk *clk_axi = NULL;
	int div = 0, div_old = 0;
	int need_adjust = 0;
	char clk_name[50] = {0};

	if (last_freq == 0) {
		last_freq = freq;
		need_adjust = 1;
	}

#ifndef CONFIG_ARCH_SCX35L
	strcpy(clk_name, "clk_ca7_axi");
#else
	if (soc_is_scx9630_v0() || soc_is_scx9830i_v0() || soc_is_scx9832a_v0()) // for sharkL
		strcpy(clk_name, "clk_ca7_axi");
	// for T8, in cpufreq-dt-sprd.c ?
#endif
	if (!clk_name[0] || !sprd_cpufreq_conf->max_axi_freq) {
		last_freq = freq;
		return;
	}

	div = get_axi_div(freq);
	div_old = get_axi_div(last_freq);
	if (!need_adjust && (div != div_old))
		need_adjust = 1;

	if (!need_adjust) {
		last_freq = freq;
		return;
	}
	last_freq = freq;

	clk_axi = clk_get_sys(NULL, clk_name);
	if (IS_ERR_OR_NULL(clk_axi)) {
		pr_err("%s(%d) err: cannot find clock %s\n", __func__, __LINE__, clk_name);
		return;
	}

	if (clk_set_rate(clk_axi, freq * 1000 / div))
		pr_err("%s(%d) err: clk_set_rate failed\n", __func__, __LINE__);

	clk_put(clk_axi);
	//dump_axi_cpu(freq);
}

static void cpufreq_set_clock(unsigned int freq)
{
	int ret;

	ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk);
	if (ret)
		pr_err("%s(%d) err: failed to set cpu parent to tdpll\n", __func__, __LINE__);

	if (freq == SHARK_TDPLL_FREQUENCY / 2
		|| freq == SHARK_TDPLL_FREQUENCY) {
		if (clk_set_rate(sprd_cpufreq_conf->clk, freq * 1000))
			pr_err("%s(%d) err: failed to set mpll rate\n", __func__, __LINE__);
	} else {
		ret = clk_set_rate(sprd_cpufreq_conf->mpllclk, freq * 1000);
		if (ret)
			pr_err("%s(%d) err: failed to set mpll rate\n", __func__, __LINE__);

		ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk);
		if (ret)
			pr_err("%s(%d) err: failed to set cpu parent to mpll\n", __func__, __LINE__);
#if 0 // for debug
		if (!(sci_glb_read(REG_PMU_APB_MPLL_REL_CFG, -1) & BIT_MPLL_AP_SEL))
			pr_err("%s(%d) err: MPLL_AP_SEL bit is not set\n", __func__, __LINE__);
#endif
		ret = clk_set_rate(sprd_cpufreq_conf->clk, freq * 1000);
		if (ret)
			pr_err("%s(%d) err: failed to set mpll rate\n", __func__, __LINE__);
	}

	//dump_clk_setting(freq);
	cpufreq_adjust_axi_clk(freq);
}

static void sprd_raw_set_cpufreq(int cpu, struct cpufreq_freqs *freq, int index)
{
#if defined(CONFIG_ARCH_SCX35)
	int ret;

#define CPUFREQ_SET_VOLTAGE() \
	do { \
		mutex_lock(&cpufreq_vddarm_lock);       \
	    ret = regulator_set_voltage(sprd_cpufreq_conf->regulator, \
			sprd_cpufreq_conf->vddarm_mv[index], \
			sprd_cpufreq_conf->vddarm_mv[index]); \
			mutex_unlock(&cpufreq_vddarm_lock);	\
		if (ret) \
			pr_err("Failed to set vdd to %d mv\n", \
				sprd_cpufreq_conf->vddarm_mv[index]); \
	} while (0)
#define CPUFREQ_SET_CLOCK() \
	do { \
		if (freq->new == SHARK_TDPLL_FREQUENCY) { \
			ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk); \
			if (ret) \
				pr_err("Failed to set cpu parent to tdpll\n"); \
		} else { \
			if (clk_get_parent(sprd_cpufreq_conf->clk) != sprd_cpufreq_conf->tdpllclk) { \
				ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk); \
				if (ret) \
					pr_err("Failed to set cpu parent to tdpll\n"); \
			} \
			ret = clk_set_rate(sprd_cpufreq_conf->mpllclk, (freq->new * 1000)); \
			if (ret) \
				pr_err("Failed to set mpll rate\n"); \
			ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk); \
			if (ret) \
				pr_err("Failed to set cpu parent to mpll\n"); \
		} \
	} while (0)
	trace_cpu_frequency(freq->new, cpu);

	if (freq->new >= sprd_raw_get_cpufreq()) {
		CPUFREQ_SET_VOLTAGE();
		cpufreq_set_clock(freq->new);
	} else {
		cpufreq_set_clock(freq->new);
		CPUFREQ_SET_VOLTAGE();
	}

	pr_info("%u --> %u, real=%u, index=%d\n",
		freq->old, freq->new, sprd_raw_get_cpufreq(), index);

#undef CPUFREQ_SET_VOLTAGE
#undef CPUFREQ_SET_CLOCK

#elif defined(CONFIG_ARCH_SC8825)
	set_mcu_clk_freq(freq->new * 1000);
#endif
	return;
}

static void sprd_real_set_cpufreq(struct cpufreq_policy *policy, unsigned int new_speed, int index)
{
	mutex_lock(&freq_lock);

	if (global_freqs.old == new_speed) {
		pr_debug("do nothing for cpu%u, new=old=%u\n",
				policy->cpu, new_speed);
		mutex_unlock(&freq_lock);
		return;
	}
	pr_info("--xing-- set %u khz for cpu%u\n",
		new_speed, policy->cpu);
	global_freqs.cpu = policy->cpu;
	global_freqs.new = new_speed;

	cpufreq_notify_transition(policy, &global_freqs, CPUFREQ_PRECHANGE);

	sprd_raw_set_cpufreq(policy->cpu, &global_freqs, index);

	cpufreq_notify_transition(policy, &global_freqs, CPUFREQ_POSTCHANGE);

	global_freqs.old = global_freqs.new;

	mutex_unlock(&freq_lock);
	return;
}

static void sprd_find_real_index(unsigned int new_speed, int *index)
{
	int i;
	struct cpufreq_frequency_table *pfreq = sprd_cpufreq_conf->freq_tbl;

	*index = pfreq[0].index;
	for (i = 0; (pfreq[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (new_speed == pfreq[i].frequency) {
			*index = pfreq[i].index;
			break;
		}
	}
	return;
}

static int sprd_update_cpu_speed(struct cpufreq_policy *policy,
	unsigned int target_speed, int index)
{
	int i, real_index = 0;
	unsigned int new_speed = 0;

	/*
	 * CONFIG_NR_CPUS cores are always in the same voltage, at the same
	 * frequency. But, cpu load is calculated individual in each cores,
	 * So we remeber the original target frequency and voltage of core0,
	 * and use the higher one
	 */

	for_each_online_cpu(i) {
		new_speed = max(new_speed, percpu_target[i]);
	}

	if (new_speed > sprd_top_frequency)
		new_speed = sprd_top_frequency;

	if (new_speed != sprd_cpufreq_conf->freq_tbl[index].frequency)
		sprd_find_real_index(new_speed, &real_index);
	else
		real_index = index;
	sprd_real_set_cpufreq(policy, new_speed, real_index);
	return 0;
}

static int sprd_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu > CONFIG_NR_CPUS) {
		pr_err("%s no such cpu id %d\n", __func__, policy->cpu);
		return -EINVAL;
	}

	return cpufreq_frequency_table_verify(policy, sprd_cpufreq_conf->freq_tbl);
}

unsigned int cpufreq_min_limit = ULONG_MAX;
unsigned int cpufreq_max_limit = 0;
unsigned int dvfs_score_select = 5;
unsigned int dvfs_unplug_select = 2;
unsigned int dvfs_plug_select = 0;
unsigned int dvfs_score_hi[4] = {0};
unsigned int dvfs_score_mid[4] = {0};
unsigned int dvfs_score_critical[4] = {0};
extern unsigned int percpu_load[4];
extern unsigned int cur_window_size[4];
extern unsigned int cur_window_index[4];
extern unsigned int ga_percpu_total_load[4][8];

static DEFINE_SPINLOCK(cpufreq_state_lock);

static int sprd_cpufreq_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int ret = -EFAULT;
	int index;
	unsigned int new_speed;
	struct cpufreq_frequency_table *table;
	int max_freq = cpufreq_max_limit;
	int min_freq = cpufreq_min_limit;
	int cur_freq = 0;
	unsigned long irq_flags;

	/* delay 30s to enable dvfs&dynamic-hotplug,
         * except requirment from termal-cooling device
         */
	if(time_before(jiffies, boot_done)){
		return 0;
	}

	if((target_freq < min_freq) || (target_freq > max_freq))
	{
		pr_err("invalid target_freq: %d min_freq %d max_freq %d\n", target_freq,min_freq,max_freq);
		return -EINVAL;
	}
	table = cpufreq_frequency_get_table(policy->cpu);

	if (cpufreq_frequency_table_target(policy, table,
					target_freq, relation, &index)) {
		pr_err("invalid target_freq: %d\n", target_freq);
		return -EINVAL;
	}

	pr_debug("CPU_%d target %d relation %d (%d-%d) selected %d\n",
			policy->cpu, target_freq, relation,
			policy->min, policy->max, table[index].frequency);

	new_speed = table[index].frequency;

	percpu_target[policy->cpu] = new_speed;
	pr_debug("%s cpu:%d new_speed:%u on cpu%d\n",
			__func__, policy->cpu, new_speed, smp_processor_id());

	ret = sprd_update_cpu_speed(policy, new_speed, index);

	return ret;

}

static unsigned int sprd_cpufreq_getspeed(unsigned int cpu)
{
	if (cpu > CONFIG_NR_CPUS) {
		pr_err("%s no such cpu id %d\n", __func__, cpu);
		return -EINVAL;
	}

	return sprd_raw_get_cpufreq();
}

static void sprd_set_cpureq_limit(void)
{
	int i;
	struct cpufreq_frequency_table *tmp = sprd_cpufreq_conf->freq_tbl;
	for (i = 0; (tmp[i].frequency != CPUFREQ_TABLE_END); i++) {
		cpufreq_min_limit = min(tmp[i].frequency, cpufreq_min_limit);
		cpufreq_max_limit = max(tmp[i].frequency, cpufreq_max_limit);
	}
	pr_info("--xing-- %s max=%u min=%u\n", __func__, cpufreq_max_limit, cpufreq_min_limit);
}

#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
#define AON_APB_CHIP_ID		REG_AON_APB_CHIP_ID0
#else
#define AON_APB_CHIP_ID		REG_AON_APB_CHIP_ID
#endif

#if defined(CONFIG_ARCH_SCX35L)
extern int  sci_efuse_Dhryst_binning_get(int *cal);
#define CONFIG_SEC_DVFS
#endif

static int sprd_freq_table_init(void)
{
	/* we init freq table here depends on which chip being used */
	if (soc_is_scx35_v0()) {
		pr_info("%s es_chip\n", __func__);
		sprd_cpufreq_conf->freq_tbl =
			sc8830_cpufreq_table_data_es.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc8830_cpufreq_table_data_es.vddarm_mv;
	} else if (soc_is_scx35_v1()) {
		pr_info("%s cs_chip\n", __func__);
		sprd_cpufreq_conf->freq_tbl =
			sc8830_cpufreq_table_data_cs.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc8830_cpufreq_table_data_cs.vddarm_mv;
	} else if (soc_is_sc7715()) {
		sprd_cpufreq_conf->freq_tbl =
			sc7715_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc7715_cpufreq_table_data.vddarm_mv;
	} else if (soc_is_scx35g_v0() || soc_is_scx30g2_v0()) {
		sprd_cpufreq_conf->freq_tbl =
			sc8830t_cpufreq_table_data_es.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc8830t_cpufreq_table_data_es.vddarm_mv;
		sprd_cpufreq_conf->max_axi_freq = 500000;
#if defined(CONFIG_ARCH_SCX35L)
	} else if (soc_is_scx9630_v0()) {
		sprd_cpufreq_conf->max_axi_freq = 675000;
#ifdef CONFIG_SEC_DVFS
		int val;
		sci_efuse_Dhryst_binning_get(&val);
		if ((val < 28) && (val >= 22)) {
			sprd_cpufreq_conf->freq_tbl = sc9630_1500m_cpufreq_table_data_new.freq_tbl;
			sprd_cpufreq_conf->vddarm_mv = sc9630_1500m_cpufreq_table_data_new.vddarm_mv;
			pr_info("scx9630 binning value %d, use 1500MHz-1.05v\n", val);
		} else {
			sprd_cpufreq_conf->freq_tbl = sc9630_cpufreq_table_data.freq_tbl;
			sprd_cpufreq_conf->vddarm_mv = sc9630_cpufreq_table_data.vddarm_mv;
			pr_info("scx9630 binning value %d, use 1500MHz-1.0v\n", val);
		}
#else
		sprd_cpufreq_conf->freq_tbl =
			sc9630_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc9630_cpufreq_table_data.vddarm_mv;
#endif
#endif
	} else if (soc_is_scx9830i_v0()) {
		sprd_cpufreq_conf->max_axi_freq = 675000;
		sprd_cpufreq_conf->freq_tbl =
			sc9832_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc9832_cpufreq_table_data.vddarm_mv;
	}
	else if (soc_is_scx9832a_v0()) {
		sprd_cpufreq_conf->max_axi_freq = 675000;
		sprd_cpufreq_conf->freq_tbl =
			sc9832_cpufreq_table_data_sharklc.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc9832_cpufreq_table_data_sharklc.vddarm_mv;
	}
	else if (soc_is_scx9820_v0()) {
		sprd_cpufreq_conf->freq_tbl =
			sc9820_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc9820_cpufreq_table_data.vddarm_mv;
	} else if (soc_is_scx7720_v0()) {
#if defined (CONFIG_ARCH_SCX20)
		if (!(sci_efuse_binning_result_get(&p_binning_data))) {
			pr_info("Select freq table according to p_binning_data=%x\n", p_binning_data);
			if (1 == p_binning_data) {
				sprd_cpufreq_conf->freq_tbl =
					sc7720_bin1_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_bin1_cpufreq_table_data.vddarm_mv;
			} else if (2 == p_binning_data) {
				sprd_cpufreq_conf->freq_tbl =
					sc7720_bin2_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_bin2_cpufreq_table_data.vddarm_mv;
			} else if (3 == p_binning_data) {
				sprd_cpufreq_conf->freq_tbl =
					sc7720_bin3_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_bin3_cpufreq_table_data.vddarm_mv;
			} else if (4 == p_binning_data) {
				sprd_cpufreq_conf->freq_tbl =
					sc7720_bin4_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_bin4_cpufreq_table_data.vddarm_mv;
			} else {
				sprd_cpufreq_conf->freq_tbl =
					sc7720_binx_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_binx_cpufreq_table_data.vddarm_mv;
			}
		} else {
				pr_info("Use default freq table ,p_binning_data=%x\n", p_binning_data);
				sprd_cpufreq_conf->freq_tbl =
					sc7720_binx_cpufreq_table_data.freq_tbl;
				sprd_cpufreq_conf->vddarm_mv =
					sc7720_binx_cpufreq_table_data.vddarm_mv;
		}
#endif

#if defined (CONFIG_ARCH_SCX35LT8)	//TODO
	} else if(__raw_readl(REG_AON_APB_CHIP_ID0) == 0x96310000){
                 sprd_cpufreq_conf->freq_tbl =
				 	sc9631l64_cpufreq_table_data_es.freq_tbl;
                 sprd_cpufreq_conf->vddarm_mv =
				 	sc9631l64_cpufreq_table_data_es.vddarm_mv;
#else
	} else if (__raw_readl(AON_APB_CHIP_ID) == 0x96310000) {
		sprd_cpufreq_conf->freq_tbl =
			sc9631l64_cpufreq_table_data_es.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc9631l64_cpufreq_table_data_es.vddarm_mv;
#endif
	} else if ((__raw_readl(AON_APB_CHIP_ID) == 0x8730d000) || (__raw_readl(AON_APB_CHIP_ID) ==
0x8730d001)) {/*for Tshark3*/
		sprd_cpufreq_conf->freq_tbl =
			sc7730_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv =
			sc7730_cpufreq_table_data.vddarm_mv;
	} else {
#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
	        pr_info("D-die chip id = 0x%08X\n", __raw_readl(REG_AON_APB_CHIP_ID0));
#endif
		pr_err("%s error chip id\n", __func__);
		return -EINVAL;
	}
	pr_info("sprd_freq_table_init \n");
	sprd_set_cpureq_limit();
	return 0;
}

static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret;
	unsigned long new_speed = 0;
	int index;
	cpufreq_frequency_table_cpuinfo(policy, sprd_cpufreq_conf->freq_tbl);
	policy->cur = sprd_raw_get_cpufreq(); /* current cpu frequency: KHz*/
	 /*
	  * transition_latency 5us is enough now
	  * but sampling too often, unbalance and irregular on each online cpu
	  * so we set 500us here.
	  */
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;

	cpufreq_frequency_table_get_attr(sprd_cpufreq_conf->freq_tbl, policy->cpu);

	new_speed = policy->cur;
	if(cpufreq_frequency_table_target(policy, sprd_cpufreq_conf->freq_tbl,
				policy->cur, CPUFREQ_RELATION_L, &index)){
		pr_err("invalid freq in sprd_cpufreq_init: %d\n", policy->cur);
	}else{
		new_speed = sprd_cpufreq_conf->freq_tbl[index].frequency;
	}
	percpu_target[policy->cpu] = new_speed;
	if(new_speed != policy->cur){
		pr_info("sprd_cpufreq_init:CPU_%d change frequency from %d to %d\n",policy->cpu, policy->cur, new_speed);
		sprd_update_cpu_speed(policy, new_speed, index);
	}

	ret = cpufreq_frequency_table_cpuinfo(policy, sprd_cpufreq_conf->freq_tbl);
	if (ret != 0)
		pr_err("%s Failed to config freq table: %d\n", __func__, ret);


	pr_info("%s policy->cpu=%d, policy->cur=%u, ret=%d\n",
		__func__, policy->cpu, policy->cur, ret);

       cpumask_setall(policy->cpus);

	return ret;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct freq_attr *sprd_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver sprd_cpufreq_driver = {
	.verify		= sprd_cpufreq_verify_speed,
	.target		= sprd_cpufreq_target,
	.get		= sprd_cpufreq_getspeed,
	.init		= sprd_cpufreq_init,
	.exit		= sprd_cpufreq_exit,
	.name		= "sprd",
	.attr		= sprd_cpufreq_attr,
#if defined(CONFIG_ARCH_SCX35)
	.flags		= CPUFREQ_SHARED
#endif
};

static ssize_t cpufreq_min_limit_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,&cpufreq_min_limit,sizeof(int));
	return sizeof(int);
}

static ssize_t cpufreq_max_limit_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,&cpufreq_max_limit,sizeof(int));
	return sizeof(int);
}

static ssize_t cpufreq_min_limit_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",cpufreq_min_limit);
	return strlen(buf) + 1;
}

static ssize_t cpufreq_max_limit_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",cpufreq_max_limit);
	return strlen(buf) + 1;
}

static ssize_t cpufreq_max_axi_freq_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf, 10, "%d\n", sprd_cpufreq_conf->max_axi_freq);
	return strlen(buf) + 1;
}

static ssize_t cpufreq_max_axi_freq_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int value;
	int temp,max_freq = 0;
	int i,j;

	strict_strtoul(buf, 16, (long unsigned int *)&value);
	i = 0;
	do{
		temp = value & 0xf;
		for(j = 0; j < i; j++)
			temp = temp * 10;
		max_freq += temp;
		value = value >> 4;
		i++;
	} while(value);

	sprd_cpufreq_conf->max_axi_freq = max_freq;
	return count;
}

static ssize_t cpufreq_min_limit_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	spin_lock_irqsave(&cpufreq_state_lock, irq_flags);
	/*
	   for debug use
	   echo 0xabcde258 > /sys/power/cpufreq_min_limit means set the minimum limit to 600Mhz
	 */
	if((value & 0xfffff000) == 0xabcde000)
	{
		cpufreq_min_limit = value & 0x00000fff;
		cpufreq_min_limit *= 1000;
		printk(KERN_ERR"cpufreq_min_limit value %s %d\n",buf,cpufreq_min_limit);
	}
	else
	{
		cpufreq_min_limit = *(int *)buf;
	}
	spin_unlock_irqrestore(&cpufreq_state_lock, irq_flags);
	return count;
}

static ssize_t cpufreq_max_limit_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	spin_lock_irqsave(&cpufreq_state_lock, irq_flags);

	/*
	   for debug use
	   echo 0xabcde4b0 > /sys/power/cpufreq_max_limit means set the maximum limit to 1200Mhz
	 */
	if((value & 0xfffff000) == 0xabcde000)
	{
		cpufreq_max_limit = value & 0x00000fff;
		cpufreq_max_limit *= 1000;
		printk(KERN_ERR"cpufreq_max_limit value %s %d\n",buf,cpufreq_max_limit);
	}
	else
	{
		cpufreq_max_limit = *(int *)buf;
	}
	spin_unlock_irqrestore(&cpufreq_state_lock, irq_flags);

	return count;
}

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static ssize_t dvfs_score_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_score_input %x\n",value);

	dvfs_score_select = (value >> 24) & 0x0f;
	if(dvfs_score_select < 4)
	{
		dvfs_score_critical[dvfs_score_select] = (value >> 16) & 0xff;
		dvfs_score_hi[dvfs_score_select] = (value >> 8) & 0xff;
		dvfs_score_mid[dvfs_score_select] = value & 0xff;
	}


	return count;
}

static ssize_t dvfs_score_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_score_select %d\n",dvfs_score_select);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[1] = %d dvfs_score_hi[1] = %d dvfs_score_mid[1] = %d\n",dvfs_score_critical[1],dvfs_score_hi[1],dvfs_score_mid[1]);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[2] = %d dvfs_score_hi[2] = %d dvfs_score_mid[2] = %d\n",dvfs_score_critical[2],dvfs_score_hi[2],dvfs_score_mid[2]);
	ret += snprintf(buf + ret,200,"dvfs_score_critical[3] = %d dvfs_score_hi[3] = %d dvfs_score_mid[3] = %d\n",dvfs_score_critical[3],dvfs_score_hi[3],dvfs_score_mid[3]);

	ret += snprintf(buf + ret,200,"percpu_total_load[0] = %d,%d->%d\n",
		percpu_load[0],ga_percpu_total_load[0][(cur_window_index[0] - 1 + 10) % 10],ga_percpu_total_load[0][cur_window_index[0]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[1] = %d,%d->%d\n",
		percpu_load[1],ga_percpu_total_load[1][(cur_window_index[1] - 1 + 10) % 10],ga_percpu_total_load[1][cur_window_index[1]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[2] = %d,%d->%d\n",
		percpu_load[2],ga_percpu_total_load[2][(cur_window_index[2] - 1 + 10) % 10],ga_percpu_total_load[2][cur_window_index[2]]);
	ret += snprintf(buf + ret,200,"percpu_total_load[3] = %d,%d->%d\n",
		percpu_load[3],ga_percpu_total_load[3][(cur_window_index[3] - 1 + 10) % 10],ga_percpu_total_load[3][cur_window_index[3]]);

	return strlen(buf) + 1;
}

static ssize_t dvfs_unplug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_score_input %x\n",value);

	dvfs_unplug_select = (value >> 24) & 0x0f;
	if(dvfs_unplug_select > 7)
	{
		cur_window_size[0]= (value >> 8) & 0xff;
		cur_window_size[1]= (value >> 8) & 0xff;
		cur_window_size[2]= (value >> 8) & 0xff;
		cur_window_size[3]= (value >> 8) & 0xff;
	}
	return count;
}

static ssize_t dvfs_unplug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_unplug_select %d\n",dvfs_unplug_select);
	ret += snprintf(buf + ret,100,"cur_window_size[0] = %d\n",cur_window_size[0]);
	ret += snprintf(buf + ret,100,"cur_window_size[1] = %d\n",cur_window_size[1]);
	ret += snprintf(buf + ret,100,"cur_window_size[2] = %d\n",cur_window_size[2]);
	ret += snprintf(buf + ret,100,"cur_window_size[3] = %d\n",cur_window_size[3]);

	return strlen(buf) + 1;
}


static ssize_t dvfs_plug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_plug_select %x\n",value);

	dvfs_plug_select = (value ) & 0x0f;
	return count;
}


static ssize_t dvfs_plug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_plug_select %d\n",dvfs_plug_select);

	return strlen(buf) + 1;
}
#endif

static ssize_t cpufreq_table_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,sprd_cpufreq_conf->freq_tbl,sizeof(* sprd_cpufreq_conf->freq_tbl));
	return sizeof(* sprd_cpufreq_conf->freq_tbl);
}

static ssize_t dvfs_prop_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	printk(KERN_ERR"dvfs_status %s\n",buf);
	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"dvfs_plug_select %x\n",value);

	dvfs_plug_select = (value ) & 0x0f;
	return count;
}

static ssize_t dvfs_prop_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"dvfs_plug_select %d\n",dvfs_plug_select);

	return strlen(buf) + 1;
}

#ifdef CONFIG_SPRD_AVS_DEBUG
extern unsigned int g_avs_log_flag;

static ssize_t avs_log_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	printk(KERN_ERR"g_avs_log_flag %s\n",buf);
	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	printk(KERN_ERR"g_avs_log_flag %x\n",value);

	g_avs_log_flag = (value ) & 0x0f;
	return count;
}

static ssize_t avs_log_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int ret = 0;

	ret = snprintf(buf + ret,50,"g_avs_log_flag %d\n",g_avs_log_flag);

	return strlen(buf) + 1;
}
#endif
static DEVICE_ATTR(cpufreq_min_limit, 0660, cpufreq_min_limit_show, cpufreq_min_limit_store);
static DEVICE_ATTR(cpufreq_max_limit, 0660, cpufreq_max_limit_show, cpufreq_max_limit_store);
static DEVICE_ATTR(cpufreq_min_limit_debug, 0440, cpufreq_min_limit_debug_show, NULL);
static DEVICE_ATTR(cpufreq_max_limit_debug, 0440, cpufreq_max_limit_debug_show, NULL);
static DEVICE_ATTR(cpufreq_table, 0440, cpufreq_table_show, NULL);
static DEVICE_ATTR(cpufreq_max_axi_freq, 0660, cpufreq_max_axi_freq_show, cpufreq_max_axi_freq_store);

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static DEVICE_ATTR(dvfs_score, 0660, dvfs_score_show, dvfs_score_store);
static DEVICE_ATTR(dvfs_unplug, 0660, dvfs_unplug_show, dvfs_unplug_store);
static DEVICE_ATTR(dvfs_plug, 0660, dvfs_plug_show, dvfs_plug_store);
#endif
static DEVICE_ATTR(dvfs_prop, 0660, dvfs_prop_show, dvfs_prop_store);

#ifdef CONFIG_SPRD_AVS_DEBUG
static DEVICE_ATTR(avs_log, 0660, avs_log_show, avs_log_store);
#endif
static struct attribute *g[] = {
	&dev_attr_cpufreq_min_limit.attr,
	&dev_attr_cpufreq_max_limit.attr,
	&dev_attr_cpufreq_min_limit_debug.attr,
	&dev_attr_cpufreq_max_limit_debug.attr,
	&dev_attr_cpufreq_table.attr,
	&dev_attr_cpufreq_max_axi_freq.attr,
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
	&dev_attr_dvfs_score.attr,
	&dev_attr_dvfs_unplug.attr,
	&dev_attr_dvfs_plug.attr,
#endif
	&dev_attr_dvfs_prop.attr,
#ifdef CONFIG_SPRD_AVS_DEBUG
	&dev_attr_avs_log.attr,
#endif
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int sprd_cpufreq_policy_notifier(
	struct notifier_block *nb, unsigned long event, void *data)
{
	return NOTIFY_OK;
}

static struct notifier_block sprd_cpufreq_policy_nb = {
	.notifier_call = sprd_cpufreq_policy_notifier,
};

static int __init sprd_cpufreq_modinit(void)
{
	int ret;
#if defined(CONFIG_SPRD_CPUFREQ_DT_DRIVER)
        struct platform_device_info devinfo = { .name = "cpufreq-dt-sprd", };

        platform_device_register_full(&devinfo);

        return;
#endif

#if defined(CONFIG_ARCH_SCX35)
	sprd_cpufreq_conf = &sc8830_cpufreq_conf;
#elif defined(CONFIG_ARCH_SC8825)
	sprd_cpufreq_conf = &sc8825_cpufreq_conf;
#endif

#if defined(CONFIG_ARCH_SCX35)
	ret = sprd_freq_table_init();
	if (ret)
		return ret;

	sprd_top_frequency = sprd_cpufreq_conf->freq_tbl[0].frequency;
	/* TODO:need verify for the initialization of limited max freq */

	sprd_cpufreq_conf->clk = clk_get_sys(NULL, "clk_mcu");
	if (IS_ERR(sprd_cpufreq_conf->clk))
		return PTR_ERR(sprd_cpufreq_conf->clk);
	if (clk_prepare_enable(sprd_cpufreq_conf->clk))
		pr_err("%s(%d) err: clk_prepare_enable failed\n", __func__, __LINE__);
	#if defined (CONFIG_ARCH_WHALE)
		sprd_cpufreq_conf->mpllclk = clk_get_sys(NULL, "clk_mpll1");
	#else
		sprd_cpufreq_conf->mpllclk = clk_get_sys(NULL, "clk_mpll");
	#endif
	if (IS_ERR(sprd_cpufreq_conf->mpllclk))
		return PTR_ERR(sprd_cpufreq_conf->mpllclk);

#if !defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX20)
	sprd_cpufreq_conf->tdpllclk = clk_get_sys(NULL, "clk_tdpll");
	if (IS_ERR(sprd_cpufreq_conf->tdpllclk))
		return PTR_ERR(sprd_cpufreq_conf->tdpllclk);
#else
//	sprd_cpufreq_conf->tdpllclk = clk_get_sys(NULL, "clk_twpll");
	#if defined (CONFIG_ARCH_WHALE)
	sprd_cpufreq_conf->tdpllclk = clk_get_sys(NULL, "clk_tw_768m");
	#else
	sprd_cpufreq_conf->tdpllclk = clk_get_sys(NULL, "clk_768m");
	#endif
	if (IS_ERR(sprd_cpufreq_conf->tdpllclk))
		return PTR_ERR(sprd_cpufreq_conf->tdpllclk);
#endif
	mutex_init(&cpufreq_vddarm_lock);
	#if defined (CONFIG_ARCH_WHALE)
	sprd_cpufreq_conf->regulator = regulator_get(NULL, "vddarm0");
	#else
	sprd_cpufreq_conf->regulator = regulator_get(NULL, "vddarm");
	#endif
	if (IS_ERR(sprd_cpufreq_conf->regulator))
		return PTR_ERR(sprd_cpufreq_conf->regulator);

	/* set max voltage first */
	/*
	regulator_set_voltage(sprd_cpufreq_conf->regulator,
		sprd_cpufreq_conf->vddarm_mv[0],
		sprd_cpufreq_conf->vddarm_mv[0]);
	*/
	clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk);
	/*
	* clk_set_rate(sprd_cpufreq_conf->mpllclk, (sprd_top_frequency * 1000));
	*/
	clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk);
	global_freqs.old = sprd_raw_get_cpufreq();

#endif

	boot_done = jiffies + DVFS_BOOT_TIME;
	ret = cpufreq_register_notifier(
		&sprd_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		return ret;

	ret = cpufreq_register_driver(&sprd_cpufreq_driver);

	ret = sysfs_create_group(power_kobj, &attr_group);
	return ret;
}

static void __exit sprd_cpufreq_modexit(void)
{
#if defined(CONFIG_ARCH_SCX35)
	if (!IS_ERR_OR_NULL(sprd_cpufreq_conf->regulator))
		regulator_put(sprd_cpufreq_conf->regulator);
#endif
	cpufreq_unregister_driver(&sprd_cpufreq_driver);
	cpufreq_unregister_notifier(
		&sprd_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
	return;
}

module_init(sprd_cpufreq_modinit);
module_exit(sprd_cpufreq_modexit);

MODULE_DESCRIPTION("cpufreq driver for Spreadtrum");
MODULE_LICENSE("GPL");
