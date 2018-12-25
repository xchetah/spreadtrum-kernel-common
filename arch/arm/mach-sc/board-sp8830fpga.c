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
#include <asm/mach-types.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>
#ifdef CONFIG_OF
#include <linux/of_platform.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#endif
#include <mach/hardware.h>
#include <linux/i2c.h>
#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
#include <linux/i2c/lis3dh.h>
#endif
#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
#include <linux/i2c/ltr_558als.h>
#endif
//#include <linux/i2c/ft53x6_ts.h>
//#include <linux/i2c/lis3dh.h>
//#include <linux/i2c/ltr_558als.h>
//#include <linux/akm8975.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/serial_sprd.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/pinmap.h>
#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
#include <linux/mpu.h>
#endif
#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
#include <linux/akm8975.h>
#endif
#include <linux/irq.h>

#include <mach/sci.h>
#include <mach/hardware.h>

#include <mach/sci_glb_regs.h>


#include "devices.h"

/* IRQ's for the multi sensor board */
#define MPUIRQ_GPIO 212
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
#include <linux/i2c/focaltech.h>
#endif

#if(defined(CONFIG_KEYBOARD_SC)||defined(CONFIG_KEYBOARD_SC_MODULE))
#include <linux/input/matrix_keypad.h>
#include <mach/kpd.h>
#endif
#if(defined(CONFIG_KEYBOARD_GPIO)||defined(CONFIG_KEYBOARD_GPIO_MODULE))
#include <linux/gpio_keys.h>
#endif
#if(defined(CONFIG_KEYBOARD_SPRD_EIC)||defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
#include <linux/sprd_eic_keys.h>
#endif
#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
#include <linux/sprd_pwm_bl.h>
#endif

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

#ifndef CONFIG_OF
#if(defined(CONFIG_KEYBOARD_SC)||defined(CONFIG_KEYBOARD_SC_MODULE))
#define CUSTOM_KEYPAD_ROWS          (SCI_ROW7 | SCI_ROW6 | SCI_ROW5 | SCI_ROW4 | SCI_ROW3 | SCI_ROW2 | SCI_ROW1 | SCI_ROW0)
#define CUSTOM_KEYPAD_COLS          (SCI_COL7 | SCI_COL6 | SCI_COL5 | SCI_COL4 | SCI_COL3 | SCI_COL2 | SCI_COL1 | SCI_COL0)
#define ROWS	(8)
#define COLS	(8)
static const unsigned int board_keymap[] = {
	KEY(0, 0, KEY_F1),

	KEY(0, 3, KEY_COFFEE),
	KEY(0, 2, KEY_QUESTION),
	KEY(2, 3, KEY_CONNECT),
	KEY(1, 2, KEY_SHOP),
	KEY(1, 1, KEY_PHONE),

	KEY(0, 1, KEY_DELETE),
	KEY(2, 2, KEY_PLAY),
	KEY(1, 0, KEY_PAGEUP),
	KEY(1, 3, KEY_PAGEDOWN),
	KEY(2, 0, KEY_EMAIL),
	KEY(2, 1, KEY_STOP),

	KEY(0, 7, KEY_KP1),
	KEY(0, 6, KEY_KP2),
	KEY(0, 5, KEY_KP3),
	KEY(1, 7, KEY_KP4),
	KEY(1, 6, KEY_KP5),
	KEY(1, 5, KEY_KP6),
	KEY(2, 7, KEY_KP7),
	KEY(2, 6, KEY_KP8),
	KEY(2, 5, KEY_KP9),
	KEY(3, 6, KEY_KP0),
	KEY(3, 7, KEY_KPASTERISK),
	KEY(3, 5, KEY_KPDOT),
	KEY(7, 2, KEY_NUMLOCK),
	KEY(7, 1, KEY_KPMINUS),
	KEY(6, 1, KEY_KPPLUS),
	KEY(7, 6, KEY_KPSLASH),
	KEY(6, 0, KEY_ENTER),

	KEY(7, 4, KEY_CAMERA),

	KEY(0, 4, KEY_F2),
	KEY(1, 4, KEY_F3),
	KEY(2, 4, KEY_F4),
	KEY(7, 7, KEY_F5),
	KEY(7, 5, KEY_F6),

	KEY(3, 4, KEY_Q),
	KEY(3, 3, KEY_W),
	KEY(3, 2, KEY_E),
	KEY(3, 1, KEY_R),
	KEY(3, 0, KEY_T),
	KEY(4, 7, KEY_Y),
	KEY(4, 6, KEY_U),
	KEY(4, 5, KEY_I),
	KEY(4, 4, KEY_O),
	KEY(4, 3, KEY_P),
	KEY(4, 2, KEY_A),
	KEY(4, 1, KEY_S),
	KEY(4, 0, KEY_D),
	KEY(5, 7, KEY_F),
	KEY(5, 6, KEY_G),
	KEY(5, 5, KEY_H),
	KEY(5, 4, KEY_J),
	KEY(5, 3, KEY_K),
	KEY(5, 2, KEY_L),
	KEY(5, 1, KEY_Z),
	KEY(5, 0, KEY_X),
	KEY(6, 7, KEY_C),
	KEY(6, 6, KEY_V),
	KEY(6, 5, KEY_B),
	KEY(6, 4, KEY_N),
	KEY(6, 3, KEY_M),
	KEY(6, 2, KEY_SPACE),
	KEY(7, 0, KEY_LEFTSHIFT),
	KEY(7, 3, KEY_LEFTCTRL),
};

static const struct matrix_keymap_data customize_keymap = {
	.keymap = board_keymap,
	.keymap_size = ARRAY_SIZE(board_keymap),
};

static struct sci_keypad_platform_data sci_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
	.debounce_time = 5000,
};
#endif

#if(defined(CONFIG_KEYBOARD_GPIO)||defined(CONFIG_KEYBOARD_GPIO_MODULE))
static struct gpio_keys_button gpio_keys_button[] = {
    {
        .code = KEY_VOLUMEDOWN,
        .type = EV_KEY,
        .gpio = GPIO_KEY_VOLUMEDOWN,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_volumedown",
    },
    {
        .code = KEY_VOLUMEUP,
        .type = EV_KEY,
        .gpio = GPIO_KEY_VOLUMEUP,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_volumeup",
    },
#if 0
    {
        .code = KEY_CAMERA,
        .type = EV_KEY,
        .gpio = GPIO_KEY_CAMERA,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_camera",
    },
    {
        .code = KEY_HOME,
        .type = EV_KEY,
        .gpio = GPIO_KEY_HOME,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_home",
    },
#endif
};

static struct gpio_keys_platform_data gpio_keys_platform_data = {
    .buttons = gpio_keys_button,
    .nbuttons = ARRAY_SIZE(gpio_keys_button),
    .rep = 0,
};
#endif

#if(defined(CONFIG_KEYBOARD_SPRD_EIC)||defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
static struct sprd_eic_keys_button sprd_eic_keys_button[] = {
    {
        .code = KEY_POWER,
        .type = EV_KEY,
        .gpio = EIC_KEY_POWER,
        .active_low = 0,
        .wakeup = 1,
        .debounce_interval = 50, /*Note: Hardware debounce (ms) */
        .desc = "key_power",
    },
#if 0
    {
        .code = KEY_VOLUMEDOWN,
        .type = EV_KEY,
        .gpio = GPIO_KEY_VOLUMEDOWN,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_volumedown",
    },
    {
        .code = KEY_VOLUMEUP,
        .type = EV_KEY,
        .gpio = GPIO_KEY_VOLUMEUP,
        .active_low = 1,
        .wakeup = 1,
        .debounce_interval = 5, /* ms */
        .desc = "key_volumeup",
    },
#endif
};

static struct sprd_eic_keys_platform_data sprd_eic_keys_platform_data = {
    .buttons = sprd_eic_keys_button,
    .nbuttons = ARRAY_SIZE(sprd_eic_keys_button),
    .rep = 0,
};
#endif

#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
static struct sprd_pwm_bl_platform_data sprd_pwm_bl_platform_data = {
	.brightness_max = 255,
	.brightness_min = 0,
	.pwm_index = 2,
	.gpio_ctrl_pin = -1,
	.gpio_active_level = 0,
};
#endif

static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_eic_gpio_device,
	&sprd_nand_device,
	&sprd_lcd_device0,
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&sprd_ram_console,
#endif
	&sprd_backlight_device,
#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
	&sprd_pwm_bl_device,
#endif
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_spi2_device,
#if(defined(CONFIG_KEYBOARD_SC)||defined(CONFIG_KEYBOARD_SC_MODULE))
	&sprd_keypad_device,
#endif
#if(defined(CONFIG_KEYBOARD_GPIO)||defined(CONFIG_KEYBOARD_GPIO_MODULE))
	&sprd_gpio_keys_device,
#endif
#if(defined(CONFIG_KEYBOARD_SPRD_EIC)||defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
	&sprd_eic_keys_device,
#endif
	&sprd_audio_platform_pcm_device,
#ifndef CONFIG_MACH_SPX35LFPGA
	&sprd_audio_cpu_dai_vaudio_device,
	&sprd_audio_cpu_dai_vbc_device,
	&sprd_audio_codec_sprd_codec_device,
	&sprd_audio_cpu_dai_i2s_device,
	&sprd_audio_cpu_dai_i2s_device1,
	&sprd_audio_cpu_dai_i2s_device2,
	&sprd_audio_cpu_dai_i2s_device3,
	&sprd_audio_codec_null_codec_device,
	&sprd_battery_device,
#endif
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
#if defined(CONFIG_SPRD_IOMMU)
//	&sprd_iommu_gsp_device,
	&sprd_iommu_mm_device,
#endif
	&sprd_emmc_device,
	&sprd_sdio0_device,
#ifndef CONFIG_MACH_SPX35LFPGA
	&sprd_sdio1_device,
	&sprd_sdio2_device,
#endif
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&sprd_sensor_device,
	&sprd_isp_device,
	&sprd_vsp_device,
	&sprd_jpg_device,
#if 0
	&sprd_ahb_bm0_device,
	&sprd_ahb_bm1_device,
	&sprd_ahb_bm2_device,
	&sprd_ahb_bm3_device,
	&sprd_ahb_bm4_device,
	&sprd_axi_bm0_device,
	&sprd_axi_bm1_device,
	&sprd_axi_bm2_device,
#endif
#if 0
	&rfkill_device,
	&brcm_bluesleep_device,
#endif
#ifdef CONFIG_SIPC_TD
	&sprd_cproc_td_device,
	&sprd_spipe_td_device,
	&sprd_slog_td_device,
	&sprd_stty_td_device,
	&sprd_seth0_td_device,
	&sprd_seth1_td_device,
	&sprd_seth2_td_device,
#endif
#ifdef CONFIG_SIPC_WCDMA
	&sprd_cproc_wcdma_device,
	&sprd_spipe_wcdma_device,
	&sprd_slog_wcdma_device,
	&sprd_stty_wcdma_device,
	&sprd_seth0_wcdma_device,
	&sprd_seth1_wcdma_device,
	&sprd_seth2_wcdma_device,
#endif
#ifdef CONFIG_SIPC_GGE
        &sprd_cproc_cp0_device,
        &sprd_spipe_gge_device,
	&sprd_slog_gge_device,
	&sprd_stty_gge_device,
	&sprd_seth0_gge_device,
	&sprd_seth1_gge_device,
	&sprd_seth2_gge_device,
#endif
#ifdef CONFIG_SIPC_LTE
        &sprd_cproc_cp1_device,
        &sprd_spipe_lte_device,
	&sprd_slog_lte_device,
	&sprd_stty_lte_device,
	&sprd_seth0_lte_device,
	&sprd_seth1_lte_device,
	&sprd_seth2_lte_device,
#endif
	&kb_backlight_device,
	&sprd_a7_pmu_device,
	&sprd_memnand_system_device,
	&sprd_memnand_userdata_device,
	&sprd_memnand_cache_device,
};
#if 0
/* BT suspend/resume */
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= GPIO_BT2AP_WAKE,
		.end	= GPIO_BT2AP_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= GPIO_AP2BT_WAKE,
		.end	= GPIO_AP2BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static struct resource rfkill_resources[] = {
	{
		.name   = "bt_power",
		.start  = GPIO_BT_POWER,
		.end    = GPIO_BT_POWER,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "bt_reset",
		.start  = GPIO_BT_RESET,
		.end    = GPIO_BT_RESET,
		.flags  = IORESOURCE_IO,
	},
};
static struct platform_device rfkill_device = {
	.name = "rfkill",
	.id = -1,
	.num_resources	= ARRAY_SIZE(rfkill_resources),
	.resource	= rfkill_resources,
};
#endif

/* keypad backlight */
static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
};
#endif /* CONFIG_OF */

static int calibration_mode = false;
static int __init calibration_start(char *str)
{
	int calibration_device =0;
	int mode=0,freq=0,device=0;
	if(str){
		pr_info("modem calibartion:%s\n", str);
		sscanf(str, "%d,%d,%d", &mode,&freq,&device);
	}
	if(device & 0x80){
		calibration_device = device & 0xf0;
		calibration_mode = true;
		pr_info("calibration device = 0x%x\n",calibration_device);
	}
	return 1;
}
__setup("calibration=", calibration_start);

int in_calibration(void)
{
	return (calibration_mode == true);
}

EXPORT_SYMBOL(in_calibration);

static void __init sprd_add_otg_device(void)
{
	/*
	 * if in calibrtaion mode, we do nothing, modem will handle everything
	 */
	platform_device_register(&sprd_otg_device);
}

static struct serial_data plat_data0 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data3 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
static struct ft5x0x_ts_platform_data ft5x0x_ts_info = { 
	.irq_gpio_number    = GPIO_TOUCH_IRQ,
	.reset_gpio_number  = GPIO_TOUCH_RESET,
	.vdd_name           = "vdd28",
};
#endif

#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
static struct ltr558_pls_platform_data ltr558_pls_info = {
	.irq_gpio_number	= GPIO_PROX_INT,
};
#endif

#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
static struct lis3dh_acc_platform_data lis3dh_plat_data = {
	.poll_interval = 10,
	.min_interval = 10,
	.g_range = LIS3DH_ACC_G_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 1
};
#endif

#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
struct akm8975_platform_data akm8975_platform_d = {
	.mag_low_x = -20480,
	.mag_high_x = 20479,
	.mag_low_y = -20480,
	.mag_high_y = 20479,
	.mag_low_z = -20480,
	.mag_high_z = 20479,
};
#endif

#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
static struct mpu_platform_data mpu9150_platform_data = {
	.int_config = 0x00,
	.level_shifter = 0,
	.orientation = { -1, 0, 0,
					  0, +1, 0,
					  0, 0, -1 },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_COMPASS,
	.sec_slave_id = COMPASS_ID_AK8963,
	.secondary_i2c_addr = 0x0C,
	.secondary_orientation = { 0, -1, 0,
					1, 0, 0,
					0, 0, 1 },
	.key = {0xec, 0x06, 0x17, 0xdf, 0x77, 0xfc, 0xe6, 0xac,
			0x7b, 0x6f, 0x12, 0x8a, 0x1d, 0x63, 0x67, 0x37},
};
#endif

static struct i2c_board_info i2c2_boardinfo[] = {
#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
	{ I2C_BOARD_INFO(LIS3DH_ACC_I2C_NAME, LIS3DH_ACC_I2C_ADDR),
	  .platform_data = &lis3dh_plat_data,
	},
#endif
#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
	{ I2C_BOARD_INFO("mpu9150", 0x68),
	  .irq = GPIO_GYRO_INT1,
	  .platform_data = &mpu9150_platform_data,
	},
#endif
#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
	{ I2C_BOARD_INFO(LTR558_I2C_NAME,  LTR558_I2C_ADDR),
	  .platform_data = &ltr558_pls_info,
	},
#endif
#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
	{ I2C_BOARD_INFO(AKM8975_I2C_NAME, AKM8975_I2C_ADDR),
	  .platform_data = &akm8975_platform_d,
	},
#endif
};

static struct i2c_board_info i2c1_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

static struct i2c_board_info i2c0_boardinfo[] = {
	{
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
		I2C_BOARD_INFO(FOCALTECH_TS_NAME, FOCALTECH_TS_ADDR),
		.platform_data = &ft5x0x_ts_info,
#endif
	},
};

static int sc8810_add_i2c_devices(void)
{
#if 0
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
#endif
	i2c_register_board_info(0, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(1, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
	return 0;
}

struct platform_device audio_pa_amplifier_device = {
	.name = "speaker-pa",
	.id = -1,
};

static int audio_pa_amplifier_l(u32 cmd, void *data)
{
	int ret = 0;
	if (cmd < 0) {
		/* get speaker amplifier status : enabled or disabled */
		ret = 0;
	} else {
		/* set speaker amplifier */
	}
	return ret;
}
#if 0
/* Control ldo for maxscend cmmb chip according to HW design */
static struct regulator *cmmb_regulator_1v8 = NULL;

#define SPI_PIN_FUNC_MASK  (0x3<<4)
#define SPI_PIN_FUNC_DEF   (0x0<<4)
#define SPI_PIN_FUNC_GPIO  (0x3<<4)

struct spi_pin_desc {
	const char   *name;
	unsigned int pin_func;
	unsigned int reg;
	unsigned int gpio;
};

static struct spi_pin_desc spi_pin_group[] = {
	{"SPI_DI",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_DI   + CTL_PIN_BASE,  158},
	{"SPI_CLK", SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_CLK  + CTL_PIN_BASE,  159},
	{"SPI_DO",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_DO   + CTL_PIN_BASE,  157},
	{"SPI_CS0", SPI_PIN_FUNC_GPIO, REG_PIN_SPI0_CSN  + CTL_PIN_BASE,  156}
};
static void sprd_restore_spi_pin_cfg(void)
{
	unsigned int reg;
	unsigned int  gpio;
	unsigned int  pin_func;
	unsigned int value;
	unsigned long flags;
	int i = 0;
	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;
		 /* free the gpios that have request */
		 gpio_free(gpio);
		 local_irq_save(flags);
		 /* config pin default spi function */
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_DEF);
		 __raw_writel(value, reg);
		 local_irq_restore(flags);
	    }
	    else {
		 /* CS should config output */
		 gpio_direction_output(gpio, 1);
	    }
	}

}


static void sprd_set_spi_pin_input(void)
{
	unsigned int reg;
	unsigned int value;
	unsigned int  gpio;
	unsigned int  pin_func;
	const char    *name;
	unsigned long flags;
	int i = 0;

	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    name = spi_pin_group[i].name;

	    /* config pin GPIO function */
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;

		 local_irq_save(flags);
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_GPIO);
		 __raw_writel(value, reg);
		 local_irq_restore(flags);
		 if (gpio_request(gpio, name)) {
		     printk("smsspi: request gpio %d failed, pin %s\n", gpio, name);
		 }

	    }

	    gpio_direction_input(gpio);
	}

}


static void mxd_cmmb_poweron(void)
{
        regulator_set_voltage(cmmb_regulator_1v8, 1700000, 1800000);
        regulator_disable(cmmb_regulator_1v8);
        msleep(3);
        regulator_enable(cmmb_regulator_1v8);
        msleep(5);

        /* enable 26M external clock */
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 1);
}

static void mxd_cmmb_poweroff(void)
{
        regulator_disable(cmmb_regulator_1v8);
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
}

static int mxd_cmmb_init(void)
{
         int ret=0;
         ret = gpio_request(GPIO_CMMB_26M_CLK_EN,   "MXD_CMMB_CLKEN");
         if (ret)
         {
                   pr_debug("mxd spi req gpio clk en err!\n");
                   goto err_gpio_init;
         }
         gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
         cmmb_regulator_1v8 = regulator_get(NULL, "vddcmmb1p8");
         return 0;

err_gpio_init:
	 gpio_free(GPIO_CMMB_26M_CLK_EN);
         return ret;
}

static struct mxd_cmmb_026x_platform_data mxd_plat_data = {
	.poweron  = mxd_cmmb_poweron,
	.poweroff = mxd_cmmb_poweroff,
	.init     = mxd_cmmb_init,
	.set_spi_pin_input   = sprd_set_spi_pin_input,
	.restore_spi_pin_cfg = sprd_restore_spi_pin_cfg,
};

static int spi_cs_gpio_map[][2] = {
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
} ;

static struct spi_board_info spi_boardinfo[] = {
	{
	.modalias = "cmmb-dev",
	.bus_num = 0,
	.chip_select = 0,
	.max_speed_hz = 8 * 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
        .platform_data = &mxd_plat_data,
	},
	{
	.modalias = "spidev",
	.bus_num = 1,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	},
	{
	.modalias = "spidev",
	.bus_num = 2,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	}
};
#endif
static void sprd_spi_init(void)
{
#if 0
	int busnum, cs, gpio;
	int i;

	struct spi_board_info *info = spi_boardinfo;

	for (i = 0; i < ARRAY_SIZE(spi_boardinfo); i++) {
		busnum = info[i].bus_num;
		cs = info[i].chip_select;
		gpio   = spi_cs_gpio_map[busnum][cs];

		info[i].controller_data = (void *)gpio;
	}

        spi_register_board_info(info, ARRAY_SIZE(spi_boardinfo));
#endif
}

static int sc8810_add_misc_devices(void)
{
	if (0) {
		platform_set_drvdata(&audio_pa_amplifier_device, audio_pa_amplifier_l);
		if (platform_device_register(&audio_pa_amplifier_device))
			pr_err("faile to install audio_pa_amplifier_device\n");
	}
	return 0;
}

int __init sc8825_regulator_init(void)
{
	static struct platform_device sc8825_regulator_device = {
		.name 	= "sprd-regulator",
		.id	= -1,
	};
	return platform_device_register(&sc8825_regulator_device);
}

int __init __clock_init_early(void)
{
#if defined(CONFIG_ARCH_SCX15)
#else
	pr_info("ahb ctl0 %08x, ctl2 %0x8 glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AP_AHB_AHB_EB),
		sci_glb_raw_read(REG_AP_AHB_AHB_RST),
		sci_glb_raw_read(REG_AON_APB_APB_EB0),
		sci_glb_raw_read(REG_AON_APB_APB_EB1),
		sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));
#endif

	sci_glb_clr(REG_AP_AHB_AHB_EB,
		BIT_BUSMON2_EB		|
		BIT_BUSMON1_EB		|
		BIT_BUSMON0_EB		|
		BIT_SPINLOCK_EB		|
#if !defined(CONFIG_ARCH_SCX35L)
		BIT_GPS_EB		|
#endif
		BIT_EMMC_EB		|
		BIT_SDIO2_EB		|
		BIT_SDIO1_EB		|
		BIT_SDIO0_EB		|
		BIT_DRM_EB		|
		BIT_NFC_EB		|
		BIT_DMA_EB		|
		BIT_USB_EB		|
#if !defined(CONFIG_ARCH_SCX35L)
		BIT_GSP_EB		|
		BIT_DISPC1_EB		|
#endif
		BIT_DISPC0_EB		|
		BIT_DSI_EB		|
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
		BIT_PWM3_EB			|
		BIT_PWM1_EB			|
		0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
		BIT_AUX1_EB			|
		BIT_AUX0_EB			|
		0);
	printk("sc clock module early init ok\n");
	return 0;
}

static inline int	__sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}
#ifdef CONFIG_OF
const struct of_device_id of_sprd_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "sprd,adi-bus", },
	{}
};
#endif
#ifdef CONFIG_OF
static const struct of_dev_auxdata of_sprd_default_bus_lookup[] = {
	 { .compatible = "sprd,sdhci-shark",  .name = "sdio_sd", .phys_addr = SPRD_SDIO0_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sdio_wifi", .phys_addr = SPRD_SDIO1_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sprd-sdhci.2", .phys_addr = SPRD_SDIO2_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sdio_emmc", .phys_addr = SPRD_EMMC_BASE  },
	 { .compatible = "sprd,sprd_backlight",  .name = "sprd_backlight" },
#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
	{ .compatible = "sprd,sprd_pwm_bl",  .name = "sprd_pwm_bl" },
#endif
#if(defined(CONFIG_KEYBOARD_SC)||defined(CONFIG_KEYBOARD_SC_MODULE))
	{.compatible = "sprd,sci-keypad", .name = "sci-keypad" },
#endif
#if(defined(CONFIG_KEYBOARD_GPIO)||defined(CONFIG_KEYBOARD_GPIO_MODULE))
	{.compatible = "gpio-keys", .name = "gpio-keys" },
#endif
#if(defined(CONFIG_KEYBOARD_SPRD_EIC)||defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
	{.compatible = "sprd,sprd-eic-keys", .name = "sprd-eic-keys" },
#endif
	{}
};
#endif

static void __init sc8830_init_machine(void)
{
	printk("sci get chip id = 0x%x\n",__sci_get_chip_id());

	sci_adc_init((void __iomem *)ADC_BASE);
#ifndef CONFIG_MACH_SPX35LFPGA
	sci_regulator_init();
#endif
#ifndef CONFIG_OF
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
#if(defined(CONFIG_KEYBOARD_SC)||defined(CONFIG_KEYBOARD_SC_MODULE))
	platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data,sizeof(sci_keypad_data));
#endif
#if(defined(CONFIG_KEYBOARD_GPIO)||defined(CONFIG_KEYBOARD_GPIO_MODULE))
	platform_device_add_data(&sprd_gpio_keys_device,(const void*)&gpio_keys_platform_data,sizeof(gpio_keys_platform_data));
#endif
#if(defined(CONFIG_KEYBOARD_SPRD_EIC)||defined(CONFIG_KEYBOARD_SPRD_EIC_MODULE))
	platform_device_add_data(&sprd_eic_keys_device,(const void*)&sprd_eic_keys_platform_data,sizeof(sprd_eic_keys_platform_data));
#endif
#if(defined(CONFIG_BACKLIGHT_SPRD_PWM)||defined(CONFIG_BACKLIGHT_SPRD_PWM_MODULE))
	platform_device_add_data(&sprd_pwm_bl_device, (const void*)&sprd_pwm_bl_platform_data, sizeof(sprd_pwm_bl_platform_data));
#endif
	platform_add_devices(devices, ARRAY_SIZE(devices));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	sprd_spi_init();
#else
	of_platform_populate(NULL, of_sprd_default_bus_match_table, of_sprd_default_bus_lookup, NULL);
#endif
}

extern void __init  sci_enable_timer_early(void);
static void __init sc8830_init_early(void)
{
	/* earlier init request than irq and timer */
	__clock_init_early();
#ifndef CONFIG_OF
	sci_enable_timer_early();
#endif
	sci_adi_init();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}
#ifdef CONFIG_OF
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
	if(of_have_populated_dt()){
		sc8830_pmu_init();
		of_clk_init(NULL);
		clocksource_of_init();
	}else{
		sci_clock_init();
		sci_enable_timer_early();
		sci_timer_init();
	}
}
static const char *sprd_boards_compat[] __initdata = {
	"sprd,sp8835eb",
	NULL,
};
#endif
extern struct smp_operations sprd_smp_ops;

MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_early	= sc8830_init_early,
	.init_irq	= sci_init_irq,
#ifdef CONFIG_OF
	.init_time		= sprd_init_time,
#else
	.init_time		= sci_timer_init,
#endif
	.init_machine	= sc8830_init_machine,
#ifdef CONFIG_OF
	.dt_compat = sprd_boards_compat,
#endif
MACHINE_END

