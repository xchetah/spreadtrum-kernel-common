/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Copyright (C) 2012 steve zhan
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

/**
*For arm read ,delay about 1us when clk_adi runs at 76.8M
*The interface timing is compatible with spi timing
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/hwspinlock.h>
#include <asm/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_lock.h>
#include <soc/sprd/adi.h>

unsigned long sprd_adi_base;
unsigned long sprd_adi_phys;

/* soc defined begin*/
#define CTL_ADI_BASE			(SPRD_ADI_BASE)

/* registers definitions for controller CTL_ADI */
#define REG_ADI_CTRL0					(CTL_ADI_BASE + 0x04)
#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
#define REG_ADI_CHNL_PRIL				(CTL_ADI_BASE + 0x08)
#define REG_ADI_CHNL_PRIH				(CTL_ADI_BASE + 0x0C)
#define REG_ADI_INT_RAW					(CTL_ADI_BASE + 0x14)
#define REG_ADI_RD_CMD					(CTL_ADI_BASE + 0x28)
#define REG_ADI_RD_DATA					(CTL_ADI_BASE + 0x2C)
#define REG_ADI_FIFO_STS				(CTL_ADI_BASE + 0x30)
#define REG_ADI_GSSI_CFG0				(CTL_ADI_BASE + 0x20)
#define REG_ADI_GSSI_CFG1				(CTL_ADI_BASE + 0x24)
#else
#define REG_ADI_CHNL_PRI				(CTL_ADI_BASE + 0x08)
#define REG_ADI_INT_RAW					(CTL_ADI_BASE + 0x10)
#define REG_ADI_RD_CMD					(CTL_ADI_BASE + 0x24)
#define REG_ADI_RD_DATA					(CTL_ADI_BASE + 0x28)
#define REG_ADI_FIFO_STS				(CTL_ADI_BASE + 0x2c)
#define REG_ADI_GSSI_CFG0				(CTL_ADI_BASE + 0x1C)
#define REG_ADI_GSSI_CFG1				(CTL_ADI_BASE + 0x20)
#endif
/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ARM_SCLK_EN                 ( BIT(1) )
#define BITS_CMMB_WR_PRI			( (1) << 4 & (BIT(4)|BIT(5)) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define BITS_PD_WR_PRI             ( (1) << 14 & (BIT(14)|BIT(15)) )
#define BITS_RFT_WR_PRI       	   ( (1) << 12 & (BIT(12)|BIT(13)) )
#define BITS_DSP_RD_PRI            ( (2) << 10 & (BIT(10)|BIT(11)) )
#define BITS_DSP_WR_PRI            ( (2) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARM_RD_PRI            ( (3) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ARM_WR_PRI            ( (3) << 4 & (BIT(4)|BIT(5)) )
#define BITS_STC_WR_PRI            ( (1) << 2 & (BIT(2)|BIT(3)) )
#define BITS_INT_STEAL_PRI         ( (3) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADI_RD_DATA */
#define BIT_RD_CMD_BUSY                 ( BIT(31) )
#define SHIFT_RD_ADDR                   ( 16 )

#define SHIFT_RD_VALU                   ( 0 )
#define MASK_RD_VALU                    ( 0xFFFF )

/* bits definitions for register REG_ADI_FIFO_STS */
#define BIT_FIFO_FULL                   ( BIT(11) )
#define FIFO_IS_FULL()	(__raw_readl((void __iomem *)REG_ADI_FIFO_STS) & BIT_FIFO_FULL)
#define BIT_FIFO_EMPTY                  ( BIT(10) )

/* special V1 (sc8830 soc) defined */
/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ADI_WR(_X_)                 ( (_X_) << 2 )
#define BITS_ADDR_BYTE_SEL(_X_)			( (_X_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define VALUE_CH_PRI	(0x0)


#if defined(CONFIG_ARCH_SCX35)
#define ANA_VIRT_BASE			( REGS_ADISLAVE_BASE )
#define ANA_PHYS_BASE			( REGS_ADISLAVE_PHYS )
#else
#define ANA_VIRT_BASE			( REGS_MISC_BASE )
#define ANA_PHYS_BASE			( REGS_MISC_PHYS )
#endif

#define ANA_ADDR_SIZE			(SZ_4K)

#define	TO_ADDR(_x_)		( ((_x_) >> SHIFT_RD_ADDR) & readback_addr_mak )
/* soc defined end*/

/*This value have a bit of depending on  real hardware fifo size*/
#define CACHE_SIZE	(16)
#define HEAD_ADD	(1)
#define TAIL_ADD	(0)

static struct __data {
	unsigned long reg;
	u16 val;
} __data_array[CACHE_SIZE];

static struct __data *head_p = &__data_array[0];
static struct __data *tail_p = &__data_array[0];
static u32 data_in_cache = 0;
static u32 readback_addr_mak __read_mostly = 0;

/*FIXME: Now define adi IP version, sc8825 is zero, sc8830 is one,
* Adi need init early that than read soc id, now using this ARCH dependency.
*/
static inline int __adi_ver(void)
{
#ifdef	CONFIG_ARCH_SC8825
	return 0;
#else
	return 1;
#endif
}

static inline int __adi_fifo_drain(void)
{
	int cnt = 1000;
	while (!(__raw_readl((void __iomem *)REG_ADI_FIFO_STS) & BIT_FIFO_EMPTY) && cnt--) {
		udelay(1);
	}
	WARN(cnt == 0, "ADI WAIT timeout!!!");
	return 0;
}

int sci_is_adi_vaddr(unsigned long vaddr)
{
	return (vaddr >= ANA_VIRT_BASE && vaddr <= (ANA_VIRT_BASE + ANA_ADDR_SIZE));
}
EXPORT_SYMBOL(sci_is_adi_vaddr);

int sci_adi_p2v(unsigned long paddr, unsigned long *vaddr)
{
	if(paddr < ANA_PHYS_BASE || paddr > (ANA_PHYS_BASE + ANA_ADDR_SIZE)) {
		return -1;
	} else {
		*vaddr = paddr - ANA_PHYS_BASE + ANA_VIRT_BASE;
	}
	return 0;
}
EXPORT_SYMBOL(sci_adi_p2v);

static inline int __adi_addr_check(unsigned long vaddr)
{
	if(!sci_is_adi_vaddr(vaddr)) {
		WARN_ONCE(1, "Maybe ADI vaddr is wrong?!!");
		return -1;
	}
	return 0;
}

static inline unsigned long __adi_translate_addr(unsigned long regvddr)
{
	regvddr = regvddr - ANA_VIRT_BASE + ANA_PHYS_BASE;
	return regvddr;
}

static inline int __adi_read(unsigned long regPddr, unsigned int *v)
{
	unsigned long val;
	int cnt = 2000;
	int retry_cnt = 0;

	/*
	 * We don't wait write fifo empty in here,
	 * Because if normal write is SYNC, that will
	 * wait fifo empty at the end of the write.
	 */
re_try:
	__raw_writel((u32)regPddr,(void __iomem *)REG_ADI_RD_CMD);

	/*
	 * wait read operation complete, RD_data[31]
	 * is set simulaneously when writing read command.
	 * RD_data[31] will be cleared after the read operation complete, 
	 */
	do {
		val = __raw_readl((void __iomem *)REG_ADI_RD_DATA);
	} while ((val & BIT_RD_CMD_BUSY) && cnt--);

	WARN(cnt == 0, "ADI READ timeout!!!");
	/* val high part should be the address of the last read operation */
	if ((!v) || TO_ADDR(val) != (regPddr & readback_addr_mak)) {
		printk("Warning:val = 0x%lx, regPaddr = 0x%lx, readback_addr_mak = 0x%x\n",
				val,regPddr,readback_addr_mak);
		if(retry_cnt++ < 5)
			goto re_try;
		else
			return -1;
	}

	*v = val & MASK_RD_VALU;
	return 0;
}

int sci_adi_read(unsigned long reg)
{
	int val = 0;
	if (!__adi_addr_check(reg)) {
		unsigned long flags;
		int ret = 0;
		reg = __adi_translate_addr(reg);
		__arch_default_lock(HWLOCK_ADI, &flags);
		ret = __adi_read(reg, &val);
		__arch_default_unlock(HWLOCK_ADI, &flags);
		if (ret) {
			printk("read error: reg = 0x%lx\n",reg);
			BUG_ON(1);
		}
	}
	return val;
}
EXPORT_SYMBOL(sci_adi_read);

static inline void __p_add(struct __data **p, u32 isHead)
{
	if (++(*p) > &__data_array[CACHE_SIZE - 1])
		(*p) = &__data_array[0];
	if (head_p == tail_p) {
		if (isHead == HEAD_ADD) {
			data_in_cache = 0;
		} else {
			data_in_cache = CACHE_SIZE;
		}
	} else {
		data_in_cache = 2;
	}
}

static inline int __adi_write(unsigned long reg, u16 val, u32 sync)
{
	tail_p->reg = reg;
	tail_p->val = val;
	__p_add(&tail_p, TAIL_ADD);
	while (!FIFO_IS_FULL() && (data_in_cache != 0)) {
		__raw_writel(head_p->val,(void __iomem *)head_p->reg);
		__p_add(&head_p, HEAD_ADD);
	}

	if (sync || data_in_cache == CACHE_SIZE) {
		__adi_fifo_drain();
		while (data_in_cache != 0) {
			while (FIFO_IS_FULL()) {
				cpu_relax();
			}
			__raw_writel(head_p->val,(void __iomem *)head_p->reg);
			__p_add(&head_p, HEAD_ADD);
		}
		__adi_fifo_drain();
	}

	return 0;
}

int sci_adi_write_fast(unsigned long reg, u16 val, u32 sync)
{
	if (!__adi_addr_check(reg)) {
		unsigned long flags;
		__arch_default_lock(HWLOCK_ADI, &flags);
		__adi_write(reg, val, sync);
		__arch_default_unlock(HWLOCK_ADI, &flags);
	}
	return 0;
}
EXPORT_SYMBOL(sci_adi_write_fast);

int sci_adi_write(unsigned long reg, u16 or_val, u16 clear_msk)
{
	if (!__adi_addr_check(reg)) {
		unsigned long flags;
		int ret = 0, val = 0;
		__arch_default_lock(HWLOCK_ADI, &flags);
		ret = __adi_read(__adi_translate_addr(reg), &val);
		if (!ret)
			__adi_write(reg, (val & ~clear_msk) | or_val, 1);
		__arch_default_unlock(HWLOCK_ADI, &flags);
		if (ret)
			BUG_ON(1);
	}
	return 0;
}
EXPORT_SYMBOL(sci_adi_write);

static void __init __adi_init(void)
{
	uint32_t value;
	value = __raw_readl((void __iomem *)REG_ADI_CTRL0);

	if (__adi_ver() == 0) {
		value &= ~BIT_ARM_SCLK_EN;
		value |= BITS_CMMB_WR_PRI;
		__raw_writel(value, (void __iomem *)REG_ADI_CTRL0);

#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
		value = __raw_readl((void __iomem *)REG_ADI_CHNL_PRIL);
#else
		value = __raw_readl((void __iomem *)REG_ADI_CHNL_PRI);
#endif
		value |= BITS_PD_WR_PRI | BITS_RFT_WR_PRI |
		    BITS_DSP_RD_PRI | BITS_DSP_WR_PRI |
		    BITS_ARM_RD_PRI | BITS_ARM_WR_PRI |
		    BITS_STC_WR_PRI | BITS_INT_STEAL_PRI;
#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
		__raw_writel(value, (void __iomem *)REG_ADI_CHNL_PRIL);
#else
		__raw_writel(value, (void __iomem *)REG_ADI_CHNL_PRI);
#endif
		readback_addr_mak = 0x7ff;
	} else if (__adi_ver() == 1) {
		if (value)
			WARN_ON(1);

		value = VALUE_CH_PRI;
#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
		__raw_writel(value, (void __iomem *)REG_ADI_CHNL_PRIL);
		__raw_writel(value, (void __iomem *)REG_ADI_CHNL_PRIH);
#else
		__raw_writel(value, (void __iomem *)REG_ADI_CHNL_PRI);
#endif
		value = __raw_readl((void __iomem *)REG_ADI_GSSI_CFG0);
		readback_addr_mak = (value & 0x3f) - ((value >> 11) & 0x1f) - 1;
		readback_addr_mak = (1<<(readback_addr_mak + 2)) - 1;
	}
}

int __init sci_adi_init(void)
{
	struct resource res;
	struct device_node *np;


	np = of_find_compatible_node(NULL, NULL, "sprd,adi");
	if (of_can_translate_address(np)) {
		of_address_to_resource(np, 0, &res);
		sprd_adi_phys = res.start;
		sprd_adi_base = ioremap_nocache(res.start,
				resource_size(&res));
		if (!sprd_adi_base) {
			printk("%s: no such memory!\n", __func__);
			return -ENOMEM;
		}
		printk("%s: sprd_adi_base=%lx\n", __func__, sprd_adi_base);
	}

#if defined(CONFIG_ARCH_SC8825)
	/* enable adi in global regs */
	sci_glb_set((unsigned long )REG_GLB_GEN0, BIT_ADI_EB);
	/* reset adi */
	sci_glb_set((unsigned long )REG_GLB_SOFT_RST, BIT_ADI_RST);
	udelay(2);
	sci_glb_clr((unsigned long )REG_GLB_SOFT_RST, BIT_ADI_RST);

#elif defined(CONFIG_ARCH_SCX35)
	/*enable adi in global regs*/
	sci_glb_set((unsigned long )REG_AON_APB_APB_EB0, BIT_ADI_EB);
#endif
	__adi_init();

	return 0;
}

