/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_lock.h>

u32 sci_glb_read(unsigned long reg, u32 msk)
{
	return __raw_readl((void *)reg) & msk;
}

int sci_glb_write(unsigned long reg, u32 val, u32 msk)
{
	unsigned long flags;
	__arch_default_lock(HWLOCK_GLB, &flags);
	__raw_writel((__raw_readl((void *)reg) & ~msk) | val, (void *)reg);
	__arch_default_unlock(HWLOCK_GLB, &flags);
	return 0;
}

static int __is_glb(unsigned long reg)
{
#if	defined (CONFIG_ARCH_SCX35)
	return 0;
#else
	return rounddown(reg, SZ_64K) == rounddown(REGS_GLB_BASE, SZ_64K) ||
	    rounddown(reg, SZ_64K) == rounddown(REGS_AHB_BASE, SZ_64K);
#endif
}

int sci_glb_set(unsigned long reg, u32 bit)
{
	if (__is_glb(reg)) {
		__raw_writel(bit, (void *)REG_GLB_SET(reg));
	} else {
		unsigned long flags;
		__arch_default_lock(HWLOCK_GLB, &flags);
		__raw_writel(__raw_readl((void *)reg) | bit, (void *)reg);
		__arch_default_unlock(HWLOCK_GLB, &flags);
	}
	return 0;
}

int sci_glb_clr(unsigned long reg, u32 bit)
{
	if (__is_glb(reg)) {
		__raw_writel(bit, (void *)REG_GLB_CLR(reg));
	} else {
		unsigned long flags;
		__arch_default_lock(HWLOCK_GLB, &flags);
		__raw_writel((__raw_readl((void *)reg) & ~bit), (void *)reg);
		__arch_default_unlock(HWLOCK_GLB, &flags);
	}
	return 0;
}

EXPORT_SYMBOL(sci_glb_read);
EXPORT_SYMBOL(sci_glb_write);
EXPORT_SYMBOL(sci_glb_set);
EXPORT_SYMBOL(sci_glb_clr);

