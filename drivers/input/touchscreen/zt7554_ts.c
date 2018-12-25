/*
 *
 * Zinitix zt7554 touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */
#include "zt7554_ts.h"
#include "zt7554_ts_spec.h"

//#include "zinitix_touch_t560.h"
#include "zinitix_touch_zxt_firmware.h"
#include <mach/gpio.h>
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {KEY_MENU, KEY_BACK};

static int cal_mode;
static int get_boot_mode(char *str)
{

	get_option(&str, &cal_mode);
	printk(KERN_DEBUG "get_boot_mode, uart_mode : %d\n", cal_mode);
	return 1;
}
__setup("calmode=", get_boot_mode);

s32 read_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;
retry:
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to send. ret:%d, try:%d\n",
							__func__, ret, count + 1);
		mdelay(1);
		if (++count < I2C_RETRY_TIMES)
			goto retry;
		return ret;
	}

	udelay(DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to recv. ret:%d\n", __func__, ret);
		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 write_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;
	u8 pkt[10];

	pkt[0] = (reg) & 0xff;
	pkt[1] = (reg >> 8) & 0xff;
	memcpy((u8 *)&pkt[2], values, length);

retry:
	ret = i2c_master_send(client , pkt , length + 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to send. ret:%d, try:%d\n",
							__func__, ret, count + 1);
		mdelay(1);
		if (++count < I2C_RETRY_TIMES)
			goto retry;
		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;

	return I2C_SUCCESS;
}

s32 write_cmd(struct i2c_client *client, u16 reg)
{
	s32 ret;
	int count = 0;

retry:
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to send. ret:%d, try:%d\n",
							__func__, ret, count + 1);
		mdelay(1);
		if (++count < I2C_RETRY_TIMES)
			goto retry;
		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return I2C_SUCCESS;
}

static inline s32 read_raw_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;

retry:
	/* select register */
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to send. ret:%d, try:%d\n",
							__func__, ret, count + 1);
		mdelay(1);
		if (++count < I2C_RETRY_TIMES)
			goto retry;
		return ret;
	}

	/* for setup tx transaction. */
	udelay(200);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to recv. ret:%d\n", __func__, ret);
		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 read_firmware_data(struct i2c_client *client, u16 addr, u8 *values, u16 length)
{
	s32 ret;

	ret = i2c_master_send(client , (u8 *)&addr , 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to send. ret:%d\n", __func__, ret);
		return ret;
	}

	/* for setup tx transaction. */
	mdelay(1);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to recv. ret:%d\n", __func__, ret);
		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static void zt7554_set_optional_mode(struct zt7554_ts_info *info, bool force)
{
	if (m_prev_optional_mode == m_optional_mode && !force)
		return;

	if (write_reg(info->client, ZT7554_OPTIONAL_SETTING, m_optional_mode) == I2C_SUCCESS) {
		m_prev_optional_mode = m_optional_mode;
		dev_info(&misc_info->client->dev, "TA setting changed to %d\n",
								m_optional_mode & 0x1);
	}
}
static void zt7554_set_ta_status(struct zt7554_ts_info *info)
{
	if (ta_connected)
		zinitix_bit_set(m_optional_mode, 0);
	else
		zinitix_bit_clr(m_optional_mode, 0);
}

void tsp_charger_enable(int status)
{
	status = (status != POWER_SUPPLY_TYPE_BATTERY);

	if (status)
		ta_connected = true;
	else
		ta_connected = false;

	zt7554_set_ta_status(misc_info);

	dev_info(&misc_info->client->dev, "TA %s\n",
			status ? "connected" : "disconnected");
}
EXPORT_SYMBOL(tsp_charger_enable);

static bool get_raw_data(struct zt7554_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct zt7554_ts_platform_data *pdata = info->pdata;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;
	int i;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		dev_err(&client->dev, "%s: other process occupied.\n", __func__);
		enable_irq(info->irq);
		up(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int))
			msleep(1);
		write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
		msleep(1);
	}

	sz = total_node * 2;

	while (gpio_get_value(pdata->gpio_int))
		msleep(1);

	if (read_raw_data(client, ZT7554_RAWDATA_REG, (char *)buff, sz) < 0) {
		dev_err(&info->client->dev, "error : read zinitix tc raw data\n");
		info->work_state = NOTHING;
		enable_irq(info->irq);
		up(&info->work_lock);
		return false;
	}

	write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return true;
}

static bool ts_get_raw_data(struct zt7554_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;

	if (down_trylock(&info->raw_data_lock)) {
		dev_err(&client->dev, "Failed to occupy sema\n");
		info->touch_info.status = 0;
		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info);

	if (read_raw_data(info->client, ZT7554_RAWDATA_REG, (char *)info->cur_data, sz) < 0) {
		dev_err(&client->dev, "Failed to read raw data\n");
		up(&info->raw_data_lock);
		return false;
	}

	info->update = 1;
	memcpy((u8 *)(&info->touch_info),
			(u8 *)&info->cur_data[total_node], sizeof(struct point_info));
	up(&info->raw_data_lock);

	return true;
}

#if ZINITIX_I2C_CHECKSUM
#define ZINITIX_I2C_CHECKSUM_WCNT 0x016a
#define ZINITIX_I2C_CHECKSUM_RESULT 0x016c
static bool i2c_checksum(struct zt7554_ts_info *info, s16 *pChecksum, u16 wlength)
{
	s16 checksum_result;
	s16 checksum_cur;
	int i;

	checksum_cur = 0;
	for (i = 0; i < wlength; i++)
		checksum_cur += (s16)pChecksum[i];

	if (read_data(info->client, ZINITIX_I2C_CHECKSUM_RESULT, (u8 *)(&checksum_result), 2) < 0) {
		dev_err(&info->client->dev, "error read i2c checksum rsult.\n");
		return false;
	}
	if (checksum_cur != checksum_result) {
		dev_err(&info->client->dev, "checksum error : %d, %d\n", checksum_cur, checksum_result);
		return false;
	}
	return true;
}

#endif

static bool ts_read_coord(struct zt7554_ts_info *info)
{
	struct i2c_client *client = info->client;

	/* for  Debugging Tool */
	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (info->update == 0) {
			if (!ts_get_raw_data(info))
				return false;
		} else
			info->touch_info.status = 0;
		dev_err(&client->dev, "status = 0x%04X\n", info->touch_info.status);
		goto out;
	}

#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum) {
		if (write_reg(info->client, ZINITIX_I2C_CHECKSUM_WCNT,
					(sizeof(struct point_info)/2)) != I2C_SUCCESS) {
			dev_err(&client->dev, "error write checksum wcnt.-\n");
			return false;
		}
	}
#endif
	if (read_data(info->client, ZT7554_POINT_STATUS_REG,
			(u8 *)(&info->touch_info), sizeof(struct point_info)) < 0) {
		dev_err(&client->dev, "Failed to read point info\n");
		return false;
	}
#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum) {
		if (i2c_checksum(info, (s16 *)(&info->touch_info),
					sizeof(struct point_info)/2) == false)
			return false;
	}
#endif
	zt7554_set_optional_mode(info, false);
out:
	if (zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO)) {
		dev_err(&client->dev, "Invalid must zero bit(%04x)\n", info->touch_info.status);
		return false;
	}
	write_cmd(info->client, ZT7554_CLEAR_INT_STATUS_CMD);

	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(unsigned long data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)data;

	info->p_esd_timeout_tmr = NULL;
	queue_work(esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct zt7554_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock, flags);
	if (info->p_esd_timeout_tmr)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif
	info->p_esd_timeout_tmr = NULL;
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->esd_timeout_tmr.expires = jiffies + (HZ * sec);
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	add_timer(&info->esd_timeout_tmr);
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_stop(struct zt7554_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock, flags);
	if (info->p_esd_timeout_tmr)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
		del_timer(info->p_esd_timeout_tmr);
#endif

	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_init(struct zt7554_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock, flags);
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void ts_tmr_work(struct work_struct *work)
{
	struct zt7554_ts_info *info =
				container_of(work, struct zt7554_ts_info, tmr_work);
	struct i2c_client *client = info->client;

	if (down_trylock(&info->work_lock)) {
		dev_err(&client->dev, "%s: Failed to occupy work lock\n", __func__);
		esd_timer_start(CHECK_ESD_TIMER, info);
		return;
	}

	if (info->work_state != NOTHING) {
		dev_info(&client->dev, "%s: Other process occupied\n", __func__);
		up(&info->work_lock);
		return;
	}
	info->work_state = ESD_TIMER;

	disable_irq(info->irq);
	zt7554_power_control(info, POWER_OFF);
	zt7554_power_control(info, POWER_ON_SEQUENCE);

	clear_report_data(info);
	if (!mini_init_touch(info))
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return;
fail_time_out_init:
	dev_err(&client->dev, "%s: Failed to restart\n", __func__);
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return;
}
#endif

static bool zt7554_power_sequence(struct zt7554_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retry = 0;
	u16 chip_code;

retry_power_sequence:
	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to send power sequence(vendor cmd enable)\n");
		goto fail_power_sequence;
	}
	udelay(10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		dev_err(&client->dev, "Failed to read chip code\n");
		goto fail_power_sequence;
	}

	dev_dbg(&client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);
	udelay(10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to send power sequence(intn clear)\n");
		goto fail_power_sequence;
	}
	udelay(10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to send power sequence(nvm init)\n");
		goto fail_power_sequence;
	}
	mdelay(2);

	if (write_reg(client, 0xc001, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to send power sequence(program start)\n");
		goto fail_power_sequence;
	}
#if 1 /* FIXME: temporary bringup code */
	msleep(FIRMWARE_ON_DELAY + 100);	/* wait for checksum cal */
	//msleep(FIRMWARE_ON_DELAY);	/* wait for checksum cal */
#endif

	return true;

fail_power_sequence:
	if (retry++ < 3) {
		msleep(CHIP_ON_DELAY);
		dev_info(&client->dev, "retry = %d\n", retry);
		goto retry_power_sequence;
	}

	dev_err(&client->dev, "Failed to send power sequence\n");
	return false;
}

static int zt7554_power(struct i2c_client *client, int on, int gpio_ctrl_pin)
{
	int ret;
	static bool is_power_on;
#if 0
	if (!touch_regulator) {
		//touch_regulator = regulator_get(&client->dev, "tsp_vdd");
		touch_regulator = regulator_get(NULL, "vddvibr");
		//touch_regulator = regulator_get(NULL, "vdd28");
		if (IS_ERR(touch_regulator)) {
			touch_regulator = NULL;
			dev_err(&client->dev, "regulator_get error!\n");
			return -EIO;
		}
	}

	if (on == POWER_OFF) {
		//if (is_power_on && regulator_is_enabled(touch_regulator)) {
		if (is_power_on) {
			is_power_on = false;
			ret = regulator_disable(touch_regulator);
			dev_info(&client->dev, "power off!\n");
			if (ret) {
				is_power_on = true;
				dev_err(&client->dev, "power off error!\n");
				return -EIO;
			}
			mdelay(CHIP_OFF_DELAY);
		} else
			dev_err(&client->dev, "already power off!\n");
	} else {
		//if (!is_power_on && !regulator_is_enabled(touch_regulator)) {
		if (!is_power_on) {
			is_power_on = true;
			regulator_set_voltage(touch_regulator, 3300000, 3300000);
			ret = regulator_enable(touch_regulator);
			dev_info(&client->dev, "power on!\n");
			if (ret) {
				is_power_on = false;
				dev_err(&client->dev, "power on error!\n");
				return -EIO;
			}
			mdelay(CHIP_ON_DELAY + 100);
		} else
			dev_err(&client->dev, "already power on!\n");
	}
#else
	if (on == POWER_OFF) {
		if (is_power_on) {
			is_power_on = false;
			dev_info(&client->dev, "power off!\n");
			gpio_request(gpio_ctrl_pin,"tsp_enable");
			gpio_direction_output(gpio_ctrl_pin,1);
			gpio_set_value(gpio_ctrl_pin,0);
			mdelay(CHIP_OFF_DELAY);
			} else
				dev_err(&client->dev, "already power off!\n");
		} else {
		if (!is_power_on) {
			is_power_on = true;
			dev_info(&client->dev, "power on!\n");
			gpio_request(gpio_ctrl_pin,"tsp_enable");
			gpio_direction_output(gpio_ctrl_pin,1);
			gpio_set_value(gpio_ctrl_pin,1);
			mdelay(CHIP_ON_DELAY + 100);
		} else
			dev_err(&client->dev, "already power on!\n");
	}
#endif

	return 0;
}
static bool zt7554_power_control(struct zt7554_ts_info *info, u8 ctl)
{
	int ret = 0;

	if (info->pdata->tsp_supply_type == TSP_REGULATOR_SUPPLY) {
		ret = info->pdata->tsp_power(info->client, ctl, info->pdata->gpio_ctrl_pin);
		if (ret)
			return false;
		if (ctl == POWER_ON_SEQUENCE) {
			msleep(CHIP_ON_DELAY);
			return zt7554_power_sequence(info);
		}
	} else {
		if (ctl == POWER_OFF) {
			gpio_direction_output(info->pdata->gpio_ldo_en, 0);
			msleep(CHIP_OFF_DELAY);
		} else {
			gpio_direction_output(info->pdata->gpio_ldo_en, 1);
			msleep(CHIP_ON_DELAY);
			if (ctl == POWER_ON_SEQUENCE)
				return zt7554_power_sequence(info);
		}
	}
	return true;
}

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(struct zt7554_ts_info *info,
	u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	u16	new_version;
	u16	new_minor_version;
	u16	new_reg_version;
#if CHECK_HWID
	u16	new_hw_id;
#endif
	new_version = (u16) (m_firmware_data[52] | (m_firmware_data[53]<<8));
	new_minor_version = (u16) (m_firmware_data[56] | (m_firmware_data[57]<<8));
	new_reg_version = (u16) (m_firmware_data[60] | (m_firmware_data[61]<<8));

#if CHECK_HWID
	new_hw_id = (u16) (m_firmware_data[48] | (m_firmware_data[49]<<8));
	dev_dbg(&info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
							cur_hw_id, new_hw_id);
#endif

	dev_info(&info->client->dev, "cur version = 0x%x, new version = 0x%x\n",
							cur_version, new_version);
	dev_info(&info->client->dev, "cur minor version = 0x%x, new minor version = 0x%x\n",
						cur_minor_version, new_minor_version);
	dev_info(&info->client->dev, "cur reg data version = 0x%x, new reg data version = 0x%x\n",
						cur_reg_version, new_reg_version);
#if 0 /* FIXME: temporary bringup code */
	return true;
#endif

	if (cal_mode) {
		dev_info(&info->client->dev, "didn't update TSP F/W!! in CAL MODE\n");
		return false;
	}

	if (cur_version > 0xFF)
		return true;
	if (cur_version < new_version)
		return true;
	else if (cur_version > new_version)
		return false;
#if CHECK_HWID
	if (cur_hw_id != new_hw_id)
		return true;
#endif
	if (cur_minor_version < new_minor_version)
		return true;
	else if (cur_minor_version > new_minor_version)
		return false;
	if (cur_reg_version < new_reg_version)
		return true;

	return false;
}
#endif

#define TC_SECTOR_SZ		8

static bool ts_upgrade_firmware(struct zt7554_ts_info *info, const u8 *firmware_data, u32 size)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	u8 *verify_data;
	int retry_cnt = 0;
	int i;
	int page_sz = 128;
	u16 chip_code;

	verify_data = kzalloc(size, GFP_KERNEL);
	if (!verify_data) {
		dev_err(&client->dev, "cannot alloc verify buffer\n");
		return false;
	}

retry_upgrade:
	zt7554_power_control(info, POWER_OFF);
	zt7554_power_control(info, POWER_ON);
#if 1 /* FIXME: temporary bringup code */
	mdelay(100);
	//mdelay(10);
#endif

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "power sequence error (vendor cmd enable)\n");
		goto fail_upgrade;
	}

	udelay(10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		dev_err(&client->dev, "failed to read chip code\n");
		goto fail_upgrade;
	}

	dev_info(&client->dev, "chip code = 0x%x\n", chip_code);

	udelay(10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		dev_err(&client->dev, "power sequence error (intn clear)\n");
		goto fail_upgrade;
	}

	udelay(10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "power sequence error (nvm init)\n");
		goto fail_upgrade;
	}

	mdelay(5);

	dev_dbg(&client->dev, "init flash\n");

	if (write_reg(client, 0xc003, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "failed to write nvm vpp on\n");
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		dev_err(&client->dev, "failed to write nvm wp disable\n");
		goto fail_upgrade;
	}

	if (write_cmd(client, ZT7554_INIT_FLASH) != I2C_SUCCESS) {
		dev_err(&client->dev, "failed to init flash\n");
		goto fail_upgrade;
	}

	dev_info(&client->dev, "writing firmware data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			if (write_data(client, ZT7554_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
				dev_err(&client->dev, "error : write zinitix tc firmare\n");
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			udelay(100);
		}
		mdelay(10);	/* for fuzing delay */
	}

	if (write_reg(client, 0xc003, 0x0000) != I2C_SUCCESS) {
		dev_err(&client->dev, "nvm write vpp off\n");
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		dev_err(&client->dev, "nvm wp enable\n");
		goto fail_upgrade;
	}

	dev_info(&client->dev, "init flash\n");

	if (write_cmd(client, ZT7554_INIT_FLASH) != I2C_SUCCESS) {
		dev_err(&client->dev, "failed to init flash\n");
		goto fail_upgrade;
	}

	dev_info(&client->dev, "read firmware data\n");

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz/TC_SECTOR_SZ; i++) {
			if (read_firmware_data(client, ZT7554_READ_FLASH,
						(u8 *)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				dev_err(&client->dev, "Failed to read firmare\n");
				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			udelay(100);
		}
	}

	/* verify */
	dev_info(&client->dev, "verify firmware data\n");
	if (memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size) == 0) {
		dev_info(&client->dev, "upgrade finished\n");
		kfree(verify_data);
		zt7554_power_control(info, POWER_OFF);
		zt7554_power_control(info, POWER_ON_SEQUENCE);
		return true;
	}

fail_upgrade:
	zt7554_power_control(info, POWER_OFF);
	if (retry_cnt++ < INIT_RETRY_CNT) {
		dev_err(&client->dev, "upgrade failed : so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;
	}

	if (verify_data)
		kfree(verify_data);

	dev_info(&client->dev, "Failed to upgrade\n");

	return false;
}

bool ts_hw_calibration(struct zt7554_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16 chip_eeprom_info;
	int time_out = 0;

#if 1   /* FIXME: temporary bringup code */
	return true;
#endif

	if (write_reg(client, ZT7554_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;
	mdelay(10);
	write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
	mdelay(10);
	write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
	mdelay(50);
	write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
	mdelay(10);

	if (write_cmd(client, ZT7554_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	mdelay(10);
	write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		mdelay(500);
		write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);

		if (read_data(client, ZT7554_EEPROM_INFO_REG, (u8 *)&chip_eeprom_info, 2) < 0)
			return false;

		dev_dbg(&client->dev, "touch eeprom info = 0x%04X\n", chip_eeprom_info);
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if (time_out++ == 4) {
			write_cmd(client, ZT7554_CALIBRATE_CMD);
			mdelay(10);
			write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
			dev_err(&client->dev, "h/w calibration retry timeout.\n");
		}

		if (time_out++ > 10) {
			dev_err(&client->dev, "h/w calibration timeout.\n");
			break;
		}

	} while (true);

	if (write_reg(client, ZT7554_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;

	if (info->cap_info.ic_int_mask) {
		if (write_reg(client, ZT7554_INT_ENABLE_FLAG, info->cap_info.ic_int_mask) != I2C_SUCCESS)
			return false;
	}

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	udelay(100);
	if (write_cmd(client, ZT7554_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	mdelay(1000);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);
	return true;
}

static bool init_touch(struct zt7554_ts_info *info)
{
	struct zt7554_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	u16 reg_val;
	int i;
	u16 chip_eeprom_info;
#if USE_CHECKSUM
	u16 chip_check_sum;
	bool checksum_err;
#endif
	int retry_cnt = 0;

	info->ref_scale_factor = TSP_INIT_TEST_RATIO;
retry_init:
	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, ZT7554_EEPROM_INFO_REG,
						(u8 *)&chip_eeprom_info, 2) < 0) {
			dev_err(&client->dev, "Failed to read eeprom info(%d)\n", i);
			mdelay(10);
			continue;
		} else
			break;
	}

	if (i == INIT_RETRY_CNT)
		goto fail_init;

#if USE_CHECKSUM
	checksum_err = false;

	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, ZT7554_CHECKSUM_RESULT,
						(u8 *)&chip_check_sum, 2) < 0) {
			mdelay(10);
			continue;
		}

		if (chip_check_sum != 0x55aa)
			checksum_err = true;
		break;
	}

	if (i == INIT_RETRY_CNT || checksum_err) {
		dev_err(&client->dev, "Failed to check firmware data\n");
		if (checksum_err && retry_cnt < INIT_RETRY_CNT)
			retry_cnt = INIT_RETRY_CNT;
		goto fail_init;
	}
#endif
	if (write_cmd(client, ZT7554_SWRESET_CMD) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to write reset command\n");
		goto fail_init;
	}

	cap->button_num = SUPPORTED_BUTTON_NUM;

	reg_val = 0;
	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);

	if (cap->button_num > 0)
		zinitix_bit_set(reg_val, BIT_ICON_EVENT);

	cap->ic_int_mask = reg_val;

	if (write_reg(client, ZT7554_INT_ENABLE_FLAG, 0x0) != I2C_SUCCESS)
		goto fail_init;

	dev_dbg(&client->dev, "%s: Send reset command\n", __func__);
	if (write_cmd(client, ZT7554_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

	/* get chip information */
	if (read_data(client, ZT7554_VENDOR_ID, (u8 *)&cap->vendor_id, 2) < 0) {
		dev_err(&client->dev, "failed to read vendor id\n");
		goto fail_init;
	}

	if (read_data(client, ZT7554_CHIP_REVISION, (u8 *)&cap->ic_revision, 2) < 0) {
		dev_err(&client->dev, "failed to read chip revision\n");
		goto fail_init;
	}

	cap->ic_fw_size = 64 * 1024;

	if (read_data(client, ZT7554_HW_ID, (u8 *)&cap->hw_id, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_THRESHOLD, (u8 *)&cap->threshold, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_BUTTON_SENSITIVITY, (u8 *)&cap->key_threshold, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_DUMMY_BUTTON_SENSITIVITY, (u8 *)&cap->dummy_threshold, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_TOTAL_NUMBER_OF_X, (u8 *)&cap->x_node_num, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_TOTAL_NUMBER_OF_Y, (u8 *)&cap->y_node_num, 2) < 0)
		goto fail_init;

	cap->total_node_num = cap->x_node_num * cap->y_node_num;

	if (read_data(client, ZT7554_DND_N_COUNT, (u8 *)&cap->N_cnt, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_DND_U_COUNT, (u8 *)&cap->u_cnt, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_AFE_FREQUENCY, (u8 *)&cap->afe_frequency, 2) < 0)
		goto fail_init;

#ifdef SUPPORTED_DND_SHIFT_VALUE
	if (read_data(client, ZT7554_DND_SHIFT_VALUE, (u8 *)&cap->shift_value, 2) < 0)
		goto fail_init;
#endif

	/* get chip firmware version */
	if (read_data(client, ZT7554_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2) < 0)
		goto fail_init;

	if (read_data(client, ZT7554_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2) < 0)
		goto fail_init;

#if TOUCH_ONESHOT_UPGRADE
	if (ts_check_need_upgrade(info, cap->fw_version,
			cap->fw_minor_version, cap->reg_data_version, cap->hw_id)) {
		dev_info(&client->dev, "%s: start upgrade firmware\n", __func__);

		if (!ts_upgrade_firmware(info, m_firmware_data, cap->ic_fw_size))
			goto fail_init;

		if (!ts_hw_calibration(info))
			goto fail_init;

		/* disable chip interrupt */
		if (write_reg(client, ZT7554_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;

		/* get chip firmware version */
		if (read_data(client, ZT7554_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2) < 0)
			goto fail_init;

		if (read_data(client, ZT7554_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2) < 0)
			goto fail_init;

		if (read_data(client, ZT7554_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2) < 0)
			goto fail_init;
	}
#endif

	if (read_data(client, ZT7554_EEPROM_INFO_REG, (u8 *)&chip_eeprom_info, 2) < 0)
		goto fail_init;

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		if (!ts_hw_calibration(info))
			goto fail_init;

		/* disable chip interrupt */
		if (write_reg(client, ZT7554_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;
	}

	/* initialize */
	if (write_reg(client, ZT7554_X_RESOLUTION, (u16)pdata->x_resolution) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, ZT7554_Y_RESOLUTION, (u16)pdata->y_resolution) != I2C_SUCCESS)
		goto fail_init;

	cap->MinX = (u32)0;
	cap->MinY = (u32)0;
	cap->MaxX = (u32)pdata->x_resolution;
	cap->MaxY = (u32)pdata->y_resolution;

	if (write_reg(client, ZT7554_BUTTON_SUPPORTED_NUM, (u16)cap->button_num) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, ZT7554_SUPPORTED_FINGER_NUM, (u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_init;

	cap->multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	cap->gesture_support = 0;

	if (write_reg(client, ZT7554_INITIAL_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, ZT7554_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_init;

#if ZINITIX_I2C_CHECKSUM
	if (read_data(client, ZINITIX_INTERNAL_FLAG_02, (u8 *)&reg_val, 2) < 0)
			goto fail_init;
	cap->i2s_checksum = !(!zinitix_bit_test(reg_val, 15));
	dev_dbg(&client->dev, "use i2s checksum = %d\n", cap->i2s_checksum);
#endif

	zt7554_set_ta_status(info);
	zt7554_set_optional_mode(info, true);

	if (write_reg(client, ZT7554_INT_ENABLE_FLAG, cap->ic_int_mask) != I2C_SUCCESS)
		goto fail_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) { /* Test Mode */
		if (write_reg(client, ZT7554_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS) {
			dev_err(&client->dev, "%s: Failed to set DELAY_RAW_FOR_HOST\n", __func__);
			goto fail_init;
		}
	}
#if ESD_TIMER_INTERVAL
	if (write_reg(client, ZT7554_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;

	read_data(client, ZT7554_PERIODICAL_INTERRUPT_INTERVAL, (u8 *)&reg_val, 2);
#endif
	tsp_charger_status_cb = tsp_charger_enable;

	dev_info(&client->dev, "%s: initialize done!\n", __func__);

	return true;

fail_init:
	if (cal_mode) {
		dev_err(&client->dev, "didn't update TSP F/W!! in CAL MODE\n");
		return false;
	}
	if (++retry_cnt <= INIT_RETRY_CNT) {
		zt7554_power_control(info, POWER_OFF);
		zt7554_power_control(info, POWER_ON_SEQUENCE);
		goto	retry_init;

	} else if (retry_cnt == INIT_RETRY_CNT + 1) {
		cap->ic_fw_size = 64 * 1024;
#if TOUCH_FORCE_UPGRADE
		if (!ts_upgrade_firmware(info, m_firmware_data, cap->ic_fw_size)) {
			dev_err(&client->dev, "firmware upgrade fail!\n");
				return false;
		}
		mdelay(100);

		/* hw calibration and make checksum */
		if (!ts_hw_calibration(info)) {
			dev_err(&client->dev, "failed to initiallize\n");
			return false;
		}
		goto retry_init;
#endif
	}
	dev_err(&client->dev, "Failed to initiallize\n");

	return false;
}

static bool mini_init_touch(struct zt7554_ts_info *info)
{
	struct zt7554_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
#if USE_CHECKSUM
	u16 chip_check_sum;
	if (read_data(client, ZT7554_CHECKSUM_RESULT, (u8 *)&chip_check_sum, 2) < 0)
		goto fail_mini_init;

	if (chip_check_sum != 0x55aa) {
		dev_err(&client->dev, "Failed to check firmware\n");
		goto fail_mini_init;
	}
#endif
	info->ref_scale_factor = TSP_INIT_TEST_RATIO;

	if (write_cmd(client, ZT7554_SWRESET_CMD) != I2C_SUCCESS) {
		dev_info(&client->dev, "Failed to write reset command\n");
		goto fail_mini_init;
	}

	/* initialize */
	if (write_reg(client, ZT7554_X_RESOLUTION, (u16)(pdata->x_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_Y_RESOLUTION, (u16)(pdata->y_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_BUTTON_SUPPORTED_NUM, (u16)info->cap_info.button_num) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_SUPPORTED_FINGER_NUM, (u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_INITIAL_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	zt7554_set_ta_status(info);
	zt7554_set_optional_mode(info, true);

	/* soft calibration */
	if (write_cmd(client, ZT7554_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, ZT7554_INT_ENABLE_FLAG, info->cap_info.ic_int_mask) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(client, ZT7554_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS){
			dev_err(&client->dev, "Failed to set ZT7554_DELAY_RAW_FOR_HOST\n");
			goto fail_mini_init;
		}
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, ZT7554_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#endif

	dev_info(&client->dev, "Successfully mini initialized\r\n");
	return true;

fail_mini_init:
	dev_err(&client->dev, "Failed to initialize mini init\n");
	zt7554_power_control(info, POWER_OFF);
	zt7554_power_control(info, POWER_ON_SEQUENCE);

	if (!init_touch(info)) {
		dev_err(&client->dev, "Failed to initialize\n");
		return false;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	return true;
}

static void clear_report_data(struct zt7554_ts_info *info)
{
	int i;
	bool reported = false;
	u8 sub_status;

	for (i = 0; i < info->cap_info.button_num; i++) {
		if (info->button[i] == ICON_BUTTON_DOWN) {
			info->button[i] = ICON_BUTTON_UP;
			input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
			reported = true;
		}
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->reported_touch_info.coord[i].sub_status;
		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			reported = true;
		}
		info->reported_touch_info.coord[i].sub_status = 0;
	}

	if (reported)
		input_sync(info->input_dev);

}

static irqreturn_t zt7554_touch_work(int irq, void *data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)data;
	struct zt7554_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u8 sub_status;
	u8 prev_sub_status;
	u32 x, y, maxX, maxY;
	u32 w;
	u32 tmp;
	u8 palm = 0;


	if (gpio_get_value(info->pdata->gpio_int)) {
		dev_err(&client->dev, "Invalid interrupt\n");
		return IRQ_HANDLED;
	}

	if (down_trylock(&info->work_lock)) {
		dev_err(&client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
		return IRQ_HANDLED;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif

	if (info->work_state != NOTHING) {
		dev_err(&client->dev, "%s: Other process occupied\n", __func__);
		udelay(DELAY_FOR_SIGNAL_DELAY);
		if (!gpio_get_value(info->pdata->gpio_int)) {
			write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
			udelay(DELAY_FOR_SIGNAL_DELAY);
		}
		goto out;
	}
	info->work_state = NORMAL;

#if ZINITIX_I2C_CHECKSUM
	i = 0;
	if (!ts_read_coord(info) || info->touch_info.status == 0xffff || info->touch_info.status == 0x1) {
		for (i = 1; i < 50; i++) {
			if (!(!ts_read_coord(info) || info->touch_info.status == 0xffff
			|| info->touch_info.status == 0x1))
				break;
		}
	}

	if (i == 50) {
		dev_err(&client->dev, "Failed to read info coord\n");
		zt7554_power_control(info, POWER_OFF);
		zt7554_power_control(info, POWER_ON_SEQUENCE);
		clear_report_data(info);
		mini_init_touch(info);
		goto out;
	}
#else
	if (!ts_read_coord(info) || info->touch_info.status == 0xffff || info->touch_info.status == 0x1) {
		dev_err(&client->dev, "Failed to read info coord\n");
		zt7554_power_control(info, POWER_OFF);
		zt7554_power_control(info, POWER_ON_SEQUENCE);
		clear_report_data(info);
		mini_init_touch(info);
		goto out;
	}
#endif
	/* invalid : maybe periodical repeated int. */
	if (info->touch_info.status == 0x0)
		goto out;

	if (zinitix_bit_test(info->touch_info.status, BIT_ICON_EVENT)) {
		if (read_data(info->client, ZT7554_ICON_STATUS_REG,
			(u8 *)(&info->icon_event_reg), 2) < 0) {
			dev_err(&client->dev, "Failed to read button info\n");
			write_cmd(client, ZT7554_CLEAR_INT_STATUS_CMD);
			goto out;
		}

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg, (BIT_O_ICON0_DOWN + i))) {
				info->button[i] = ICON_BUTTON_DOWN;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 1);
				dev_info(&client->dev, "Button down\n");
			}
		}

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg, (BIT_O_ICON0_UP + i))) {
				info->button[i] = ICON_BUTTON_UP;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
				dev_info(&client->dev, "Button up\n");
			}
		}
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->touch_info.coord[i].sub_status;
		prev_sub_status = info->reported_touch_info.coord[i].sub_status;

		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			x = info->touch_info.coord[i].x;
			y = info->touch_info.coord[i].y;
			w = info->touch_info.coord[i].width;

			 /* transformation from touch to screen orientation */
			//if (pdata->orientation & TOUCH_V_FLIP)
			//	y = info->cap_info.MaxY + info->cap_info.MinY - y;

			//if (pdata->orientation & TOUCH_H_FLIP)
			//	x = info->cap_info.MaxX + info->cap_info.MinX - x;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;
/*
			if (pdata->orientation & TOUCH_XY_SWAP) {
				zinitix_swap_v(x, y, tmp);
				zinitix_swap_v(maxX, maxY, tmp);
			}
*/
			info->touch_info.coord[i].x = x;
			info->touch_info.coord[i].y = y;
			if (zinitix_bit_test(sub_status, SUB_BIT_DOWN)) {
#if TOUCH_BOOSTER
				if (!min_handle) {
					min_handle = cpufreq_limit_min_freq(touch_cpufreq_lock, "TSP");
					if (IS_ERR(min_handle)) {
						dev_err(&client->dev, "cannot get cpufreq_min lock\n");
						min_handle = NULL;
					}
					_store_cpu_num_min_limit(2);
					dev_dbg(&client->dev, "cpu freq on\n");
				}
				info->finger_cnt++;
#endif
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&client->dev, "Finger [%02d] x = %d, y = %d,"
						" w = %d\n", i, x, y, w);
#else
				dev_info(&client->dev, "Finger down\n");
#endif
			}
			if (w == 0)
				w = 1;

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)w);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)w);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR,
								(u32)((palm == 1) ? (w - 40) : w));
			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		} else if (zinitix_bit_test(sub_status, SUB_BIT_UP) ||
			zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
#if TOUCH_BOOSTER
			info->finger_cnt--;
			if (!info->finger_cnt) {
				cpufreq_limit_put(min_handle);
				min_handle = NULL;
				_store_cpu_num_min_limit(1);
				dev_dbg(&client->dev, "cpu freq off\n");
			}
#endif

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			dev_info(&client->dev, "Finger [%02d] up\n", i);
#else
			dev_info(&client->dev, "Finger up\n");
#endif
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		} else
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
	}
	memcpy((char *)&info->reported_touch_info, (char *)&info->touch_info,
							sizeof(struct point_info));
	input_sync(info->input_dev);

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	up(&info->work_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void zt7554_ts_late_resume(struct early_suspend *h)
{
	struct zt7554_ts_info *info = misc_info;
	//struct capa_info *cap = &(info->cap_info);

	if (!info)
		return;

	down(&info->work_lock);
	if (info->work_state != RESUME && info->work_state != EALRY_SUSPEND) {
		dev_err(&info->client->dev, "%s: invalid work proceedure\n", __func__);
		up(&info->work_lock);
		return;
	}
	zt7554_power_control(info, POWER_ON_SEQUENCE);
	info->work_state = RESUME;
	if (!mini_init_touch(info))
		goto fail_late_resume;

#if 0 /* FIXME: below codes are not necessary */
	if (read_data(info->client, ZT7554_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2));
	if (read_data(info->client, ZT7554_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2));
	if (read_data(info->client, ZT7554_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2));
	if (read_data(info->client, ZT7554_HW_ID, (u8 *)&cap->hw_id, 2));
	if (read_data(info->client, ZT7554_VENDOR_ID, (u8 *)&cap->vendor_id, 2));
#endif

	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	return;

fail_late_resume:
	dev_err(&info->client->dev, "failed to late resume\n");
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	return;
}

static void zt7554_ts_early_suspend(struct early_suspend *h)
{
	struct zt7554_ts_info *info = misc_info;

	if (!info)
		return;

	disable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		dev_err(&info->client->dev, "%s: invalid work proceedure\n", __func__);
		up(&info->work_lock);
		enable_irq(info->irq);
		return;
	}
	info->work_state = EALRY_SUSPEND;

	clear_report_data(info);
#if ESD_TIMER_INTERVAL
	write_reg(info->client, ZT7554_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
#endif

	zt7554_power_control(info, POWER_OFF);
#if TOUCH_BOOSTER
	if (min_handle) {
		dev_err(&info->client->dev, "%s: Cpu was not in Normal Freq..\n", __func__);
		if (cpufreq_limit_put(min_handle) < 0)
			dev_err(&info->client->dev, "Error in scaling down cpu frequency\n");
		min_handle = NULL;
		info->finger_cnt = 0;
		dev_dbg(&info->client->dev, "cpu freq off\n");
	}
#endif
	up(&info->work_lock);
	return;
}
#endif	/* CONFIG_HAS_EARLYSUSPEND */

#if defined(CONFIG_PM)
static int zt7554_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zt7554_ts_info *info = i2c_get_clientdata(client);
	//struct capa_info *cap = &(info->cap_info);

	down(&info->work_lock);
	if (info->work_state != SUSPEND) {
		dev_err(&client->dev, "%s: Invalid work proceedure\n", __func__);
		up(&info->work_lock);
		return 0;
	}
	zt7554_power_control(info, POWER_ON_SEQUENCE);

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->work_state = RESUME;
#else
	info->work_state = NOTHING;
	if (!mini_init_touch(info))
		dev_err(&client->dev, "Failed to resume\n");
#if 0 /* FIXME: below codes are not necessary */
	if (read_data(client, ZT7554_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2));
	if (read_data(client, ZT7554_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2));
	if (read_data(client, ZT7554_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2));
	if (read_data(client, ZT7554_HW_ID, (u8 *)&cap->hw_id, 2));
	if (read_data(client, ZT7554_VENDOR_ID, (u8 *)&cap->vendor_id, 2));
#endif
	enable_irq(info->irq);
#endif

	up(&info->work_lock);

	return 0;
}

static int zt7554_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zt7554_ts_info *info = i2c_get_clientdata(client);

#ifndef CONFIG_HAS_EARLYSUSPEND
	disable_irq(info->irq);
#endif
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING && info->work_state != SUSPEND) {
		dev_err(&client->dev, "%s: Invalid work proceedure\n", __func__);
		up(&info->work_lock);
#ifndef CONFIG_HAS_EARLYSUSPEND
		enable_irq(info->irq);
#endif
		return 0;
	}

#ifndef CONFIG_HAS_EARLYSUSPEND
	clear_report_data(info);
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif
#endif
	write_cmd(info->client, ZT7554_SLEEP_CMD);
	zt7554_power_control(info, POWER_OFF);
	info->work_state = SUSPEND;

	up(&info->work_lock);

	return 0;
}
#endif

bool ts_set_touchmode(u16 value)
{
	int i;
	u16 N_cnt;
	u16 U_cnt;
	u16 freq;
	u16 reg_val;

	disable_irq(misc_info->irq);

	down(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		dev_err(&misc_info->client->dev, "other process occupied.\n");
		enable_irq(misc_info->irq);
		up(&misc_info->work_lock);
		return -1;
	}

	misc_info->work_state = SET_MODE;

	/* SDND mode */
	if (value == TOUCH_DND_MODE) {
		N_cnt = SEC_SDND_N_COUNT;
		U_cnt = SEC_SDND_U_COUNT;
		freq = SEC_SDND_FREQUENCY;
		read_data(misc_info->client, 0x011e, (u8 *)&reg_val, 2);
		zinitix_bit_clr(reg_val, 1);
		write_reg(misc_info->client, 0x011e, reg_val);
#ifdef SUPPORTED_DND_SHIFT_VALUE
		if (write_reg(misc_info->client, ZT7554_DND_SHIFT_VALUE,
						SEC_DND_SHIFT_VALUE) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set SDND_SHIFT_VALUE\n");
#endif
		if (write_reg(misc_info->client, ZT7554_DND_N_COUNT, N_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_SDND_N_COUNT %d.\n", N_cnt);
		if (write_reg(misc_info->client, ZT7554_DND_U_COUNT, U_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_SDND_U_COUNT %d.\n", U_cnt);
		if (write_reg(misc_info->client, ZT7554_AFE_FREQUENCY, freq) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_SAFE_FREQUENCY %d.\n", freq);
	} else if (value == TOUCH_PDND_MODE) {	/* DND mode */
		N_cnt = SEC_SDND_N_COUNT;
		U_cnt = SEC_SDND_U_COUNT;
		freq = SEC_SDND_FREQUENCY;
		read_data(misc_info->client, 0x011e, (u8 *)&reg_val, 2);
		zinitix_bit_clr(reg_val, 1);
		write_reg(misc_info->client, 0x011e, reg_val);

		if (write_reg(misc_info->client, ZT7554_DND_N_COUNT, N_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_DND_N_COUNT %d.\n", N_cnt);
		if (write_reg(misc_info->client, ZT7554_DND_U_COUNT, U_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_DND_U_COUNT %d.\n", U_cnt);
		if (write_reg(misc_info->client, ZT7554_AFE_FREQUENCY, freq) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_AFE_FREQUENCY %d.\n", freq);
	}

	else if (misc_info->touch_mode == TOUCH_DND_MODE || misc_info->touch_mode == TOUCH_PDND_MODE) {
#ifdef SUPPORTED_DND_SHIFT_VALUE
		if (write_reg(misc_info->client, ZT7554_DND_SHIFT_VALUE,
						misc_info->cap_info.shift_value) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set DND_SHIFT_VALUE\n");
#endif
		if (write_reg(misc_info->client, ZT7554_DND_N_COUNT,
						misc_info->cap_info.N_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to reset ZT7554_AFE_FREQUENCY %d.\n",
									misc_info->cap_info.N_cnt);
		if (write_reg(misc_info->client, ZT7554_DND_U_COUNT,
						misc_info->cap_info.u_cnt) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to reset ZT7554_DND_U_COUNT %d.\n",
						misc_info->cap_info.u_cnt);
		if (write_reg(misc_info->client, ZT7554_AFE_FREQUENCY,
						misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to reset ZT7554_AFE_FREQUENCY %d.\n",
						misc_info->cap_info.afe_frequency);
	}

	if (value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

	if (misc_info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(misc_info->client, ZT7554_DELAY_RAW_FOR_HOST,
			RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Fail to set ZT7554_DELAY_RAW_FOR_HOST\n");
	}

	if (write_reg(misc_info->client, ZT7554_TOUCH_MODE,
					misc_info->touch_mode) != I2C_SUCCESS)
		dev_err(&misc_info->client->dev, "Fail to set ZINITX_TOUCH_MODE\n");

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		mdelay(20);
		write_cmd(misc_info->client, ZT7554_CLEAR_INT_STATUS_CMD);
	}

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	up(&misc_info->work_lock);

	return 1;
}

int ts_upgrade_sequence(const u8 *firmware_data)
{
	disable_irq(misc_info->irq);
	down(&misc_info->work_lock);
	misc_info->work_state = UPGRADE;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	clear_report_data(misc_info);

	dev_info(&misc_info->client->dev, "start upgrade firmware\n");
	if (!ts_upgrade_firmware(misc_info, firmware_data, misc_info->cap_info.ic_fw_size)) {
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;
	}

	if (!init_touch(misc_info)) {
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	enable_irq(misc_info->irq);
	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);

	return 0;
}

#ifdef SEC_FACTORY_TEST
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

static struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},

	/* vendor dependant command */
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_reference_diff", run_reference_PDiff),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("run_dnd_read", run_preference_read),},
	{TSP_CMD("get_dnd", get_preference),},
	{TSP_CMD("run_delta_read", run_delta_read),},
	{TSP_CMD("get_delta", get_delta),},
};

static inline void set_cmd_result(struct zt7554_ts_info *info, char *buff, int len)
{
	strncat(info->factory_info->cmd_result, buff, len);
}

static inline void set_default_result(struct zt7554_ts_info *info)
{
	char delim = ':';
	memset(info->factory_info->cmd_result, 0x00, ARRAY_SIZE(info->factory_info->cmd_result));
	memcpy(info->factory_info->cmd_result, info->factory_info->cmd, strlen(info->factory_info->cmd));
	strncat(info->factory_info->cmd_result, &delim, 1);
}

static void fw_update(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	int ret = 0;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char result[16] = {0};

	set_default_result(info);

	switch (info->factory_info->cmd_param[0]) {
	case BUILT_IN:
		ret = ts_upgrade_sequence((u8 *)m_firmware_data);
		if (ret < 0) {
			info->factory_info->cmd_state = FAIL;
			return;
		}
		break;
	case UMS:
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(info->pdata->ext_fw_name, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			dev_err(&client->dev, "file open error:%d\n", (s32)fp);
			info->factory_info->cmd_state = FAIL;
			goto err_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		if (fsize != info->cap_info.ic_fw_size) {
			dev_err(&client->dev, "invalid fw size!!\n");
			info->factory_info->cmd_state = FAIL;
			goto err_open;
		}

		buff = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!buff) {
			dev_err(&client->dev, "failed to alloc buffer for fw\n");
			info->factory_info->cmd_state = FAIL;
			goto err_alloc;
		}

		nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
		if (nread != fsize) {
			info->factory_info->cmd_state = FAIL;
			goto err_fw_size;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		dev_info(&client->dev, "ums fw is loaded!!\n");

		ret = ts_upgrade_sequence((u8 *)buff);
		if (ret < 0) {
			kfree(buff);
			info->factory_info->cmd_state = FAIL;
			goto update_fail;
		}
		break;

	default:
		dev_err(&client->dev, "invalid fw file type!!\n");
		goto update_fail;
	}

	info->factory_info->cmd_state = OK;
	snprintf(result, sizeof(result) , "%s", "OK");
	set_cmd_result(info, result, strnlen(result, sizeof(result)));
	kfree(buff);

	return;


if (fp != NULL) {
err_fw_size:
	kfree(buff);
err_alloc:
	filp_close(fp, NULL);
err_open:
	set_fs(old_fs);
}
update_fail:
	snprintf(result, sizeof(result) , "%s", "NG");
	set_cmd_result(info, result, strnlen(result, sizeof(result)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	u16 fw_version, fw_minor_version, reg_version, hw_id, vendor_id;
	u32 version, length;

	set_default_result(info);

	fw_version = (u16)(m_firmware_data[52] | (m_firmware_data[53] << 8));
	fw_minor_version = (u16)(m_firmware_data[56] | (m_firmware_data[57] << 8));
	reg_version = (u16)(m_firmware_data[60] | (m_firmware_data[61] << 8));
	hw_id = (u16)(m_firmware_data[48] | (m_firmware_data[49] << 8));
	version = (u32)((u32)(hw_id & 0xff) << 16) | ((fw_version & 0xf) << 12)
				| ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(finfo->cmd_buff, length + 1, "%s", "ZI");
	snprintf(finfo->cmd_buff + length, sizeof(finfo->cmd_buff) - length, "%06X", version);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	u16 fw_version, fw_minor_version, reg_version, hw_id, vendor_id;
	u32 version, length;

	set_default_result(info);

	fw_version = info->cap_info.fw_version;
	fw_minor_version = info->cap_info.fw_minor_version;
	reg_version = info->cap_info.reg_data_version;
	hw_id = info->cap_info.hw_id;
	vendor_id = ntohs(info->cap_info.vendor_id);
	version = (u32)((u32)(hw_id & 0xff) << 16) | ((fw_version & 0xf) << 12)
				| ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(finfo->cmd_buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(finfo->cmd_buff + length, sizeof(finfo->cmd_buff) - length, "%06X", version);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_threshold(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
				"%d", info->cap_info.threshold);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

#define ZT7554_VENDOR_NAME "ZINITIX"

static void get_chip_vendor(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", ZT7554_VENDOR_NAME);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

#define ZT7554_CHIP_NAME "ZT7554"

static void get_chip_name(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", ZT7554_CHIP_NAME);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_x_num(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
				"%u", info->cap_info.x_node_num);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_y_num(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
				"%u", info->cap_info.y_node_num);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void not_support_cmd(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	sprintf(finfo->cmd_buff, "%s", "NA");
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	info->factory_info->cmd_state = NOT_APPLICABLE;

	dev_dbg(&client->dev, "%s: \"%s(%d)\"\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void run_reference_read(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	u32 min, max;
	s32 i, j;

	set_default_result(info);

	ts_set_touchmode(TOUCH_DND_MODE);
	get_raw_data(info, (u8 *)raw_data->ref_data, 10);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = raw_data->ref_data[0];
	max = raw_data->ref_data[0];

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			dev_dbg(&client->dev, "ref_data[%d] : %d\n", i * info->cap_info.y_node_num + j,
					raw_data->ref_data[i * info->cap_info.y_node_num + j]);

			if (raw_data->ref_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->ref_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->ref_data[i * info->cap_info.y_node_num + j];

			if (raw_data->ref_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->ref_data[i * info->cap_info.y_node_num + j];

		}
	}

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%d,%d\n", min, max);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: \"%s\"(%d)\n", __func__, finfo->cmd_buff,
				strlen(finfo->cmd_buff));

	return;
}

#define PDIFF_DEBUG 1
#if PDIFF_DEBUG
int print_diff[29*18] = {0,};
#endif

static bool run_reference_PDiff_read(void *device_data, const u16 h_diff[][17],
		const u16 v_diff[][18], int bHidden1, int bMenu, int bBack, int bHidden2)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	int i, j, diff_val, pre_val, next_val, x_num, y_num;
	bool pass = true;
	int buttons[4] = {bHidden1, bMenu, bBack, bHidden2};

	ts_set_touchmode(TOUCH_PDND_MODE);
	get_raw_data(info, (u8 *)info->dnd_data, 10);
	ts_set_touchmode(TOUCH_POINT_MODE);

	x_num = info->cap_info.x_node_num;
	y_num = info->cap_info.y_node_num;
#if PDIFF_DEBUG
	printk(KERN_DEBUG "%s : ++++++ DND SPEC +++++++++\n", __func__);
	for (i = 0; i < x_num; i++) {
		printk(KERN_DEBUG "%s : ", __func__);
		for (j = 0; j < y_num; j++)
			printk(KERN_DEBUG "%5d ", info->dnd_data[i*y_num+j]);
		printk(KERN_DEBUG "\n");
	}
#endif
	printk(KERN_DEBUG "%s : ------- DND SPEC ----------\n", __func__);
	printk(KERN_DEBUG "%s : TSP Diff test scale factor = %d\n", __func__, info->ref_scale_factor);
	printk(KERN_DEBUG "%s : H Diff start\n", __func__);

	/* H DIff */
	info->hdiff_max_x = info->hdiff_max_y = info->hdiff_min_x = info->hdiff_min_y = 0;
	info->hdiff_max_val = -32768;
	info->hdiff_min_val = 32767;

	for (i = 0; i < x_num - 1; i++) {
		for (j = 0; j < y_num - 1; j++) {
			printk(KERN_DEBUG "%d ", info->dnd_data[i*info->cap_info.y_node_num+j]);
			next_val = info->dnd_data[(i*y_num)+(j+1)];
			pre_val = info->dnd_data[(i*y_num)+j];
			diff_val = (next_val > pre_val) ? (next_val - pre_val) : (pre_val - next_val);
			pre_val = (info->ref_scale_factor == 100) ? ((s16)h_diff[i][j]) :
				((s16)(((s32)h_diff[i][j] * info->ref_scale_factor) / 100));
			if (diff_val > pre_val)
				pass = false;
#if PDIFF_DEBUG
			print_diff[i * y_num + j] = diff_val;
#endif
			if (info->hdiff_max_val < diff_val) {
				info->hdiff_max_val = diff_val;
				info->hdiff_max_x = i;
				info->hdiff_max_y = j + 1;
			}
			if (info->hdiff_min_val > diff_val) {
				info->hdiff_min_val =  diff_val;
				info->hdiff_min_x = i;
				info->hdiff_min_y = j + 1;
			}
		}
		printk(KERN_DEBUG "\n");
	}
#if PDIFF_DEBUG
	printk(KERN_DEBUG "%s : ++++++ h_diff SPEC +++++++++\n", __func__);
	for (i = 0; i < x_num - 1; i++) {
		printk(KERN_DEBUG "%s : ", __func__);
		for (j = 0; j < y_num - 1; j++)
			printk(KERN_DEBUG "%5d ", (u16)h_diff[i][j]);
		printk(KERN_DEBUG "\n");
	}
	printk(KERN_DEBUG "%s : ------- h_diff SPEC ----------\n", __func__);
	printk(KERN_DEBUG "%s : ++++++ calculated h_diff SPEC +++++++++\n", __func__);
	for (i = 0; i < x_num-1; i++) {
		printk(KERN_DEBUG "%s : ", __func__);
		for (j = 0; j < y_num-1; j++)
			printk(KERN_DEBUG "%5d ", print_diff[i * y_num + j]);
		printk(KERN_DEBUG "\n");
	}
	printk(KERN_DEBUG "%s : ------- calculated h_diff SPEC ----------\n", __func__);
#endif
	printk(KERN_DEBUG "%s :", __func__);
	printk(KERN_DEBUG "H diff %s\n", pass ? "pass" : "fail");

	printk(KERN_DEBUG "%s : V Diff start\n", __func__);
	info->vdiff_max_x = info->vdiff_max_y = info->vdiff_min_x = info->vdiff_min_y = 0;
	info->vdiff_max_val = -32768;
	info->vdiff_min_val = 32767;

	/* V DIff  View */
	for (i = 0; i < x_num - 2; i++) {
		for (j = 0; j < y_num; j++) {
			printk(KERN_DEBUG "%d ", info->dnd_data[i * info->cap_info.y_node_num + j]);
			next_val = info->dnd_data[(i * y_num) + j];
			pre_val = info->dnd_data[(i * y_num) + j + y_num];
			diff_val = (next_val > pre_val) ? (next_val - pre_val) : (pre_val - next_val);
			pre_val = (info->ref_scale_factor == 100) ? ((s16)v_diff[i][j]) :
				((s16)(((s32)v_diff[i][j] * info->ref_scale_factor) / 100));
			if (diff_val > pre_val)
				pass = false;
#if PDIFF_DEBUG
			print_diff[i * y_num + j] = diff_val;
#endif
			if (info->vdiff_max_val < diff_val) {
				info->vdiff_max_val = diff_val;
				info->vdiff_max_x = i;
				info->vdiff_max_y = j + 1;
			}
			if (info->vdiff_min_val > diff_val) {
				info->vdiff_min_val = diff_val;
				info->vdiff_min_x = i;
				info->vdiff_min_y = j + 1;
			}
		}
		printk(KERN_DEBUG "\n");
	}
	printk(KERN_DEBUG "%s : ", __func__);
	printk(KERN_DEBUG "V Diff view %s\n", pass ? "pass" : "fail");

#if PDIFF_DEBUG
	printk(KERN_DEBUG "%s : ++++++ v_diff SPEC +++++++++\n", __func__);
	for (i = 0; i < x_num - 2; i++) {
		printk(KERN_DEBUG "%s : ", __func__);
		for (j = 0; j < y_num; j++)
			printk(KERN_DEBUG "%5d ", (u16)v_diff[i][j]);
		printk(KERN_DEBUG "\n");
	}
	printk(KERN_DEBUG "%s : ------- v_diff SPEC ----------\n", __func__);
	printk(KERN_DEBUG "%s : ++++++ calculated v_diff SPEC +++++++++\n", __func__);
	for (i = 0; i < x_num - 2; i++) {
		printk(KERN_DEBUG "%s : ", __func__);
		for (j = 0; j < y_num; j++)
			printk(KERN_DEBUG "%5d ", print_diff[i * y_num + j]);
		printk(KERN_DEBUG "\n");
	}
	printk(KERN_DEBUG "%s : ------- calculated v_diff SPEC ----------\n", __func__);
#endif

	/* V DIff  button */
	if (info->cap_info.button_num) {
		printk(KERN_DEBUG "%s : TSP Button scale = %d\n", __func__, info->ref_scale_factor);
		printk(KERN_DEBUG "%s : TSP Button Diff Spec. = %d %d %d %d\n", __func__,
			v_diff[x_num-2][buttons[0]], v_diff[x_num-2][buttons[1]],
			v_diff[x_num-2][buttons[2]], v_diff[x_num-2][buttons[3]]);

		for (i = 0; i < 4; i++) {
			int nButton = buttons[i];
			if (nButton < 0)
				continue;
			next_val = info->dnd_data[(x_num - 1) * y_num + nButton];
			pre_val = info->dnd_data[(x_num - 2) * y_num + nButton];
			diff_val = (next_val > pre_val) ? (next_val - pre_val) : (pre_val - next_val);
			pre_val = (info->ref_scale_factor == 100) ?
				((s16)v_diff[x_num - 2][nButton] + info->ref_btn_option) :
				((s16)(((s32)v_diff[x_num - 2][nButton] * info->ref_scale_factor) / 100)
									+ info->ref_btn_option);

			if (diff_val > pre_val) {
				pass = false;
				if (info->vdiff_max_val < diff_val) {
					info->vdiff_max_val = diff_val;
					info->vdiff_max_x = x_num - 1;
					info->vdiff_max_y = nButton;
				}
				if (info->vdiff_min_val > diff_val) {
					info->vdiff_min_val = diff_val;
					info->vdiff_min_x = x_num - 1;
					info->vdiff_min_y = nButton;
				}
			}
#if PDIFF_DEBUG
			printk(KERN_DEBUG "%s : ", __func__);
			printk(KERN_DEBUG "buttons[%d]'s diff_val is %d\n", i, diff_val);
#endif
		}
	}
	printk(KERN_DEBUG "%s : ", __func__);
	printk(KERN_DEBUG "Button Diff %s\n", pass ? "pass" : "fail");

	return pass;
}



static void run_reference_PDiff(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct tsp_factory_info *finfo = info->factory_info;
	char buff[40] = {0};
	bool check;

	finfo->cmd_state = RUNNING;
	set_default_result(info);

	dev_dbg(&info->client->dev, "run_ref_diff check hw_id = 0x%04X\n", info->cap_info.hw_id);

	check = run_reference_PDiff_read(device_data, dnd_h_diff_00, dnd_v_diff_00, -1, 4, 13, -1);
	if (check) {
		dev_dbg(&info->client->dev, "diff pass\n");
		sprintf(buff, "%d,%d,%d,%d,pass\n", info->hdiff_min_val,
				info->hdiff_max_val, info->vdiff_min_val, info->vdiff_max_val);
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		finfo->cmd_state = OK;
	} else {
		dev_dbg(&info->client->dev, "diff fail\n");
		sprintf(buff, "%d,%d,%d,%d,fail\n", info->vdiff_min_val,
				info->vdiff_max_val, info->vdiff_min_val, info->vdiff_max_val);
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		finfo->cmd_state = FAIL;
	}

}
static void get_reference(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	set_default_result(info);

	x_node = finfo->cmd_param[0];
	y_node = finfo->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", "abnormal");
		set_cmd_result(info, finfo->cmd_buff,
						strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
		info->factory_info->cmd_state = FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->ref_data[node_num];
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%u", val);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;
	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void run_preference_read(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	u16 min, max;
	s32 i, j;

	set_default_result(info);

	ts_set_touchmode(TOUCH_PDND_MODE);
	get_raw_data(info, (u8 *)raw_data->pref_data, 10);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			dev_dbg(&client->dev, "pref_data[%d]: %d\n",
					i * info->cap_info.y_node_num + j,
					raw_data->pref_data[i * info->cap_info.y_node_num + j]);
			if (raw_data->pref_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->pref_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->pref_data[i * info->cap_info.y_node_num + j];

			if (raw_data->pref_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->pref_data[i * info->cap_info.y_node_num + j];

		}
	}

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%d,%d\n", min, max);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: \"%s\"(%d)\n", __func__, finfo->cmd_buff,
				strlen(finfo->cmd_buff));

	return;
}

static void get_preference(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	set_default_result(info);

	x_node = finfo->cmd_param[0];
	y_node = finfo->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", "abnormal");
		set_cmd_result(info, finfo->cmd_buff,
						strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
		info->factory_info->cmd_state = FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->pref_data[node_num];
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%u", val);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void run_delta_read(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	s16 min, max;
	s32 i, j;

	set_default_result(info);

	ts_set_touchmode(TOUCH_DELTA_MODE);
	get_raw_data(info, (u8 *)(u8 *)raw_data->delta_data, 10);
	ts_set_touchmode(TOUCH_POINT_MODE);
	finfo->cmd_state = OK;

	min = (s16)0x7FFF;
	max = (s16)0x8000;

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			dev_dbg(&client->dev, "delta_data[%d]: %d\n",
					j + i, raw_data->delta_data[j + i]);
			if (raw_data->delta_data[i * info->cap_info.y_node_num + j] < min &&
				raw_data->delta_data[i * info->cap_info.y_node_num + j] != 0)
				min = raw_data->delta_data[i * info->cap_info.y_node_num + j];
			if (raw_data->delta_data[i * info->cap_info.y_node_num + j] > max)
				max = raw_data->delta_data[i * info->cap_info.y_node_num + j];

		}
	}

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%d,%d\n", min, max);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: \"%s\"(%d)\n", __func__, finfo->cmd_buff,
				strlen(finfo->cmd_buff));

	return;
}

static void get_delta(void *device_data)
{
	struct zt7554_ts_info *info = (struct zt7554_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	set_default_result(info);

	x_node = finfo->cmd_param[0];
	y_node = finfo->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", "abnormal");
		set_cmd_result(info, finfo->cmd_buff,
						strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
		info->factory_info->cmd_state = FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->delta_data[node_num];
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%u", val);
	set_cmd_result(info, finfo->cmd_buff,
					strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	info->factory_info->cmd_state = OK;

	dev_dbg(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
				strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct zt7554_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (finfo->cmd_is_running == true) {
		dev_err(&client->dev, "%s: other cmd is running\n", __func__);
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&finfo->cmd_lock);
	finfo->cmd_is_running = true;
	mutex_unlock(&finfo->cmd_lock);

	finfo->cmd_state = RUNNING;

	for (i = 0; i < ARRAY_SIZE(finfo->cmd_param); i++)
		finfo->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;

	memset(finfo->cmd, 0x00, ARRAY_SIZE(finfo->cmd));
	memcpy(finfo->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &finfo->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &finfo->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				finfo->cmd_param[param_cnt] = (int)simple_strtol(buff, NULL, 10);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_dbg(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);

	tsp_cmd_ptr->cmd_func(info);

	mutex_lock(&finfo->cmd_lock);
	finfo->cmd_is_running = false;
	mutex_unlock(&finfo->cmd_lock);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct zt7554_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	dev_dbg(&client->dev, "tsp cmd: status:%d\n", finfo->cmd_state);

	if (finfo->cmd_state == WAITING)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "WAITING");
	else if (finfo->cmd_state == RUNNING)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "RUNNING");
	else if (finfo->cmd_state == OK)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "OK");
	else if (finfo->cmd_state == FAIL)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "FAIL");
	else if (finfo->cmd_state == NOT_APPLICABLE)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "NOT_APPLICABLE");

	return snprintf(buf, sizeof(finfo->cmd_buff), "%s\n", finfo->cmd_buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct zt7554_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	dev_dbg(&client->dev, "tsp cmd: result: %s\n", finfo->cmd_result);

	finfo->cmd_state = WAITING;

	return snprintf(buf, sizeof(finfo->cmd_result), "%s\n", finfo->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

#ifdef SUPPORTED_TOUCH_KEY
static ssize_t show_touchkey_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zt7554_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);

	dev_dbg(&client->dev, "%s: key threshold = %d\n", __func__, cap->key_threshold);

	return snprintf(buf, 41, "%d", cap->key_threshold);
#endif
}

static ssize_t show_touchkey_sensitivity(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct zt7554_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u16 val;
	int ret;
	int i;

	if (!strcmp(attr->attr.name, "touchkey_recent"))
		i = 0;
	else if (!strcmp(attr->attr.name, "touchkey_back"))
		i = 1;
	else {
		dev_err(&client->dev, "%s: Invalid attribute\n", __func__);
		goto err_out;
	}
	ret = read_data(client, ZT7554_BTN_WIDTH + i, (u8 *)&val, 2);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read %d's key sensitivity\n", i);
		goto err_out;
	}

	dev_info(&client->dev, "%d's key sensitivity = %d\n", i, val);

	return snprintf(buf, 6, "%d", val);

err_out:
	return sprintf(buf, "NG");
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, show_touchkey_threshold, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, show_touchkey_sensitivity, NULL);
static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent.attr,
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int init_sec_factory(struct zt7554_ts_info *info)
{
	struct device *factory_ts_dev;
#ifdef SUPPORTED_TOUCH_KEY
	struct device *factory_tk_dev;
#endif
	struct tsp_factory_info *factory_info;
	struct tsp_raw_data *raw_data;
	int ret;
	int i;

	factory_info = kzalloc(sizeof(struct tsp_factory_info), GFP_KERNEL);
	if (unlikely(!factory_info)) {
		dev_err(&info->client->dev, "failed to allocate factory_info\n");
		ret = -ENOMEM;
		goto err_alloc1;
	}

	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		dev_err(&info->client->dev, "failed to allocate raw_data\n");
		ret = -ENOMEM;
		goto err_alloc2;
	}

	INIT_LIST_HEAD(&factory_info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &factory_info->cmd_list_head);

	factory_ts_dev = device_create(sec_class, NULL, 0, info, "tsp");
	if (unlikely(!factory_ts_dev)) {
		dev_err(&info->client->dev, "ailed to create factory dev\n");
		ret = -ENODEV;
		goto err_create_device;
	}

#ifdef SUPPORTED_TOUCH_KEY
	factory_tk_dev = device_create(sec_class, NULL, 0, info, "sec_touchkey");
	if (IS_ERR(factory_tk_dev)) {
		dev_err(&info->client->dev, "failed to create factory dev\n");
		ret = -ENODEV;
		goto err_create_device;
	}
#endif

	ret = sysfs_create_group(&factory_ts_dev->kobj, &touchscreen_attr_group);
	if (unlikely(ret)) {
		dev_err(&info->client->dev, "Failed to create touchscreen sysfs group\n");
		goto err_create_sysfs;
	}

#ifdef SUPPORTED_TOUCH_KEY
	ret = sysfs_create_group(&factory_tk_dev->kobj, &touchkey_attr_group);
	if (unlikely(ret)) {
		dev_err(&info->client->dev, "Failed to create touchkey sysfs group\n");
		goto err_create_sysfs;
	}
#endif

	mutex_init(&factory_info->cmd_lock);
	factory_info->cmd_is_running = false;

	info->factory_info = factory_info;
	info->raw_data = raw_data;

	return ret;

err_create_sysfs:
err_create_device:
	kfree(raw_data);
err_alloc2:
	kfree(factory_info);
err_alloc1:

	return ret;
}
#endif /* end of SEC_FACTORY_TEST */

#ifdef CONFIG_OF
static const struct of_device_id tsp_dt_ids[] = {
	{ .compatible = "Zinitix,zt7554_ts", },
	{},
};
MODULE_DEVICE_TABLE(of, tsp_dt_ids);
#else
#define tsp_dt_ids NULL
#endif

static int zt7554_ts_probe_dt(struct device_node *np, struct device *dev,
					struct zt7554_ts_platform_data *pdata)
{
	int ret;

	if (!np)
		return -EINVAL;

	/* gpio irq */
	pdata->gpio_int = of_get_named_gpio(np, "gpios", 0);
	if (pdata->gpio_int < 0) {
		dev_err(dev, "failed to get irq number\n");
		return -EINVAL;
	}
	ret = gpio_request(pdata->gpio_int, "zt7554_irq");
	if (ret < 0) {
		dev_err(dev, "failed to request gpio_irq\n");
		return -EINVAL;
	}
	gpio_direction_input(pdata->gpio_int);

#if 0 /* FIXME: use *.h format firmware file */
	/* built-in firmware */
	ret = of_property_read_string(np, "zt7554,fw_name", &pdata->fw_name);
	if (ret < 0) {
		dev_err(dev, "failed to get built-in firmware path\n");
		return -EINVAL;
	}
#endif

	/* external firmware */
	ret = of_property_read_string(np, "zt7554,ext_fw_name", &pdata->ext_fw_name);
	if (ret < 0) {
		dev_err(dev, "failed to get external firmware path!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "zt7554,x_resolution", &pdata->x_resolution);
	if (ret < 0) {
		dev_err(dev, "failed to get x_resolution\n");
		return ret;
	}

	ret = of_property_read_u32(np, "zt7554,y_resolution", &pdata->y_resolution);
	if (ret < 0) {
		dev_err(dev, "failed to get y_resolution\n");
		return ret;
	}

	ret = of_property_read_u32(np, "zt7554,supply_type", &pdata->tsp_supply_type);
	if (ret < 0) {
		dev_err(dev, "failed to get tsp supply type. default: regulator\n");
		pdata->tsp_supply_type = TSP_REGULATOR_SUPPLY;
	}

	ret = of_property_read_u32(np, "gpio_ctrl_pin", &pdata->gpio_ctrl_pin);
	if (ret < 0) {
		dev_err(dev, "failed to get tsp control pin\n");
		pdata->gpio_ctrl_pin = -1;
	}

//	pdata->orientation = 4;
	pdata->tsp_power = zt7554_power;
	return 0;

}
static int zt7554_ts_probe(struct i2c_client *client, const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct zt7554_ts_platform_data *pdata = client->dev.platform_data;
	struct zt7554_ts_info *info;
	struct input_dev *input_dev;
	int ret = -1;
	int i;
	struct device_node *np = client->dev.of_node;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = zt7554_ts_probe_dt(np, &client->dev, pdata);
		if (ret)
			goto err_no_platform_data;

	} else if (!pdata) {
		dev_err(&client->dev, "Not exist platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Not compatible i2c function\n");
		ret = -EIO;
		goto err_no_platform_data;
	}

	info = kzalloc(sizeof(struct zt7554_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_no_platform_data;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		goto err_alloc;
	}

	info->input_dev = input_dev;
	info->work_state = PROBE;

	/* power on */
	if (!zt7554_power_control(info, POWER_ON_SEQUENCE)) {
		ret = -EPERM;
		goto err_power_sequence;
	}

	memset(&info->reported_touch_info, 0x0, sizeof(struct point_info));

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;

	if (!init_touch(info))
		goto err_input_unregister_device;

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		info->button[i] = ICON_BUTTON_UNCHANGE;

	snprintf(info->phys, sizeof(info->phys), "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->phys = info->phys;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		set_bit(BUTTON_MAPPING_KEY[i], info->input_dev->keybit);

	if (pdata->orientation & TOUCH_XY_SWAP) {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			info->cap_info.MinX, info->cap_info.MaxX + ABS_PT_OFFSET, 0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			info->cap_info.MinY, info->cap_info.MaxY + ABS_PT_OFFSET, 0, 0);
	} else {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			info->cap_info.MinX, info->cap_info.MaxX + ABS_PT_OFFSET, 0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			info->cap_info.MinY, info->cap_info.MaxY + ABS_PT_OFFSET, 0, 0);
	}

	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	set_bit(MT_TOOL_FINGER, info->input_dev->keybit);
	input_mt_init_slots(info->input_dev, info->cap_info.multi_fingers, 0);

	input_set_drvdata(info->input_dev, info);
	ret = input_register_device(info->input_dev);
	if (ret) {
		dev_err(&info->client->dev, "unable to register input device\n");
		goto err_input_register_device;
	}

	info->work_state = NOTHING;
#if TOUCH_BOOSTER
	info->finger_cnt = 0;
#endif
	sema_init(&info->work_lock, 1);

#if ESD_TIMER_INTERVAL
	spin_lock_init(&info->lock);
	INIT_WORK(&info->tmr_work, ts_tmr_work);
	esd_tmr_workqueue =
		create_singlethread_workqueue("esd_tmr_workqueue");

	if (!esd_tmr_workqueue) {
		dev_err(&client->dev, "Failed to create esd tmr work queue\n");
		ret = -EPERM;
		goto err_esd_input_unregister_device;
	}

	esd_timer_init(info);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	info->irq = gpio_to_irq(pdata->gpio_int);
	if (info->irq < 0) {
		dev_err(&client->dev, "failed to get gpio_to_irq\n");
		goto err_gpio_irq;
	}
	ret = request_threaded_irq(info->irq, NULL, zt7554_touch_work,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , ZT7554_TS_DEVICE, info);

	if (ret) {
		dev_err(&client->dev, "failed to request irq.\n");
		goto err_request_irq;
	}
	sprd_i2c_ctl_chg_clk(1, 400000); /* up h/w i2c 1 400k */
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = zt7554_ts_early_suspend;
	info->early_suspend.resume = zt7554_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&client->dev);
#endif

	sema_init(&info->raw_data_lock, 1);

#if ZINITIX_MISC_DEBUG
	ret = misc_register(&touch_misc_device);
	if (ret) {
		dev_err(&client->dev, "Failed to register touch misc device\n");
		goto err_misc_register;
	}
#endif

#ifdef SEC_FACTORY_TEST
	ret = init_sec_factory(info);
	if (ret) {
		dev_err(&client->dev, "Failed to init sec factory device\n");

		goto err_kthread_create_failed;
	}
#endif
	dev_info(&client->dev, "zinitix touch probe done.\n");

	return 0;

#ifdef SEC_FACTORY_TEST
err_kthread_create_failed:
	kfree(info->factory_info);
	kfree(info->raw_data);
#endif
#if ZINITIX_MISC_DEBUG
err_misc_register:
#endif
	free_irq(info->irq, info);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif
err_request_irq:
err_gpio_irq:
#if ESD_TIMER_INTERVAL
err_esd_input_unregister_device:
#endif
err_input_unregister_device:
	input_unregister_device(info->input_dev);
err_input_register_device:
	tsp_charger_status_cb = NULL;
err_power_sequence:
	input_free_device(info->input_dev);
err_alloc:
	kfree(info);
err_no_platform_data:
	if (IS_ENABLED(CONFIG_OF))
		devm_kfree(&client->dev, (void *)pdata);

	dev_info(&client->dev, "Failed to probe\n");
	return ret;
}

static int zt7554_ts_remove(struct i2c_client *client)
{
	struct zt7554_ts_info *info = i2c_get_clientdata(client);
	struct zt7554_ts_platform_data *pdata = info->pdata;

	disable_irq(info->irq);
	down(&info->work_lock);

	info->work_state = REMOVE;

#ifdef SEC_FACTORY_TEST
	kfree(info->factory_info);
	kfree(info->raw_data);
#endif
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	write_reg(info->client, ZT7554_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
	destroy_workqueue(esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);

#if ZINITIX_MISC_DEBUG
	misc_deregister(&touch_misc_device);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);

	tsp_charger_status_cb = NULL;

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	up(&info->work_lock);
	kfree(info);

	return 0;
}

void zt7554_ts_shutdown(struct i2c_client *client)
{
	struct zt7554_ts_info *info = i2c_get_clientdata(client);

	disable_irq(info->irq);
	down(&info->work_lock);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	esd_timer_stop(info);
#endif
	up(&info->work_lock);
	zt7554_power_control(info, POWER_OFF);
}

static struct i2c_device_id zt7554_idtable[] = {
	{ZT7554_TS_DEVICE, 0},
	{ }
};

static const struct dev_pm_ops zt7554_ts_pm_ops = {
#if defined(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(zt7554_ts_suspend, zt7554_ts_resume, NULL)
#else
	.suspend = zt7554_ts_suspend,
	.resume =  zt7554_ts_resume,
#endif
};

static struct i2c_driver zt7554_ts_driver = {
	.probe	= zt7554_ts_probe,
	.remove	= zt7554_ts_remove,
	.shutdown = zt7554_ts_shutdown,
	.id_table	= zt7554_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZT7554_TS_DEVICE,
		.of_match_table = tsp_dt_ids,
#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm		= &zt7554_ts_pm_ops,
#endif
	},
};

static int __init zt7554_ts_init(void)
{
	return i2c_add_driver(&zt7554_ts_driver);
}

static void __exit zt7554_ts_exit(void)
{
	i2c_del_driver(&zt7554_ts_driver);
}

module_init(zt7554_ts_init);
module_exit(zt7554_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_AUTHOR("<mika.kim@samsung.com>");
MODULE_LICENSE("GPL");
