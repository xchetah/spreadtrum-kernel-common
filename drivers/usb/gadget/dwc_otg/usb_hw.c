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
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/arch_misc.h>
#include <linux/io.h>//__raw_writel
#include <asm/gpio.h>
#include <asm/hardirq.h>
#include "usb_hw.h"
#include "dwc_otg_driver.h"
#ifdef CONFIG_MFD_SM5504
#include <linux/mfd/sm5504.h>
#endif

static int    gpio_vbus=GPIO_INVALID ;
static int    gpio_id=GPIO_INVALID;
static uint32_t tune_from_dt = 0x44073e33;
static struct regulator *usb_regulator;
static char * phy_type = "usb20_synops_phy";
static char * usb_ldo_name = "vddusb";
/*for usb20_sprd_phy*/
static uint32_t reg_gusbcfg = 0;

static void usb_ldo_switch(int is_on)
{

	if(!IS_ERR_OR_NULL(usb_regulator)){
		if(is_on){
			regulator_set_voltage(usb_regulator,3300000,3300000);
			regulator_enable(usb_regulator);
		}else{
			regulator_disable(usb_regulator);
		}
	}
}

static void usb_enable_module(int en)
{
	if (en){
		sci_glb_clr(REG_AON_APB_PWR_CTRL,BIT_USB_PHY_PD);
		sci_glb_set(REG_AP_AHB_AHB_EB,BIT_USB_EB);
	}else {
		sci_glb_set(REG_AON_APB_PWR_CTRL,BIT_USB_PHY_PD);
		sci_glb_clr(REG_AP_AHB_AHB_EB,BIT_USB_EB);
	}
}

void usb_phy_init(struct platform_device *_dev)
{
	struct device_node *np = _dev->dev.of_node;
	struct device_node *np_regulator = NULL;
	const __be32 *phandle = NULL;

	phandle = of_get_property(np, "usb-supply", NULL);
	if (phandle == NULL){
		pr_info("read usb-supply error\n");
		return;
	}

	np_regulator = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (phandle == NULL){
		pr_info("get regulator error\n");
		return;
	}

	if(of_property_read_string(np_regulator, "regulator-name", &usb_ldo_name)){
		pr_info("read regulator-name error\n");
		return;
	}

	if (of_property_read_u32(np, "tune_value", &tune_from_dt)){
		pr_info("read tune_value error\n");
		return;
	}
	if(soc_is_scx9832a_v0())
	{
		uint32_t compa_tune;
		if (of_property_read_u32(np, "compa_tune", &compa_tune)) {
			pr_info("read compatible tune error\n");
		} else {
			tune_from_dt = compa_tune;
		}
	}
	if (of_property_read_string(np, "phy-type", &phy_type)){
		pr_info("read phy-type error\n");
		return;
	}
	pr_info("Usb_hw.c: [%s]usb-supply %s phy-type %s  tune from uboot: 0x%x\n",__FUNCTION__, usb_ldo_name,phy_type,tune_from_dt);

	if (!strcmp(phy_type, "usb20_sprd_phy")){
		dwc_otg_device_t *dwc_otg_device = platform_get_drvdata(_dev);
		reg_gusbcfg = dwc_otg_device->os_dep.base + GUSBCFG_OFFSET;
	}
	__raw_writel(tune_from_dt,REG_AP_USB_PHY_TUNE);

	usb_regulator = regulator_get(NULL,usb_ldo_name);
	if(!IS_ERR_OR_NULL(usb_regulator))
		regulator_set_voltage(usb_regulator,3600000,3600000);
}

static void usb_phy_dpdm_rst(void)
{
	if(soc_is_scx9832a_v0()) {
		sci_glb_clr(REG_AP_USB_PHY_CTRL,BIT_USB_DMPULLUP);
		return;
	}
	if (!strcmp(phy_type, "usb20_synops_phy")){
		/*Workaround:Reset DM default state*/
		sci_glb_set(REG_AP_USB_PHY_CTRL,BIT_USB_DMPULLUP);
	}
}

/*
spreadtrum usb phy shuttle has two reset control pins:
utmi_RST:soft reset
utmi_PORN:power on reset
pike chip:
pin ctrl AP_TOP_USB_PHY_RST:0x402A0000[0]<---->utmi_PORN
ahb reg REG_AP_AHB_AHB_RST[7]<------->utmi_RST
*/
void sprd_usb_phy_rst(void)
{
	if (!strcmp(phy_type, "usb20_sprd_phy")){
		sci_glb_set(reg_gusbcfg,BIT_PHY_INTERFACE);/*usc2s8c SPRD phy width:16 bit*/
		sci_glb_clr(AP_TOP_USB_PHY_RST, BIT_PHY_UTMI_PORN);/*for SPRD phy utmi_PORN*/
		sci_glb_set(REG_AP_AHB_AHB_RST, BIT_USB_PHY_SOFT_RSET);/*for SPRD phy utmi_rst*/
		mdelay(5);
		sci_glb_set(AP_TOP_USB_PHY_RST, BIT_PHY_UTMI_PORN);
		sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_USB_PHY_SOFT_RSET);

#ifdef CONFIG_ARCH_SCX20
{
	/* Workaround#510491: In Pike(SC7731C) AC chips, the default value of
	 * phy_tune1 (even after the chip being calibrated) does not tune the
	 * USB PHY best. we know the workaround is ugly but it has to be there.
	 */
		extern int sci_efuse_usb_phy_tune_get(unsigned int *);
		unsigned int phy_tune1, ret, tfhres;

		phy_tune1 = __raw_readl(REG_AP_USB_PHY_TUNE1);
		ret = sci_efuse_usb_phy_tune_get(&tfhres);
		pr_info("%s, tfhres: 0x%x, phy_tune1: 0x%x\n",
			__func__, tfhres, phy_tune1);
		if (!ret) {
			tfhres = tfhres > 6 ? tfhres - 6 : 0;
			phy_tune1 &= ~(BITS_USB20_TFHSRES(0x1f));
			phy_tune1 |= (BITS_USB20_TFHSRES(tfhres));
		}
		phy_tune1 &= ~(BITS_USB20_TUNEEQ(0x7));
		phy_tune1 |= BITS_USB20_TUNEEQ(0x2);

		__raw_writel(phy_tune1, REG_AP_USB_PHY_TUNE1);
}
#endif
	}else if (!strcmp(phy_type, "usb20_synops_phy")){
		sci_glb_set(REG_AP_AHB_AHB_RST,BIT_USB_PHY_SOFT_RSET);
		mdelay(3);
		sci_glb_clr(REG_AP_AHB_AHB_RST,BIT_USB_PHY_SOFT_RSET);
		mdelay(3);
	}else{
		pr_info("[%s] Error phy_type : %s\n", __FUNCTION__, phy_type);
	}

	usb_phy_dpdm_rst();
}

static void usb_startup(void)
{
	if (!strcmp(phy_type, "usb20_sprd_phy")){
		mdelay(5);
		sci_glb_set(REG_AP_AHB_AHB_RST, BIT_USB_SOFT_RSET|BIT_USB_UTMI_SOFT_RSET);
		mdelay(5);
		sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_USB_SOFT_RSET|BIT_USB_UTMI_SOFT_RSET);
		sprd_usb_phy_rst();
	}else if (!strcmp(phy_type, "usb20_synops_phy")){
		sci_glb_set(REG_AP_AHB_AHB_RST, BIT_USB_SOFT_RSET|BIT_USB_UTMI_SOFT_RSET|BIT_USB_PHY_SOFT_RSET);
		mdelay(5);
		sci_glb_clr(REG_AP_AHB_AHB_RST, BIT_USB_SOFT_RSET|BIT_USB_UTMI_SOFT_RSET|BIT_USB_PHY_SOFT_RSET);
		sci_glb_set(REG_AP_AHB_AHB_EB,BIT_USB_EB);
	}
	mdelay(3);
}

void udc_enable(void)
{
	pr_info("%s phy_type[%s] usb_ldo_name[%s] tune_from_dt[0x%x]\n", __func__,phy_type,usb_ldo_name,tune_from_dt);
	usb_phy_dpdm_rst();
	usb_ldo_switch(1);
	mdelay(10);
	usb_enable_module(1);
	mdelay(2);
	usb_startup();
}

void udc_disable(void)
{
    pr_info("%s \n", __func__);
    usb_enable_module(0);
    usb_ldo_switch(0);
}


int usb_alloc_vbus_irq(int gpio)
{
	int irq;

	gpio_request(gpio,"sprd_otg");
	gpio_direction_input(gpio);
	irq = gpio_to_irq(gpio);
	gpio_vbus= gpio;

	return irq;
}

void usb_free_vbus_irq(int irq,int gpio)
{
	gpio_free(gpio);
}

int usb_get_vbus_irq(void)
{
	int value;

	value = gpio_to_irq(gpio_vbus);

	return value;
}
int usb_get_vbus_state(void)
{
	int value;
	value = gpio_get_value(gpio_vbus);
	return !!value;
}

void usb_set_vbus_irq_type(int irq, int irq_type)
{
	if (irq_type == VBUS_PLUG_IN)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
	else if (irq_type == VBUS_PLUG_OUT)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else {
		pr_warning("error type for usb vbus\n");
	}

	return;
}

#ifndef DWC_DEVICE_ONLY
void charge_pump_set(int gpio, int state)
{
	struct regulator *usb_regulator = NULL;
#define  USB_CHG_PUMP_NAME	"chg_pump"

	if( GPIO_INVALID != gpio) {
		gpio_request(gpio, "chg_ pump");
		gpio_direction_output(gpio, 1);
		gpio_set_value(gpio, state);
	} else {
		if(usb_regulator == NULL)
			usb_regulator = regulator_get(NULL, USB_CHG_PUMP_NAME);
		if (usb_regulator) {
			if (state)
				regulator_enable(usb_regulator);
			else
				regulator_disable(usb_regulator);
			regulator_put(usb_regulator);
		}
	}
}

int usb_alloc_id_irq(int gpio)
{
	int irq;

	gpio_request(gpio,"USB OTG CABLE");
	gpio_direction_input(gpio);
	irq = gpio_to_irq(gpio);
	gpio_id=gpio;

	return irq;
}

void usb_free_id_irq(int irq,int gpio)
{
	gpio_free(gpio);
}

int usb_get_id_irq(void)
{
	int value;

	value = gpio_to_irq(gpio_id);

	return value;
}

#ifdef CONFIG_MFD_SM5504
extern bool sm5504_get_otg_status(void);
#endif
/*
 *if otg cable is connected , id state =0
 *as host turn on vbus, in this case shouldn't call this handler
 */
int usb_get_id_state(void)
{
#ifdef CONFIG_MFD_SM5504
	int value = 0;
	value = !sm5504_get_otg_status();
	return value;
#else
	int value;
	value = gpio_get_value(gpio_id);
	return !!value;
#endif
}

void usb_set_id_irq_type(int irq, int irq_type)
{
	if (irq_type == OTG_CABLE_PLUG_IN)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else if (irq_type == OTG_CABLE_PLUG_OUT)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
	else {
		pr_warning("error type for usb vbus\n");
	}

	return;
}
#endif


EXPORT_SYMBOL(udc_disable);
EXPORT_SYMBOL(udc_enable);
