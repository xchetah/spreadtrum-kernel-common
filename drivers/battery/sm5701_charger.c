/*
 *  SM5701_charger.c
 *  SiliconMitus SM5701 Charger Driver
 *
 *  Copyright (C) 2014 SiliconMitus
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mfd/sm5701_core.h>
#include <linux/battery/sec_charger.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/gpio-pxa.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

extern int sec_chg_dt_init(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata);
extern led_state_charger;

static enum power_supply_property sec_charger_props[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_CHARGE_TYPE,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_CURRENT_MAX,
        POWER_SUPPLY_PROP_CURRENT_AVG,
        POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void SM5701_charger_initialize(struct SM5701_charger_data *charger);

static int SM5701_get_battery_present(struct SM5701_charger_data *charger)
{
	u8 data;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_STATUS1, &data);

	pr_info("%s: SM5701_STATUS1 (0x%02x)\n", __func__, data);

	data = ((data & SM5701_STATUS1_NOBAT) >> SM5701_STATUS1_NOBAT_SHIFT);

	return !data;
}

static int SM5701_get_charging_status(struct SM5701_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int nCHG;
	u8 stat2, chg_en, cln;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_STATUS2, &stat2);
	pr_info("%s : SM5701_STATUS2 : 0x%02x\n", __func__, stat2);

//	Clear interrupt register 2
	SM5701_reg_read(charger->SM5701->i2c, SM5701_INT2, &cln);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CNTL, &chg_en);
	chg_en &= SM5701_CNTL_OPERATIONMODE;

	nCHG = gpio_get_value(charger->pdata->chg_gpio_en);

	if((stat2 & SM5701_STATUS2_DONE) || (stat2 & SM5701_STATUS2_TOPOFF)) {
		status = POWER_SUPPLY_STATUS_FULL;
		charger->is_fullcharged = true;
		pr_info("%s : Status, Power Supply Full \n", __func__);
	} else {
		if (nCHG)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	}

	return (int)status;
}

static int SM5701_get_charging_health(struct SM5701_charger_data *charger)
{
	static int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 stat1, cln;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_STATUS1, &stat1);

	//	Clear interrupt register 1
	SM5701_reg_read(charger->SM5701->i2c, SM5701_INT1, &cln);

	pr_info("%s : Health, SM5701_STATUS1 : 0x%02x\n", __func__, stat1);

	if (stat1 & SM5701_STATUS1_VBUSOVP) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (stat1 & SM5701_STATUS1_VBUSUVLO) {
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
			health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	} else if (stat1 & SM5701_INT1_VBUSOK) {
		health = POWER_SUPPLY_HEALTH_GOOD;
	}

	return (int)health;
}

static u8 SM5701_set_batreg_voltage(
		struct SM5701_charger_data *charger, int batreg_voltage)
{
	u8 data = 0;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL3, &data);

	data &= ~BATREG_MASK;

	if (charger->dev_id < 3) {
		if ((batreg_voltage*10) < 40125)
			data = 0x00;
		else if ((batreg_voltage*10) > 44000)
			batreg_voltage = 4400;
		else
			data = ((batreg_voltage*10 - 40125) / 125);
	} else {
		if (batreg_voltage <= 3725)
			data = 0x0;
		else if (batreg_voltage >= 4400)
			data = 0x1F;
		else if (batreg_voltage <= 4300)
			data = (batreg_voltage - 3725) / 25;
		else
			data = (batreg_voltage - 4300) * 10 / 125 + 23;
	}

	SM5701_reg_write(charger->SM5701->i2c, SM5701_CHGCNTL3, data);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL3, &data);
	pr_info("%s : SM5701_CHGCNTL3 (Battery regulation voltage) : 0x%02x\n",
		__func__, data);

	return data;
}

static u8 SM5701_set_vbuslimit_current(
		struct SM5701_charger_data *charger, int input_current)
{
	u8 data = 0, temp = 0;
	SM5701_reg_read(charger->SM5701->i2c, SM5701_VBUSCNTL, &data);
	data &= ~SM5701_VBUSCNTL_VBUSLIMIT;

	if (input_current >= 1200)
		input_current = 1200;

	if(input_current <= 100)
		data &= ~SM5701_VBUSCNTL_VBUSLIMIT;
	else if(input_current <= 500)
		data |= 0x01;
	else {
		temp = (input_current/100)-5;
		data |= temp;
	}

	SM5701_reg_write(charger->SM5701->i2c, SM5701_VBUSCNTL, data);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_VBUSCNTL, &data);
	pr_info("%s : SM5701_VBUSCNTL (Input current limit) : 0x%02x\n",
		__func__, data);

	return data;
}

static u8 SM5701_set_topoff(
		struct SM5701_charger_data *charger, int topoff_current)
{
	u8 data = 0;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL1, &data);

	data &= ~SM5701_CHGCNTL1_TOPOFF;

	if(topoff_current < 100)
		topoff_current = 100;
	else if (topoff_current > 475)
		topoff_current = 475;

	data |= (topoff_current - 100) / 25;

	SM5701_reg_write(charger->SM5701->i2c, SM5701_CHGCNTL1, data);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL1, &data);
	pr_info("%s : SM5701_CHGCNTL1 (Top-off current threshold) : 0x%02x\n",
		__func__, data);

	return data;
}

static u8 SM5701_set_fastchg_current(
		struct SM5701_charger_data *charger, int fast_charging_current)
{
	u8 data = 0;

	if(fast_charging_current < 100)
		fast_charging_current = 100;
	else if (fast_charging_current > 1600)
		fast_charging_current = 1600;

	data = (fast_charging_current - 100) / 25;

	SM5701_reg_write(charger->SM5701->i2c, SM5701_CHGCNTL2, data);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL2, &data);
	pr_info("%s : SM5701_CHGCNTL2 (fastchg current) : 0x%02x\n",
		__func__, data);

	return data;
}

static int SM5701_get_charging_current(
	struct SM5701_charger_data *charger)
{
	u8 data = 0;
	int get_current = 0;

	if (charger->charging_current) {
		SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL2, &data);
		data &= SM5701_CHGCNTL2_FASTCHG;
		charger->charging_curr_step = 25;
		get_current = (100 + (data * charger->charging_curr_step));
		if (get_current <= 1600)
			get_current = (100 + (data * charger->charging_curr_step));
	} else {
		get_current = 100;
	}

	get_current = (100 + (data * charger->charging_curr_step));

	pr_debug("%s : Charging current : %dmA\n", __func__, get_current);
	return get_current;
}

static u8 SM5701_toggle_charger(struct SM5701_charger_data *charger, int enable)
{
	u8 chg_en = 0, mask = 0;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_CNTL, &chg_en);

	mask = charger->dev_id < 3 ? OP_MODE_CHG_ON : OP_MODE_CHG_ON_REV3;
	chg_en &= ~mask;

	if ((led_state_charger == LED_DISABLE) || (charger->dev_id != 4)) {
		if (enable)
			chg_en |= mask;

		SM5701_reg_write(charger->SM5701->i2c, SM5701_CNTL, chg_en);
		gpio_direction_output((charger->pdata->chg_gpio_en), !enable);

		pr_info("%s: SM5701 Charger toggled!! \n", __func__);

		SM5701_reg_read(charger->SM5701->i2c, SM5701_CNTL, &chg_en);
		pr_info("%s : CNTL register (0x09) : 0x%02x\n", __func__, chg_en);

	} else {
		 SM5701_set_operationmode(SM5701_OPERATIONMODE_FLASH_ON);
		pr_info("%s: SM5701 Charger toggled!! - flash on!! \n", __func__);
		gpio_direction_output((charger->pdata->chg_gpio_en), !enable);
	}

	return chg_en;
}

static void SM5701_isr_work(struct work_struct *work)
{
	union power_supply_propval val, value;
	struct SM5701_charger_data *charger =
		container_of(work, struct SM5701_charger_data, isr_work.work);;
	int full_check_type;

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_CHARGE_NOW, val);
	if (val.intval == SEC_BATTERY_CHARGING_1ST)
		full_check_type = charger->pdata->full_check_type;
	else
		full_check_type = charger->pdata->full_check_type_2nd;

	if (full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT) {
		val.intval = SM5701_get_charging_status(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_FULL:
			pr_err("%s: Interrupted by Full\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_STATUS, val);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
		SEC_BATTERY_OVP_UVLO_CHGINT) {
		val.intval = SM5701_get_charging_health(charger);

		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		/* case POWER_SUPPLY_HEALTH_UNDERVOLTAGE: */
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
		/* case POWER_SUPPLY_HEALTH_UNDERVOLTAGE: */
			psy_do_property("battery", get,
				POWER_SUPPLY_PROP_ONLINE, value);

			if (value.intval != POWER_SUPPLY_TYPE_BATTERY) {
				pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_HEALTH, val);
			}
			break;
		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}

	val.intval = SM5701_get_battery_present(charger);
	pr_info("%s: battery status : %d\n", __func__, val.intval);
	if (!val.intval)
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_PRESENT, val);
}

static irqreturn_t SM5701_irq_thread(int irq, void *irq_data)
{
	struct SM5701_charger_data *charger = irq_data;
	pr_info("*** %s ***\n", __func__);
	// Temporary delay of the interrupt to prevent MUIC interrupt running after it
	schedule_delayed_work(&charger->isr_work, HZ * 0.25);
	return IRQ_HANDLED;
}

static int SM5701_debugfs_show(struct seq_file *s, void *data)
{
	struct SM5701_charger_data *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SM5701 CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = SM5701_INTMASK1; reg <= SM5701_FLEDCNTL6; reg++) {
		SM5701_reg_read(charger->SM5701->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int SM5701_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, SM5701_debugfs_show, inode->i_private);
}

static const struct file_operations SM5701_debugfs_fops = {
	.open           = SM5701_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void SM5701_charger_initialize(struct SM5701_charger_data *charger)
{
	u8 reg_data = 0, status1 = 0;

	SM5701_reg_read(charger->SM5701->i2c,
			SM5701_DEVICE_ID, &charger->dev_id);
	pr_info("%s: SM5701 Charger init, CHIP REV : %2d !! \n",
			__func__, charger->dev_id);
	charger->is_fullcharged = false;

	SM5701_reg_read(charger->SM5701->i2c, SM5701_VBUSCNTL, &reg_data);
	reg_data &= ~SM5701_VBUSCNTL_AICLEN;
	SM5701_reg_write(charger->SM5701->i2c, SM5701_VBUSCNTL, reg_data);

	SM5701_reg_read(charger->SM5701->i2c, SM5701_STATUS1, &status1);
	pr_info("%s : SM5701_STATUS1 : 0x%02x\n", __func__, status1);

/* NOBAT, Enable OVP, UVLO, VBUSOK interrupts */
	reg_data = 0x63;
	SM5701_reg_write(charger->SM5701->i2c, SM5701_INTMASK1, reg_data);

/* Mask CHGON, FASTTMROFFM */
	reg_data = 0xFC;
	SM5701_reg_write(charger->SM5701->i2c, SM5701_INTMASK2, reg_data);

/* Set OVPSEL to 6.35V
	SM5701_reg_read(charger->SM5701->i2c, SM5701_CNTL, &reg_data);
	reg_data |= 0x1A;
	SM5701_reg_write(charger->SM5701->i2c, SM5701_CNTL, reg_data);
*/

	/* Operating Frequency in PWM BUCK mode : 2.4KHz */
	SM5701_reg_read(charger->SM5701->i2c, SM5701_CNTL, &reg_data);
	reg_data &= ~0xC0;
	reg_data |= (FREQ_24 | 0x4);
	SM5701_reg_write(charger->SM5701->i2c, SM5701_CNTL, reg_data);

	/* Disable AUTOSTOP */
	SM5701_reg_read(charger->SM5701->i2c, SM5701_CHGCNTL1, &reg_data);
	reg_data &= ~SM5701_CHGCNTL1_AUTOSTOP;
	SM5701_reg_write(charger->SM5701->i2c, SM5701_CHGCNTL1, reg_data);

	(void) debugfs_create_file("SM5701_regs",
		S_IRUGO, NULL, (void *)charger, &SM5701_debugfs_fops);

	SM5701_test_read(charger->SM5701->i2c);
}

static int sec_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct SM5701_charger_data *charger =
		container_of(psy, struct SM5701_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		printk("[%s] got val->intval:%d\n",__func__, val->intval);
		if(val->intval != POWER_SUPPLY_TYPE_OTG) WARN_ON(1);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->is_fullcharged)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = SM5701_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = SM5701_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = SM5701_get_battery_present(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->charging_current_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = SM5701_get_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (charger->aicl_on)
		{
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
			pr_info("%s: slow-charging mode\n", __func__);
		}
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct SM5701_charger_data *charger =
		container_of(psy, struct SM5701_charger_data, psy_chg);
	union power_supply_propval value;
//	int set_charging_current, set_charging_current_max;
//	const int usb_charging_current = charger->pdata->charging_current[
//		POWER_SUPPLY_TYPE_USB].fast_charging_current;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			/* Disable Charger */
			charger->is_charging = false;
			charger->nchgen = true;
			charger->aicl_on = false;
#if 0
			set_charging_current = 0;
			set_charging_current_max =
				charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].input_current_limit;
#endif
			pr_info("%s : Disable Charger, Battery Supply!\n", __func__);
			// nCHG_EN is logic low so set 1 to disable charger
			charger->is_fullcharged = false;
		} else if(val->intval == POWER_SUPPLY_TYPE_OTG){
			charger->is_charging = false;
			charger->nchgen = true;
			charger->aicl_on = false;
			charger->is_fullcharged = false;
			charger->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			SM5701_set_bstout(SM5701_BSTOUT_5P0);
			SM5701_set_operationmode(SM5701_OPERATIONMODE_OTG_ON_FLASH_ON);
			SM5701_test_read(charger->SM5701->i2c);	

		} else {
			charger->is_charging = true;
			charger->nchgen = false;
			charger->charging_current_max =
					charger->pdata->charging_current
					[charger->cable_type].input_current_limit;
			charger->charging_current =
					charger->pdata->charging_current
					[charger->cable_type].fast_charging_current;
			/* decrease the charging current according to siop level */

		// If siop is used set charging current according to the following
#if 0
			set_charging_current =
				charger->charging_current * charger->siop_level / 100;
			if (set_charging_current > 0 &&
					set_charging_current < usb_charging_current)
				set_charging_current = usb_charging_current;

				set_charging_current_max =
					charger->charging_current_max;
#endif
		}
		SM5701_operation_mode_function_control();
		pr_info("%s : STATUS OF CHARGER ON(0)/OFF(1): %d\n", __func__, charger->nchgen);
		SM5701_toggle_charger(charger, charger->is_charging);

		/* if battery full, only disable charging  */
		if ((charger->status == POWER_SUPPLY_STATUS_CHARGING) ||
			(charger->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(charger->status == POWER_SUPPLY_STATUS_FULL) ||
			(value.intval == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ||
			(value.intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT)) {

			/* Set float voltage */
			pr_info("%s : float voltage (%dmV)\n",
				__func__, charger->pdata->chg_float_voltage);
			SM5701_set_batreg_voltage(
				charger, charger->pdata->chg_float_voltage);

			/* if battery is removed, put vbus current limit to minimum */
			if ((value.intval == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ||
			    (value.intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT))
				SM5701_set_vbuslimit_current(charger, 100);
			else {
				/* Set input current limit */
				pr_info("%s : vbus current limit (%dmA)\n",
					__func__, charger->pdata->charging_current
					[charger->cable_type].input_current_limit);

				SM5701_set_vbuslimit_current(
					charger, charger->pdata->charging_current
					[charger->cable_type].input_current_limit);
			}

			/* Set topoff current */
			pr_info("%s : topoff current (%dmA)\n",
				__func__, charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st);
			SM5701_set_topoff(
				charger, charger->pdata->charging_current[
					charger->cable_type].full_check_current_1st);

			/* Set fast charge current */
			pr_info("%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->siop_level);
			SM5701_set_fastchg_current(
				charger, charger->charging_current);
		}
		break;
	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		charger->charging_current_max = val->intval;
		break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		SM5701_set_fastchg_current(charger,
				val->intval);
		SM5701_set_vbuslimit_current(charger,
				val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id SM5701_charger_match_table[] = {
        { .compatible = "sm,sm5701-charger",},
        {},
};
#else
#define SM5701_charger_match_table NULL
#endif /* CONFIG_OF */

static int SM5701_charger_probe(struct platform_device *pdev)
{
	struct SM5701_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct SM5701_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct SM5701_charger_data *charger;
	int ret = 0;

	pr_info("%s: SM5701 Charger Probe Start\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.parent->of_node) {
		pdev->dev.of_node = of_find_compatible_node(
			of_node_get(pdev->dev.parent->of_node), NULL,
			SM5701_charger_match_table[0].compatible);
    }

	charger->SM5701 = iodev;
	charger->SM5701->dev = &pdev->dev;

    if (pdev->dev.of_node) {
		charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
		if (!charger->pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = sec_chg_dt_init(pdev->dev.of_node, &pdev->dev, charger->pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else
		charger->pdata = pdata->charger_data;

	platform_set_drvdata(pdev, charger);

    if (charger->pdata->charger_name == NULL)
            charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	charger->siop_level = 100;
	SM5701_charger_initialize(charger);

	charger->input_curr_limit_step = 500;
	charger->charging_curr_step= 25;
    ret = power_supply_register(&pdev->dev, &charger->psy_chg);
    if (ret) {
            pr_err("%s: Failed to Register psy_chg\n", __func__);
            goto err_power_supply_register;
    }

    SM5701_set_charger_data(charger);
	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, SM5701_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, SM5701_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				/* charger->pdata->chg_irq_attr, */
				"charger-irq", charger);
		if (ret) {
			dev_err(&pdev->dev,
					"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_request_irq;
		}

		ret = enable_irq_wake(charger->pdata->chg_irq);
		if (ret < 0)
			dev_err(&pdev->dev,
					"%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
	}
	pr_info("%s: SM5701 Charger Probe Loaded\n", __func__);
	return 0;

err_request_irq:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
	pr_info("%s: err_power_supply_register error \n", __func__);
err_parse_dt:
	pr_info("%s: err_parse_dt error \n", __func__);
err_parse_dt_nomem:
	pr_info("%s: err_parse_dt_nomem error \n", __func__);
	kfree(charger);
	return ret;
}

static int SM5701_charger_remove(struct platform_device *pdev)
{
	struct SM5701_charger_data *charger =
				platform_get_drvdata(pdev);
	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

bool SM5701_charger_suspend(struct SM5701_charger_data *charger)
{
	pr_info("%s: CHARGER - SM5701(suspend mode)!!\n", __func__);
	return true;
}

bool SM5701_charger_resume(struct SM5701_charger_data *charger)
{
	pr_info("%s: CHARGER - SM5701(resume mode)!!\n", __func__);
	return true;
}

static void SM5701_charger_shutdown(struct device *dev)
{
	struct SM5701_charger_data *charger =
				dev_get_drvdata(dev);

	pr_info("%s: SM5701 Charger driver shutdown\n", __func__);
	if (!charger->SM5701->i2c) {
		pr_err("%s: no SM5701 i2c client\n", __func__);
		return;
	}
}

static const struct platform_device_id SM5701_charger_id[] = {
	{ "sm5701-charger", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, SM5701_charger_id);

static struct platform_driver SM5701_charger_driver = {
	.driver = {
		.name = "sm5701-charger",
		.owner = THIS_MODULE,
		.of_match_table = SM5701_charger_match_table,
		.shutdown = SM5701_charger_shutdown,
	},
	.probe = SM5701_charger_probe,
	.remove = SM5701_charger_remove,
	.id_table = SM5701_charger_id,
};

static int __init SM5701_charger_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&SM5701_charger_driver);
}

subsys_initcall(SM5701_charger_init);

static void __exit SM5701_charger_exit(void)
{
	platform_driver_unregister(&SM5701_charger_driver);
}

module_exit(SM5701_charger_exit);

MODULE_DESCRIPTION("SILICONMITUS SM5701 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:SM5701_charger");

