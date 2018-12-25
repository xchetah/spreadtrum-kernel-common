/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef	_SPRD_MM_H
#define _SPRD_MM_H

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/io.h>

#define BIT_0                             0x01
#define BIT_1                             0x02
#define BIT_2                             0x04
#define BIT_3                             0x08
#define BIT_4                             0x10
#define BIT_5                             0x20
#define BIT_6                             0x40
#define BIT_7                             0x80
#define BIT_8                             0x0100
#define BIT_9                             0x0200
#define BIT_10                            0x0400
#define BIT_11                            0x0800
#define BIT_12                            0x1000
#define BIT_13                            0x2000
#define BIT_14                            0x4000
#define BIT_15                            0x8000
#define BIT_16                            0x010000
#define BIT_17                            0x020000
#define BIT_18                            0x040000
#define BIT_19                            0x080000
#define BIT_20                            0x100000
#define BIT_21                            0x200000
#define BIT_22                            0x400000
#define BIT_23                            0x800000
#define BIT_24                            0x01000000
#define BIT_25                            0x02000000
#define BIT_26                            0x04000000
#define BIT_27                            0x08000000
#define BIT_28                            0x10000000
#define BIT_29                            0x20000000
#define BIT_30                            0x40000000
#define BIT_31                            0x80000000


#define IO_PTR volatile void __iomem *
#define REG_RD(a) __raw_readl((IO_PTR)(a))
#define REG_WR(a,v) __raw_writel((v),(IO_PTR)(a))
#define REG_AWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) & (v)), ((IO_PTR)(a)))
#define REG_OWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) | (v)), ((IO_PTR)(a)))
#define REG_XWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) ^ (v)), ((IO_PTR)(a)))
#define REG_MWR(a,m,v) \
	do { \
		uint32_t _tmp = __raw_readl((IO_PTR)(a)); \
		_tmp &= ~(m); \
		__raw_writel((_tmp | ((m) & (v))), ((IO_PTR)(a))); \
	} while(0)



#define MM_ERROR_STR                                      "Error L %d, %s, %s \n"
#define MM_ERROR_ARGS                                     __LINE__,__FILE__,__FUNCTION__
#define MM_RTN_IF_ERR(rtn)   \
		do { \
			if(rtn) { \
				printk(MM_ERROR_STR, MM_ERROR_ARGS); \
				return -(rtn); \
			} \
		} while(0)


#define MM_DEBUG
#define MM_DEBUG_STR                                      "Debug L %d, %s, %s: "
#define MM_DEBUG_ARGS                                     __LINE__,__FILE__,__FUNCTION__

#ifdef MM_DEBUG
	#define MM_TRACE(format,...)   printk(MM_DEBUG_STR format, MM_DEBUG_ARGS, ##__VA_ARGS__)
#else
	#define MM_TRACE(format,...)   pr_debug(format,...)
#endif


#ifdef CONFIG_SC_FPGA
#define CONFIG_SC_FPGA_PIN
#define CONFIG_SC_FPGA_CLK
#define CONFIG_SC_FPGA_LDO
#endif

#ifdef CONFIG_OF
#ifndef CONFIG_SC_FPGA_CLK
#define clk_enable      clk_prepare_enable
#define clk_disable     clk_disable_unprepare
#else
#define clk_enable(x)      1
#define clk_disable(x)     1
#define clk_prepare_enable(x)      1
#define clk_disable_unprepare(x)     1
#endif
#endif

#ifdef  CONFIG_ARCH_WHALE
#define REG_PMU_APB_PD_MM_TOP_CFG   REG_PMU_APB_PD_CAM_SYS_CFG
#define BIT_PD_MM_TOP_FORCE_SHUTDOWN   (BIT_PMU_APB_PD_CAM_SYS_FORCE_SHUTDOWN|BIT_PMU_APB_PD_CAM_SYS_AUTO_SHUTDOWN_EN)
#define BIT_MM_EB  BIT_AON_APB_CAM_EB

#define REG_PMU_APB_CP_SOFT_RST     REG_PMU_APB_SYS_SOFT_RST
#endif

int32_t clk_mm_i_eb(struct device_node *dn, uint32_t enable);

void mm_clk_register_trace(void);

#endif
