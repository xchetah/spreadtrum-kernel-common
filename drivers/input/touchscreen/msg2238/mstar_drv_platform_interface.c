////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2014 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (??MStar Confidential Information??) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

/**
 *
 * @file    mstar_drv_platform_interface.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_platform_interface.h"
#include "mstar_drv_main.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_porting_layer.h"
#include <mach/regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

/*=============================================================*/
// EXTERN VARIABLE DECLARATION
/*=============================================================*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u32 g_GestureWakeupMode[2];
extern u8 g_GestureWakeupFlag;

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern u8 g_GestureDebugFlag;
extern u8 g_GestureDebugMode;
#endif //CONFIG_ENABLE_GESTURE_DEBUG_MODE

#endif //CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern u8 g_EnableTpProximity;
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

/*=============================================================*/
// GLOBAL VARIABLE DEFINITION
/*=============================================================*/

extern struct input_dev *g_InputDevice;

#ifdef TP_READ_VER
static u32 tp_version_code;
#endif

#ifdef CTP_EXCHANGE_KEY
static unsigned int tp_setkey_define = 1;
#endif

static struct regulator *vddsim2_regulator = NULL;

/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/

#ifdef CONFIG_ENABLE_NOTIFIER_FB
int MsDrvInterfaceTouchDeviceFbNotifierCallback(struct notifier_block *pSelf, unsigned long nEvent, void *pData)
{
    struct fb_event *pEventData = pData;
    int *pBlank;

    if (pEventData && pEventData->data && nEvent == FB_EVENT_BLANK)
    {
        pBlank = pEventData->data;

        if (*pBlank == FB_BLANK_UNBLANK)
        {
            DBG("*** %s() TP Resume ***\n", __func__);
            
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_EnableTpProximity == 1)
            {
                DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
                return 0;
            }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
            if (g_GestureDebugMode == 1)
            {
                DrvIcFwLyrCloseGestureDebugMode();
            }
#endif //CONFIG_ENABLE_GESTURE_DEBUG_MODE

            if (g_GestureWakeupFlag == 1)
            {
                DrvIcFwLyrCloseGestureWakeup();
            }
            else
            {
                DrvPlatformLyrEnableFingerTouchReport(); 
            }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

            DrvPlatformLyrTouchDevicePowerOn();

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
            DrvIcFwLyrRestoreFirmwareModeToLogDataMode(); // Mark this function call for avoiding device driver may spend longer time to resume from suspend state.
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
            DrvPlatformLyrEnableFingerTouchReport(); 
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
        }
        else if (*pBlank == FB_BLANK_POWERDOWN)
        {
            DBG("*** %s() TP Suspend ***\n", __func__);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_EnableTpProximity == 1)
            {
                DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
                return 0;
            }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
            if (g_GestureWakeupMode[0] != 0x00000000 || g_GestureWakeupMode[1] != 0x00000000)
            {
                DrvIcFwLyrOpenGestureWakeup(&g_GestureWakeupMode[0]);
                return 0;
            }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

            DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
            input_sync(g_InputDevice);

            DrvPlatformLyrDisableFingerTouchReport();
            DrvPlatformLyrTouchDevicePowerOff(); 
        }
    }

    return 0;
}

#else

void MsDrvInterfaceTouchDeviceSuspend(struct early_suspend *pSuspend)
{
    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    if (g_EnableTpProximity == 1)
    {
        DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
        return;
    }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_GestureWakeupMode[0] != 0x00000000 || g_GestureWakeupMode[1] != 0x00000000)
    {
        DrvIcFwLyrOpenGestureWakeup(&g_GestureWakeupMode[0]);
        return;
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

    DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
    input_sync(g_InputDevice);

    DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDevicePowerOff(); 
}

void MsDrvInterfaceTouchDeviceResume(struct early_suspend *pSuspend)
{
    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    if (g_EnableTpProximity == 1)
    {
        DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
        return;
    }
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    if (g_GestureDebugMode == 1)
    {
        DrvIcFwLyrCloseGestureDebugMode();
    }
#endif //CONFIG_ENABLE_GESTURE_DEBUG_MODE

    if (g_GestureWakeupFlag == 1)
    {
        DrvIcFwLyrCloseGestureWakeup();
    }
    else
    {
        DrvPlatformLyrEnableFingerTouchReport(); 
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
    
    DrvPlatformLyrTouchDevicePowerOn();
    
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    DrvIcFwLyrRestoreFirmwareModeToLogDataMode(); // Mark this function call for avoiding device driver may spend longer time to resume from suspend state.
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
    DrvPlatformLyrEnableFingerTouchReport(); 
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
}
#endif //CONFIG_ENABLE_NOTIFIER_FB


#ifdef TP_READ_VER
static ssize_t tpd_info_read(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	printk("***FW Version tpd_info_read = 0x%x ***\n", tp_version_code);
	
	copy_to_user(buf, (void *)&tp_version_code, sizeof(tp_version_code));

	return count;
}

static int tpd_info_open(struct inode *inode, struct file *f)
{
	return 0;
}

static int tpd_info_release(struct inode *inode, struct file *f)
{
	return 0;
}

static const struct file_operations tpd_info_fops = {
	.owner = THIS_MODULE,
	.open = tpd_info_open,
	.release = tpd_info_release,
	.read =  tpd_info_read,
};
static struct miscdevice tpd_info = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tp_ver",
	.fops = &tpd_info_fops,
};
#endif

#ifdef CTP_EXCHANGE_KEY
int MsKeyIsExchange(void)
{
	return tp_setkey_define;
}

static ssize_t ctp_key_define_write(struct file *f, char __user *buf, size_t count, loff_t *pos)
{
	if(copy_from_user(&tp_setkey_define, buf, sizeof(tp_setkey_define)))
	{
		printk("__ctp_key_define_write__: call copy_from_user() fail!!\r\n");
		return -EFAULT;
	}

	printk("__ctp_key_define_write__: tp_setkey_define = %d\r\n", tp_setkey_define);

	tp_setkey_define &= 0x1;

	return count;
}


static int ctp_key_define_open(struct inode *inode, struct file *f)
{
	return 0;
}

static int ctp_key_define_release(struct inode *inode, struct file *f)
{
	return 0;
}

static const struct file_operations ctp_key_define_fops = {
	.owner = THIS_MODULE,
	.open = ctp_key_define_open,
	.release = ctp_key_define_release,
	.write = ctp_key_define_write,
};

static struct miscdevice ctp_key_define_struct = {
	.name = "tp_setkey",
	.fops = &ctp_key_define_fops,
	.minor = MISC_DYNAMIC_MINOR,
};
#endif

extern int DrvFwCtrlHandleTestIIC(void);
extern u32 GetFirmwareVersionFor22xx(void);

/* probe function is used for matching and initializing input device */
s32 /*__devinit*/ MsDrvInterfaceTouchDeviceProbe(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
	s32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);

	if((nRetVal=DrvPlatformLyrTouchDeviceRequestGPIO()) < 0)
	{
		return nRetVal;
	}

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	//    DrvPlatformLyrTouchDeviceRegulatorPowerOn();
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

	sprd_i2c_ctl_chg_clk(pClient->adapter->nr, 100000);

	vddsim2_regulator = regulator_get(NULL, "vddsim2");
	if (IS_ERR(vddsim2_regulator))
	{
		DrvPlatformLyrTouchDeviceFreeGPIO();
		printk("touch_driver_probe: regulator_get vddsim2 error!!\n");
		return -1;
	}

	regulator_set_voltage(vddsim2_regulator, 2800000, 2800000);
	regulator_enable(vddsim2_regulator);

	DrvPlatformLyrTouchDevicePowerOn();

	if(DrvFwCtrlHandleTestIIC() < 0)
	{
		printk("%s: test iic bus fail!!\n", __func__);
		DrvPlatformLyrTouchDeviceFreeGPIO();
		regulator_disable(vddsim2_regulator);
		regulator_put(vddsim2_regulator);
		vddsim2_regulator = NULL;
		return -1;
	}

	DrvPlatformLyrInputDeviceInitialize(pClient);

	nRetVal = DrvMainTouchDeviceInitialize();
	if (nRetVal == -ENODEV)
	{
		printk("DrvMainTouchDeviceInitialize error: %d\r\n", nRetVal);
		DrvPlatformLyrTouchDeviceRemove(pClient);
		regulator_disable(vddsim2_regulator);
		regulator_put(vddsim2_regulator);
		vddsim2_regulator = NULL;
		return nRetVal;
	}

	DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler();

	DrvPlatformLyrTouchDeviceRegisterEarlySuspend();

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
	DrvIcFwLyrCheckFirmwareUpdateBySwId();
#endif //CONFIG_UPDATE_FIRMWARE_BY_SW_ID

#ifdef TP_READ_VER
	tp_version_code = GetFirmwareVersionFor22xx();

	if (misc_register(&tpd_info) < 0)
	{
		printk("msg2233_tp: tpd_info_device register failed\n");
	}
#endif

#ifdef CTP_EXCHANGE_KEY
	if (misc_register(&ctp_key_define_struct) < 0)
	{
		printk("Creat ctp_key_define_struct device file error!!\n");
	}
#endif

	DBG("*** MStar touch driver registered ***\n");

	return nRetVal;
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 /*__devexit*/ MsDrvInterfaceTouchDeviceRemove(struct i2c_client *pClient)
{
	DBG("*** %s() ***\n", __func__);

	regulator_disable(vddsim2_regulator);
	regulator_put(vddsim2_regulator);
	vddsim2_regulator = NULL;
	return DrvPlatformLyrTouchDeviceRemove(pClient);
}

void MsDrvInterfaceTouchDeviceSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
    DBG("*** %s() ***\n", __func__);

    DrvPlatformLyrSetIicDataRate(pClient, nIicDataRate);
}    
