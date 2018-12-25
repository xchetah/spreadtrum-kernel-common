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
 * @file    mstar_drv_self_mp_test.h
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */

#ifndef __MSTAR_DRV_SELF_MP_TEST_H__
#define __MSTAR_DRV_SELF_MP_TEST_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE                                                             */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"

#ifdef CONFIG_ENABLE_ITO_MP_TEST

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION                                         */
/*--------------------------------------------------------------------------*/

#define CTP_MP_TEST_RETRY_COUNT (3)


#define OPEN_TEST_NON_BORDER_AREA_THRESHOLD (35) // range : 25~60
#define OPEN_TEST_BORDER_AREA_THRESHOLD     (40) // range : 25~60

#define	SHORT_TEST_THRESHOLD                (3500)

#define	MP_TEST_MODE_OPEN_TEST              (0x01)
#define	MP_TEST_MODE_SHORT_TEST             (0x02)

#define MAX_CHANNEL_NUM   (48)

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA)
#define PIN_GUARD_RING    (46) // For MSG21XXA
#define GPO_SETTING_SIZE  (3)  // For MSG21XXA
#elif defined(CONFIG_ENABLE_CHIP_MSG22XX)
#define RIU_BASE_ADDR       (0)    // For MSG22XX
#define RIU_WRITE_LENGTH    (144)  // For MSG22XX
#define CSUB_REF            (0) //(18)   // For MSG22XX
#define CSUB_REF_MAX        (0x3F) // For MSG22XX
#endif

#define REG_INTR_FIQ_MASK           (0x04)          
#define FIQ_E_FRAME_READY_MASK      (1 << 8)


//#define PROC_MSG_ITO_TEST     "msg-ito-test"
//#define PROC_ITO_TEST_DEBUG   "debug"

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR MACRO DEFINITION                                            */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* DATA TYPE DEFINITION                                                     */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* GLOBAL VARIABLE DEFINITION                                               */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION                                              */
/*--------------------------------------------------------------------------*/

extern void DrvMpTestCreateMpTestWorkQueue(void);
extern void DrvMpTestGetTestDataLog(ItoTestMode_e eItoTestMode, u8 *pDataLog, u32 *pLength);
extern void DrvMpTestGetTestFailChannel(ItoTestMode_e eItoTestMode, u8 *pFailChannel, u32 *pFailChannelCount);
extern s32 DrvMpTestGetTestResult(void);
extern void DrvMpTestScheduleMpTestWork(ItoTestMode_e eItoTestMode);

#endif //CONFIG_ENABLE_ITO_MP_TEST

#endif  /* __MSTAR_DRV_SELF_MP_TEST_H__ */