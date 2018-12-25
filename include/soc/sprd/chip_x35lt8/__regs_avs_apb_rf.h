/* the head file modifier:     g   2015-03-19 15:41:46*/

/*
* Copyright (C) 2013 Spreadtrum Communications Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*************************************************
* Automatically generated C header: do not edit *
*************************************************
*/

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, Pls include sci_glb_regs.h"
#endif 


#ifndef __H_REGS_AVS_APB_RF_HEADFILE_H__
#define __H_REGS_AVS_APB_RF_HEADFILE_H__ __FILE__

#define  REGS_AVS_APB_RF

/* registers definitions for AVS_APB_RF */
#define REG_AVS_APB_RF_AVS_TUNE_LMT_CFG			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0000)/*AVS_TUNE_LMT_CFG*/
#define REG_AVS_APB_RF_AVS_TUNE_STATUS			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0004)/*AVS_TUNE_STATUS*/
#define REG_AVS_APB_RF_AVS_CFG				SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0008)/*AVS_CFG*/
#define REG_AVS_APB_RF_AVS_TUNE_STEP_CFG		SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x000C)/*AVS_TUNE_STEP_CFG*/
#define REG_AVS_APB_RF_AVS_WAIT_CFG			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0010)/*AVS_WAIT_CFG*/
#define REG_AVS_APB_RF_AVS_INT_CFG			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0014)/*AVS_INT_CFG*/
#define REG_AVS_APB_RF_AVS_START_SINGLE_ACT		SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0018)/*AVS_START_SINGLE_ACT*/
#define REG_AVS_APB_RF_AVS_START_REPEAT_ACT		SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x001C)/*AVS_START_REPEAT_ACT*/
#define REG_AVS_APB_RF_AVS_HPM_EN			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0020)/*AVS_HPM_EN*/
#define REG_AVS_APB_RF_AVS_HPM_RPT_ANLS			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0024)/*AVS_HPM_RPT_ANLS*/
#define REG_AVS_APB_RF_AVS_HPM_RPT_VLD_STATUS_0		SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0028)/*AVS_HPM_RPT_VLD_STATUS_0*/
#define REG_AVS_APB_RF_AVS_HPM_RPT_VLD_STATUS_1		SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x002C)/*AVS_HPM_RPT_VLD_STATUS_1*/
#define REG_AVS_APB_RF_AVS_FSM_STS			SCI_ADDR(REGS_AVS_APB_RF_BASE, 0x0030)/*AVS_FSM_STS*/
#define REG_AVS_APB_RF_FSM_STS 				REG_AVS_APB_RF_AVS_FSM_STS


/* bits definitions for register REG_AVS_APB_RF_AVS_TUNE_LMT_CFG */
#define BITS_VOLT_TUNE_VAL_MAX(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VOLT_TUNE_VAL_MIN(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_TUNE_STATUS */
#define BITS_VOLT_TUNE_VAL(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VOLT_OBS_VAL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_CFG */
#define BIT_HPM1_RPT_CHKSUM_ERR_EN				( BIT(17) )
#define BIT_HPM0_RPT_CHKSUM_ERR_EN				( BIT(16) )
#define BIT_PAUSE_OCCUR_ERR_EN					( BIT(8) )
#define BIT_VOLT_TUNE_FORBID_EN					( BIT(4) )
#define BIT_VOLT_OBS_FORBID_EN					( BIT(0) )

/* bits definitions for register REG_AVS_APB_RF_AVS_TUNE_STEP_CFG */
#define BITS_VOLT_TUNE_DOWN_STEP(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)) )
#define BITS_VOLT_TUNE_UP_STEP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_WAIT_CFG */
#define BITS_RND_INTVAL_WAIT_NUM(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_VOLT_STB_WAIT_NUM(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_INT_CFG */
#define BIT_AVS_TUNE_DONE_INT_MASK_STATUS			( BIT(13) )
#define BIT_AVS_TUNE_ERR_INT_MASK_STATUS			( BIT(12) )
#define BIT_AVS_TUNE_DONE_INT_RAW_STATUS			( BIT(9) )
#define BIT_AVS_TUNE_ERR_INT_RAW_STATUS				( BIT(8) )
#define BIT_AVS_TUNE_DONE_INT_CLR				( BIT(5) )
#define BIT_AVS_TUNE_ERR_INT_CLR				( BIT(4) )
#define BIT_AVS_TUNE_DONE_INT_EN				( BIT(1) )
#define BIT_AVS_TUNE_ERR_INT_EN					( BIT(0) )

/* bits definitions for register REG_AVS_APB_RF_AVS_START_SINGLE_ACT */
#define BIT_AVS_START_SINGLE_ACT				( BIT(0) )

/* bits definitions for register REG_AVS_APB_RF_AVS_START_REPEAT_ACT */
#define BIT_AVS_START_REPEAT_ACT				( BIT(0) )

/* bits definitions for register REG_AVS_APB_RF_AVS_HPM_EN */
#define BIT_HPM1_EN						( BIT(1) )
#define BIT_HPM0_EN						( BIT(0) )

/* bits definitions for register REG_AVS_APB_RF_AVS_HPM_RPT_ANLS */
#define BITS_HPM_RPT_TUNE_DWN_MARK(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM_RPT_TUNE_UP_MARK(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_HPM_RPT_VLD_STATUS_0 */
#define BITS_HPM1_RPT_VLD(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM0_RPT_VLD(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AVS_APB_RF_AVS_HPM_RPT_VLD_STATUS_1 */

/* bits definitions for register REG_AVS_APB_RF_AVS_FSM_STS */
#define BITS_AVS_FSM_STS(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

#endif
