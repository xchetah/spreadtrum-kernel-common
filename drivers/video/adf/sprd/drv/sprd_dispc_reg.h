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

#ifndef _DISPC_REG_H_
#define _DISPC_REG_H_

#include <linux/io.h>

#include "sprd_chip_common.h"

#define DC_OFFSET(dev_id)  (0X1000*(dev_id))

/* DISPC regs offset */
#define DISPC_CTRL		(0x0000)
#define DISPC_SIZE_XY		(0x0004)
#define DISPC_RSTN		(0x0008)
#define DISPC_BUF_THRES (0x000C)

#define DISPC_STS			(0x0010)

#define DISPC_LVDS_CTRL			(0x0018)

#define DISPC_IMG_CTRL			(0x0020)
#define DISPC_IMG_Y_BASE_ADDR	(0x0024)
#define DISPC_IMG_UV_BASE_ADDR	(0x0028)
#define DISPC_IMG_V_BASE_ADDR	(0x002c)
#define DISPC_IMG_SIZE_XY			(0x0030)
#define DISPC_IMG_PITCH			(0x0034)
#define DISPC_IMG_DISP_XY			(0x0038)
#define DISPC_BG_COLOR			(0x003c)

#define DISPC_OSD_CTRL		(0x0040)
#define DISPC_OSD_BASE_ADDR	(0x0044)
#define DISPC_OSD_SIZE_XY		(0x0048)
#define DISPC_OSD_PITCH		(0x004c)
#define DISPC_OSD_DISP_XY		(0x0050)
#define DISPC_OSD_ALPHA		(0x0054)
#define DISPC_OSD_CK			(0x0058)

#define DISPC_Y2R_CTRL			(0x0060)
#ifndef CONFIG_FB_SCX35
#define DISPC_Y2R_Y_PARAM		(0x0064)
#define DISPC_Y2R_U_PARAM		(0x0068)
#define DISPC_Y2R_V_PARAM		(0x006c)
#else
#define DISPC_Y2R_CONTRAST		(0x0064)
#define DISPC_Y2R_SATURATION		(0x0068)
#define DISPC_Y2R_BRIGHTNESS		(0x006c)
#endif

#define DISPC_INT_EN			(0x0070)
#define DISPC_INT_CLR			(0x0074)
#define DISPC_INT_STATUS		(0x0078)
#define DISPC_INT_RAW		(0x007c)

#define DISPC_DPI_CTRL		(0x0080)
#define DISPC_DPI_H_TIMING	(0x0084)
#define DISPC_DPI_V_TIMING	(0x0088)
#define DISPC_DPI_STS0		(0x008c)
#define DISPC_DPI_STS1		(0x0090)

#define DISPC_DBI_CTRL		(0x00a0)
#define DISPC_DBI_TIMING0		(0x00a4)
#define DISPC_DBI_TIMING1		(0x00a8)
#define DISPC_DBI_RDATA		(0x00ac)
#define DISPC_DBI_CMD		(0x00b0)
#define DISPC_DBI_DATA		(0x00b4)
#define DISPC_DBI_QUEUE		(0x00b8)
#define DISPC_TE_SYNC_DELAY	(0x00bc)

/*shadow register , read only*/
#define SHDW_IMG_CTRL				(0x00E0)
#define SHDW_IMG_Y_BASE_ADDR		(0x00E4)
#define SHDW_IMG_UV_BASE_ADDR		(0x00E8)
#define SHDW_IMG_V_BASE_ADDR		(0x00EC)
#define SHDW_IMG_SIZE_XY			(0x00F0)
#define SHDW_IMG_PITCH				(0x00F4)
#define SHDW_IMG_DISP_XY			(0x00F8)
#define SHDW_BG_COLOR				(0x00FC)
#define SHDW_OSD_CTRL				(0x0100)
#define SHDW_OSD_BASE_ADDR			(0x0104)
#define SHDW_OSD_SIZE_XY			(0x0108)
#define SHDW_OSD_PITCH				(0x010C)
#define SHDW_OSD_DISP_XY			(0x0110)
#define SHDW_OSD_ALPHA				(0x0114)
#define SHDW_OSD_CK					(0x0118)
#define SHDW_Y2R_CTRL				(0x0120)
#define SHDW_Y2R_CONTRAST			(0x0124)
#define SHDW_Y2R_SATURATION			(0x0128)
#define SHDW_Y2R_BRIGHTNESS			(0x012C)
#define SHDW_DPI_H_TIMING			(0x0130)
#define SHDW_DPI_V_TIMING			(0x0134)

#define AHB_MATRIX_CLOCK (0x0208)
#define REG_AHB_MATRIX_CLOCK (AHB_MATRIX_CLOCK + SPRD_AHB_BASE)

#define DISPC_INT_DONE_MASK          BIT(0)
#define DISPC_INT_TE_MASK            BIT(1)
#define DISPC_INT_ERR_MASK           BIT(2)
#define DISPC_INT_EDPI_TE_MASK       BIT(3)
#define DISPC_INT_UPDATE_DONE_MASK   BIT(4)
#define DISPC_INT_DPI_VSYNC_MASK     BIT(5)

#if (defined(CONFIG_FB_SCX30G) || defined(CONFIG_FB_SCX35L))
#define DISPC_INT_HWVSYNC DISPC_INT_DPI_VSYNC_MASK
#else
#define DISPC_INT_HWVSYNC DISPC_INT_DONE_MASK
#endif

enum {
	DISPC_INT_DONE,
	DISPC_INT_TE,
	DISPC_INT_ERR,
	DISPC_INT_EDPI_TE,
	DISPC_INT_UPDATE_DONE,
	DISPC_INT_MAX,
};

#define DISPC_INTERRUPT_SET(dev_id, bit, val)\
{\
	uint32_t reg_val = dispc_read(dev_id , DISPC_INT_EN);\
	reg_val = (val == 1) ? (reg_val | (1UL<<bit)) :  \
	(reg_val & (~(1UL<<bit)));\
	dispc_write(dev_id, reg_val, DISPC_INT_EN);\
}

extern unsigned long g_dispc_base_addr;

static inline uint32_t dispc_read(int dev_id, uint32_t reg)
{
	return __raw_readl((void __iomem *)(g_dispc_base_addr
		+ DC_OFFSET(dev_id) + reg));
}

static inline void dispc_write(int dev_id, uint32_t value, uint32_t reg)
{
	__raw_writel(value, (void __iomem *)(g_dispc_base_addr
		+ DC_OFFSET(dev_id) + reg));
}

static inline void dispc_set_bits(int dev_id, uint32_t bits, uint32_t reg)
{
	dispc_write(dev_id, dispc_read(dev_id, reg) | bits, reg);
}

static inline void dispc_clear_bits(int dev_id, uint32_t bits, uint32_t reg)
{
	dispc_write(dev_id, dispc_read(dev_id, reg) & ~bits, reg);
}

#endif
