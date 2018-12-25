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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_i2c.h>

#include <asm/io.h>

//#include <mach/globalregs.h>

#ifdef CONFIG_ARCH_SCX35
//#include <mach/hardware.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#endif

#define SPRD_I2C_CTL_ID	(6)

/*Note: The defined below are for tiger and later chipset. */
/*If we don't use cmd buffer function, the define work well for the old chipset*/
/*register offset*/
#define I2C_CTL	0x0000
#define I2C_CMD	0x0004
#define I2C_CLKD0	0x0008
#define I2C_CLKD1	0x000C
#define I2C_RST	0x0010
#define I2C_CMD_BUF	0x0014
#define I2C_CMD_BUF_CTL	0x0018
#define I2C_STA_STO_DVD  0X001C

/*The corresponding bit of I2C_CTL register*/
#define I2C_CTL_INT	(1 << 0)	/* I2c interrupt */
#define I2C_CTL_ACK	(1 << 1)	/* I2c received ack value */
#define I2C_CTL_BUSY	 (1 << 2)	/* I2c data line value */
#define I2C_CTL_IE	(1 << 3)	/* I2c interrupt enable */
#define I2C_CTL_EN	(1 << 4)	/* I2c module enable */
#define I2C_CTL_SCL_LINE	(1 << 5)	/*scl line signal */
#define I2C_CTL_SDA_LINE	(1 << 6)	/* sda line signal */
#define I2C_CTL_NOACK_INT_EN	(1 << 7)	/* no ack int enable */
#define I2C_CTL_NOACK_INT_STS		(1 << 8)	/* no ack int status */
#define I2C_CTL_NOACK_INT_CLR	(1 << 9)	/* no ack int clear */
#define I2C_CTL_PULLUP_MODE	(1 << 10)		 /* I2c PULL UP*/

#if defined(CONFIG_I2C_SPRD_R6P0_DUTY)
#define I2C_CTL_DUTY			(1 << 11)		 /* I2c DUTY*/
#endif

/*The corresponding bit of I2C_CMD register*/
#define I2C_CMD_INT_ACK	(1 << 0)	/* I2c interrupt clear bit */
#define I2C_CMD_TX_ACK	(1 << 1)	/* I2c transmit ack that need to be send */
#define I2C_CMD_WRITE	(1 << 2)	/* I2c write command */
#define I2C_CMD_READ	(1 << 3)	/* I2c read command */
#define I2C_CMD_STOP	(1 << 4)	/* I2c stop command */
#define I2C_CMD_START	(1 << 5)	/* I2c start command */
#define I2C_CMD_ACK	(1 << 6)	/* I2c received ack  value */
#define I2C_CMD_BUSY	(1 << 7)	/* I2c busy in exec commands */
#define I2C_CMD_DATA	0xFF00	/* I2c data received or data need to be transmitted */

/*The corresponding bit of I2C_RST register*/
#define I2C_RST_RST	(1 << 0)	/* I2c reset bit */

/*The corresponding bit of I2C_CMD_BUF_CTL register*/
#define I2C_CTL_CMDBUF_EN	(1 << 0)	/* Enable the cmd buffer mode */
#define I2C_CTL_CMDBUF_EXEC	(1 << 1)	/* Start to exec the cmd in the cmd buffer */

#ifdef CONFIG_I2C_RESUME_EARLY
#include <linux/syscore_ops.h>
static struct platform_device *pdev_chip_i2c[SPRD_I2C_CTL_ID];
#endif

/* i2c data structure*/
struct sprd_i2c {
	struct i2c_msg *msg;
	struct i2c_adapter adap;
	void __iomem *membase;
	struct clk *clk;
	int irq;
	struct sprd_platform_i2c *pdata;
};

struct sprd_platform_i2c {
	unsigned int normal_freq;	/* normal bus frequency */
	unsigned int fast_freq;	/* fast frequency for the bus */
	unsigned int min_freq;	/* min frequency for the bus */
	unsigned int pull_up_mode;
};

static struct sprd_platform_i2c sprd_platform_i2c_default = {
	.normal_freq = 100 * 1000,
	.fast_freq = 400 * 1000,
	.min_freq = 10 * 1000,
	.pull_up_mode = 0,
};

static struct sprd_i2c *sprd_i2c_ctl_id[SPRD_I2C_CTL_ID];
static unsigned int sprd_version_num = 0x0600; /*I2c have version number after r6p0*/

static inline int
sprd_i2c_poll_ctl_status(struct sprd_i2c *pi2c, unsigned long bit)
{
	int loop_cntr = 5000;

	do {
		udelay(1);
	}
	while (!(__raw_readl(pi2c->membase + I2C_CTL) & bit)
	       && (--loop_cntr > 0));

	if (loop_cntr > 0)
		return 1;
	else
		return -1;
}

static inline int
sprd_i2c_poll_cmd_status(struct sprd_i2c *pi2c, unsigned long bit)
{
	int loop_cntr = 5000;

	do {
		udelay(1);
	}
	while ((__raw_readl(pi2c->membase + I2C_CMD) & bit)
	       && (--loop_cntr > 0));

	if (loop_cntr > 0)
		return 1;
	else
		return -1;
}

static inline int sprd_i2c_wait_int(struct sprd_i2c *pi2c)
{
	return sprd_i2c_poll_ctl_status(pi2c, I2C_CTL_INT);
}

static inline int sprd_i2c_wait_busy(struct sprd_i2c *pi2c)
{
	return sprd_i2c_poll_cmd_status(pi2c, I2C_CMD_BUSY);
}

static inline int sprd_i2c_wait_ack(struct sprd_i2c *pi2c)
{
	return sprd_i2c_poll_cmd_status(pi2c, I2C_CMD_ACK);
}

static inline void sprd_i2c_clear_int(struct sprd_i2c *pi2c)
{
	unsigned int cmd = 0;

	sprd_i2c_wait_busy(pi2c);

	cmd = (__raw_readl(pi2c->membase + I2C_CMD) & 0xff00) | I2C_CMD_INT_ACK;
	__raw_writel(cmd, pi2c->membase + I2C_CMD);
}

static inline void dump_i2c_reg(struct sprd_i2c *pi2c)
{
	printk(KERN_ERR ": ======dump i2c-%d reg=======\n", pi2c->adap.nr);
	printk(KERN_ERR ": I2C_CTRL:0x%x\n",__raw_readl(pi2c->membase + I2C_CTL));
	printk(KERN_ERR ": I2C_CMD:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD));
	printk(KERN_ERR ": I2C_DVD0:0x%x\n",__raw_readl(pi2c->membase + I2C_CLKD0));
	printk(KERN_ERR ": I2C_DVD1:0x%x\n",__raw_readl(pi2c->membase + I2C_CLKD1));
	printk(KERN_ERR ": I2C_RST:0x%x\n",__raw_readl(pi2c->membase + I2C_RST));
	printk(KERN_ERR ": I2C_CMD_BUF:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD_BUF));
	printk(KERN_ERR ": I2C_CMD_BUF_CTL:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD_BUF_CTL));
}

static inline int sprd_wait_trx_done(struct sprd_i2c *pi2c)
{
	int rc;

	rc = sprd_i2c_wait_int(pi2c);
	if (rc < 0) {
		dev_err(&pi2c->adap.dev, "%s() err! rc=%d\n", __func__, rc);
		dump_i2c_reg(pi2c);
		return rc;
	}

	sprd_i2c_clear_int(pi2c);

	return sprd_i2c_wait_ack(pi2c);
}

static int
sprd_i2c_write_byte(struct sprd_i2c *pi2c, char byte, int stop, int is_last_msg)
{
	int rc = 0;
	int cmd;

	if (stop && is_last_msg) {
		cmd = (byte << 8) | I2C_CMD_WRITE | I2C_CMD_STOP;
	} else {
		cmd = (byte << 8) | I2C_CMD_WRITE;
	}

	dev_dbg(&pi2c->adap.dev, "%s() cmd=%x\n", __func__, cmd);
	__raw_writel(cmd, pi2c->membase + I2C_CMD);

	rc = sprd_wait_trx_done(pi2c);
	return rc;
}

static int sprd_i2c_read_byte(struct sprd_i2c *pi2c, char *byte, int stop)
{
	int rc = 0;
	int cmd;

	if (stop) {
		cmd = I2C_CMD_READ | I2C_CMD_STOP | I2C_CMD_TX_ACK;
	} else {
		cmd = I2C_CMD_READ;
	}
	__raw_writel(cmd, pi2c->membase + I2C_CMD);
	dev_dbg(&pi2c->adap.dev, "%s() cmd=%x\n", __func__, cmd);

	rc = sprd_wait_trx_done(pi2c);
	if (rc < 0) {
		dev_err(&pi2c->adap.dev, "%s() err! rc=%d\n", __func__, rc);
		return rc;
	}

	*byte = (unsigned char)(__raw_readl(pi2c->membase + I2C_CMD) >> 8);
	dev_dbg(&pi2c->adap.dev, "%s() byte=%x, cmd reg=%x\n", __func__, *byte,
		__raw_readl(pi2c->membase + I2C_CMD));

	return rc;
}

static int
sprd_i2c_writebytes(struct sprd_i2c *pi2c, const char *buf, int count,
		    int is_last_msg)
{
	int ii;
	int rc = 0;

	for (ii = 0; rc >= 0 && ii != count; ++ii)
		rc = sprd_i2c_write_byte(pi2c, buf[ii], ii == count - 1,
					 is_last_msg);
	return rc;
}

static int sprd_i2c_readbytes(struct sprd_i2c *pi2c, char *buf, int count)
{
	int ii;
	int rc = 0;

	for (ii = 0; rc >= 0 && ii != count; ++ii)
		rc = sprd_i2c_read_byte(pi2c, &buf[ii], ii == count - 1);

	return rc;
}

static int sprd_i2c_send_target_addr(struct sprd_i2c *pi2c, struct i2c_msg *msg)
{
	int rc = 0;
	int cmd = 0;
	int cmd2 = 0;
	int tmp = 0;

	if (msg->flags & I2C_M_TEN) {
		cmd = 0xf0 | (((msg->addr >> 8) & 0x03) << 1);
		cmd2 = msg->addr & 0xff;
	} else {
		cmd = (msg->addr & 0x7f) << 1;
	}

	if (msg->flags & I2C_M_RD)
		cmd |= 1;

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
//  dev_info (&pi2c->adap.dev, "%s() ctl=%x\n", __func__, tmp);

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp | I2C_CTL_EN | I2C_CTL_IE, pi2c->membase + I2C_CTL);

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
// dev_info (&pi2c->adap.dev, "%s() ctl=%x\n", __func__, tmp);

	cmd = (cmd << 8) | I2C_CMD_START | I2C_CMD_WRITE;
//  dev_info (&pi2c->adap.dev, "%s() cmd=%x\n", __func__, cmd);
	__raw_writel(cmd, pi2c->membase + I2C_CMD);

	rc = sprd_wait_trx_done(pi2c);
	if (rc < 0) {
//      dev_err (&pi2c->adap.dev, "%s() rc=%d\n", __func__, rc);
		return rc;
	}

	if ((msg->flags & I2C_M_TEN) && (!(msg->flags & I2C_M_RD))) {
		cmd2 = (cmd2 << 8) | I2C_CMD_WRITE;
//      dev_info (&pi2c->adap.dev, "%s() cmd2=%x\n", __func__, cmd2);
		__raw_writel(cmd2, pi2c->membase + I2C_CMD);

		rc = sprd_wait_trx_done(pi2c);
		if (rc < 0) {
//        dev_err (&pi2c->adap.dev, "%s() rc=%d\n", __func__, rc);
			return rc;
		}
	}

	return rc;
}

static int
sprd_i2c_handle_msg(struct i2c_adapter *i2c_adap, struct i2c_msg *pmsg,
		    int is_last_msg)
{
	struct sprd_i2c *pi2c = i2c_adap->algo_data;
	int rc;

	dev_dbg(&i2c_adap->dev, "%s() flag=%x, adr=%x, len=%d\n", __func__,
		pmsg->flags, pmsg->addr, pmsg->len);

	rc = sprd_i2c_send_target_addr(pi2c, pmsg);
	if (rc < 0) {
		dev_err(&i2c_adap->dev, "%s() rc=%d\n", __func__, rc);
		dump_i2c_reg(pi2c);
		return rc;
	}

	if ((pmsg->flags & I2C_M_RD)) {
		return sprd_i2c_readbytes(pi2c, pmsg->buf, pmsg->len);
	} else {
		return sprd_i2c_writebytes(pi2c, pmsg->buf, pmsg->len,
					   is_last_msg);
	}
}

static int
sprd_i2c_master_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg *msgs,
		     int num)
{
	int im = 0;
	int ret = 0;
	struct sprd_i2c *pi2c = i2c_adap->algo_data;
	clk_enable(pi2c->clk);

	dev_dbg(&i2c_adap->dev, "%s() msg num=%d\n", __func__, num);

	for (im = 0; ret >= 0 && im != num; im++) {
		dev_dbg(&i2c_adap->dev, "%s() msg im=%d\n", __func__, im);
		ret = sprd_i2c_handle_msg(i2c_adap, &msgs[im], im == num - 1);
	}
	clk_disable(pi2c->clk);

	return (ret >= 0)? im : -1;
}

static u32 sprd_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm sprd_i2c_algo = {
	.master_xfer = sprd_i2c_master_xfer,
	.functionality = sprd_i2c_func,
};

static void sprd_i2c_set_clk(struct sprd_i2c *pi2c, unsigned int freq)
{
	unsigned int apb_clk;
	unsigned int i2c_div;

	apb_clk = 26000000;
	#ifdef CONFIG_ARCH_SCX15
	i2c_div = apb_clk / (4 * freq) - 3;
	#else
	i2c_div = apb_clk / (4 * freq) - 1;
	#endif

	#if defined(CONFIG_I2C_SPRD_R6P0_DUTY)
	if(freq == 400000){
		__raw_writel(0x0012000c, pi2c->membase + I2C_CLKD0);
	}
	else
	__raw_writel(((i2c_div & 0xffff)<<16|(i2c_div & 0xffff)), pi2c->membase + I2C_CLKD0);
	#else
	__raw_writel(i2c_div & 0xffff, pi2c->membase + I2C_CLKD0);
	#endif

	/*fixed 400k start time issue after r7p0*/
	if ((sprd_version_num >>8) >= 0x07) {
		__raw_writel(0X75, pi2c->membase + I2C_STA_STO_DVD);
		if(freq == 400000)
			__raw_writel(0X10, pi2c->membase + I2C_STA_STO_DVD);
	}


	#if defined(CONFIG_I2C_SPRD_R6P0_DUTY)
	__raw_writel((i2c_div >> 16|(i2c_div& 0xffff0000)), pi2c->membase + I2C_CLKD1);
	#else
	__raw_writel(i2c_div >> 16, pi2c->membase + I2C_CLKD1);
	#endif
}

void sprd_i2c_ctl_chg_clk(unsigned int id_nr, unsigned int freq)
{
	unsigned int tmp;
	struct sprd_i2c *pi2c = sprd_i2c_ctl_id[id_nr] ;

	clk_enable(pi2c->clk);
	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp & (~I2C_CTL_EN), pi2c->membase + I2C_CTL);
	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	pi2c->pdata->normal_freq = freq;
	sprd_i2c_set_clk(pi2c, pi2c->pdata->normal_freq);

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp | I2C_CTL_EN, pi2c->membase + I2C_CTL);
	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	clk_disable(pi2c->clk);
	
}

EXPORT_SYMBOL_GPL(sprd_i2c_ctl_chg_clk);

static int sprd_i2c_clk_init(struct sprd_i2c *pi2c)
{
	char * clock_name;
	struct device_node *np = pi2c->adap.dev.of_node;
	char buf[256] = { 0 };

#if defined(CONFIG_ARCH_SCX35)
	if(of_property_read_string(np, "clock-names", &clock_name)) {
		if (unlikely(pi2c->adap.nr >= 5))
			strcpy(buf, "clk_i2c");
		else
			sprintf(buf, "clk_i2c%d", pi2c->adap.nr);
		clock_name = buf;
	}
	dev_err(&pi2c->adap.dev, "%s clock-names=%s\n",
				__func__, clock_name);
		
	pi2c->clk = clk_get(NULL, clock_name);
	if (IS_ERR(pi2c->clk)) {
		return -ENODEV;
	}

#elif defined(CONFIG_ARCH_SC8825)
		/*enable i2c clock */
		sprd_greg_set_bits(REG_TYPE_GLOBAL, (0x07 << 29) | BIT(4), GR_GEN0);
		/*reset i2c module */
		sprd_greg_set_bits(REG_TYPE_GLOBAL, (0x07 << 2) | 0x01, GR_SOFT_RST);
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, (0x07 << 2) | 0x01, GR_SOFT_RST);
		/*flush cmd buffer */
		__raw_writel(I2C_RST_RST, pi2c->membase + I2C_RST);
		__raw_writel(0, pi2c->membase + I2C_RST);
#endif
return 0;

}

static void sprd_i2c_enable(struct sprd_i2c *pi2c)
{
	unsigned int tmp;

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp & ~I2C_CTL_EN, pi2c->membase + I2C_CTL);
	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp & ~I2C_CTL_IE, pi2c->membase + I2C_CTL);
	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	__raw_writel(tmp & ~I2C_CTL_CMDBUF_EN, pi2c->membase + I2C_CTL);

	dev_err(&pi2c->adap.dev, "%s() freq=%d\n", __func__,
		pi2c->pdata->normal_freq);
	sprd_i2c_set_clk(pi2c, pi2c->pdata->normal_freq);

	tmp = __raw_readl(pi2c->membase + I2C_CTL);
	sprd_version_num = tmp >> 16;
	#if defined(CONFIG_I2C_SPRD_R6P0_DUTY)
	tmp |= I2C_CTL_EN | I2C_CTL_IE|I2C_CTL_DUTY;
	#else
	tmp |= I2C_CTL_EN | I2C_CTL_IE;
	#endif
	if (pi2c->pdata->pull_up_mode)
		tmp |= I2C_CTL_PULLUP_MODE;

	__raw_writel(tmp, pi2c->membase + I2C_CTL);
	__raw_writel(I2C_CMD_INT_ACK, pi2c->membase + I2C_CMD);

}

static int sprd_i2c_probe(struct platform_device *pdev)
{
	struct sprd_i2c *pi2c;
	struct resource *res;
	u32 prop = 0;
	int ret;
	struct device_node *np = pdev->dev.of_node;

	if (np)
		pdev->id = of_alias_get_id(np, "i2c");

	pi2c = kzalloc(sizeof(struct sprd_i2c), GFP_KERNEL);
	if (!pi2c) {
		ret = -ENOMEM;
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto free_adapter;
	}

	i2c_set_adapdata(&pi2c->adap, pi2c);
	snprintf(pi2c->adap.name, sizeof(pi2c->adap.name), "%s", "sprd-i2c");
	pi2c->adap.owner = THIS_MODULE;
	pi2c->adap.retries = 3;
	pi2c->adap.algo = &sprd_i2c_algo;
	pi2c->adap.algo_data = pi2c;
	pi2c->adap.dev.parent = &pdev->dev;
	pi2c->adap.nr = pdev->id;
	pi2c->membase = (void *)ioremap_nocache(res->start,
			res->end - res->start);
	if (!pi2c->membase)
		panic("ioremap failed!\n");

	pi2c->pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct sprd_platform_i2c), GFP_KERNEL);
	if (!pi2c->pdata) {
		ret = -ENOMEM;
		goto free_adapter;
	}

	memcpy(pi2c->pdata, &sprd_platform_i2c_default,
			sizeof(struct sprd_platform_i2c));

	if (!of_property_read_u32(pdev->dev.of_node, "clock-frequency", &prop))
		pi2c->pdata->normal_freq = prop;

	pi2c->adap.dev.of_node = pdev->dev.of_node;

	of_property_read_u32(pdev->dev.of_node, "pull_up_mode",
				&pi2c->pdata->pull_up_mode);

	dev_info(&pdev->dev, "%s() id=%d, base=%p \n", __func__, pi2c->adap.nr,
		 pi2c->membase);
	ret = sprd_i2c_clk_init(pi2c);
	if (ret) {
		dev_err(&pdev->dev, "get src clk failed\n");
		goto release_region;
	}

	clk_prepare_enable(pi2c->clk);
	sprd_i2c_enable(pi2c);
	
	clk_disable(pi2c->clk);


	ret = i2c_add_numbered_adapter(&pi2c->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "add_adapter failed!\n");
		clk_unprepare(pi2c->clk);
		goto release_region;
	}

	sprd_i2c_ctl_id[pdev->id] = pi2c;
	platform_set_drvdata(pdev, pi2c);

#ifdef CONFIG_I2C_RESUME_EARLY
	pdev_chip_i2c[pdev->id] = pdev;
#endif
	of_i2c_register_devices(&pi2c->adap);

	return 0;

release_region:
	//release_mem_region(res->start, resource_size(res));
free_adapter:
	kfree(pi2c);
out:
	return ret;
}

static int sprd_i2c_remove(struct platform_device *pdev)
{
	struct sprd_i2c *pi2c = platform_get_drvdata(pdev);
	//struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	i2c_del_adapter(&pi2c->adap);
	//release_mem_region(res->start, resource_size(res));
	clk_unprepare(pi2c->clk);
	kfree(pi2c);
	#ifdef CONFIG_I2C_RESUME_EARLY
	pdev_chip_i2c[pdev->id] = NULL;
	#endif
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if defined (CONFIG_PM) && defined(CONFIG_ARCH_SCX35)
static int i2c_controller_suspend(struct platform_device *pdev,
				      pm_message_t state)
{
	return 0;
}

static int i2c_controller_resume(struct platform_device *pdev)
{
	struct sprd_i2c *pi2c = platform_get_drvdata(pdev);

	clk_enable(pi2c->clk);
	sprd_i2c_enable(pi2c);	
	clk_disable(pi2c->clk);
	return 0;
}

#ifdef CONFIG_I2C_RESUME_EARLY
static int i2c_controller_suspend_late(void)
{
	int i;
	pm_message_t state = {
		.event = 0
	};

	for (i = 0; i < ARRAY_SIZE(sprd_i2c_ctl_id); i++) {
		if (pdev_chip_i2c[i] == NULL)
			continue;
		i2c_controller_suspend(pdev_chip_i2c[i], state);
	}

	return 0;
}

static void i2c_controller_resume_early(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_i2c_ctl_id); i++) {
		if (pdev_chip_i2c[i] == NULL)
			continue;
		i2c_controller_resume(pdev_chip_i2c[i]);
	}
}

static struct syscore_ops sprd_i2c_syscore_ops = {
	.suspend = i2c_controller_suspend_late,
	.resume = i2c_controller_resume_early,
};

static int __init sprd_i2c_syscore_init(void)
{
	register_syscore_ops(&sprd_i2c_syscore_ops);
	return 0;
}
subsys_initcall(sprd_i2c_syscore_init);
#endif /* CONFIG_I2C_RESUME_EARLY */
#else
#define i2c_controller_suspend	NULL
#define i2c_controller_resume	NULL
#endif

static struct of_device_id sprd_i2c_of_match[] = {
	{ .compatible = "sprd,i2c", },
	{ }
};

static struct platform_driver sprd_i2c_driver = {
	.probe = sprd_i2c_probe,
	.remove = sprd_i2c_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd-i2c",
		   .of_match_table = of_match_ptr(sprd_i2c_of_match),
		   },
#ifndef CONFIG_I2C_RESUME_EARLY
	.suspend = i2c_controller_suspend,
	.resume = i2c_controller_resume,
#endif
};

static int __init sprd_i2c_init(void)
{
	return platform_driver_register(&sprd_i2c_driver);
}
#if defined(CONFIG_REGULATOR_EXT_DCDC)
arch_initcall_sync(sprd_i2c_init);
#else
subsys_initcall(sprd_i2c_init);
#endif

static void __exit sprd_i2c_exit(void)
{
	platform_driver_unregister(&sprd_i2c_driver);
}

module_exit(sprd_i2c_exit);

MODULE_DESCRIPTION("sprd iic algorithm and driver");
MODULE_AUTHOR("hao.liu, <hao.liu@spreadtrum.com>");
MODULE_LICENSE("GPL");
