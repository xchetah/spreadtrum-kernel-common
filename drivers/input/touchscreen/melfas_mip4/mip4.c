/*
 * MELFAS MIP4 (MCS6000) Touchscreen 
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.c : Main functions
 *
 *
 * Version : 2015.04.28
 *
 */

#include "mip4.h"

#if MIP_USE_WAKEUP_GESTURE
struct wake_lock mip_wake_lock;
#endif

static int cal_mode;
static int get_boot_mode(char *str)
{

	get_option(&str, &cal_mode);
	printk("get_boot_mode, uart_mode : %d\n", cal_mode);
	return 1;
}
__setup("calmode=",get_boot_mode);


/**
* Reboot chip
*
* Caution : IRQ must be disabled before mip_reboot and enabled after mip_reboot.
*/
void mip_reboot(struct mip_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	i2c_lock_adapter(adapter);

	mip_power_off(info);
	mip_power_on(info);

	i2c_unlock_adapter(adapter);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/**
* I2C Read
*/

int mip_i2c_read(struct mip_ts_info *info, char *write_buf, unsigned int write_len, 
						char *read_buf, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	/*
	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = read_len,
		},
	};

	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			goto DONE;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}
		else if(res != ARRAY_SIZE(msg)){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	*/

	if(mip_i2c_write(info, write_buf, write_len)){
		return -1;
	}
	

	while(retry--){
		res = i2c_master_recv(info->client, read_buf, read_len);
		
		if(res == read_len){
			return 0;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv - errno [%d]\n", __func__, res);
		}
		else if(res != read_len){
			dev_err(&info->client->dev, "%s [ERROR] length mismatch - read[%d] result[%d]\n", __func__, read_len, res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	mip_reboot(info);
	return -1;
}


/**
* I2C Read (Continue)
*/
int mip_i2c_read_next(struct mip_ts_info *info, char *write_buf, unsigned int write_len, 
					char *read_buf, int start_idx, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;
	u8 rbuf[read_len];

	/*
	while(retry--){
		res = i2c_master_recv(info->client, rbuf, read_len);
		
		if(res == read_len){
			goto DONE;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv - errno [%d]\n", __func__, res);
		}
		else if(res != read_len){
			dev_err(&info->client->dev, "%s [ERROR] length mismatch - read[%d] result[%d]\n", __func__, read_len, res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}
	*/

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = read_len,
		},
	};
	
	while(retry--){	
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if(res == ARRAY_SIZE(msg)){
			memcpy(&read_buf[start_idx], rbuf, read_len);
			return 0;
		}else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		}else if(res != ARRAY_SIZE(msg)){
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%d] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	mip_reboot(info);
	return -1;
}

/**
* I2C Write
*/
int mip_i2c_write(struct mip_ts_info *info, char *write_buf, unsigned int write_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	while(retry--){
		res = i2c_master_send(info->client, write_buf, write_len);

		if(res == write_len){
			return 0;
		}
		else if(res < 0){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		}
		else if(res != write_len){
			dev_err(&info->client->dev, "%s [ERROR] length mismatch - write[%d] result[%d]\n", __func__, write_len, res);
		}			
		else{
			dev_err(&info->client->dev, "%s [ERROR] unknown error [%d]\n", __func__, res);
		}
	}

	mip_reboot(info);
	return -1;
}

/**
* Enable device
*/
int mip_enable(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	if (info->enabled){
		dev_err(&info->client->dev, "%s [ERROR] device already enabled\n", __func__);
		goto EXIT;
	}

#if MIP_USE_WAKEUP_GESTURE
	mip_set_power_state(info, MIP_CTRL_POWER_ACTIVE);

	if(wake_lock_active(&mip_wake_lock)){
		wake_unlock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_unlock\n", __func__);
	}
	
	info->nap_mode = false;
	dev_dbg(&info->client->dev, "%s - nap mode : off\n", __func__);	
#else	
	mip_power_on(info);
#endif

#if 1
	if(info->disable_esd == true){
		//Disable ESD alert
		mip_disable_esd_alert(info);
	}	
#endif

	mutex_lock(&info->lock);

	enable_irq(info->client->irq);
	info->enabled = true;

	mutex_unlock(&info->lock);
	
EXIT:
	dev_info(&info->client->dev, MIP_DEVICE_NAME" - Enabled\n");
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Disable device
*/
int mip_disable(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	if (!info->enabled){
		dev_err(&info->client->dev, "%s [ERROR] device already disabled\n", __func__);
		goto EXIT;
	}
	
	mip_clear_input(info);

#if MIP_USE_WAKEUP_GESTURE
	info->wakeup_gesture_code = 0;

	mip_set_wakeup_gesture_type(info, MIP_EVENT_GESTURE_ALL);
	mip_set_power_state(info, MIP_CTRL_POWER_LOW);
	
	info->nap_mode = true;
	dev_dbg(&info->client->dev, "%s - nap mode : on\n", __func__);

	if(!wake_lock_active(&mip_wake_lock)) {
		wake_lock(&mip_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_lock\n", __func__);
	}
#else
	mutex_lock(&info->lock);

	disable_irq(info->client->irq);
	mip_power_off(info);

	mutex_unlock(&info->lock);	
#endif

	info->enabled = false;

EXIT:	
	dev_info(&info->client->dev, MIP_DEVICE_NAME" - Disabled\n");
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/**
* Get ready status
*/
int mip_get_ready_status(struct mip_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_READY_STATUS;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}
	ret = rbuf[0];

	//check status
	if((ret == MIP_CTRL_STATUS_NONE) || (ret == MIP_CTRL_STATUS_LOG) || (ret == MIP_CTRL_STATUS_READY)){
		//dev_dbg(&info->client->dev, "%s - status [0x%02X]\n", __func__, ret);
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR] Unknown status [0x%02X]\n", __func__, ret);
		goto ERROR;
	}

	if(ret == MIP_CTRL_STATUS_LOG){
		//skip log event
		wbuf[0] = MIP_R0_LOG;
		wbuf[1] = MIP_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if(mip_i2c_write(info, wbuf, 3)){
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		}
	}
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/**
* Read chip firmware version
*/
int mip_get_fw_version(struct mip_ts_info *info, u8 *ver_buf)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;
	
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_VERSION_BOOT;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 8)){
		goto ERROR;
	};

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf, 0xFF, sizeof(ver_buf));
	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Read chip firmware version for u16
*/
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;
	
	if(mip_get_fw_version(info, rbuf)){
		goto ERROR;
	}

	for(i = 0; i < MIP_FW_MAX_SECT_NUM; i++){
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];
	}	
	
	return 0;

ERROR:
	memset(ver_buf_u16, 0xFFFF, sizeof(ver_buf_u16));
	
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;	
}

/**
* Set power state
*/
int mip_set_power_state(struct mip_ts_info *info, u8 mode)
{
	u8 wbuf[3];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - mode[%02X]\n", __func__, mode);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Set wake-up gesture type
*/
int mip_set_wakeup_gesture_type(struct mip_ts_info *info, u32 type)
{
	u8 wbuf[6];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - type[%08X]\n", __func__, type);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_GESTURE_TYPE;
	wbuf[2] = (type >> 24) & 0xFF;
	wbuf[3] = (type >> 16) & 0xFF;
	wbuf[4] = (type >> 8) & 0xFF;
	wbuf[5] = type & 0xFF;
	if(mip_i2c_write(info, wbuf, 6)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Disable ESD alert
*/
int mip_disable_esd_alert(struct mip_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[4];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		goto ERROR;
	}	

	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		goto ERROR;
	}	

	if(rbuf[0] != 1){
		dev_dbg(&info->client->dev, "%s [ERROR] failed\n", __func__);
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Alert event handler - ESD
*/
static int mip_alert_handler_esd(struct mip_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - frame_cnt[%d]\n", __func__, frame_cnt);

	if(frame_cnt == 0){
		//sensor crack, not ESD
		info->esd_cnt++;
		dev_dbg(&info->client->dev, "%s - esd_cnt[%d]\n", __func__, info->esd_cnt);

		if(info->disable_esd == true){
			mip_disable_esd_alert(info);
			info->esd_cnt = 0;
		}
		else if(info->esd_cnt > ESD_COUNT_FOR_DISABLE){
			//Disable ESD alert
			if(mip_disable_esd_alert(info)){
			}
			else{
				info->disable_esd = true;
				info->esd_cnt = 0;
			}
		}
		else{
			//Reset chip
			mip_reboot(info);
		}
	}
	else{
		//ESD detected
		//Reset chip
		mip_reboot(info);
		info->esd_cnt = 0;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:	
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

/**
* Alert event handler - Wake-up
*/
static int mip_alert_handler_wakeup(struct mip_ts_info *info, u8 *rbuf)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(mip_wakeup_event_handler(info, rbuf)){
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/**
* Interrupt handler
*/
static irqreturn_t mip_interrupt(int irq, void *dev_id)
{
	struct mip_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 wbuf[8];
	u8 rbuf[256];
	unsigned int size = 0;
	u8 category = 0;
	u8 alert_type = 0;

	//dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Read packet info
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_INFO;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto ERROR;
	}

	//Check packet info
	size = (rbuf[0] & 0x7F);	
	category = ((rbuf[0] >> 7) & 0x1);
	//dev_dbg(&client->dev, "%s - packet info : size[%d] category[%d]\n", __func__, size, category);

	//Check size
	if(size <= 0){
		goto EXIT;
	}

	//Read packet data
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_PACKET_DATA;
	if(mip_i2c_read(info, wbuf, 2, rbuf, size)){
		dev_err(&client->dev, "%s [ERROR] Read packet data\n", __func__);
		goto ERROR;
	}

	//Event handler
	if(category == 0){
		//Touch event
		info->esd_cnt = 0;
		
		mip_input_event_handler(info, size, rbuf);
	}
	else{
		//Alert event
		alert_type = rbuf[0];
		
		dev_dbg(&client->dev, "%s - alert type [%d]\n", __func__, alert_type);
				
		if(alert_type == MIP_ALERT_ESD){
			//ESD detection
			if(mip_alert_handler_esd(info, rbuf)){
				goto ERROR;
			}
		}
		else if(alert_type == MIP_ALERT_WAKEUP){
			//Wake-up gesture
			if(mip_alert_handler_wakeup(info, rbuf)){
				goto ERROR;
			}
		}
		else{
			dev_err(&client->dev, "%s [ERROR] Unknown alert type [%d]\n", __func__, alert_type);
			goto ERROR;
		}		
	}

EXIT:
	//dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;
	
ERROR:
	if(RESET_ON_EVENT_ERROR){	
		dev_info(&client->dev, "%s - Reset on error\n", __func__);
		
		mip_disable(info);
		mip_clear_input(info);
		mip_enable(info);
	}

	dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return IRQ_HANDLED;
}

int mip_fw_update_from_kernel(struct mip_ts_info *info, bool force)
{
	const struct firmware *fw;
	int retires = 3;
	int ret = -1;

	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);
	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		return ret;
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	do {
		ret = mip_flash_fw(info, fw->data, fw->size, force, true);
		if (ret >= fw_err_none)
			break;
	} while (--retires);

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	if (!retires) {
		dev_err(&info->client->dev, "%s [ERROR] mip_flash_fw failed\n", __func__);
		ret = -1;
	}
	release_firmware(fw);

	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);
printk("~~~~ %s %d, %d\n", __FUNCTION__, __LINE__, ret);

	return ret;
}

int mip_fw_update_from_storage(struct mip_ts_info *info, const char *path, bool force)
{
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	unsigned char *fw_data;
	int ret = 0;

	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, path);
		ret = fw_err_file_open;
		goto ERROR;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size == 0) {
		dev_err(&info->client->dev, "fw size error!\n");
		ret = -EIO;
		goto ERROR1;
	}

	fw_data = kzalloc(fw_size, GFP_KERNEL);
	if (!fw_data) {
		dev_err(&info->client->dev, "failed to allocate fw_data!\n");
		ret = -ENOMEM;
		goto ERROR1;
	}
	nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
	mdelay(20);
	if (nread != fw_size) {
		dev_err(&info->client->dev, "vfs_read error\n");
		ret = fw_err_file_read;
	} else
		ret = mip_flash_fw(info, fw_data, fw_size, force, true);
	if (!ret)
		dev_info(&info->client->dev, "succeeded update external firmware\n");
	kfree(fw_data);
ERROR1:
	filp_close(fp, current->files);

ERROR:
	set_fs(old_fs);
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);
	if (ret)
		dev_info(&info->client->dev, "failed to update external firmware\n");
	return ret;
}

static ssize_t mip_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;
	
	memset(info->print_buf, 0, PAGE_SIZE);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	ret = mip_fw_update_from_storage(info, info->pdata->ext_fw_name, true);
	
	switch(ret){
		case fw_err_none:
			sprintf(data, "F/W update success.\n");
			break;
		case fw_err_uptodate:
			sprintf(data, "F/W is already up-to-date.\n");
			break;
		case fw_err_download:
			sprintf(data, "F/W update failed : Download error\n");
			break;
		case fw_err_file_type:
			sprintf(data, "F/W update failed : File type error\n");
			break;
		case fw_err_file_open:			
			sprintf(data, "F/W update failed : File open error [%s]\n", info->fw_path_ext);
			break;
		case fw_err_file_read:
			sprintf(data, "F/W update failed : File read error\n");
			break;
		default:
			sprintf(data, "F/W update failed.\n");
			break;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	
	strcat(info->print_buf, data);
	result = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return result;
}
static DEVICE_ATTR(fw_update, 0666, mip_sys_fw_update, NULL);

/**
* Sysfs attr info
*/
static struct attribute *mip_attrs[] = {
	&dev_attr_fw_update.attr,
	NULL,
};

/**
* Sysfs attr group info
*/
static const struct attribute_group mip_attr_group = {
	.attrs = mip_attrs,
};

/**
* Initial config
*/
static int mip_init_config(struct mip_ts_info *info)
{
	u8 wbuf[8];
	u8 rbuf[64];
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Product name
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mip_i2c_read(info, wbuf, 2, rbuf, 16);
	memcpy(info->product_name, rbuf, 16);
	dev_dbg(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);

	//Firmware version
//	mip_get_fw_version(info, rbuf);
//	memcpy(info->fw_version, rbuf, 8);
//	dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", __func__, info->fw_version[0], info->fw_version[1], info->fw_version[2], info->fw_version[3], info->fw_version[4], info->fw_version[5], info->fw_version[6], info->fw_version[7]);	

	//Resolution
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);

	//Set resolution using platform data
	info->max_x = info->pdata->max_x;
	info->max_y = info->pdata->max_y;

	//Node info
	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	dev_dbg(&info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n", __func__, info->node_x, info->node_y, info->node_key);


	//Protocol
#if MIP_AUTOSET_EVENT_FORMAT
	wbuf[0] = MIP_R0_EVENT;
	wbuf[1] = MIP_R1_EVENT_SUPPORTED_FUNC;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);
	info->event_format = (rbuf[4]) | (rbuf[5] << 8);
	info->event_size = rbuf[6];
#else
	info->event_format = 0;
	info->event_size = 6;
#endif
	dev_dbg(&info->client->dev, "%s - event_format[%d] event_size[%d] \n", __func__, info->event_format, info->event_size);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
	
//ERROR:
	//dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	//return 1;
}

/**
* Initialize driver
*/
static int mip_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mip_ts_info *info;
	struct input_dev *input_dev;
	struct melfas_platform_data *pdata;
	int ret = 0;
	u8 rbuf[8];

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)){
		dev_err(&client->dev, "%s [ERROR] i2c_check_functionality\n", __func__);
		return -EIO;
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	if (IS_ENABLED(CONFIG_OF)) {
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct melfas_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "failed to allocate pdata!\n");
			return -ENOMEM;
		}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
		ret = mip_parse_devicetree(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "failed to parse_dt\n");
			return ret;
		}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	} else {
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "no platform data\n");
			return -EINVAL;
		}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	info = kzalloc(sizeof(struct mip_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "failed to allocate info data\n");
		return -ENOMEM;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	info->pdata = pdata;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);

	info->client = client;
	info->input_dev = input_dev;
	info->irq = -1;
	info->init = true;
	info->power = -1;

	mutex_init(&info->lock);

	//Init input device
	info->input_dev->name = "MELFAS_" CHIP_NAME "_Touchscreen";
	snprintf(info->phys, sizeof(info->phys), "%s/input1", info->input_dev->name);

	info->input_dev->phys = info->phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &client->dev;

	//Create device
	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input device\n");
		ret = -EIO;
		goto err_input_register_device;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);

	mip_power_on(info);

	ret = mip_get_fw_version(info, rbuf);
	if (ret) {
		dev_err(&client->dev, "failed to read fw version!\n");
		ret = -EAGAIN;
		mip_power_off(info);
		goto err_fw_update;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);

	ret = mip_fw_update_from_kernel(info, false);
	if (ret < 0) {
		dev_err(&client->dev, "failed to update firmware\n");
			ret = -EAGAIN;
			goto err_fw_update;
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	mip_init_config(info);

	//Config input interface	
	mip_config_input(info);

	client->irq = gpio_to_irq(info->pdata->gpio_intr);
	ret = request_threaded_irq(client->irq, NULL, mip_interrupt,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, MIP_DEVICE_NAME, info);
	if (ret) {
		dev_err(&client->dev, "failed to register irq handler!\n");
		goto err_req_irq;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);

	disable_irq(client->irq);
	info->irq = client->irq;
	sprd_i2c_ctl_chg_clk(1, 100000); // up h/w i2c 1 400k

#if MIP_USE_WAKEUP_GESTURE
	//Wake-lock for wake-up gesture mode
	wake_lock_init(&mip_wake_lock, WAKE_LOCK_SUSPEND, "mip_wake_lock");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +1;
	info->early_suspend.suspend = mip_early_suspend;
	info->early_suspend.resume = mip_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	//Enable device
	mip_enable(info);

#if MIP_USE_DEV
	//Create dev node (optional)
	if(mip_dev_create(info)){
		dev_err(&client->dev, "%s [ERROR] mip_dev_create\n", __func__);
		ret = -EAGAIN;
		goto err_dev_mode;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	//Create dev
	info->class = class_create(THIS_MODULE, MIP_DEVICE_NAME);
	device_create(info->class, NULL, info->mip_dev, NULL, MIP_DEVICE_NAME);
#endif

#if MIP_USE_SYS
	if (mip_sysfs_cmd_create(info)){
		dev_err(&client->dev, "%s [ERROR] mip_sysfs_cmd_create\n", __func__);
		ret = -EAGAIN;
		goto err_test_mode;
	}
printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
#endif

#if MIP_USE_CMD
	//Create sysfs for command mode (optional)
	if (mip_sysfs_cmd_create(info)){
		dev_err(&client->dev, "%s [ERROR] mip_sysfs_cmd_create\n", __func__);
		ret = -EAGAIN;
		goto err_cmd_mode;
	}
#endif

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	//Create sysfs
	if (sysfs_create_group(&client->dev.kobj, &mip_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		ret = -EAGAIN;
		goto err_group;
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	if (sysfs_create_link(NULL, &client->dev.kobj, MIP_DEVICE_NAME)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		ret = -EAGAIN;
		goto err_link;
	}

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	dev_info(&client->dev, "MELFAS " CHIP_NAME " probe done.\n");
	return 0;

err_link:
err_group:
#if MIP_USE_CMD
err_cmd_mode:
#endif
#if MIP_USE_SYS
err_test_mode:
#endif
#if MIP_USE_DEV
err_dev_mode:
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif
	free_irq(client->irq, info);
err_req_irq:
err_fw_update:
	input_unregister_device(input_dev);
err_input_register_device:
	input_free_device(input_dev);
	input_dev = NULL;
	gpio_free(info->pdata->gpio_intr);
err_input_alloc:
	kfree(info);

	dev_err(&client->dev, "MELFAS " CHIP_NAME " probe failed.\n");

printk("~~~~ %s %d\n", __FUNCTION__, __LINE__);
	return ret;
}

/**
* Remove driver
*/
static int mip_remove(struct i2c_client *client)
{
	struct mip_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0){
		free_irq(info->irq, info);
	}

#if MIP_USE_CMD
	mip_sysfs_cmd_remove(info);
#endif

#if MIP_USE_SYS
	mip_sysfs_remove(info);
#endif

	sysfs_remove_group(&info->client->dev.kobj, &mip_attr_group);
	sysfs_remove_link(NULL, MIP_DEVICE_NAME);
	kfree(info->print_buf);

#if MIP_USE_DEV
	device_destroy(info->class, info->mip_dev);
	class_destroy(info->class);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	input_unregister_device(info->input_dev);

	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
/**
* Device suspend event handler
*/
int mip_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	
	dev_dbg(&client->dev, "%s [START]\n", __func__);
	
	mip_disable(info);
	
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return 0;

}

/**
* Device resume event handler
*/
int mip_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	mip_enable(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return ret;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/**
* Early suspend handler
*/
void mip_early_suspend(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);
	
	mip_suspend(&info->client->dev);
}

/**
* Late resume handler
*/
void mip_late_resume(struct early_suspend *h)
{
	struct mip_ts_info *info = container_of(h, struct mip_ts_info, early_suspend);

	mip_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
/**
* PM info
*/
const struct dev_pm_ops mip_pm_ops = {
#if 0
	SET_SYSTEM_SLEEP_PM_OPS(mip_suspend, mip_resume)
#else
	.suspend	= mip_suspend,
	.resume = mip_resume,
#endif
};
#endif

#if MIP_USE_DEVICETREE
/**
* Device tree match table
*/
static const struct of_device_id mip_match_table[] = {
	{ .compatible = "melfas,"MIP_DEVICE_NAME,},
	{},
};
MODULE_DEVICE_TABLE(of, mip_match_table);
#endif

/**
* I2C Device ID
*/
static const struct i2c_device_id mip_id[] = {
	{MIP_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mip_id);

/**
* I2C driver info
*/
static struct i2c_driver mip_driver = {
	.id_table = mip_id,
	.probe = mip_probe,
	.remove = mip_remove,
	.driver = {
		.name = MIP_DEVICE_NAME,
		.owner = THIS_MODULE,
#if MIP_USE_DEVICETREE
		.of_match_table = mip_match_table,
#endif
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm 	= &mip_pm_ops,
#endif
	},
};

/**
* Init driver
*/
//extern unsigned int lpcharge;

static int __init mip_init(void)
{
//	if (!lpcharge)
	return i2c_add_driver(&mip_driver);
//	else
//		return 0;
}

/**
* Exit driver
*/
static void __exit mip_exit(void)
{
	i2c_del_driver(&mip_driver);
}

module_init(mip_init);
module_exit(mip_exit);

MODULE_DESCRIPTION("MELFAS MIP4 (MCS6000) Touchscreen");
MODULE_VERSION("2015.04.28");
MODULE_AUTHOR("Jee, SangWon <jeesw@melfas.com>");
MODULE_LICENSE("GPL"); 

