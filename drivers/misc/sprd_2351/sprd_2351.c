#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <soc/sprd/hardware.h>
#include <linux/sprd_2351.h>
#include <asm/io.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#ifdef CONFIG_OF
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <linux/sipc.h>
static struct sprd_2351_data rfspi;

static int s_gpio_ctrl_num = 0;
static bool s_is_gpio_register = false;
static bool s_is_vddwpa_register = false;
#define GPIO_RF2351_POWER_CTRL s_gpio_ctrl_num

void sprd_sr2351_gpio_ctrl_power_register(int gpio_num)
{
    s_gpio_ctrl_num  = gpio_num;
    s_is_gpio_register = true;
    return;
}

static int sprd_rfspi_wait_write_idle(void)
{
	u32 time_out;
	u32 reg_data;

	for (time_out = 0, reg_data = 0x10; \
		reg_data & BIT_4; time_out++) {/*wait fifo empty*/
		if (time_out > 1000) {
			RF2351_PRINT("mspi time out!\r\n");
			return -1;
		}
		reg_data = sci_glb_read(RFSPI_CFG0,-1UL);
		//RF2351_PRINT("reg_data is: %x\n", reg_data);
	}

	return 0;
}

static unsigned int sprd_rfspi_write(u16 Addr, u16 Data)
{
	u32 reg_data = 0;
	if (sprd_rfspi_wait_write_idle() == -1) {
		RF2351_PRINT("Error: mspi is busy\n");
		return -1;
	}
	reg_data = (Addr << 16) | Data;
	sci_glb_write(RFSPI_MCU_WCMD, reg_data, -1UL);

	sprd_rfspi_wait_write_idle();
	return 0;
}

static int sprd_rfspi_wait_read_idle(void)
{
	u32 time_out;
	u32 reg_data;

	for (time_out = 0, reg_data = 0; \
		!(reg_data & BIT_7); time_out++) {/*wait fifo empty*/
		if (time_out > 1000) {
			RF2351_PRINT("mspi time out!\r\n");
			return -1;
		}
		reg_data = sci_glb_read(RFSPI_CFG0,-1UL);
		//RF2351_PRINT("reg_data is: %x\n", reg_data);
	}
	
	return 0;
}

static unsigned int sprd_rfspi_read(u16 Addr, u32 *Read_data)
{
	u32 reg_data = 0;
	if (sprd_rfspi_wait_read_idle() == -1) {
		RF2351_PRINT("Error: mspi is busy\n");
		return -1;
	}
	
	reg_data = (Addr << 16) | BIT_31;
	sci_glb_write(RFSPI_MCU_RCMD, reg_data, -1UL);

	sprd_rfspi_wait_read_idle();
	*Read_data = sci_glb_read(RFSPI_MCU_RDATA,-1UL);
	return 0;
}

static void sprd_rfspi_clk_enable(void)
{
	if(rfspi.clk)
		#ifdef CONFIG_OF
		clk_prepare_enable(rfspi.clk);
		#else
		clk_enable(rfspi.clk);
		#endif
	else 
		RF2351_PRINT("rfspi.clk is NULL\n");
}

static void sprd_rfspi_clk_disable(void)
{
	if(rfspi.clk)
		#ifdef CONFIG_OF
		clk_disable_unprepare(rfspi.clk);
		#else
		clk_disable(rfspi.clk);
		#endif
	else
		RF2351_PRINT("rfspi.clk is NULL\n");
}

static void sprd_mspi_enable(void)
{
	sci_glb_set(APB_EB0, RFSPI_ENABLE_CTL);
}

static void sprd_mspi_disable(void)
{
	/*Because of BT can't work when connet BT Earphones if disable the mspi*/
	//sci_glb_clr(APB_EB0, RFSPI_ENABLE_CTL);
}

static unsigned int sprd_rfspi_enable(void)
{
	rfspi.count++;
	if(rfspi.count == 1)
	{
		#ifndef CONFIG_OF
		rfspi.clk = clk_get(NULL, "clk_cpll");
		if(rfspi.clk == NULL)
{
			RF2351_PRINT("rfspi get clk_cpll failed\n");
			return -1;
		}
		#endif
		sprd_rfspi_clk_enable();
		sprd_mspi_enable();
	}

	return 0;
}

static unsigned int sprd_rfspi_disable(void)
{
	if(rfspi.count == 1)
	{
		sprd_mspi_disable();
		sprd_rfspi_clk_disable();
		#ifndef CONFIG_OF
		clk_put(rfspi.clk);
		#endif
	}
	rfspi.count--;
	if(rfspi.count < 0)
		rfspi.count = 0;

	return 0;
}

static DEFINE_MUTEX(rf2351_lock);
static int rf2351_power_count=0;



void rf2351_gpio_ctrl_power_enable(int flag)
{
    if(!s_is_gpio_register)
    return;

    if(!flag && (0 == rf2351_power_count))//avoid calling the func  first time with flag =0
    return;
    
    RF2351_PRINT("rf2351_power_count =%d\n", rf2351_power_count);
    mutex_lock(&rf2351_lock);
    if(0 == rf2351_power_count)
    {
        if (gpio_request(GPIO_RF2351_POWER_CTRL, "rf2351_power_pin"))
        {
            RF2351_PRINT("request gpio %d failed\n",GPIO_RF2351_POWER_CTRL);
        }
        gpio_direction_output(GPIO_RF2351_POWER_CTRL, 1);	 
    }

    if(flag)
    {
        if(0 == rf2351_power_count)
        {
            gpio_set_value(GPIO_RF2351_POWER_CTRL, 1);
           RF2351_PRINT("rf2351_gpio_ctrl_power_enable  on\n"); 
        }
        rf2351_power_count++;
    }
    else
    {
         rf2351_power_count--;
        if(0 == rf2351_power_count)
        {
            gpio_set_value(GPIO_RF2351_POWER_CTRL, 0);
            gpio_free(GPIO_RF2351_POWER_CTRL);
            RF2351_PRINT("rf2351_gpio_ctrl_power_enable  off \n"); 
        }
    }
     mutex_unlock(&rf2351_lock);
}


EXPORT_SYMBOL(rf2351_gpio_ctrl_power_enable);

void sprd_sr2351_vddwpa_ctrl_power_register(void)
{
    s_is_vddwpa_register = true;
}
static int rf2351_cnt = 0;
int rf2351_vddwpa_ctrl_power_enable(int flag)
{
    static struct regulator *wpa_rf2351 = NULL;
    static int f_enabled = 0;

    if (!s_is_vddwpa_register)
    return 0;

    printk("[wpa_rf2351] LDO control : %s\n", flag ? "ON" : "OFF");

    if (flag ) {
		rf2351_cnt++;
		if(0 != f_enabled)
			return 0;
#ifdef CONFIG_ARCH_SCX20		//pike
        wpa_rf2351 = regulator_get(NULL, "vddsim2");
#else
		wpa_rf2351 = regulator_get(NULL, "vddwpa");
#endif
        if (IS_ERR(wpa_rf2351)) {
            printk("rf2351 could not find the vddwpa regulator\n");
            wpa_rf2351 = NULL;
            return EIO;
        } else {
#ifdef CONFIG_ARCH_SCX20
            regulator_set_voltage(wpa_rf2351, 2800000, 2800000);
#else
			regulator_set_voltage(wpa_rf2351, 3400000, 3400000);
#endif
            regulator_enable(wpa_rf2351);
        }
        f_enabled = 1;
    }
    if (!flag) {
		rf2351_cnt--;
		if(0 != rf2351_cnt)
			return 0;
        if (wpa_rf2351) {
            regulator_disable(wpa_rf2351);
            regulator_put(wpa_rf2351);
            wpa_rf2351 = NULL;
        }
        f_enabled = 0;
    }

    return 0;
}

EXPORT_SYMBOL(rf2351_vddwpa_ctrl_power_enable);

static struct sprd_2351_interface sprd_rf2351_ops = {
	.name = "rf2351",
	.mspi_enable = sprd_rfspi_enable,
	.mspi_disable = sprd_rfspi_disable,
	.read_reg = sprd_rfspi_read,
	.write_reg = sprd_rfspi_write,
};

int sprd_get_rf2351_ops(struct sprd_2351_interface **rf2351_ops)
{
	*rf2351_ops = &sprd_rf2351_ops;

	return 0;
}
EXPORT_SYMBOL(sprd_get_rf2351_ops);

int sprd_put_rf2351_ops(struct sprd_2351_interface **rf2351_ops)
{
	*rf2351_ops = NULL;

	return 0;
}
EXPORT_SYMBOL(sprd_put_rf2351_ops);

#if 0

#ifdef CONFIG_ARCH_SCX30G
#define SCI_IOMAP_BASE	0xF5000000

#define SCI_IOMAP(x)	(SCI_IOMAP_BASE + (x))
#define SPRD_PMU_BASE			SCI_IOMAP(0x230000)
#define WCN_REG_CLK_ADDR                               (SPRD_PMU_BASE + 0x68)
#else
#define SCI_IOMAP_BASE	0xF5000000

#define SCI_IOMAP(x)	(SCI_IOMAP_BASE + (x))
#define SPRD_PMU_BASE			SCI_IOMAP(0x230000)
#define WCN_REG_CLK_ADDR                               (SPRD_PMU_BASE + 0x60)
#endif
#endif
#ifdef CONFIG_ARCH_SCX30G
/*
return      1:cp2 work
	        0:cp2 not work
*/
int get_cp2_state()
{
	int reg_value = 0;
	/* cp2 force shutdown */
	//reg_value = sci_glb_read(WCN_REG_CLK_ADDR, 0x02000000);
	printk("[sprd_2351]:reg_value is 0x%x\n",reg_value);
	if(reg_value)
	{
		printk("[sprd_2351]:cp2 shutdown\n");
		return 0;
	}else {
		return 1;
	}
}
EXPORT_SYMBOL(get_cp2_state);

/*
return      1:ok
	        little than 0:fail
cam status: 1:on
  			0:off
*/
int sprd_switch_cp2_clk(int cam_status)
{
	char ret_buf[20]={0};
	char cmd_buf[20]={0};
	int reg_value = 0;
	int send_len=0,ret_len=0;
	if(!get_cp2_state())
		return 0;
	
	if(cam_status) //on
		sprintf((char *)cmd_buf, "at+switchclk=1?\r");
	else //off
		sprintf((char *)cmd_buf, "at+switchclk=0?\r");

	send_len = sbuf_write(SIPC_ID_WCN, SMSG_CH_PIPE, 12,(char *)cmd_buf, strlen(cmd_buf), msecs_to_jiffies(5000));
	if(send_len< 0){
		printk("[sprd_2351]cmd [%s] sbuf_write error!\n",cmd_buf);
		return send_len;
	} else if (send_len != (strlen(cmd_buf))) {
		printk("[sprd_2351]cmd [%s] sbuf_write not completely with send len %d\n",cmd_buf, send_len);
		return -EIO;
	}

	ret_len = sbuf_read(SIPC_ID_WCN, SMSG_CH_PIPE, 12,ret_buf,20, msecs_to_jiffies(5000));
	if(ret_len < 14){
		printk("[sprd_2351]cmd [%s] sbuf_read error, ret_len is %d\n",cmd_buf,ret_len);
		return -EIO;
	}


	if((strncmp(ret_buf,"+ok:switch clk",14) == 0) || (strncmp(ret_buf,"+ok:restore clk",14) == 0)){	
		printk("[sprd_2351] sprd_switch_cp2_clk end,ret_buf is %s\n",ret_buf);
		return 1;
	}
	else {
		printk("[sprd_2351] sprd_switch_cp2_clk error,ret_buf is %s\n",ret_buf);
		return -1;
	}
	
}
EXPORT_SYMBOL(sprd_switch_cp2_clk);

#endif

#ifdef CONFIG_OF
static struct rf2351_addr
{
	u32 rfspi_base;
	u32 apb_base;
}rf2351_base;

u32 rf2351_get_rfspi_base(void)
{
	return rf2351_base.rfspi_base;
}

u32 rf2351_get_apb_base(void)
{
	return rf2351_base.apb_base;
}

static int __init sprd_rfspi_init(void)
{
	int ret;

	struct device_node *np;
	struct resource res;

	np = of_find_node_by_name(NULL, "sprd_rf2351");
	if (!np) {
		RF2351_PRINT("Can't get the sprd_rfspi node!\n");
		return -ENODEV;
	}
	RF2351_PRINT(" find the sprd_rfspi node!\n");

	ret = of_address_to_resource(np, 0, &res);
	if (ret < 0) {
		RF2351_PRINT("Can't get the rfspi reg base!\n");
		return -EIO;
	}
	rf2351_base.rfspi_base =  (unsigned long)ioremap_nocache(res.start, resource_size(&res));
	RF2351_PRINT("rfspi reg base is 0x%x\n", rf2351_base.rfspi_base);

	ret = of_address_to_resource(np, 1, &res);
	if (ret < 0) {
		RF2351_PRINT("Can't get the rfspi reg base!\n");
		return -EIO;
	}
	rf2351_base.apb_base =  (unsigned long)ioremap_nocache(res.start, resource_size(&res));
	RF2351_PRINT("rfspi reg base is 0x%x\n", rf2351_base.apb_base);

	rfspi.clk = of_clk_get_by_name(np,"clk_cpll");
	if (IS_ERR(rfspi.clk)) {
		RF2351_PRINT("get clk_cpll fail!\n");
		return -1;
	} else {
		RF2351_PRINT("get clk_cpll ok!\n");
	}

	return ret;
}

module_init(sprd_rfspi_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd 2351 rfspi driver");
#endif


