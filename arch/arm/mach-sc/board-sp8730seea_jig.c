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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/input.h>

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <soc/sprd/hardware.h>
#include <linux/i2c.h>
#if (defined(CONFIG_INPUT_LIS3DH_I2C) \
	|| defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
#include <linux/i2c/lis3dh.h>
#endif
#if (defined(CONFIG_INPUT_LTR558_I2C) \
	|| defined(CONFIG_INPUT_LTR558_I2C_MODULE))
#include <linux/i2c/ltr_558als.h>
#endif
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <soc/sprd/board.h>
#include <soc/sprd/serial_sprd.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#include <soc/sprd/pinmap.h>
#include <linux/irq.h>
#include <linux/input/matrix_keypad.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/kpd.h>
#include <soc/sprd/sci_glb_regs.h>

#if (defined(CONFIG_INV_MPU_IIO) || defined(CONFIG_INV_MPU_IIO_MODULE))
#include <linux/mpu.h>
#endif
#if (defined(CONFIG_SENSORS_AK8975) || defined(CONFIG_SENSORS_AK8975_MODULE))
#include <linux/akm8975.h>
#endif



#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#if (defined(CONFIG_TOUCHSCREEN_FOCALTECH) \
	|| defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
#include <linux/i2c/focaltech.h>
#endif
#include <soc/sprd/i2s.h>
#include <linux/sprd_2351.h>

#if (defined(CONFIG_KEYBOARD_SC) \
	|| defined(CONFIG_KEYBOARD_SC_MODULE))
#include <linux/input/matrix_keypad.h>
#include <soc/sprd/kpd.h>
#endif
#if (defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE))
#include <linux/gpio_keys.h>
#endif
#if (defined(CONFIG_KEYBOARD_SPRD_EIC) \
	|| defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
#include <linux/sprd_eic_keys.h>
#endif
#if (defined(CONFIG_BACKLIGHT_SPRD_PWM) \
	|| defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
#include <linux/sprd_pwm_bl.h>
#endif
#if (defined(CONFIG_INPUT_HEADSET_SPRD_SC2723) \
	|| defined(CONFIG_INPUT_HEADSET_SPRD_SC2723_MODULE))
#include <linux/headset_sprd_sc2723.h>
#endif

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);

#if (defined(CONFIG_BACKLIGHT_SPRD_PWM) \
	|| defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
static struct sprd_pwm_bl_platform_data sprd_pwm_bl_platform_data = {
	.brightness_max = 255,
	.brightness_min = 0,
	.pwm_index = 3,
	.gpio_ctrl_pin = -1,
	.gpio_active_level = 0,
};
#endif

#if (defined(CONFIG_INPUT_HEADSET_SPRD_SC2723) \
	|| defined(CONFIG_INPUT_HEADSET_SPRD_SC2723_MODULE))
static struct headset_buttons headset_sprd_sc2723_buttons[] = {
	{
		.adc_min = HEADSET_ADC_MIN_KEY_MEDIA,
		.adc_max = HEADSET_ADC_MAX_KEY_MEDIA,
		.code = KEY_MEDIA,
	},
#if 0
	{
		.adc_min = HEADSET_ADC_MIN_KEY_VOLUMEUP,
		.adc_max = HEADSET_ADC_MAX_KEY_VOLUMEUP,
		.code = KEY_VOLUMEUP,
	},
	{
		.adc_min = HEADSET_ADC_MIN_KEY_VOLUMEDOWN,
		.adc_max = HEADSET_ADC_MAX_KEY_VOLUMEDOWN,
		.code = KEY_VOLUMEDOWN,
	},
#endif
};

static struct sprd_headset_platform_data headset_sprd_sc2723_pdata = {
	.gpio_switch = HEADSET_SWITCH_GPIO,
	.gpio_detect = HEADSET_DETECT_GPIO,
	.gpio_button = HEADSET_BUTTON_GPIO,
	.irq_trigger_level_detect = HEADSET_IRQ_TRIGGER_LEVEL_DETECT,
	.irq_trigger_level_button = HEADSET_IRQ_TRIGGER_LEVEL_BUTTON,
	.adc_threshold_3pole_detect = HEADSET_ADC_THRESHOLD_3POLE_DETECT,
	.adc_threshold_4pole_detect = HEADSET_ADC_THRESHOLD_4POLE_DETECT,
	.irq_threshold_buttont = HEADSET_IRQ_THRESHOLD_BUTTON,
	.voltage_headmicbias = HEADSET_HEADMICBIAS_VOLTAGE,
	.headset_buttons = headset_sprd_sc2723_buttons,
	.nbuttons = ARRAY_SIZE(headset_sprd_sc2723_buttons),
	.external_headmicbias_power_on = NULL,
};
#endif



int __init __clock_init_early(void)
{
	pr_info("ahb ctl0 %08x, ctl2 %08x glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AP_AHB_AHB_EB),
		sci_glb_raw_read(REG_AP_AHB_AHB_RST),
		sci_glb_raw_read(REG_AON_APB_APB_EB0),
		sci_glb_raw_read(REG_AON_APB_APB_EB1),
		sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));

	sci_glb_clr(REG_AP_AHB_AHB_EB,
		BIT_BUSMON2_EB		|
		BIT_BUSMON1_EB		|
		BIT_BUSMON0_EB		|
		BIT_GPS_EB		|
		BIT_DRM_EB		|
		BIT_NFC_EB		|
		0);
	sci_glb_clr(REG_AP_APB_APB_EB,
		BIT_INTC3_EB		|
		BIT_INTC2_EB		|
		BIT_INTC1_EB		|
		BIT_IIS1_EB		|
		BIT_UART2_EB		|
		BIT_UART0_EB		|
		BIT_SPI1_EB		|
		BIT_SPI0_EB		|
		BIT_IIS0_EB		|
		BIT_I2C0_EB		|
		BIT_SPI2_EB		|
		BIT_UART3_EB		|
		0);
	sci_glb_clr(REG_AON_APB_APB_RTC_EB,
		BIT_KPD_RTC_EB		|
		BIT_KPD_EB		|
		BIT_EFUSE_EB		|
		0);

	sci_glb_clr(REG_AON_APB_APB_EB0,
		BIT_AUDIF_EB			|
		BIT_VBC_EB			|

		BIT_PWM1_EB			|
		0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
		BIT_AUX1_EB			|
		BIT_AUX0_EB			|
		0);

	printk("sc clock module early init ok\n");
	return 0;
}

static inline int __sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}
const struct of_device_id of_sprd_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "sprd,adi-bus", },
	{}
};

static struct of_dev_auxdata of_sprd_default_bus_lookup[] = {
	{ .compatible = "sprd,sprd_backlight", .name = "sprd_backlight" },
#if (defined(CONFIG_BACKLIGHT_SPRD_PWM) \
	|| defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
	{ .compatible = "sprd,sprd_pwm_bl",  .name = "sprd_pwm_bl" },
#endif
#if (defined(CONFIG_KEYBOARD_SC) \
	|| defined(CONFIG_KEYBOARD_SC_MODULE))
	{.compatible = "sprd,sci-keypad", .name = "sci-keypad" },
#endif
#if (defined(CONFIG_KEYBOARD_GPIO) \
	|| defined(CONFIG_KEYBOARD_GPIO_MODULE))
	{.compatible = "gpio-keys", .name = "gpio-keys" },
#endif
#if (defined(CONFIG_KEYBOARD_SPRD_EIC) \
	|| defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
	{.compatible = "sprd,sprd-eic-keys", .name = "sprd-eic-keys" },
#endif
#if (defined(CONFIG_INPUT_HEADSET_SPRD_SC2723) \
	|| defined(CONFIG_INPUT_HEADSET_SPRD_SC2723_MODULE))
	{ .compatible = "sprd,headset_sprd_sc2723",  .name = "headset_sprd_sc2723" },
#endif
	 {}
};

struct iotable_sprd io_addr_sprd;
EXPORT_SYMBOL(io_addr_sprd);

int iotable_build()
{
	struct device_node *np;
	struct resource res;

#define ADD_SPRD_DEVICE(compat, id)			\
do {							\
	np = of_find_compatible_node(NULL, NULL, compat);\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr =			\
		ioremap_nocache(res.start, io_addr_sprd.id.length);\
		pr_debug("sprd io map: phys=%08x virt=%08x size=%08x\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)
#define ADD_SPRD_DEVICE_BY_NAME(name, id)		\
do {							\
	np = of_find_node_by_name(NULL, name);		\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr =			\
		ioremap_nocache(res.start, io_addr_sprd.id.length);\
		pr_debug("sprd io map: phys=%16lx virt=%16lx size=%16lx\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)
	ADD_SPRD_DEVICE("sprd,ahb", AHB);
	ADD_SPRD_DEVICE("sprd,aonapb", AONAPB);
	ADD_SPRD_DEVICE("sprd,aonckg", AONCKG);
	ADD_SPRD_DEVICE("sprd,apbreg", APBREG);
	ADD_SPRD_DEVICE("sprd,core", CORE);
	ADD_SPRD_DEVICE("sprd,mmahb", MMAHB);
	ADD_SPRD_DEVICE("sprd,pmu", PMU);
	ADD_SPRD_DEVICE("sprd,mmckg", MMCKG);
	ADD_SPRD_DEVICE("sprd,gpuapb", GPUAPB);
	ADD_SPRD_DEVICE("sprd,apbckg", APBCKG);
	ADD_SPRD_DEVICE("sprd,gpuckg", GPUCKG);
	ADD_SPRD_DEVICE("sprd,int", INT);
	ADD_SPRD_DEVICE("sprd,intc0", INTC0);
	ADD_SPRD_DEVICE("sprd,intc1", INTC1);
	ADD_SPRD_DEVICE("sprd,intc2", INTC2);
	ADD_SPRD_DEVICE("sprd,intc3", INTC3);
	ADD_SPRD_DEVICE("sprd,uidefuse", UIDEFUSE);
	ADD_SPRD_DEVICE("sprd,isp", ISP);
	ADD_SPRD_DEVICE("sprd,ca7wdg", CA7WDG);
	ADD_SPRD_DEVICE("sprd,csi2", CSI2);
	ADD_SPRD_DEVICE("sprd,d-eic-gpio", EIC);
	ADD_SPRD_DEVICE("sprd,wdg", WDG);
	ADD_SPRD_DEVICE("sprd,ipi", IPI);
	ADD_SPRD_DEVICE("sprd,dcam", DCAM);
	ADD_SPRD_DEVICE("sprd,syscnt", SYSCNT);
	ADD_SPRD_DEVICE("sprd,dma0", DMA0);
	ADD_SPRD_DEVICE("sprd,pub", PUB);
	ADD_SPRD_DEVICE("sprd,pin", PIN);
	ADD_SPRD_DEVICE("sprd,d-gpio-gpio", GPIO);
	ADD_SPRD_DEVICE("sprd,codecahb", CODECAHB);
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock0", HWSPINLOCK0);
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock1", HWSPINLOCK1);

	return 0;
}

static void __init sc8830_init_machine(void)
{
	printk("sci get chip id = 0x%x\n", __sci_get_chip_id());

	/* sci_adc_init((void __iomem *)ADC_BASE); */
	sci_adc_init();
	sci_regulator_init();
	of_sprd_default_bus_lookup[0].phys_addr = 0x20300000;
	of_sprd_default_bus_lookup[1].phys_addr = 0x20400000;
	of_sprd_default_bus_lookup[2].phys_addr = 0x20500000;
	of_sprd_default_bus_lookup[3].phys_addr = 0x20600000;
	of_platform_populate(NULL, of_sprd_default_bus_match_table, of_sprd_default_bus_lookup, NULL);
	/* sprd_sr2351_vddwpa_ctrl_power_register(); */
}

const struct of_device_id of_sprd_late_bus_match_table[] = {
	{ .compatible = "sprd,sound", },
	{}
};

static void __init sc8830_init_late(void)
{
	of_platform_populate(of_find_node_by_path("/sprd-audio-devices"),
				of_sprd_late_bus_match_table, NULL, NULL);
}

extern void __init sci_enable_timer_early(void);
extern void __init sc_init_chip_id(void);

void __init sprd_init_before_irq(void)
{
	iotable_build();
	sc_init_chip_id();
	/* earlier init request than irq and timer */
	__clock_init_early();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}
static void __init sc8830_pmu_init(void)
{
	__raw_writel(__raw_readl(REG_PMU_APB_PD_MM_TOP_CFG)
		& ~(BIT_PD_MM_TOP_FORCE_SHUTDOWN),
		REG_PMU_APB_PD_MM_TOP_CFG);

	__raw_writel(__raw_readl(REG_PMU_APB_PD_GPU_TOP_CFG)
		& ~(BIT_PD_GPU_TOP_FORCE_SHUTDOWN),
		REG_PMU_APB_PD_GPU_TOP_CFG);

	__raw_writel(__raw_readl(REG_AON_APB_APB_EB0) | BIT_MM_EB |
		BIT_GPU_EB, REG_AON_APB_APB_EB0);

	__raw_writel(__raw_readl(REG_AON_APB_APB_EB1) | BIT_CODEC_EB, REG_AON_APB_APB_EB1);
	__raw_writel(__raw_readl(REG_MM_AHB_AHB_EB) | BIT_MM_CKG_EB,
		REG_MM_AHB_AHB_EB);
	__raw_writel(__raw_readl(REG_MM_AHB_GEN_CKG_CFG)
		| BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN,
		REG_MM_AHB_GEN_CKG_CFG);
	__raw_writel(__raw_readl(REG_MM_CLK_MM_AHB_CFG) | 0x3,
		REG_MM_CLK_MM_AHB_CFG);
}

static void sprd_init_time(void)
{
	if (of_have_populated_dt()) {
		sc8830_pmu_init();
		of_clk_init(NULL);
		clocksource_of_init();
	} else{
		sci_clock_init();
		sci_enable_timer_early();
		sci_timer_init();
	}
}
static const char *sprd_boards_compat[] __initdata = {
	"sprd,sp8835eb",
	NULL,
};
extern struct smp_operations sprd_smp_ops;

MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_irq	= sci_init_irq,
	.init_time	= sprd_init_time,
	.init_machine	= sc8830_init_machine,
	.init_late	= sc8830_init_late,
	.dt_compat	= sprd_boards_compat,
MACHINE_END

