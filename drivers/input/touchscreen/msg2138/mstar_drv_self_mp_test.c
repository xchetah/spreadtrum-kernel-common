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
 * @file    mstar_drv_self_mp_test.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_self_mp_test.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_self_fw_control.h"
#include "mstar_drv_platform_porting_layer.h"

#ifdef CONFIG_ENABLE_ITO_MP_TEST

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
// Modify.
#include "open_test_ANA1_X.h"
#include "open_test_ANA2_X.h"
#include "open_test_ANA1_B_X.h"
#include "open_test_ANA2_B_X.h"
#include "open_test_ANA3_X.h"

#include "open_test_ANA1_Y.h"
#include "open_test_ANA2_Y.h"
#include "open_test_ANA1_B_Y.h"
#include "open_test_ANA2_B_Y.h"
#include "open_test_ANA3_Y.h"

// Modify.
#include "short_test_ANA1_X.h"
#include "short_test_ANA2_X.h"
#include "short_test_ANA3_X.h"
#include "short_test_ANA4_X.h"

#include "short_test_ANA1_Y.h"
#include "short_test_ANA2_Y.h"
#include "short_test_ANA3_Y.h"
#include "short_test_ANA4_Y.h"
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
// Modify.
#include "open_test_RIU1_X.h"
#include "open_test_RIU2_X.h"
#include "open_test_RIU3_X.h"

#include "open_test_RIU1_Y.h"
#include "open_test_RIU2_Y.h"
#include "open_test_RIU3_Y.h"

// Modify.
#include "short_test_RIU1_X.h"
#include "short_test_RIU2_X.h"
#include "short_test_RIU3_X.h"
#include "short_test_RIU4_X.h"

#include "short_test_RIU1_Y.h"
#include "short_test_RIU2_Y.h"
#include "short_test_RIU3_Y.h"
#include "short_test_RIU4_Y.h"
#endif

/*=============================================================*/
// PREPROCESSOR CONSTANT DEFINITION
/*=============================================================*/

// Modify.
#define TP_OF_X    (1) //(2)
#define TP_OF_Y    (4)

/*=============================================================*/
// EXTERN VARIABLE DECLARATION
/*=============================================================*/

extern struct i2c_client *g_I2cClient;

/*=============================================================*/
// LOCAL VARIABLE DEFINITION
/*=============================================================*/

static u32 _gIsInMpTest = 0;
static u32 _gTestRetryCount = CTP_MP_TEST_RETRY_COUNT;
static ItoTestMode_e _gItoTestMode = 0;

static s32 _gCtpMpTestStatus = ITO_TEST_UNDER_TESTING;
static u8 _gTestFailChannel[MAX_CHANNEL_NUM] = {0};
static u32 _gTestFailChannelCount = 0;

static struct work_struct _gCtpItoTestWork;
static struct workqueue_struct *_gCtpMpTestWorkQueue = NULL;

static s16 _gRawData1[MAX_CHANNEL_NUM] = {0};
static s16 _gRawData2[MAX_CHANNEL_NUM] = {0};
static s16 _gRawData3[MAX_CHANNEL_NUM] = {0};
static s16 _gRawData4[MAX_CHANNEL_NUM] = {0};
static s8 _gDataFlag1[MAX_CHANNEL_NUM] = {0};
static s8 _gDataFlag2[MAX_CHANNEL_NUM] = {0};
static s8 _gDataFlag3[MAX_CHANNEL_NUM] = {0};
static s8 _gDataFlag4[MAX_CHANNEL_NUM] = {0};

static u8 _gItoTestKeyNum = 0;
static u8 _gItoTestDummyNum = 0;
static u8 _gItoTestTriangleNum = 0;
static u8 _gIsEnable2R = 0;

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)

static u8 _gLTP = 1;	

// _gOpen1~_gOpen3 are for MSG21XXA
static u16 *_gOpen1 = NULL;
static u16 *_gOpen1B = NULL;
static u16 *_gOpen2 = NULL;
static u16 *_gOpen2B = NULL;
static u16 *_gOpen3 = NULL;

// _gShort_1~_gShort_4 are for MSG21XXA
static u16 *_gShort_1 = NULL;
static u16 *_gShort_2 = NULL;
static u16 *_gShort_3 = NULL;
static u16 *_gShort_4 = NULL;

// _gShort_1_GPO~_gShort_4_GPO are for MSG21XXA
static u16 *_gShort_1_GPO = NULL;
static u16 *_gShort_2_GPO = NULL;
static u16 *_gShort_3_GPO = NULL;
static u16 *_gShort_4_GPO = NULL;

#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)

// _gOpenRIU1~_gOpenRIU3 are for MSG22XX
static u32 *_gOpenRIU1 = NULL;
static u32 *_gOpenRIU2 = NULL;
static u32 *_gOpenRIU3 = NULL;

// _gShort_RIU1~_gShort_RIU4 are for MSG22XX
static u32 *_gShort_RIU1 = NULL;
static u32 *_gShort_RIU2 = NULL;
static u32 *_gShort_RIU3 = NULL;
static u32 *_gShort_RIU4 = NULL;

// _gOpenSubFrameNum1~_gShortSubFrameNum4 are for MSG22XX
static u8 _gOpenSubFrameNum1 = 0;
static u8 _gOpenSubFrameNum2 = 0;
static u8 _gOpenSubFrameNum3 = 0;
static u8 _gShortSubFrameNum1 = 0;
static u8 _gShortSubFrameNum2 = 0;
static u8 _gShortSubFrameNum3 = 0;
static u8 _gShortSubFrameNum4 = 0;

#endif

static u8 *_gMAP1 = NULL;
static u8 *_gMAP2 = NULL;
static u8 *_gMAP3 = NULL;
static u8 *_gMAP40_1 = NULL;
static u8 *_gMAP40_2 = NULL;
static u8 *_gMAP40_3 = NULL;
static u8 *_gMAP40_4 = NULL;
static u8 *_gMAP41_1 = NULL;
static u8 *_gMAP41_2 = NULL;
static u8 *_gMAP41_3 = NULL;
static u8 *_gMAP41_4 = NULL;

static u8 *_gSHORT_MAP1 = NULL;
static u8 *_gSHORT_MAP2 = NULL;
static u8 *_gSHORT_MAP3 = NULL;
static u8 *_gSHORT_MAP4 = NULL;

//static struct proc_dir_entry *_gMsgItoTest = NULL;
//static struct proc_dir_entry *_gDebug = NULL;
ItoTestResult_e _gItoTestResult = ITO_TEST_OK;
 
/*=============================================================*/
// EXTERN FUNCTION DECLARATION
/*=============================================================*/


/*=============================================================*/
// LOCAL FUNCTION DEFINITION
/*=============================================================*/

static u16 _DrvMpTestItoTestGetTpType(void)
{
    u16 nMajor = 0, nMinor = 0;

    DBG("*** %s() ***\n", __func__);
        
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    {
        u8 szDbBusTxData[3] = {0};
        u8 szDbBusRxData[4] = {0};

        szDbBusTxData[0] = 0x53;
        szDbBusTxData[1] = 0x00;
        szDbBusTxData[2] = 0x2A;

        IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
        IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

        nMajor = (szDbBusRxData[1]<<8) + szDbBusRxData[0];
        nMinor = (szDbBusRxData[3]<<8) + szDbBusRxData[2];
    }
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    {
        u16 nRegData1, nRegData2;

        DrvPlatformLyrTouchDeviceResetHw();
    
        DbBusEnterSerialDebugMode();
        DbBusStopMCU();
        DbBusIICUseBus();
        DbBusIICReshape();
//        mdelay(100);
	msleep(100);
        
        // Stop mcu
        RegSetLByteValue(0x0FE6, 0x01); 

        // Stop watchdog
        RegSet16BitValue(0x3C60, 0xAA55);

        // RIU password
        RegSet16BitValue(0x161A, 0xABBA); 

        // Clear pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        RegSet16BitValue(0x1600, 0xBFF4); // Set start address for customer firmware version on main block
    
        // Enable burst mode
//        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01));

        // Set pce
        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x40)); 
    
        RegSetLByteValue(0x160E, 0x01); 

        nRegData1 = RegGet16BitValue(0x1604);
        nRegData2 = RegGet16BitValue(0x1606);

        nMajor = (((nRegData1 >> 8) & 0xFF) << 8) + (nRegData1 & 0xFF);
        nMinor = (((nRegData2 >> 8) & 0xFF) << 8) + (nRegData2 & 0xFF);

        // Clear burst mode
//        RegSet16BitValue(0x160C, RegGet16BitValue(0x160C) & (~0x01));      

        RegSet16BitValue(0x1600, 0x0000); 

        // Clear RIU password
        RegSet16BitValue(0x161A, 0x0000); 

        DbBusIICNotUseBus();
        DbBusNotStopMCU();
        DbBusExitSerialDebugMode();

        DrvPlatformLyrTouchDeviceResetHw();
//        mdelay(100);
	msleep(100);
    }
#endif    

    DBG("*** major = %d ***\n", nMajor);
    DBG("*** minor = %d ***\n", nMinor);
    
    return nMajor;
}

static u16 _DrvMpTestItoTestChooseTpType(void)
{
    u16 nTpType = 0;
    u32 i = 0;
    
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    // _gOpen1~_gOpen3 are for MSG21XXA
    _gOpen1 = NULL;
    _gOpen1B = NULL;
    _gOpen2 = NULL;
    _gOpen2B = NULL;
    _gOpen3 = NULL;

    // _gShort_1~_gShort_4 are for MSG21XXA
    _gShort_1 = NULL;
    _gShort_2 = NULL;
    _gShort_3 = NULL;
    _gShort_4 = NULL;

    _gShort_1_GPO = NULL;
    _gShort_2_GPO = NULL;
    _gShort_3_GPO = NULL;
    _gShort_4_GPO = NULL;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    // _gOpenRIU1~_gOpenRIU3 are for MSG22XX
    _gOpenRIU1 = NULL;
    _gOpenRIU2 = NULL;
    _gOpenRIU3 = NULL;

    // _gShort_RIU1~_gShort_RIU4 are for MSG22XX
    _gShort_RIU1 = NULL;
    _gShort_RIU2 = NULL;
    _gShort_RIU3 = NULL;
    _gShort_RIU4 = NULL;

    _gOpenSubFrameNum1 = 0;
    _gOpenSubFrameNum2 = 0;
    _gOpenSubFrameNum3 = 0;
    _gShortSubFrameNum1 = 0;
    _gShortSubFrameNum2 = 0;
    _gShortSubFrameNum3 = 0;
    _gShortSubFrameNum4 = 0;
#endif

    _gMAP1 = NULL;
    _gMAP2 = NULL;
    _gMAP3 = NULL;
    _gMAP40_1 = NULL;
    _gMAP40_2 = NULL;
    _gMAP40_3 = NULL;
    _gMAP40_4 = NULL;
    _gMAP41_1 = NULL;
    _gMAP41_2 = NULL;
    _gMAP41_3 = NULL;
    _gMAP41_4 = NULL;
    
    _gSHORT_MAP1 = NULL;
    _gSHORT_MAP2 = NULL;
    _gSHORT_MAP3 = NULL;
    _gSHORT_MAP4 = NULL;
    
    _gItoTestKeyNum = 0;
    _gItoTestDummyNum = 0;
    _gItoTestTriangleNum = 0;
    _gIsEnable2R = 0;

    for (i = 0; i < 10; i ++)
    {
        nTpType = _DrvMpTestItoTestGetTpType();
        DBG("nTpType = %d, i = %d\n", nTpType, i);

        if (TP_OF_X == nTpType || TP_OF_Y == nTpType) // Modify.
        {
            break;
        }
        else if (i < 5)
        {
//            mdelay(100);  
	      msleep(100);
        }
        else
        {
            DrvPlatformLyrTouchDeviceResetHw();
        }
    }
    
    if (TP_OF_X == nTpType) // Modify. 
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        _gOpen1 = open_1_X;
        _gOpen1B = open_1B_X;
        _gOpen2 = open_2_X;
        _gOpen2B = open_2B_X;
        _gOpen3 = open_3_X;

        _gShort_1 = short_1_X;
        _gShort_2 = short_2_X;
        _gShort_3 = short_3_X;
        _gShort_4 = short_4_X;

        _gShort_1_GPO = short_1_X_GPO;
        _gShort_2_GPO = short_2_X_GPO;
        _gShort_3_GPO = short_3_X_GPO;
        _gShort_4_GPO = short_4_X_GPO;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        _gOpenRIU1 = open_1_X;
        _gOpenRIU2 = open_2_X;
        _gOpenRIU3 = open_3_X;

        _gShort_RIU1 = short_1_X;
        _gShort_RIU2 = short_2_X;
        _gShort_RIU3 = short_3_X;
        _gShort_RIU4 = short_4_X;

        _gOpenSubFrameNum1 = NUM_OPEN_1_SENSOR_X;
        _gOpenSubFrameNum2 = NUM_OPEN_2_SENSOR_X;
        _gOpenSubFrameNum3 = NUM_OPEN_3_SENSOR_X;
        _gShortSubFrameNum1 = NUM_SHORT_1_SENSOR_X;
        _gShortSubFrameNum2 = NUM_SHORT_2_SENSOR_X;
        _gShortSubFrameNum3 = NUM_SHORT_3_SENSOR_X;
        _gShortSubFrameNum4 = NUM_SHORT_4_SENSOR_X;
#endif

        _gMAP1 = MAP1_X;
        _gMAP2 = MAP2_X;
        _gMAP3 = MAP3_X;
        _gMAP40_1 = MAP40_1_X;
        _gMAP40_2 = MAP40_2_X;
        _gMAP40_3 = MAP40_3_X;
        _gMAP40_4 = MAP40_4_X;
        _gMAP41_1 = MAP41_1_X;
        _gMAP41_2 = MAP41_2_X;
        _gMAP41_3 = MAP41_3_X;
        _gMAP41_4 = MAP41_4_X;

        _gSHORT_MAP1 = SHORT_MAP1_X;
        _gSHORT_MAP2 = SHORT_MAP2_X;
        _gSHORT_MAP3 = SHORT_MAP3_X;
        _gSHORT_MAP4 = SHORT_MAP4_X;
        
        _gItoTestKeyNum = NUM_KEY_X;
        _gItoTestDummyNum = NUM_DUMMY_X;
        _gItoTestTriangleNum = NUM_SENSOR_X;
        _gIsEnable2R = ENABLE_2R_X;
    }
    else if (TP_OF_Y == nTpType) // Modify. 
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        _gOpen1 = open_1_Y;
        _gOpen1B = open_1B_Y;
        _gOpen2 = open_2_Y;
        _gOpen2B = open_2B_Y;
        _gOpen3 = open_3_Y;

        _gShort_1 = short_1_Y;
        _gShort_2 = short_2_Y;
        _gShort_3 = short_3_Y;
        _gShort_4 = short_4_Y;

        _gShort_1_GPO = short_1_Y_GPO;
        _gShort_2_GPO = short_2_Y_GPO;
        _gShort_3_GPO = short_3_Y_GPO;
        _gShort_4_GPO = short_4_Y_GPO;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        _gOpenRIU1 = open_1_Y;
        _gOpenRIU2 = open_2_Y;
        _gOpenRIU3 = open_3_Y;

        _gShort_RIU1 = short_1_Y;
        _gShort_RIU2 = short_2_Y;
        _gShort_RIU3 = short_3_Y;
        _gShort_RIU4 = short_4_Y;

        _gOpenSubFrameNum1 = NUM_OPEN_1_SENSOR_Y;
        _gOpenSubFrameNum2 = NUM_OPEN_2_SENSOR_Y;
        _gOpenSubFrameNum3 = NUM_OPEN_3_SENSOR_Y;
        _gShortSubFrameNum1 = NUM_SHORT_1_SENSOR_Y;
        _gShortSubFrameNum2 = NUM_SHORT_2_SENSOR_Y;
        _gShortSubFrameNum3 = NUM_SHORT_3_SENSOR_Y;
        _gShortSubFrameNum4 = NUM_SHORT_4_SENSOR_Y;
#endif

        _gMAP1 = MAP1_Y;
        _gMAP2 = MAP2_Y;
        _gMAP3 = MAP3_Y;
        _gMAP40_1 = MAP40_1_Y;
        _gMAP40_2 = MAP40_2_Y;
        _gMAP40_3 = MAP40_3_Y;
        _gMAP40_4 = MAP40_4_Y;
        _gMAP41_1 = MAP41_1_Y;
        _gMAP41_2 = MAP41_2_Y;
        _gMAP41_3 = MAP41_3_Y;
        _gMAP41_4 = MAP41_4_Y;

        _gSHORT_MAP1 = SHORT_MAP1_Y;
        _gSHORT_MAP2 = SHORT_MAP2_Y;
        _gSHORT_MAP3 = SHORT_MAP3_Y;
        _gSHORT_MAP4 = SHORT_MAP4_Y;
        
        _gItoTestKeyNum = NUM_KEY_Y;
        _gItoTestDummyNum = NUM_DUMMY_Y;
        _gItoTestTriangleNum = NUM_SENSOR_Y;
        _gIsEnable2R = ENABLE_2R_Y;
    }
    else
    {
        nTpType = 0;
    }
    
    return nTpType;
}

static void _DrvMpTestItoTestSwReset(void)
{
    DBG("*** %s() ***\n", __func__);

    RegSet16BitValue(0x1100, 0xFFFF); //bank:ana, addr:h0000
    RegSet16BitValue(0x1100, 0x0000);
//    mdelay(50); // mdelay(15)
    msleep(50);
}

//------------------------------------------------------------------------------//

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)

static u16 _DrvMpTestItoTestGetNum(void)
{
    u32 i;
    u16 nSensorNum = 0;
    u16 nRegVal1, nRegVal2;
 
    DBG("*** %s() ***\n", __func__);

    nRegVal1 = RegGet16BitValue(0x114A); //bank:ana, addr:h0025  
    DBG("nRegValue1 = %d\n", nRegVal1);
    
    if ((nRegVal1 & BIT1) == BIT1)
    {
        nRegVal1 = RegGet16BitValue(0x120A); //bank:ana2, addr:h0005  			
        nRegVal1 = nRegVal1 & 0x0F;
    	
        nRegVal2 = RegGet16BitValue(0x1216); //bank:ana2, addr:h000b    		
        nRegVal2 = ((nRegVal2 >> 1) & 0x0F) + 1;
    	
        nSensorNum = nRegVal1 * nRegVal2;
    }
    else
    {
        for (i = 0; i < 4; i ++)
        {
            nSensorNum += (RegGet16BitValue(0x120A)>>(4*i))&0x0F; //bank:ana2, addr:h0005  
        }
    }
    DBG("nSensorNum = %d\n", nSensorNum);

    return nSensorNum;        
}

static void _DrvMpTestItoTestDisableFilterNoiseDetect(void)
{
    u16 nRegValue;

    DBG("*** %s() ***\n", __func__);
     
    // Disable DIG/ANA drop
    nRegValue = RegGet16BitValue(0x1302); 
      
    RegSet16BitValue(0x1302, nRegValue & (~(BIT2 | BIT1 | BIT0)));      
}

static void _DrvMpTestItoTestPolling(void)
{
    u16 nRegInt = 0x0000;
    u16 nRegVal;

    DBG("*** %s() ***\n", __func__);

    RegSet16BitValue(0x130C, BIT15); //bank:fir, addr:h0006         
    RegSet16BitValue(0x1214, (RegGet16BitValue(0x1214) | BIT0)); //bank:ana2, addr:h000a        
            
    DBG("polling start\n");

    do
    {
        nRegInt = RegGet16BitValue(0x3D18); //bank:intr_ctrl, addr:h000c
    } while(( nRegInt & FIQ_E_FRAME_READY_MASK ) == 0x0000);

    DBG("polling end\n");
    
    nRegVal = RegGet16BitValue(0x3D18); 
    RegSet16BitValue(0x3D18, nRegVal & (~FIQ_E_FRAME_READY_MASK));      
}

static void _DrvMpTestItoOpenTestSetV(u8 nEnable, u8 nPrs)	
{
    u16 nRegVal;        
    
    DBG("*** %s() nEnable = %d, nPrs = %d ***\n", __func__, nEnable, nPrs);
    
    nRegVal = RegGet16BitValue(0x1208); //bank:ana2, addr:h0004
    nRegVal = nRegVal & 0xF1; 							
    
    if (nPrs == 0)
    {
        RegSet16BitValue(0x1208, nRegVal|0x0C); 		
    }
    else if (nPrs == 1)
    {
        RegSet16BitValue(0x1208, nRegVal|0x0E); 		     	
    }
    else
    {
        RegSet16BitValue(0x1208, nRegVal|0x02); 			
    }    
    
    if (nEnable)
    {
        nRegVal = RegGet16BitValue(0x1106); //bank:ana, addr:h0003  
        RegSet16BitValue(0x1106, nRegVal|0x03);   	
    }
    else
    {
        nRegVal = RegGet16BitValue(0x1106);    
        nRegVal = nRegVal & 0xFC;					
        RegSet16BitValue(0x1106, nRegVal);         
    }
}

static u16 _DrvMpTestItoTestMsg21xxaGetDataOut(s16 *pRawData)
{
    u32 i;
    u16 szRawData[MAX_CHANNEL_NUM] = {0};
    u16 nSensorNum;
    u16 nRegInt;
    u8  szDbBusTxData[8] = {0};
    u8  szDbBusRxData[MAX_CHANNEL_NUM*2] = {0};

    DBG("*** %s() ***\n", __func__);

    nSensorNum = _DrvMpTestItoTestGetNum();
    
    if ((nSensorNum*2) > (MAX_CHANNEL_NUM*2))
    {
        DBG("Danger. nSensorNum = %d\n", nSensorNum);
        return nSensorNum;
    }

    nRegInt = RegGet16BitValue((0x3d<<8) | (REG_INTR_FIQ_MASK<<1)); 
    RegSet16BitValue((0x3d<<8) | (REG_INTR_FIQ_MASK<<1), (nRegInt & (u16)(~FIQ_E_FRAME_READY_MASK))); 
    
    _DrvMpTestItoTestPolling();
    
    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x13; //bank:fir, addr:h0020 
    szDbBusTxData[2] = 0x40;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
//    mdelay(20);
    msleep(20);
    IicReadData(SLAVE_I2C_ID_DBBUS, &szDbBusRxData[0], (nSensorNum * 2));
//    mdelay(100);
    msleep(100);
    
    for (i = 0; i < nSensorNum * 2; i ++)
    {
        DBG("szDbBusRxData[%d] = %d\n", i, szDbBusRxData[i]);
    }
 
    nRegInt = RegGet16BitValue((0x3d<<8) | (REG_INTR_FIQ_MASK<<1)); 
    RegSet16BitValue((0x3d<<8) | (REG_INTR_FIQ_MASK<<1), (nRegInt | (u16)FIQ_E_FRAME_READY_MASK)); 

    for (i = 0; i < nSensorNum; i ++)
    {
        szRawData[i] = (szDbBusRxData[2 * i + 1] << 8 ) | (szDbBusRxData[2 * i]);
        pRawData[i] = (s16)szRawData[i];
    }
    
    return nSensorNum;
}

static void _DrvMpTestItoTestMsg21xxaSendDataIn(u8 nStep)
{
    u32	i;
    u16 *pType = NULL;        
    u8 	szDbBusTxData[512] = {0};

    DBG("*** %s() nStep = %d ***\n", __func__, nStep);

    if (nStep == 0) //39-4 (2R)
    {
        pType = &_gShort_4[0];
    }
    else if (nStep == 1) //39-1
    {
        pType = &_gShort_1[0];      	
    }
    else if (nStep == 2) //39-2
    {
        pType = &_gShort_2[0];      	
    }
    else if (nStep == 3) //39-3
    {
        pType = &_gShort_3[0];        
    }
    else if (nStep == 4)
    {
        pType = &_gOpen1[0];        
    }
    else if (nStep == 5)
    {
        pType = &_gOpen2[0];      	
    }
    else if (nStep == 6)
    {
        pType = &_gOpen3[0];      	
    }
    else if (nStep == 9)
    {
        pType = &_gOpen1B[0];        
    }
    else if (nStep == 10)
    {
        pType = &_gOpen2B[0];      	
    } 
     
    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x11; //bank:ana, addr:h0000
    szDbBusTxData[2] = 0x00;    
    for (i = 0; i <= 0x3E ; i ++)
    {
        szDbBusTxData[3+2*i] = pType[i] & 0xFF;
        szDbBusTxData[4+2*i] = (pType[i] >> 8) & 0xFF;    	
    }
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+0x3F*2);

    szDbBusTxData[2] = 0x7A * 2; //bank:ana, addr:h007a
    for (i = 0x7A; i <= 0x7D ; i ++)
    {
        szDbBusTxData[3+2*(i-0x7A)] = 0;
        szDbBusTxData[4+2*(i-0x7A)] = 0;    	    	
    }
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+8);
    
    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x12; //bank:ana2, addr:h0005
      
    szDbBusTxData[2] = 0x05 * 2; 
    szDbBusTxData[3] = pType[128+0x05] & 0xFF;
    szDbBusTxData[4] = (pType[128+0x05] >> 8) & 0xFF;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 5);
    
    szDbBusTxData[2] = 0x0B * 2; //bank:ana2, addr:h000b
    szDbBusTxData[3] = pType[128+0x0B] & 0xFF;
    szDbBusTxData[4] = (pType[128+0x0B] >> 8) & 0xFF;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 5);
    
    szDbBusTxData[2] = 0x12 * 2; //bank:ana2, addr:h0012
    szDbBusTxData[3] = pType[128+0x12] & 0xFF;
    szDbBusTxData[4] = (pType[128+0x12] >> 8) & 0xFF;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 5);
    
    szDbBusTxData[2] = 0x15 * 2; //bank:ana2, addr:h0015
    szDbBusTxData[3] = pType[128+0x15] & 0xFF;
    szDbBusTxData[4] = (pType[128+0x15] >> 8) & 0xFF;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 5);
/*
//#if 1 //for AC mod --showlo
    szDbBusTxData[1] = 0x13;
    szDbBusTxData[2] = 0x12 * 2;
    szDbBusTxData[3] = 0x30;
    szDbBusTxData[4] = 0x30;
    IicWriteData(SLAVE_I2C_ID_DBBUS, szDbBusTxData, 5);        
        
    szDbBusTxData[2] = 0x14 * 2;
    szDbBusTxData[3] = 0X30;
    szDbBusTxData[4] = 0X30;
    IicWriteData(SLAVE_I2C_ID_DBBUS, szDbBusTxData, 5);     
        
    szDbBusTxData[1] = 0x12;
    for (i = 0x0D; i <= 0x10; i ++) //for AC noise(++)
    {
	      szDbBusTxData[2] = i * 2;
	      szDbBusTxData[3] = pType[128+i] & 0xFF;
	      szDbBusTxData[4] = (pType[128+i] >> 8) & 0xFF;
	      IicWriteData(SLAVE_I2C_ID_DBBUS, szDbBusTxData, 5);  
    }

    for (i = 0x16; i <= 0x18; i ++) //for AC noise
    {
	      szDbBusTxData[2] = i * 2;
	      szDbBusTxData[3] = pType[128+i] & 0xFF;
	      szDbBusTxData[4] = (pType[128+i] >> 8) & 0xFF;
	      IicWriteData(SLAVE_I2C_ID_DBBUS, szDbBusTxData, 5);  
    }
//#endif
*/
}

static void _DrvMpTestItoOpenTestMsg21xxaSetC(u8 nCSubStep)
{
    u32 i;
    u8 szDbBusTxData[MAX_CHANNEL_NUM+3];
    u8 nHighLevelCSub = false;
    u8 nCSubNew;
     
    DBG("*** %s() nCSubStep = %d ***\n", __func__, nCSubStep);

    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x11; //bank:ana, addr:h0042       
    szDbBusTxData[2] = 0x84;        
    
    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        nCSubNew = nCSubStep;        
        nHighLevelCSub = false;   
        
        if (nCSubNew > 0x1F)
        {
            nCSubNew = nCSubNew - 0x14;
            nHighLevelCSub = true;
        }
           
        szDbBusTxData[3+i] = nCSubNew & 0x1F;        
        if (nHighLevelCSub == true)
        {
            szDbBusTxData[3+i] |= BIT5;
        }
    }
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], MAX_CHANNEL_NUM+3);

    szDbBusTxData[2] = 0xB4; //bank:ana, addr:h005a        
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], MAX_CHANNEL_NUM+3);
}

static void _DrvMpTestItoOpenTestMsg21xxaFirst(u8 nItemId, s16 *pRawData, s8 *pDataFlag)		
{
    u32 i, j;
    s16 szTmpRawData[MAX_CHANNEL_NUM] = {0};
    u16	nRegVal;
    u8  nLoop;
    u8  nSensorNum1 = 0, nSensorNum2 = 0, nTotalSensor = 0;
    u8 	*pMapping = NULL;
    
//    nSensorNum1 = 0;
//    nSensorNum2 = 0;	

    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);
	
    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073

    RegSet16BitValue(0x1E24, 0x0500); //bank:chip, addr:h0012
    RegSet16BitValue(0x1E2A, 0x0000); //bank:chip, addr:h0015
    RegSet16BitValue(0x1EE6, 0x6E00); //bank:chip, addr:h0073
    RegSet16BitValue(0x1EE8, 0x0071); //bank:chip, addr:h0074
	    
    if (nItemId == 40)    			
    {
        pMapping = &_gMAP1[0];
        if (_gIsEnable2R)
        {
            nTotalSensor = _gItoTestTriangleNum/2; 
        }
        else
        {
            nTotalSensor = _gItoTestTriangleNum/2 + _gItoTestKeyNum + _gItoTestDummyNum;
        }
    }
    else if (nItemId == 41)    		
    {
        pMapping = &_gMAP2[0];
        if (_gIsEnable2R)
        {
            nTotalSensor = _gItoTestTriangleNum/2; 
        }
        else
        {
            nTotalSensor = _gItoTestTriangleNum/2 + _gItoTestKeyNum + _gItoTestDummyNum;
        }
    }
    else if (nItemId == 42)    		
    {
        pMapping = &_gMAP3[0];      
        nTotalSensor = _gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum; 
    }
        	    
    nLoop = 1;
    if (nItemId != 42)
    {
        if (nTotalSensor > 11)
        {
            nLoop = 2;
        }
    }	
    
    DBG("nLoop = %d\n", nLoop);
	
    for (i = 0; i < nLoop; i ++)
    {
        if (i == 0)
        {
            _DrvMpTestItoTestMsg21xxaSendDataIn(nItemId - 36);
        }
        else
        { 
            if (nItemId == 40)
            { 
                _DrvMpTestItoTestMsg21xxaSendDataIn(9);
            }
            else
            { 		
                _DrvMpTestItoTestMsg21xxaSendDataIn(10);
            }
        }
	
        _DrvMpTestItoTestDisableFilterNoiseDetect();
	
        _DrvMpTestItoOpenTestSetV(1, 0);    
        nRegVal = RegGet16BitValue(0x110E); //bank:ana, addr:h0007   			
        RegSet16BitValue(0x110E, nRegVal | BIT11);				 		
	
        if (_gLTP == 1)
        {
            _DrvMpTestItoOpenTestMsg21xxaSetC(32);
        }	    	
        else
        {	    	
	    	_DrvMpTestItoOpenTestMsg21xxaSetC(0);
        }
        
        _DrvMpTestItoTestSwReset();
		
        if (i == 0)	 
        {      
            nSensorNum1 = _DrvMpTestItoTestMsg21xxaGetDataOut(szTmpRawData);
            DBG("nSensorNum1 = %d\n", nSensorNum1);
        }
        else	
        {      
            nSensorNum2 = _DrvMpTestItoTestMsg21xxaGetDataOut(&szTmpRawData[nSensorNum1]);
            DBG("nSensorNum1 = %d, nSensorNum2 = %d\n", nSensorNum1, nSensorNum2);
        }
    }
    
    for (j = 0; j < nTotalSensor; j ++)
    {
        if (_gLTP == 1)
        {
            pRawData[pMapping[j]] = szTmpRawData[j] + 4096;
            pDataFlag[pMapping[j]] = 1;
        }
        else
        {
            pRawData[pMapping[j]] = szTmpRawData[j];	
            pDataFlag[pMapping[j]] = 1;
        }
    }	
}

static void _DrvMpTestItoShortTestChangeGPOSetting(u8 nItemId)
{
    u8 szDbBusTxData[3+GPO_SETTING_SIZE*2] = {0};
    u16 szGPOSetting[3] = {0};
    u32 i;
    
    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);
    
    if (nItemId == 0) // 39-4
    {
        szGPOSetting[0] = _gShort_4_GPO[0];		
        szGPOSetting[1] = _gShort_4_GPO[1];		
        szGPOSetting[2] = _gShort_4_GPO[2];		
        szGPOSetting[2] |= (1 << (int)(PIN_GUARD_RING % 16));
    }
    else if (nItemId == 1) // 39-1
    {
        szGPOSetting[0] = _gShort_1_GPO[0];		
        szGPOSetting[1] = _gShort_1_GPO[1];		
        szGPOSetting[2] = _gShort_1_GPO[2];		
        szGPOSetting[2] |= (1 << (int)(PIN_GUARD_RING % 16));
    }
    else if (nItemId == 2) // 39-2
    {
        szGPOSetting[0] = _gShort_2_GPO[0];		
        szGPOSetting[1] = _gShort_2_GPO[1];		
        szGPOSetting[2] = _gShort_2_GPO[2];		
        szGPOSetting[2] |= (1 << (int)(PIN_GUARD_RING % 16));
    }
    else if (nItemId == 3) // 39-3
    {
        szGPOSetting[0] = _gShort_3_GPO[0];		
        szGPOSetting[1] = _gShort_3_GPO[1];		
        szGPOSetting[2] = _gShort_3_GPO[2];		
        szGPOSetting[2] |= (1 << (int)(PIN_GUARD_RING % 16));
    }
    else
    {
        DBG("Invalid item id for changing GPIO setting of short test.\n");

        return;
    }

    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x12;
    szDbBusTxData[2] = 0x48;

    for (i = 0; i < GPO_SETTING_SIZE; i ++)
    {
        szDbBusTxData[3+2*i] = szGPOSetting[i] & 0xFF;
        szDbBusTxData[4+2*i] = (szGPOSetting[i] >> 8) & 0xFF;    	
    }

    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+GPO_SETTING_SIZE*2);    
}

static void _DrvMpTestItoShortTestChangeRmodeSetting(u8 nMode)
{
    u8 szDbBusTxData[6] = {0};

    DBG("*** %s() nMode = %d ***\n", __func__, nMode);

    // AFE R-mode enable(Bit-12)
    RegSetLByteValue(0x1103, 0x10);

    // drv_mux_OV (Bit-8 1:enable)
    RegSetLByteValue(0x1107, 0x55);
    
    if (nMode == 1) // P_CODE: 0V
    {
        RegSet16BitValue(0x110E, 0x073A);
    }
    else if (nMode == 0) // N_CODE: 2.4V
    {
        RegSet16BitValue(0x110E, 0x073B);
    }

    // SW2 rising & SW3 rising return to 0
    RegSetLByteValue(0x1227, 0x01);
    // turn off the chopping
    RegSetLByteValue(0x1208, 0x0C);
    // idle driver ov
    RegSetLByteValue(0x1241, 0xC0);
	  
	  // AFE ov
    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x12;
    szDbBusTxData[2] = 0x44;
    szDbBusTxData[3] = 0xFF;
    szDbBusTxData[4] = 0xFF;
    szDbBusTxData[5] = 0xFF;

    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 6);        
}	

static void _DrvMpTestItoShortTestMsg21xxaFirst(u8 nItemId, s16 *pRawData, s8 *pDataFlag)		
{
    u32 i;
    s16 szTmpRawData[MAX_CHANNEL_NUM] = {0};
    s16 szTmpRawData2[MAX_CHANNEL_NUM] = {0};
    u8  nSensorNum, nSensorNum2, nNumOfSensorMapping1, nNumOfSensorMapping2, nSensorCount = 0;
    u8 	*pMapping = NULL;
    
    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);

    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073
    // chip top op0
    RegSet16BitValue(0x1E24, 0x0500); //bank:chip, addr:h0012
    RegSet16BitValue(0x1E2A, 0x0000); //bank:chip, addr:h0015
    RegSet16BitValue(0x1EE6, 0x6E00); //bank:chip, addr:h0073
    RegSet16BitValue(0x1EE8, 0x0071); //bank:chip, addr:h0074
	    
    if ((_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) % 2 != 0)
    {
        nNumOfSensorMapping1 = (_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) / 2 + 1;
        nNumOfSensorMapping2 = nNumOfSensorMapping1;
    }
    else
    {
        nNumOfSensorMapping1 = (_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) / 2;
        nNumOfSensorMapping2 = nNumOfSensorMapping1;
        if (nNumOfSensorMapping2 % 2 != 0)
        {	
            nNumOfSensorMapping2 ++;
        }
    }        

    if (nItemId == 0) // 39-4 (2R)    			
    {
        pMapping = &_gSHORT_MAP4[0];
        nSensorCount = _gItoTestTriangleNum/2; 
    }
    else if (nItemId == 1) // 39-1    			    		
    {
        pMapping = &_gSHORT_MAP1[0];
        nSensorCount = nNumOfSensorMapping1; 
    }
    else if (nItemId == 2) // 39-2   		
    {
        pMapping = &_gSHORT_MAP2[0];      
        nSensorCount = nNumOfSensorMapping2; 
    }
    else if (nItemId == 3) // 39-3    		
    {
        pMapping = &_gSHORT_MAP3[0];      
        nSensorCount = _gItoTestTriangleNum; 
    }
    DBG("nSensorCount = %d\n", nSensorCount);
        	    
    _DrvMpTestItoTestMsg21xxaSendDataIn(nItemId);
    
    _DrvMpTestItoTestDisableFilterNoiseDetect();

    _DrvMpTestItoShortTestChangeRmodeSetting(1);
    _DrvMpTestItoShortTestChangeGPOSetting(nItemId);
    _DrvMpTestItoTestSwReset();
    nSensorNum = _DrvMpTestItoTestMsg21xxaGetDataOut(szTmpRawData);
    DBG("nSensorNum = %d\n", nSensorNum);

    _DrvMpTestItoShortTestChangeRmodeSetting(0);
    _DrvMpTestItoShortTestChangeGPOSetting(nItemId);
    _DrvMpTestItoTestSwReset();
    nSensorNum2 = _DrvMpTestItoTestMsg21xxaGetDataOut(szTmpRawData2);
    DBG("nSensorNum2 = %d\n", nSensorNum2);
    
    for (i = 0; i < nSensorCount; i ++)
    {
        pRawData[pMapping[i]] = szTmpRawData[i] - szTmpRawData2[i];	
        pDataFlag[pMapping[i]] = 1;
    }	
}

#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)

static u16 _DrvMpTestItoTestMsg22xxGetDataOut(s16 *pRawData, u16 nSubFrameNum)
{
    u32 i;
    u16 szRawData[MAX_CHANNEL_NUM*2] = {0};
    u16 nRegInt = 0x0000;
    u16 nSize = nSubFrameNum * 4;
    u8  szDbBusTxData[8] = {0};
    u8  szDbBusRxData[MAX_CHANNEL_NUM*4] = {0};

    DBG("*** %s() ***\n", __func__);

    RegSet16BitValueOff(0x120A, BIT1); //one-shot mode
    RegSet16BitValueOff(0x3D08, BIT8); //FIQ_E_FRAME_READY_MASK
    //RegSet16BitValueOn(0x130C, BIT15); //MCU read done
    RegSet16BitValueOn(0x120A, BIT0); //trigger one-shot 

    DBG("polling start\n");

    do
    {
        nRegInt = RegGet16BitValue(0x3D18); //bank:intr_ctrl, addr:h000c
    } while(( nRegInt & FIQ_E_FRAME_READY_MASK ) == 0x0000);

    DBG("polling end\n");

    RegSet16BitValueOff(0x3D18, BIT8); //Clear frame-ready interrupt status

    //ReadRegBurst start   
    szDbBusTxData[0] = 0x10;
    szDbBusTxData[1] = 0x15; //bank:fir, addr:h0000 
    szDbBusTxData[2] = 0x00;
    IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
//    mdelay(20);
    msleep(20);
//    IicReadData(SLAVE_I2C_ID_DBBUS, &szDbBusRxData[0], (MAX_CHANNEL_NUM * 4));
    IicReadData(SLAVE_I2C_ID_DBBUS, &szDbBusRxData[0], (nSubFrameNum * 4 * 2));
//    mdelay(100);
    msleep(100);

//    for (i = 0; i < (MAX_CHANNEL_NUM * 4); i ++)
    for (i = 0; i < (nSubFrameNum * 4 * 2); i ++)
    {
        DBG("szDbBusRxData[%d] = %d\n", i, szDbBusRxData[i]);
    }
    //ReadRegBurst stop
 
    RegSet16BitValueOn(0x3D08, BIT8); //FIQ_E_FRAME_READY_MASK

//    for (i = 0; i < (MAX_CHANNEL_NUM * 2); i ++)
    for (i = 0; i < nSize; i ++)
    {
        szRawData[i] = (szDbBusRxData[2 * i + 1] << 8 ) | (szDbBusRxData[2 * i]);
        pRawData[i] = (s16)szRawData[i];
    }
    
//    return (MAX_CHANNEL_NUM * 2);
    return nSize;
}

static void _DrvMpTestItoTestMsg22xxSendDataIn(u8 nStep, u16 nSubFrameNum)
{
    u32	i;
    u32 *pType = NULL;
    u16 nRiuWriteLength = nSubFrameNum * 6;

    DBG("*** %s() nStep = %d, nSubFrameNum = %d ***\n", __func__, nStep, nSubFrameNum);

    if (nStep == 0) //39-4 (2R)
    {
        pType = &_gShort_RIU4[0];
    }
    else if (nStep == 1) //39-1
    {
        pType = &_gShort_RIU1[0];      	
    }
    else if (nStep == 2) //39-2
    {
        pType = &_gShort_RIU2[0];      	
    }
    else if (nStep == 3) //39-3
    {
        pType = &_gShort_RIU3[0];        
    }
    else if (nStep == 4)
    {
        pType = &_gOpenRIU1[0];        
    }
    else if (nStep == 5)
    {
        pType = &_gOpenRIU2[0];      	
    }
    else if (nStep == 6)
    {
        pType = &_gOpenRIU3[0];      	
    }

    RegSet16BitValueOn(0x1192, BIT4); // force on enable sensor mux and csub sel sram clock
    RegSet16BitValueOff(0x1192, BIT5); // mem clk sel 
    RegSet16BitValueOff(0x1100, BIT3); // tgen soft rst
    RegSet16BitValue(0x1180, RIU_BASE_ADDR); // sensor mux sram read/write base address
    RegSet16BitValue(0x1182, nRiuWriteLength); // sensor mux sram write length
    RegSet16BitValueOn(0x1188, BIT0); // reg_mem0_w_start

    for (i = RIU_BASE_ADDR; i < (RIU_BASE_ADDR + nRiuWriteLength); i ++)
    {
        RegSet16BitValue(0x118A, (u16)(pType[i]));
        RegSet16BitValue(0x118C, (u16)(pType[i] >> 16));
    }
}

static void _DrvMpTestItoTestMsg22xxSetC(u8 nCSubStep)
{
    u32 i;
    u16 nCSubNew, nRegVal;
     
    DBG("*** %s() nCSubStep = %d ***\n", __func__, nCSubStep);
    
    nCSubNew = (nCSubStep > CSUB_REF_MAX) ? CSUB_REF_MAX : nCSubStep; // 6 bits
    nCSubNew = (nCSubNew | (nCSubNew << 8));

    nRegVal = RegGet16BitValue(0x11C8); // csub sel overwrite enable, will referance value of 11C0

    if (nRegVal == 0x000F)
    {
        RegSet16BitValue(0x11C0, nCSubNew);
    }
    else
    {
        RegSet16BitValueOn(0x1192, BIT4);   // force on enable sensor mux  and csub sel sram clock
        RegSet16BitValueOff(0x1192, BIT5);   // mem clk sel 
        RegSet16BitValueOff(0x1100, BIT3);   // tgen soft rst
        RegSet16BitValue(0x1184, 0);          // nAddr
        RegSet16BitValue(0x1186, MAX_CHANNEL_NUM);         // nLen
        RegSet16BitValueOn(0x1188, BIT2);   // reg_mem0_w_start

        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            RegSet16BitValue( 0x118E, nCSubNew);
            RegSet16BitValue( 0x1190, nCSubNew);
        }
    }
}

static void _DrvMpTestItoTestMsg22xxRegReset(void)
{
    DBG("*** %s() ***\n", __func__);

    RegSet16BitValueOn(0x1102, (BIT3 | BIT4 | BIT5 | BIT6 | BIT7));
    RegSet16BitValueOff(0x1130, (BIT0 | BIT1 | BIT2 | BIT3 | BIT8));

    RegSet16BitValueOn(0x1112, BIT15);
    RegSet16BitValueOff(0x1112, BIT13);

    RegSet16BitValueOn(0x1250, ((5 << 0) & 0x007F));
    RegSet16BitValueOn(0x1250, ((1 << 8) & 0x7F00));
    RegSet16BitValueOff(0x1250, 0x8080);

    RegSet16BitValueOff(0x1312, (BIT12 | BIT14));

    RegSet16BitValueOn(0x1300, BIT15);
    RegSet16BitValueOff(0x1300, (BIT10 | BIT11 | BIT12 | BIT13 | BIT14));

    RegSet16BitValueOn(0x1130, BIT9);

    RegSet16BitValueOn(0x1318, (BIT12 | BIT13));

    //RegSet16BitValue(0x121A, ((8 - 1) & 0x01FF));       // sampling number group A
    RegSet16BitValue(0x121C, ((8 - 1) & 0x01FF));       // sampling number group B
    RegSet16BitValueOn(0x131A, 0x2000);
    RegSet16BitValueOn(0x131C, BIT9);
    RegSet16BitValueOff(0x131C, (BIT8 | BIT10));


    RegSet16BitValueOff(0x1174, 0x0F00);

    RegSet16BitValue(0x1240, 0x0000);       // mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense
    RegSet16BitValue(0x1242, 0x0000);       // mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense

    RegSet16BitValue(0x1212, 0xFFFF);       // timing group A/B selection for total 24 subframes, 0: group A, 1: group B
    RegSet16BitValue(0x1214, 0x00FF);       // timing group A/B selection for total 24 subframes, 0: group A, 1: group B
    //RegSet16BitValue( 0x1212, 0);       // timing group A/B selection for total 24 subframes, 0: group A, 1: group B
    //RegSet16BitValue( 0x1214, 0);       // timing group A/B selection for total 24 subframes, 0: group A, 1: group B

    RegSet16BitValue(0x121E, 0xFFFF);   //sample number group A/B selection for total 24 subframes, 0: group A, 1: group B       
    RegSet16BitValue(0x1220, 0x00FF);   //sample number group A/B selection for total 24 subframes, 0: group A, 1: group B    
    //RegSet16BitValue( 0x121E, 0);   //sample number group A/B selection for total 24 subframes, 0: group A, 1: group B       
    //RegSet16BitValue( 0x1220, 0);   //sample number group A/B selection for total 24 subframes, 0: group A, 1: group B    

    RegSet16BitValue(0x120E, 0x0);  // noise sense mode selection for total 24 subframes
    RegSet16BitValue(0x1210, 0x0);  // noise sense mode selection for total 24 subframes

    RegSet16BitValue(0x128C, 0x0F);  // ADC afe gain correction bypass
//    mdelay(50);
    msleep(50);
}

static void _DrvMpTestItoTestMsg22xxGetChargeDumpTime(u16 nMode, u16 *pChargeTime, u16 *pDumpTime)
{
    u16 nChargeTime = 0, nDumpTime = 0;
    u16 nMinChargeTime = 0xFFFF, nMinDumpTime = 0xFFFF, nMaxChargeTime = 0x0000, nMaxDumpTime = 0x0000;
    
    DBG("*** %s() ***\n", __func__);

    nChargeTime = RegGet16BitValue(0x1226);
    nDumpTime = RegGet16BitValue(0x122A);

    if (nMinChargeTime > nChargeTime)
    {
        nMinChargeTime = nChargeTime;
    }

    if (nMaxChargeTime < nChargeTime)
    {
        nMaxChargeTime = nChargeTime;
    }

    if (nMinDumpTime > nDumpTime)
    {
        nMinDumpTime = nDumpTime;
    }

    if (nMaxDumpTime < nDumpTime)
    {
        nMaxDumpTime = nDumpTime;
    }

    DBG("nChargeTime = %d, nDumpTime = %d\n", nChargeTime, nDumpTime);
    
    if (nMode == 1)
    {
        *pChargeTime = nMaxChargeTime;
        *pDumpTime = nMaxDumpTime;
    }
    else
    {
        *pChargeTime = nMinChargeTime;
        *pDumpTime = nMinDumpTime;
    }
}

static void _DrvMpTestItoOpenTestMsg22xxFirst(u8 nItemId, s16 *pRawData, s8 *pDataFlag)		
{
    u32 i;
    s16 szTmpRawData[MAX_CHANNEL_NUM*2] = {0};
    u16 nSubFrameNum = 0;
    u16 nChargeTime, nDumpTime;
    u8 	*pMapping = NULL;
    
    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);
	
    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073

    _DrvMpTestItoTestMsg22xxRegReset();

    switch (nItemId)
    {
        case 40:
            pMapping = &_gMAP1[0];
            nSubFrameNum = _gOpenSubFrameNum1;
            break;
        case 41:
            pMapping = &_gMAP2[0];
            nSubFrameNum = _gOpenSubFrameNum2;
            break;
        case 42:
            pMapping = &_gMAP3[0];
            nSubFrameNum = _gOpenSubFrameNum3;
            break;
    }

    if (nSubFrameNum > 24) // MAX_CHANNEL_NUM/2
    {
        nSubFrameNum = 24;
    }


    _DrvMpTestItoTestMsg22xxSendDataIn(nItemId - 36, nSubFrameNum * 6);

    if (true)
    {
        RegSet16BitValue(0x1110, 0x0060); //2.4V -> 1.2V    
    }
    else
    {
        RegSet16BitValue(0x1110, 0x0020); //3.0V -> 0.6V
    }
    
    RegSet16BitValue(0x11C8, 0x000F); //csub sel overwrite enable, will referance value of 11C0
    RegSet16BitValue(0x1174, 0x0F06); // 1 : sel idel driver for sensor pad that connected to AFE
    RegSet16BitValue(0x1208, 0x0006); //PRS1
    RegSet16BitValue(0x1240, 0xFFFF); //mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense
    RegSet16BitValue(0x1242, 0x00FF); //mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense
    RegSet16BitValue(0x1216, (nSubFrameNum - 1) << 1); // subframe numbers, 0:1subframe, 1:2subframe  

    _DrvMpTestItoTestMsg22xxGetChargeDumpTime(1, &nChargeTime, &nDumpTime);

    RegSet16BitValue(0x1226, nChargeTime);
    RegSet16BitValue(0x122A, nDumpTime);

    _DrvMpTestItoTestMsg22xxSetC(CSUB_REF);
    _DrvMpTestItoTestSwReset();
    _DrvMpTestItoTestMsg22xxGetDataOut(szTmpRawData, nSubFrameNum);

//    for (i = 0; i < (MAX_CHANNEL_NUM/2) ; i ++)
    for (i = 0; i < nSubFrameNum ; i ++)
    {
//        DBG("szTmpRawData[%d * 4] >> 3 = %d\n", i, szTmpRawData[i * 4] >> 3); // add for debug
//        DBG("pMapping[%d] = %d\n", i, pMapping[i]); // add for debug

        pRawData[pMapping[i]] = (szTmpRawData[i * 4] >> 3);  // Filter to ADC 
        pDataFlag[pMapping[i]] = 1;
    }
}

static void _DrvMpTestItoShortTestMsg22xxFirst(u8 nItemId, s16 *pRawData, s8 *pDataFlag)		
{
    u32 i, j;
    s16 szRawData[MAX_CHANNEL_NUM*2] = {0};
    s16 szTmpRawData[MAX_CHANNEL_NUM*2] = {0};
    u16 nSensorNum = 0, nSubFrameNum = 0;
    u8 	*pMapping = NULL;
    
    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);

    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073

    _DrvMpTestItoTestMsg22xxRegReset();

    switch(nItemId)
    {
        case 0: // 39-4 (2R)
            pMapping = &_gSHORT_MAP4[0];
            nSensorNum = _gShortSubFrameNum4;
            break;
        case 1: // 39-1
            pMapping = &_gSHORT_MAP1[0];
            nSensorNum = _gShortSubFrameNum1;
            break;
        case 2: // 39-2
            pMapping = &_gSHORT_MAP2[0];
            nSensorNum = _gShortSubFrameNum2;
            break;
        case 3: // 39-3
            pMapping = &_gSHORT_MAP3[0];
            nSensorNum = _gShortSubFrameNum3;
            break;            
    }
    
    if (nSensorNum > 24) // MAX_CHANNEL_NUM/2
    {
        nSubFrameNum = 24;
    }
    else
    {
        nSubFrameNum = nSensorNum;
    }

    _DrvMpTestItoTestMsg22xxSendDataIn(nItemId, nSubFrameNum * 6);


    //RegSet16BitValue(0x1110, 0x0030); // [6:4}  011 : 2.1V -> 1.5V
    RegSet16BitValue(0x1110, 0x0060); // 2.4V -> 1.2V
    RegSet16BitValue(0x11C8, 0x000F); // csub sel overwrite enable, will referance value of 11C0
    RegSet16BitValue(0x1174, 0x0006); // [11:8] 000 : sel active driver for sensor pad that connected to AFE
    RegSet16BitValue(0x1208, 0x0006); // PRS1
    RegSet16BitValueOn(0x1104, BIT14); // R mode 
    RegSet16BitValue(0x1216, (nSubFrameNum - 1) << 1); // subframe numbers, 0:1subframe, 1:2subframe
    RegSet16BitValue(0x1176, 0x0000); // CFB 10p
    RegSet16BitValue(0x1226, 0x000B); // Charge 4ns
    RegSet16BitValue(0x122A, 0x000B); // Dump 4ns     


    _DrvMpTestItoTestMsg22xxSetC(CSUB_REF);
    _DrvMpTestItoTestSwReset();
    _DrvMpTestItoTestMsg22xxGetDataOut(szTmpRawData, nSubFrameNum);
    
    if (nSensorNum > 24)
    {
        j = 0;

//        for (i = 0; i < (MAX_CHANNEL_NUM/2); i ++)
        for (i = 0; i < nSubFrameNum; i ++)
        {
            szRawData[j] = (szTmpRawData[i * 4] >> 2);
            j ++;
            
            if ((nSensorNum - 24) > i)
            {
                szRawData[j] = (szTmpRawData[i * 4 + 1] >> 2);
                j ++;
            }    
        }

//        for (i = 0; i < (MAX_CHANNEL_NUM/2); i ++)
        for (i = 0; i < nSubFrameNum; i ++)
        {
//            DBG("szRawData[%d] = %d\n", i, szRawData[i]); // add for debug
//            DBG("pMapping[%d] = %d\n", i, pMapping[i]); // add for debug

            pRawData[pMapping[i]] = szRawData[i];
            pDataFlag[pMapping[i]] = 1;
        }
    }
    else
    {
//        for (i = 0; i < (MAX_CHANNEL_NUM/2); i ++)
        for (i = 0; i < nSubFrameNum; i ++)
        {
//            DBG("szTmpRawData[%d * 4] >> 2 = %d\n", i, szTmpRawData[i * 4] >> 2); // add for debug
//            DBG("pMapping[%d] = %d\n", i, pMapping[i]); // add for debug

            pRawData[pMapping[i]] = (szTmpRawData[i * 4] >> 2);    
            pDataFlag[pMapping[i]] = 1;
        }
    }
}

#endif

//------------------------------------------------------------------------------//

static ItoTestResult_e _DrvMpTestItoOpenTestSecond(u8 nItemId)
{
    ItoTestResult_e nRetVal = ITO_TEST_OK;
    u32 i;
    s32 nTmpRawDataJg1 = 0;
    s32 nTmpRawDataJg2 = 0;
    s32 nTmpJgAvgThMax1 = 0;
    s32 nTmpJgAvgThMin1 = 0;
    s32 nTmpJgAvgThMax2 = 0;
    s32 nTmpJgAvgThMin2 = 0;

    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 40)    			
    {
        for (i = 0; i < (_gItoTestTriangleNum/2)-2; i ++)
        {
            nTmpRawDataJg1 += _gRawData1[_gMAP40_1[i]];
        }

        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg2 += _gRawData1[_gMAP40_2[i]];
        }
    }
    else if (nItemId == 41)    		
    {
        for (i = 0; i < (_gItoTestTriangleNum/2)-2; i ++)
        {
            nTmpRawDataJg1 += _gRawData2[_gMAP41_1[i]];
        }
		
        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg2 += _gRawData2[_gMAP41_2[i]];
        }
    }

    nTmpJgAvgThMax1 = (nTmpRawDataJg1 / ((_gItoTestTriangleNum/2)-2)) * ( 100 + OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin1 = (nTmpRawDataJg1 / ((_gItoTestTriangleNum/2)-2)) * ( 100 - OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMax2 = (nTmpRawDataJg2 / 2) * ( 100 + OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin2 = (nTmpRawDataJg2 / 2 ) * ( 100 - OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
	
    DBG("nItemId = %d, nTmpRawDataJg1 = %d, nTmpJgAvgThMax1 = %d, nTmpJgAvgThMin1 = %d, nTmpRawDataJg2 = %d, nTmpJgAvgThMax2 = %d, nTmpJgAvgThMin2 = %d\n", nItemId, nTmpRawDataJg1, nTmpJgAvgThMax1, nTmpJgAvgThMin1, nTmpRawDataJg2, nTmpJgAvgThMax2, nTmpJgAvgThMin2);

    if (nItemId == 40) 
    {
        for (i = 0; i < (_gItoTestTriangleNum/2)-2; i ++)
        {
            if (_gRawData1[_gMAP40_1[i]] > nTmpJgAvgThMax1 || _gRawData1[_gMAP40_1[i]] < nTmpJgAvgThMin1)
            { 
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_1[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData1[_gMAP40_2[i]] > nTmpJgAvgThMax2 || _gRawData1[_gMAP40_2[i]] < nTmpJgAvgThMin2)
            { 
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_2[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        } 
    }
    else if (nItemId == 41) 
    {
        for (i = 0; i < (_gItoTestTriangleNum/2)-2; i ++)
        {
            if (_gRawData2[_gMAP41_1[i]] > nTmpJgAvgThMax1 || _gRawData2[_gMAP41_1[i]] < nTmpJgAvgThMin1) 
            { 
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_1[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
        
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData2[_gMAP41_2[i]] > nTmpJgAvgThMax2 || _gRawData2[_gMAP41_2[i]] < nTmpJgAvgThMin2) 
            { 
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_2[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
    }

    return nRetVal;
}

static ItoTestResult_e _DrvMpTestItoOpenTestSecond2r(u8 nItemId)
{
    ItoTestResult_e nRetVal = ITO_TEST_OK;
    u32 i;
    s32 nTmpRawDataJg1 = 0;
    s32 nTmpRawDataJg2 = 0;
    s32 nTmpRawDataJg3 = 0;
    s32 nTmpRawDataJg4 = 0;
    s32 nTmpJgAvgThMax1 = 0;
    s32 nTmpJgAvgThMin1 = 0;
    s32 nTmpJgAvgThMax2 = 0;
    s32 nTmpJgAvgThMin2 = 0;
    s32 nTmpJgAvgThMax3 = 0;
    s32 nTmpJgAvgThMin3 = 0;
    s32 nTmpJgAvgThMax4 = 0;
    s32 nTmpJgAvgThMin4 = 0;

    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 40)    			
    {
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            nTmpRawDataJg1 += _gRawData1[_gMAP40_1[i]];  //first region: non-border 
        }
		
        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg2 += _gRawData1[_gMAP40_2[i]];  //first region: border
        }

        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            nTmpRawDataJg3 += _gRawData1[_gMAP40_3[i]];  //second region: non-border
        }
		
        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg4 += _gRawData1[_gMAP40_4[i]];  //second region: border
        }
    }
    else if (nItemId == 41)    		
    {
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            nTmpRawDataJg1 += _gRawData2[_gMAP41_1[i]];  //first region: non-border
        }
		
        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg2 += _gRawData2[_gMAP41_2[i]];  //first region: border
        }
		
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            nTmpRawDataJg3 += _gRawData2[_gMAP41_3[i]];  //second region: non-border
        }
		
        for (i = 0; i < 2; i ++)
        {
            nTmpRawDataJg4 += _gRawData2[_gMAP41_4[i]];  //second region: border
        }
    }

    nTmpJgAvgThMax1 = (nTmpRawDataJg1 / ((_gItoTestTriangleNum/4)-2)) * ( 100 + OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin1 = (nTmpRawDataJg1 / ((_gItoTestTriangleNum/4)-2)) * ( 100 - OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMax2 = (nTmpRawDataJg2 / 2) * ( 100 + OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin2 = (nTmpRawDataJg2 / 2) * ( 100 - OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMax3 = (nTmpRawDataJg3 / ((_gItoTestTriangleNum/4)-2)) * ( 100 + OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin3 = (nTmpRawDataJg3 / ((_gItoTestTriangleNum/4)-2)) * ( 100 - OPEN_TEST_NON_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMax4 = (nTmpRawDataJg4 / 2) * ( 100 + OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin4 = (nTmpRawDataJg4 / 2) * ( 100 - OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
		
    DBG("nItemId = %d, nTmpRawDataJg1 = %d, nTmpJgAvgThMax1 = %d, nTmpJgAvgThMin1 = %d, nTmpRawDataJg2 = %d, nTmpJgAvgThMax2 = %d, nTmpJgAvgThMin2 = %d\n", nItemId, nTmpRawDataJg1, nTmpJgAvgThMax1, nTmpJgAvgThMin1, nTmpRawDataJg2, nTmpJgAvgThMax2, nTmpJgAvgThMin2);
    DBG("nTmpRawDataJg3 = %d, nTmpJgAvgThMax3 = %d, nTmpJgAvgThMin3 = %d, nTmpRawDataJg4 = %d, nTmpJgAvgThMax4 = %d, nTmpJgAvgThMin4 = %d\n", nTmpRawDataJg3, nTmpJgAvgThMax3, nTmpJgAvgThMin3, nTmpRawDataJg4, nTmpJgAvgThMax4, nTmpJgAvgThMin4);

    if (nItemId == 40) 
    {
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            if (_gRawData1[_gMAP40_1[i]] > nTmpJgAvgThMax1 || _gRawData1[_gMAP40_1[i]] < nTmpJgAvgThMin1) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_1[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData1[_gMAP40_2[i]] > nTmpJgAvgThMax2 || _gRawData1[_gMAP40_2[i]] < nTmpJgAvgThMin2) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_2[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        } 
		
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            if (_gRawData1[_gMAP40_3[i]] > nTmpJgAvgThMax3 || _gRawData1[_gMAP40_3[i]] < nTmpJgAvgThMin3) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_3[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData1[_gMAP40_4[i]] > nTmpJgAvgThMax4 || _gRawData1[_gMAP40_4[i]] < nTmpJgAvgThMin4) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP40_4[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        } 
    }
    else if (nItemId == 41) 
    {
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            if (_gRawData2[_gMAP41_1[i]] > nTmpJgAvgThMax1 || _gRawData2[_gMAP41_1[i]] < nTmpJgAvgThMin1) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_1[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData2[_gMAP41_2[i]] > nTmpJgAvgThMax2 || _gRawData2[_gMAP41_2[i]] < nTmpJgAvgThMin2) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_2[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < (_gItoTestTriangleNum/4)-2; i ++)
        {
            if (_gRawData2[_gMAP41_3[i]] > nTmpJgAvgThMax3 || _gRawData2[_gMAP41_3[i]] < nTmpJgAvgThMin3) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_3[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
		
        for (i = 0; i < 2; i ++)
        {
            if (_gRawData2[_gMAP41_4[i]] > nTmpJgAvgThMax4 || _gRawData2[_gMAP41_4[i]] < nTmpJgAvgThMin4) 
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gMAP41_4[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        } 
    }

    return nRetVal;
}

static ItoTestResult_e _DrvMpTestItoShortTestSecond(u8 nItemId)
{
    ItoTestResult_e nRetVal = ITO_TEST_OK;
    u32 i;
    u8 nSensorCount = 0;
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    u8 nNumOfSensorMapping1, nNumOfSensorMapping2;
#endif //CONFIG_ENABLE_CHIP_MSG21XXA
	
    DBG("*** %s() nItemId = %d ***\n", __func__, nItemId);

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    if ((_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) % 2 != 0)
    {
        nNumOfSensorMapping1 = (_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) / 2 + 1;
        nNumOfSensorMapping2 = nNumOfSensorMapping1;
    }
    else
    {
        nNumOfSensorMapping1 = (_gItoTestTriangleNum + _gItoTestKeyNum + _gItoTestDummyNum) / 2;
        nNumOfSensorMapping2 = nNumOfSensorMapping1;
        if (nNumOfSensorMapping2 % 2 != 0)
        {	
            nNumOfSensorMapping2 ++;
        }
    }        
#endif //CONFIG_ENABLE_CHIP_MSG21XXA

    if (nItemId == 0) // 39-4 (2R)   
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        nSensorCount = _gItoTestTriangleNum/2;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        nSensorCount = _gShortSubFrameNum4;
#endif        
        for (i = 0; i < nSensorCount; i ++)
        {
            if (_gRawData4[_gSHORT_MAP4[i]] > SHORT_TEST_THRESHOLD)
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gSHORT_MAP4[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
    }
    else if (nItemId == 1) // 39-1
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        nSensorCount = nNumOfSensorMapping1;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        nSensorCount = _gShortSubFrameNum1;
#endif        
        for (i = 0; i < nSensorCount; i ++)
        {
            if (_gRawData1[_gSHORT_MAP1[i]] > SHORT_TEST_THRESHOLD)
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gSHORT_MAP1[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
    }
    else if (nItemId == 2) // 39-2
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        nSensorCount = nNumOfSensorMapping2;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        nSensorCount = _gShortSubFrameNum2;
#endif        
        for (i = 0; i < nSensorCount; i ++)
        {
            if (_gRawData2[_gSHORT_MAP2[i]] > SHORT_TEST_THRESHOLD)
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gSHORT_MAP2[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
    }
    else if (nItemId == 3) // 39-3
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        nSensorCount = _gItoTestTriangleNum;
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        nSensorCount = _gShortSubFrameNum3;
#endif        
        for (i = 0; i < nSensorCount; i ++)
        {
            if (_gRawData3[_gSHORT_MAP3[i]] > SHORT_TEST_THRESHOLD)
            {
                _gTestFailChannel[_gTestFailChannelCount] = _gSHORT_MAP3[i];
                _gTestFailChannelCount ++; 
                nRetVal = ITO_TEST_FAIL;
            }
        }
    }
    DBG("nSensorCount = %d\n", nSensorCount);

    return nRetVal;
}

s32 _DrvMpTestItoOpenTest(void)
{
    ItoTestResult_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 = ITO_TEST_OK;
    u32 i;

    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    DmaAlloc();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

    DBG("open test start\n");

    DrvPlatformLyrSetIicDataRate(g_I2cClient, 50000); //50 KHZ
  
    DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDeviceResetHw();
    
    if (!_DrvMpTestItoTestChooseTpType())
    {
        DBG("Choose Tp Type failed\n");
        nRetVal1 = ITO_TEST_GET_TP_TYPE_ERROR;
        goto ITO_TEST_END;
    }
    
    DbBusEnterSerialDebugMode();
    DbBusStopMCU();
    DbBusIICUseBus();
    DbBusIICReshape();
//    mdelay(100);
    msleep(100);

    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073

    // Stop watchdog
    RegSet16BitValue(0x3C60, 0xAA55); //bank:reg_PIU_MISC_0, addr:h0030

//    mdelay(50);
    msleep(50);
    
    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        _gRawData1[i] = 0;
        _gRawData2[i] = 0;
        _gRawData3[i] = 0;
        _gDataFlag1[i] = 0;
        _gDataFlag2[i] = 0;
        _gDataFlag3[i] = 0;
    }	
	
    _gTestFailChannelCount = 0; // Reset _gTestFailChannelCount to 0 before test start

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoOpenTestMsg21xxaFirst(40, _gRawData1, _gDataFlag1);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoOpenTestMsg22xxFirst(40, _gRawData1, _gDataFlag1);
#endif

    if (_gIsEnable2R)
    {
        nRetVal2 = _DrvMpTestItoOpenTestSecond2r(40);
    }
    else
    {
        nRetVal2 = _DrvMpTestItoOpenTestSecond(40);
    }
    
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoOpenTestMsg21xxaFirst(41, _gRawData2, _gDataFlag2);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoOpenTestMsg22xxFirst(41, _gRawData2, _gDataFlag2);
#endif

    if (_gIsEnable2R)
    {
        nRetVal3 = _DrvMpTestItoOpenTestSecond2r(41);
    }
    else
    {
        nRetVal3 = _DrvMpTestItoOpenTestSecond(41);
    }
/*    
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoOpenTestMsg21xxaFirst(42, _gRawData3, _gDataFlag3);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoOpenTestMsg22xxFirst(42, _gRawData3, _gDataFlag3);
#endif
*/
    
    ITO_TEST_END:
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    DmaFree();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

    DrvPlatformLyrSetIicDataRate(g_I2cClient, 100000); //100 KHZ
    
    DrvPlatformLyrTouchDeviceResetHw();
    DrvPlatformLyrEnableFingerTouchReport();

    DBG("open test end\n");

    if ((nRetVal1 != ITO_TEST_OK) && (nRetVal2 == ITO_TEST_OK) && (nRetVal3 == ITO_TEST_OK))
    {
        return ITO_TEST_GET_TP_TYPE_ERROR;		
    }
    else if ((nRetVal1 == ITO_TEST_OK) && ((nRetVal2 != ITO_TEST_OK) || (nRetVal3 != ITO_TEST_OK)))
    {
        return ITO_TEST_FAIL;	
    }
    else
    {
        return ITO_TEST_OK;	
    }
}

static ItoTestResult_e _DrvMpTestItoShortTest(void)
{
    ItoTestResult_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 = ITO_TEST_OK, nRetVal4 = ITO_TEST_OK, nRetVal5 = ITO_TEST_OK;
    u32 i = 0;

    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    DmaAlloc();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

    DBG("short test start\n");

    DrvPlatformLyrSetIicDataRate(g_I2cClient, 50000); //50 KHZ
  
    DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDeviceResetHw(); 
    
    if (!_DrvMpTestItoTestChooseTpType())
    {
        DBG("Choose Tp Type failed\n");
        nRetVal1 = ITO_TEST_GET_TP_TYPE_ERROR;
        goto ITO_TEST_END;
    }
    
    DbBusEnterSerialDebugMode();
    DbBusStopMCU();
    DbBusIICUseBus();
    DbBusIICReshape();
//    mdelay(100);
    msleep(100);

    // Stop cpu
    RegSet16BitValue(0x0FE6, 0x0001); //bank:mheg5, addr:h0073

    // Stop watchdog
    RegSet16BitValue(0x3C60, 0xAA55); //bank:reg_PIU_MISC_0, addr:h0030

//    mdelay(50);
    msleep(50);
    
    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        _gRawData1[i] = 0;
        _gRawData2[i] = 0;
        _gRawData3[i] = 0;
        _gRawData4[i] = 0;
        _gDataFlag1[i] = 0;
        _gDataFlag2[i] = 0;
        _gDataFlag3[i] = 0;
        _gDataFlag4[i] = 0;
    }	
	
    _gTestFailChannelCount = 0; // Reset _gTestFailChannelCount to 0 before test start
	
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoShortTestMsg21xxaFirst(1, _gRawData1, _gDataFlag1);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoShortTestMsg22xxFirst(1, _gRawData1, _gDataFlag1);
#endif

    nRetVal2 = _DrvMpTestItoShortTestSecond(1);

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoShortTestMsg21xxaFirst(2, _gRawData2, _gDataFlag2);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoShortTestMsg22xxFirst(2, _gRawData2, _gDataFlag2);
#endif

    nRetVal3 = _DrvMpTestItoShortTestSecond(2);

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
    _DrvMpTestItoShortTestMsg21xxaFirst(3, _gRawData3, _gDataFlag3);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
    _DrvMpTestItoShortTestMsg22xxFirst(3, _gRawData3, _gDataFlag3);
#endif

    nRetVal4 = _DrvMpTestItoShortTestSecond(3);
    
    if (_gIsEnable2R)
    {
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
        _DrvMpTestItoShortTestMsg21xxaFirst(0, _gRawData4, _gDataFlag4);
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
        _DrvMpTestItoShortTestMsg22xxFirst(0, _gRawData4, _gDataFlag4);
#endif
        
        nRetVal5 = _DrvMpTestItoShortTestSecond(0);
    }

    ITO_TEST_END:
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    DmaFree();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

    DrvPlatformLyrSetIicDataRate(g_I2cClient, 100000); //100 KHZ
    
    DrvPlatformLyrTouchDeviceResetHw();
    DrvPlatformLyrEnableFingerTouchReport();

    DBG("short test end\n");
    
    if ((nRetVal1 != ITO_TEST_OK) && (nRetVal2 == ITO_TEST_OK) && (nRetVal3 == ITO_TEST_OK) && (nRetVal4 == ITO_TEST_OK) && (nRetVal5 == ITO_TEST_OK))
    {
        return ITO_TEST_GET_TP_TYPE_ERROR;		
    }
    else if ((nRetVal1 == ITO_TEST_OK) && ((nRetVal2 != ITO_TEST_OK) || (nRetVal3 != ITO_TEST_OK) || (nRetVal4 != ITO_TEST_OK) || (nRetVal5 != ITO_TEST_OK)))
    {
        return ITO_TEST_FAIL;	
    }
    else
    {
        return ITO_TEST_OK;	
    }
}

/*
static s32 DrvMpTestProcReadDebug(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    s32 nCount = 0;
    
    DBG("*** %s() ***\n", __func__);

    _gItoTestResult = _DrvMpTestItoOpenTest();
    
    if (ITO_TEST_OK == _gItoTestResult)
    {
        DBG("ITO_TEST_OK");
    }
    else if (ITO_TEST_FAIL == _gItoTestResult)
    {
        DBG("ITO_TEST_FAIL");
    }
    else if (ITO_TEST_GET_TP_TYPE_ERROR == _gItoTestResult)
    {
        DBG("ITO_TEST_GET_TP_TYPE_ERROR");
    }
    
    *eof = 1;
    
    return nCount;
}

static s32 DrvMpTestProcWriteDebug(struct file *file, const char *buffer, unsigned long count, void *data)
{    
    u32 i;
    
    DBG("*** %s() ***\n", __func__);

    DBG("ito test result : %d", _gItoTestResult);

    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        DBG("_gRawData1[%d]=%d\n", i, _gRawData1[i]);
    }
    mdelay(5);

    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        DBG("_gRawData2[%d]=%d\n", i, _gRawData2[i]);
    }
    mdelay(5);

    for (i = 0; i < MAX_CHANNEL_NUM; i ++)
    {
        DBG("_gRawData3[%d]=%d;\n",i , _gRawData3[i]);
    }
    mdelay(5);
    
    return count;
}
*/

static void _DrvMpTestItoTestDoWork(struct work_struct *pWork)
{
    s32 nRetVal = ITO_TEST_OK;
    
    DBG("*** %s() _gIsInMpTest = %d, _gTestRetryCount = %d ***\n", __func__, _gIsInMpTest, _gTestRetryCount);

    if (_gItoTestMode == ITO_TEST_MODE_OPEN_TEST)
    {
        nRetVal = _DrvMpTestItoOpenTest();
    }
    else if (_gItoTestMode == ITO_TEST_MODE_SHORT_TEST)
    {
        nRetVal = _DrvMpTestItoShortTest();
    }
    else
    {
        DBG("*** Undefined Mp Test Mode = %d ***\n", _gItoTestMode);
        return;
    }

    DBG("*** ctp mp test result = %d ***\n", nRetVal);
    
    if (nRetVal == ITO_TEST_OK)
    {
        _gCtpMpTestStatus = ITO_TEST_OK;
        _gIsInMpTest = 0;
        DBG("mp test success\n");

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
        DrvFwCtrlRestoreFirmwareModeToLogDataMode();    
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG
    }
    else
    {
        _gTestRetryCount --;
        if (_gTestRetryCount > 0)
        {
            DBG("_gTestRetryCount = %d\n", _gTestRetryCount);
            queue_work(_gCtpMpTestWorkQueue, &_gCtpItoTestWork);
        }
        else
        {
            if (nRetVal == ITO_TEST_FAIL)
            {
                _gCtpMpTestStatus = ITO_TEST_FAIL;
            }
            else if (nRetVal == ITO_TEST_GET_TP_TYPE_ERROR)
            {
                _gCtpMpTestStatus = ITO_TEST_GET_TP_TYPE_ERROR;
            }
            else
            {
                _gCtpMpTestStatus = ITO_TEST_UNDEFINED_ERROR;
            }
              
            _gIsInMpTest = 0;
            DBG("mp test failed\n");

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
            DrvFwCtrlRestoreFirmwareModeToLogDataMode();    
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG
        }
    }
}

/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/

/*
void DrvMpTestCreateProcEntry(void)
{
    _gMsgItoTest = proc_mkdir(PROC_MSG_ITO_TEST, NULL);
    _gDebug = create_proc_entry(PROC_ITO_TEST_DEBUG, 0777, _gMsgItoTest);

    if (NULL == _gDebug) 
    {
        DBG("create_proc_entry PROC_ITO_TEST_DEBUG FAIL\n");
    } 
    else 
    {
        _gDebug->read_proc = DrvMpTestProcReadDebug;
        _gDebug->write_proc = DrvMpTestProcWriteDebug;
        DBG("create_proc_entry PROC_ITO_TEST_DEBUG OK\n");
    }
}
*/

s32 DrvMpTestGetTestResult(void)
{
    DBG("*** %s() ***\n", __func__);
    DBG("_gCtpMpTestStatus = %d\n", _gCtpMpTestStatus);

    return _gCtpMpTestStatus;
}

void DrvMpTestGetTestFailChannel(ItoTestMode_e eItoTestMode, u8 *pFailChannel, u32 *pFailChannelCount)
{
    u32 i;
    
    DBG("*** %s() ***\n", __func__);
    DBG("_gTestFailChannelCount = %d\n", _gTestFailChannelCount);
    
    for (i = 0; i < _gTestFailChannelCount; i ++)
    {
    	  pFailChannel[i] = _gTestFailChannel[i];
    }
    
    *pFailChannelCount = _gTestFailChannelCount;
}

void DrvMpTestGetTestDataLog(ItoTestMode_e eItoTestMode, u8 *pDataLog, u32 *pLength)
{
    u32 i;
    u8 nHighByte, nLowByte;
    
    DBG("*** %s() ***\n", __func__);
    
    if (eItoTestMode == ITO_TEST_MODE_OPEN_TEST)
    {
        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            nHighByte = (_gRawData1[i] >> 8) & 0xFF;
            nLowByte = (_gRawData1[i]) & 0xFF;
    	  
            if (_gDataFlag1[i] == 1)
            {
                pDataLog[i*4] = 1; // indicate it is a on-use channel number
            }
            else
            {
                pDataLog[i*4] = 0; // indicate it is a non-use channel number
            }
            
            if (_gRawData1[i] >= 0)
            {
                pDataLog[i*4+1] = 0; // + : a positive number
            }
            else
            {
                pDataLog[i*4+1] = 1; // - : a negative number
            }

            pDataLog[i*4+2] = nHighByte;
            pDataLog[i*4+3] = nLowByte;
        }

        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            nHighByte = (_gRawData2[i] >> 8) & 0xFF;
            nLowByte = (_gRawData2[i]) & 0xFF;
        
            if (_gDataFlag2[i] == 1)
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*4] = 1; // indicate it is a on-use channel number
            }
            else
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*4] = 0; // indicate it is a non-use channel number
            }

            if (_gRawData2[i] >= 0)
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*4] = 0; // + : a positive number
            }
            else
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*4] = 1; // - : a negative number
            }

            pDataLog[(i*4+2)+MAX_CHANNEL_NUM*4] = nHighByte;
            pDataLog[(i*4+3)+MAX_CHANNEL_NUM*4] = nLowByte;
        }

        *pLength = MAX_CHANNEL_NUM*8;
    }
    else if (eItoTestMode == ITO_TEST_MODE_SHORT_TEST)
    {
        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            nHighByte = (_gRawData1[i] >> 8) & 0xFF;
            nLowByte = (_gRawData1[i]) & 0xFF;

            if (_gDataFlag1[i] == 1)
            {
                pDataLog[i*4] = 1; // indicate it is a on-use channel number
            }
            else
            {
                pDataLog[i*4] = 0; // indicate it is a non-use channel number
            }

            if (_gRawData1[i] >= 0)
            {
                pDataLog[i*4+1] = 0; // + : a positive number
            }
            else
            {
                pDataLog[i*4+1] = 1; // - : a negative number
            }

            pDataLog[i*4+2] = nHighByte;
            pDataLog[i*4+3] = nLowByte;
        }

        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            nHighByte = (_gRawData2[i] >> 8) & 0xFF;
            nLowByte = (_gRawData2[i]) & 0xFF;
        
            if (_gDataFlag2[i] == 1)
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*4] = 1; // indicate it is a on-use channel number
            }
            else
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*4] = 0; // indicate it is a non-use channel number
            }

            if (_gRawData2[i] >= 0)
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*4] = 0; // + : a positive number
            }
            else
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*4] = 1; // - : a negative number
            }

            pDataLog[i*4+2+MAX_CHANNEL_NUM*4] = nHighByte;
            pDataLog[i*4+3+MAX_CHANNEL_NUM*4] = nLowByte;
        }

        for (i = 0; i < MAX_CHANNEL_NUM; i ++)
        {
            nHighByte = (_gRawData3[i] >> 8) & 0xFF;
            nLowByte = (_gRawData3[i]) & 0xFF;
        
            if (_gDataFlag3[i] == 1)
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*8] = 1; // indicate it is a on-use channel number
            }
            else
            {
                pDataLog[i*4+MAX_CHANNEL_NUM*8] = 0; // indicate it is a non-use channel number
            }

            if (_gRawData3[i] >= 0)
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*8] = 0; // + : a positive number
            }
            else
            {
                pDataLog[(i*4+1)+MAX_CHANNEL_NUM*8] = 1; // - : a negative number
            }

            pDataLog[(i*4+2)+MAX_CHANNEL_NUM*8] = nHighByte;
            pDataLog[(i*4+3)+MAX_CHANNEL_NUM*8] = nLowByte;
        }

        if (_gIsEnable2R)
        {
            for (i = 0; i < MAX_CHANNEL_NUM; i ++)
            {
                nHighByte = (_gRawData4[i] >> 8) & 0xFF;
                nLowByte = (_gRawData4[i]) & 0xFF;
        
                if (_gDataFlag4[i] == 1)
                {
                    pDataLog[i*4+MAX_CHANNEL_NUM*12] = 1; // indicate it is a on-use channel number
                }
                else
                {
                    pDataLog[i*4+MAX_CHANNEL_NUM*12] = 0; // indicate it is a non-use channel number
                }

                if (_gRawData4[i] >= 0)
                {
                    pDataLog[(i*4+1)+MAX_CHANNEL_NUM*12] = 0; // + : a positive number
                }
                else
                {
                    pDataLog[(i*4+1)+MAX_CHANNEL_NUM*12] = 1; // - : a negative number
                }

                pDataLog[(i*4+2)+MAX_CHANNEL_NUM*12] = nHighByte;
                pDataLog[(i*4+3)+MAX_CHANNEL_NUM*12] = nLowByte;
            }
        }
        
        *pLength = MAX_CHANNEL_NUM*16;
    }
    else 
    {
        DBG("*** Undefined MP Test Mode ***\n");
    }
}

void DrvMpTestScheduleMpTestWork(ItoTestMode_e eItoTestMode)
{
    DBG("*** %s() ***\n", __func__);

    if (_gIsInMpTest == 0)
    {
        DBG("ctp mp test start\n");
        
        _gItoTestMode = eItoTestMode;
        _gIsInMpTest = 1;
        _gTestRetryCount = CTP_MP_TEST_RETRY_COUNT;
        _gCtpMpTestStatus = ITO_TEST_UNDER_TESTING;
        
        queue_work(_gCtpMpTestWorkQueue, &_gCtpItoTestWork);
    }
}

void DrvMpTestCreateMpTestWorkQueue(void)
{
    DBG("*** %s() ***\n", __func__);

    _gCtpMpTestWorkQueue = create_singlethread_workqueue("ctp_mp_test");
    INIT_WORK(&_gCtpItoTestWork, _DrvMpTestItoTestDoWork);
}

#endif //CONFIG_ENABLE_ITO_MP_TEST

