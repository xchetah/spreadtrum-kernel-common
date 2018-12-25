////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2012 MStar Semiconductor, Inc.
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
 * @file    mstar_drv_sprd.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "mstar_drv_platform_interface.h"

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <soc/sprd/regulator.h>
#include <linux/regulator/consumer.h>
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

/*=============================================================*/
// CONSTANT VALUE DEFINITION
/*=============================================================*/
//#define MSG_TP_IC_NAME "pixcir_ts" //"msg21xxA" or "msg22xx" or "msg26xxM" /* Please define the mstar touch ic name based on the mutual-capacitive ic or self capacitive ic that you are using */
/*=============================================================*/
// VARIABLE DEFINITION
/*=============================================================*/

struct i2c_client *g_I2cClient = NULL;
int TOUCH_SCREEN_X_MAX = 0;
int TOUCH_SCREEN_Y_MAX = 0;
int MS_TS_MSG_IC_GPIO_RST = 0;
int MS_TS_MSG_IC_GPIO_INT = 0;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
struct regulator *g_ReguVdd = NULL;
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

extern u8 DrvFwCtrlGetChipType(void);
extern s32 DrvPlatformLyrTouchDeviceRequestGPIO(void);

#ifdef CONFIG_OF
static struct msg2138_ts_platform_data *pixcir_ts_parse_dt(struct device *dev)
{
	struct msg2138_ts_platform_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct msg2138_ts_platform_data");
		return NULL;
	}
	pdata->reset_gpio_number = of_get_gpio(np, 0);
	if(pdata->reset_gpio_number < 0){
		dev_err(dev, "fail to get reset_gpio_number\n");
		goto fail;
	}
	pdata->irq_gpio_number = of_get_gpio(np, 1);
	if(pdata->irq_gpio_number < 0){
		dev_err(dev, "fail to get irq_gpio_number\n");
		goto fail;
	}
	ret = of_property_read_string(np, "vdd_name", &pdata->vdd_name);
	if(ret){
		dev_err(dev, "fail to get vdd_name\n");
		goto fail;
	}
	ret = of_property_read_u32_array(np, "virtualkeys", &pdata->virtualkeys,12);
	if(ret){
		dev_err(dev, "fail to get virtualkeys\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "TP_MAX_X", &pdata->TP_MAX_X);
	if(ret){
		dev_err(dev, "fail to get TP_MAX_X\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "TP_MAX_Y", &pdata->TP_MAX_Y);
	if(ret){
		dev_err(dev, "fail to get TP_MAX_Y\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif

/*=============================================================*/
// FUNCTION DEFINITION
/*=============================================================*/

/* probe function is used for matching and initializing input device */
static int /*__devinit*/ touch_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct msg2138_ts_platform_data *pdata;
    int err = 0, i = 0;

    DBG("*** %s ***\n", __FUNCTION__);

    if (client == NULL)
    {
        DBG("i2c client is NULL\n");
        return -1;
    }
    pdata = client->dev.platform_data;

#ifdef CONFIG_OF
    struct device_node *np = client->dev.of_node;
    if (np && !pdata){
        pdata = pixcir_ts_parse_dt(&client->dev);
        if(pdata){
		client->dev.platform_data = pdata;
        } else {
		err = -ENOMEM;
		goto exit_alloc_platform_data_failed;
        }
    }
#endif

    g_I2cClient = client;
    MS_TS_MSG_IC_GPIO_RST = pdata->reset_gpio_number;
    MS_TS_MSG_IC_GPIO_INT = pdata->irq_gpio_number;
    TOUCH_SCREEN_X_MAX = pdata->TP_MAX_X;
    TOUCH_SCREEN_Y_MAX = pdata->TP_MAX_Y;

    for(i=0; i<MAX_FINGER_NUM*2; i++) {
        point_slot[i].active = 0;
    }
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    g_ReguVdd = regulator_get(&g_I2cClient->dev, pdata->vdd_name);
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON
    err = DrvPlatformLyrTouchDeviceRequestGPIO();
    if(err < 0) {
        pr_err("[Mstar] DrvPlatformLyrTouchDeviceRequestGPIO failed %d\n", err);
        return err;
    }
    if(DrvFwCtrlGetChipType() < 1) {//erro ==0, id[1,2,3,7A]
		err = -ENODEV;
		goto exit_chip_id_erro;
    }
    return MsDrvInterfaceTouchDeviceProbe(g_I2cClient, id);
exit_chip_id_erro:
exit_alloc_platform_data_failed:
    return err;
}

/* remove function is triggered when the input device is removed from input sub-system */
static int /*__devexit*/ touch_driver_remove(struct i2c_client *client)
{
    DBG("*** %s ***\n", __FUNCTION__);

    return MsDrvInterfaceTouchDeviceRemove(client);
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id touch_device_id[] =
{
    {MSG_TP_IC_NAME, MSG_TP_I2C_ADDR}, //SLAVE_I2C_ID_DWI2C
    {}, /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static const struct of_device_id msg2138_of_match[] = {
       { .compatible = "Mstar,msg2138_ts", },
       { }
};
MODULE_DEVICE_TABLE(of, msg2138_of_match);

static struct i2c_driver touch_device_driver =
{
    .driver = {
        .name = MSG_TP_IC_NAME,
        .owner = THIS_MODULE,
        .of_match_table = msg2138_of_match,
    },
    .probe = touch_driver_probe,
    .remove = touch_driver_remove,
/*    .remove = __devexit_p(touch_driver_remove), */
    .id_table = touch_device_id,
};

static int /*__init*/ touch_driver_init(void)
{
    int ret;

    /* register driver */
    ret = i2c_add_driver(&touch_device_driver);
    if (ret < 0)
    {
        DBG("add touch device driver i2c driver failed.\n");
        return -ENODEV;
    }
    DBG("add touch device driver i2c driver.\n");

    return ret;
}

static void /*__exit*/ touch_driver_exit(void)
{
    DBG("remove touch device driver i2c driver.\n");

    i2c_del_driver(&touch_device_driver);
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_LICENSE("GPL");
