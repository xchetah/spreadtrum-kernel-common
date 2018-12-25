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

#ifndef _LCDC_REG_H_
#define _LCDC_REG_H_

#include <soc/sprd/hardware.h>

#define AHB_CTL0                (SPRD_AHB_BASE + 0x200)
#define AHB_SOFT_RST            (SPRD_AHB_BASE + 0x210)

/* LCDC regs offset */
#define LCDC_CTRL			(0x0000)
#define LCDC_DISP_SIZE			(0x0004)
#define LCDC_LCM_START			(0x0008)
#define LCDC_LCM_SIZE			(0x000c)
#define LCDC_BG_COLOR			(0x0010)
#define LCDC_FIFO_STATUS		(0x0014)

#define LCDC_IMG_CTRL			(0x0020)
#define LCDC_IMG_Y_BASE_ADDR		(0x0024)
#define LCDC_IMG_UV_BASE_ADDR		(0x0028)
#define LCDC_IMG_SIZE_XY		(0x002c)
#define LCDC_IMG_PITCH			(0x0030)
#define LCDC_IMG_DISP_XY		(0x0034)

#define LCDC_OSD1_CTRL			(0x0050)
#define LCDC_OSD2_CTRL			(0x0080)
#define LCDC_OSD3_CTRL			(0x00b0)
#define LCDC_OSD4_CTRL			(0x00e0)
#define LCDC_OSD5_CTRL			(0x0110)

#define LCDC_OSD1_BASE_ADDR		(0x0054)
#define LCDC_OSD1_ALPHA_BASE_ADDR	(0x0058)
#define LCDC_OSD1_SIZE_XY		(0x005c)
#define LCDC_OSD1_PITCH			(0x0060)
#define LCDC_OSD1_DISP_XY		(0x0064)
#define LCDC_OSD1_ALPHA			(0x0068)
#define LCDC_OSD1_GREY_RGB		(0x006c)
#define LCDC_OSD1_CK			(0x0070)

#define LCDC_OSD2_BASE_ADDR		(0x0084)
#define LCDC_OSD2_SIZE_XY		(0x008c)
#define LCDC_OSD2_PITCH			(0x0090)
#define LCDC_OSD2_DISP_XY		(0x0094)
#define LCDC_OSD2_ALPHA			(0x0098)
#define LCDC_OSD2_GREY_RGB		(0x009c)
#define LCDC_OSD2_CK			(0x00a0)

#define LCDC_IRQ_EN			(0x0170)
#define LCDC_IRQ_CLR			(0x0174)
#define LCDC_IRQ_STATUS			(0x0178)
#define LCDC_IRQ_RAW			(0x017c)

#define LCM_CTRL			(0x0180)
#define LCM_TIMING0			(0x0184)
#define LCM_TIMING1			(0x0188)
#define LCM_RDDATA			(0x018c)
#define LCM_RSTN			(0x0190)
#define LCM_CMD				(0x01A0)
#define LCM_DATA			(0x01A4)

extern unsigned long g_dispc_base_addr;

static inline uint32_t lcdc_read(uint32_t reg)
{
	return __raw_readl(SPRD_LCDC_BASE + reg);
}

static inline void lcdc_write(uint32_t value, uint32_t reg)
{
	__raw_writel(value, (SPRD_LCDC_BASE + reg));
}

static inline void lcdc_set_bits(uint32_t bits, uint32_t reg)
{
	lcdc_write(lcdc_read(reg) | bits, reg);
}

static inline void lcdc_clear_bits(uint32_t bits, uint32_t reg)
{
	lcdc_write(lcdc_read(reg) & ~bits, reg);
}

#endif
