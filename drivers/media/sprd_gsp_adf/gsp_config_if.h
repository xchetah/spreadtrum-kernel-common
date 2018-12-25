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

#ifndef __GSP_CONFIG_IF_H_
#define __GSP_CONFIG_IF_H_


/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/


#include <video/gsp_types_shark_adf.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/globalregs.h> //define IRQ_GSP_INT
#include <soc/sprd/sci.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include "gsp_drv_common.h"
#include "gsp_kcfg.h"




/**---------------------------------------------------------------------------*
 **                         struct Definition                              *
 **---------------------------------------------------------------------------*/

typedef struct _GSP_LAYER1_REG_T_TAG_
{
	union _GSP_LAYER1_CFG_TAG gsp_layer1_cfg_u;
	union _GSP_LAYER1_Y_ADDR_TAG gsp_layer1_y_addr_u;
	union _GSP_LAYER1_UV_ADDR_TAG gsp_layer1_uv_addr_u;
	union _GSP_LAYER1_VA_ADDR_TAG gsp_layer1_va_addr_u;
	union _GSP_LAYER1_PITCH_TAG gsp_layer1_pitch_u;
	union _GSP_LAYER1_CLIP_START_TAG gsp_layer1_clip_start_u;
	union _GSP_LAYER1_CLIP_SIZE_TAG gsp_layer1_clip_size_u;
	union _GSP_LAYER1_DES_START_TAG gsp_layer1_des_start_u;
	union _GSP_LAYER1_GREY_RGB_TAG gsp_layer1_grey_rgb_u;
	union _GSP_LAYER1_ENDIAN_TAG gsp_layer1_endian_u;
	union _GSP_LAYER1_ALPHA_TAG gsp_layer1_alpha_u;
	union _GSP_LAYER1_CK_TAG gsp_layer1_ck_u;
}
GSP_LAYER1_REG_T;

typedef struct _GSP_CMDQ_REG_T_TAG_
{
	union _GSP_CMD_ADDR_TAG gsp_cmd_addr_u;
	union _GSP_CMD_CFG_TAG gsp_cmd_cfg_u;
}
GSP_CMDQ_REG_T;

#define GSP_L1_CMDQ_SET(cfg)\
	*(GSP_CMDQ_REG_T*)(&((volatile GSP_REG_T*)(addr))->gsp_cmd_addr_u) = (cfg)


/**---------------------------------------------------------------------------*
 **                         Macro Definition                              *
 **---------------------------------------------------------------------------*/


#ifdef CONFIG_FPGA
/*GSP job config relative*/
#define GSP_MOD_EN          (SPRD_AHB_BASE)
#define GSP_SOFT_RESET      (SPRD_AHB_BASE + 0x04)

/*GSP DDR access relative*/
/*GSP access DDR through matrix to AXI, must enable gsp-gate on this matrix*/
#define GSP_EMC_MATRIX_BASE		(SPRD_AONAPB_BASE + 0x04)
/*[11] gsp-gate bit on matrix , EMC is DDR controller, should always enabled*/
#define GSP_EMC_MATRIX_BIT		(1<<11)

//GSP inner work loggy clock ctl
#define GSP_CLOCK_BASE		(SPRD_APBCKG_BASE + 0x28)
#define GSP_CLOCK_256M_BIT  (3)// div form  PLL clock, use[1:0] 2bit,  0:96M 1:153.6M 2:192M 3:256M

/*force enable GSP inner work loggy clock, used for debug*/
#define GSP_AUTO_GATE_ENABLE_BASE		(SPRD_AHB_BASE + 0x40)
/*[8] is gate switch, 1:GSP work clk enable by busy signal, 0:force enable, control by busy will save power*/
#define GSP_AUTO_GATE_ENABLE_BIT		(1<<8)

/*GSP register set clock , through AHB bus*/
#define GSP_AHB_CLOCK_BASE      (SPRD_APBCKG_BASE + 0x20)
#define GSP_AHB_CLOCK_26M_BIT   (0)// [1:0] is used by GSP, 0:26M
#define GSP_AHB_CLOCK_192M_BIT  (3)// [1:0] is used by GSP, 0:26M 1:76M 2:128M 3:192M


/*interrupt relative*/
#define TB_GSP_INT 			(0x33)  /*gsp hardware irq number*/
#define GSP_IRQ_BIT			(1<<19) /*gsp hardware irq bit, == (TB_GSP_INT % 32)*/
#define GSP_SOFT_RST_BIT    (1<<3) /*gsp chip module soft reset bit*/
#define GSP_MOD_EN_BIT      (1<<3) /*gsp chip module enable bit*/

#else
/*GSP job config relative*/
#define GSP_MOD_EN          (REG_AP_AHB_AHB_EB)
#define GSP_SOFT_RESET      (REG_AP_AHB_AHB_RST)

/*GSP DDR access relative*/
#define GSP_EMC_MATRIX_BASE		(REG_AON_APB_APB_EB1) // GSP access DDR through matrix to AXI, must enable gsp-gate on this matrix
#define GSP_EMC_MATRIX_BIT		(BIT_GSP_EMC_EB) // [13] gsp-gate bit on matrix , EMC is DDR controller, should always enabled
#define GSP_CLK_SEL_BIT_MASK		(0x3)

/*GSP inner work loggy clock ctl*/
#define GSP_CLOCK_BASE		(REG_AP_CLK_GSP_CFG)
typedef enum _GSP_core_freq_
{
	GSP_CLOCK_96M_BIT,
	GSP_CLOCK_153P6M_BIT,
	GSP_CLOCK_192M_BIT,
	GSP_CLOCK_256M_BIT
}
GSP_CORE_GREQ;


/*force enable GSP inner work loggy clock, used for debug*/
#define GSP_AUTO_GATE_ENABLE_BASE		(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG)
/*[8] is gate switch, 1:GSP work clk enable by busy signal, 0:force enable, control by busy will save power*/
#define GSP_AUTO_GATE_ENABLE_BIT		(BIT_GSP_AUTO_GATE_EN)
#define GSP_CKG_FORCE_ENABLE_BIT		(BIT_GSP_CKG_FORCE_EN)

/*GSP register set clock , through AHB bus*/
#define GSP_AHB_CLOCK_BASE      (REG_AP_CLK_AP_AHB_CFG)
#define GSP_AHB_CLOCK_26M_BIT   (0)/*[1:0] is used by GSP, 0:26M*/
#define GSP_AHB_CLOCK_192M_BIT  (3)/*[1:0] is used by GSP, 0:26M 1:76M 2:128M 3:192M*/

//interrupt relative
#ifndef CONFIG_OF
#define TB_GSP_INT 			(IRQ_GSP_INT)  //gsp hardware irq number
#define GSP_IRQ_BIT			SCI_INTC_IRQ_BIT(TB_GSP_INT) //gsp hardware irq bit, == (TB_GSP_INT % 32)
#endif

#define GSP_SOFT_RST_BIT    (BIT_GSP_SOFT_RST) //gsp chip module soft reset bit
#define GSP_MOD_EN_BIT      (BIT_GSP_EB) //gsp chip module enable bit

#endif

#ifndef CONFIG_OF
#define addr        (SPRD_GSP_BASE)
#endif

#define GSP_HOR_COEF_BASE(addr)   (addr + 0x90)
#define GSP_VER_COEF_BASE(addr)   (addr + 0x110)
#define GSP_L1_BASE(addr)		(addr + 0x60)


#ifdef CONFIG_OF
#define GSP_CLOCK_PARENT3		("clk_gsp_parent")
#else
#define GSP_CLOCK_PARENT3		("clk_256m")
#define GSP_CLOCK_PARENT2		("clk_192m")
#define GSP_CLOCK_PARENT1		("clk_153m6")
#define GSP_CLOCK_PARENT0		("clk_96m")
#endif

#ifdef CONFIG_ARCH_SCX35LT8
#define GSP_CLOCK_NAME			("clk_gsp0")
#else
#define GSP_CLOCK_NAME			("clk_gsp")
#endif

#define GSP_EMC_CLOCK_PARENT_NAME		("clk_aon_apb")
#define GSP_EMC_CLOCK_NAME				("clk_gsp_emc")


#ifdef CONFIG_FPGA
#define GSP_EMC_MATRIX_ENABLE() (*(volatile uint32_t*)(GSP_EMC_MATRIX_BASE) |= GSP_EMC_MATRIX_BIT)
#define GSP_CLOCK_SET(sel)\
{\
	*(volatile uint32_t*)(GSP_CLOCK_BASE) &= ~3;\
	*(volatile uint32_t*)(GSP_CLOCK_BASE) |= (sel);\
}
#define GSP_AUTO_GATE_ENABLE()  (*(volatile uint32_t*)(GSP_AUTO_GATE_ENABLE_BASE) |= GSP_AUTO_GATE_ENABLE_BIT)
#define GSP_AHB_CLOCK_SET(sel)\
{\
	*(volatile uint32_t*)(GSP_AHB_CLOCK_BASE) &= ~3;\
	*(volatile uint32_t*)(GSP_AHB_CLOCK_BASE) |= (sel);\
}
#define GSP_AHB_CLOCK_GET(sel)  (*(volatile uint32_t*)(GSP_AHB_CLOCK_BASE)&0x3)


//0x402B001C multi-media force shutdown [25]
//0x402E0000 MM enable
#define GSP_ENABLE_MM(addr)\
{\
	*(volatile unsigned long *)(SPRD_PMU_BASE+0x1c) &= ~(1<<25);\
	*(volatile unsigned long *)(SPRD_AONAPB_BASE) |= (1<<25);\
}

#define GSP_HWMODULE_SOFTRESET()\
	*(volatile uint32_t *)(GSP_SOFT_RESET) |= GSP_SOFT_RST_BIT;\
udelay(10);\
*(volatile uint32_t *)(GSP_SOFT_RESET) &= (~GSP_SOFT_RST_BIT)

#define GSP_HWMODULE_ENABLE()\
	*(volatile uint32_t *)(GSP_MOD_EN) |= (GSP_MOD_EN_BIT)

#define GSP_HWMODULE_DISABLE(bit)\
	*(volatile uint32_t *)(GSP_MOD_EN) &= (~(GSP_MOD_EN_BIT))

#else

#define GSP_REG_READ(reg)  (*(volatile uint32_t*)(reg))
#define GSP_REG_WRITE(reg,value)	(*(volatile uint32_t*)reg = value)

#define GSP_EMC_MATRIX_ENABLE()     sci_glb_set(GSP_EMC_MATRIX_BASE, GSP_EMC_MATRIX_BIT)
#define GSP_CLOCK_SET(sel)          sci_glb_write(GSP_CLOCK_BASE, (sel), 0x3)
#define GSP_AUTO_GATE_ENABLE()      sci_glb_set(GSP_AUTO_GATE_ENABLE_BASE, GSP_AUTO_GATE_ENABLE_BIT)
#define GSP_AUTO_GATE_DISABLE()     sci_glb_clr(GSP_AUTO_GATE_ENABLE_BASE, GSP_AUTO_GATE_ENABLE_BIT)
#define GSP_FORCE_GATE_ENABLE()      sci_glb_set(GSP_AUTO_GATE_ENABLE_BASE, GSP_CKG_FORCE_ENABLE_BIT)
#define GSP_AHB_CLOCK_SET(sel)      sci_glb_write(GSP_AHB_CLOCK_BASE, (sel), 0x3)
#define GSP_AHB_CLOCK_GET(addr)      	sci_glb_read(GSP_AHB_CLOCK_BASE,0x3)

//#define sci_get_chip_id()			GSP_REG_READ(SPRD_AONAPB_BASE+0xFC)



//0x402B001C multi-media force shutdown [25]
//0x402E0000 MM enable
#define GSP_ENABLE_MM(addr)\
{\
	sci_glb_clr((REG_PMU_APB_PD_MM_TOP_CFG),(BIT_PD_MM_TOP_FORCE_SHUTDOWN));\
	sci_glb_set(REG_AON_APB_APB_EB0,(BIT_PD_MM_TOP_FORCE_SHUTDOWN));\
}


#ifndef CONFIG_OF
#define GSP_MMU_CTRL_BASE (SPRD_GSPMMU_BASE+0x8000)
#endif

#define GSP_HWMODULE_SOFTRESET(reg_addr)\
{\
	sci_glb_set(GSP_SOFT_RESET,GSP_SOFT_RST_BIT);\
	udelay(10);\
	sci_glb_clr(GSP_SOFT_RESET,GSP_SOFT_RST_BIT);\
	GSP_REG_WRITE(reg_addr, 0x10000001);\
}
#define GSP_HWMODULE_ENABLE()			sci_glb_set(GSP_MOD_EN,GSP_MOD_EN_BIT)
#define GSP_HWMODULE_DISABLE()		sci_glb_clr(GSP_MOD_EN,GSP_MOD_EN_BIT)

#endif
#define GSP_EMC_GAP_SET(interval, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.dist_rb = (interval)

#ifndef CONFIG_64BIT /*arch 32bit*/
#define GSP_L0_ADDR_SET(value, addr)\
	*(volatile struct gsp_data_addr*)(&((GSP_REG_T*)(addr))->gsp_layer0_y_addr_u) = (value)
#define GSP_L1_ADDR_SET(value, addr)\
	*(volatile struct gsp_data_addr*)(&((GSP_REG_T*)(addr))->gsp_layer1_y_addr_u) = (value)
#define GSP_Ld_ADDR_SET(value, addr)\
	*(volatile struct gsp_data_addr*)(&((GSP_REG_T*)(addr))->gsp_des_y_addr_u) = (value)

#define GSP_L0_CLIPRECT_SET(rect, addr)\
	*(volatile GSP_RECT_T*)(&((GSP_REG_T*)(addr))->gsp_layer0_clip_start_u) = (rect)
#define GSP_L1_CLIPRECT_SET(rect, addr)\
	*(volatile GSP_RECT_T*)(&((GSP_REG_T*)(addr))->gsp_layer1_clip_start_u) = (rect)

#else /*arch 64bit*/

#define GSP_L0_ADDR_SET(value, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_y_addr_u.dwValue = (uint32_t)(value.addr_y);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_uv_addr_u.dwValue = (uint32_t)(value.addr_uv);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_va_addr_u.dwValue = (uint32_t)(value.addr_v)

#define GSP_L1_ADDR_SET(value, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_y_addr_u.dwValue = (uint32_t)(value.addr_y);\
((volatile GSP_REG_T*)(addr))->gsp_layer1_uv_addr_u.dwValue = (uint32_t)(value.addr_uv);\
((volatile GSP_REG_T*)(addr))->gsp_layer1_va_addr_u.dwValue = (uint32_t)(value.addr_v)

#define GSP_Ld_ADDR_SET(value, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_des_y_addr_u.dwValue = (uint32_t)(value.addr_y);\
((volatile GSP_REG_T*)(addr))->gsp_des_uv_addr_u.dwValue = (uint32_t)(value.addr_uv);\
((volatile GSP_REG_T*)(addr))->gsp_des_v_addr_u.dwValue = (uint32_t)(value.addr_v)

#define GSP_L0_CLIPRECT_SET(rect, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_clip_start_u.mBits.clip_start_x_l0 = (rect.st_x);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_clip_start_u.mBits.clip_start_y_l0 = (rect.st_y);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_clip_size_u.mBits.clip_size_x_l0 = (rect.rect_w);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_clip_size_u.mBits.clip_size_y_l0 = (rect.rect_h)
#define GSP_L1_CLIPRECT_SET(rect, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_clip_start_u.mBits.clip_start_x_l1 = (rect.st_x);\
((volatile GSP_REG_T*)(addr))->gsp_layer1_clip_start_u.mBits.clip_start_y_l1 = (rect.st_y);\
((volatile GSP_REG_T*)(addr))->gsp_layer1_clip_size_u.mBits.clip_size_x_l1 = (rect.rect_w);\
((volatile GSP_REG_T*)(addr))->gsp_layer1_clip_size_u.mBits.clip_size_y_l1 = (rect.rect_h)
#endif

#define GSP_L0_PITCH_GET(addr)  (*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_pitch_u))
#define GSP_L0_PITCH_SET(pitch, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_pitch_u) = (pitch)
#define GSP_L1_PITCH_SET(pitch, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_pitch_u) = (pitch)
#define GSP_Ld_PITCH_SET(pitch, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_des_pitch_u) = (pitch)


#define GSP_L0_ADDRY_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer0_y_addr_u.dwValue)
#define GSP_L0_ADDRUV_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer0_uv_addr_u.dwValue)
#define GSP_L0_ADDRVA_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer0_va_addr_u.dwValue)
#define GSP_L1_ADDRY_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer1_y_addr_u.dwValue)
#define GSP_L1_ADDRUV_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer1_uv_addr_u.dwValue)
#define GSP_L1_ADDRVA_GET(addr)    (((GSP_REG_T*)(addr))->gsp_layer1_va_addr_u.dwValue)
#define GSP_Ld_ADDRY_GET(addr)    (((GSP_REG_T*)(addr))->gsp_des_y_addr_u.dwValue)
#define GSP_Ld_ADDRUV_GET(addr)    (((GSP_REG_T*)(addr))->gsp_des_uv_addr_u.dwValue)
#define GSP_Ld_ADDRVA_GET(addr)    (((GSP_REG_T*)(addr))->gsp_des_v_addr_u.dwValue)


#define GSP_L0_DESRECT_SET(rect, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_des_start_u) = *((uint32_t*)&(rect).st_x);\
*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_des_size_u) = *((uint32_t*)&(rect).rect_w)
#define GSP_L1_DESPOS_SET(pos, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_des_start_u) = *(uint32_t*)(&(pos))


#define GSP_L0_GREY_SET(grey, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_grey_rgb_u) = *(uint32_t*)(&(grey))
#define GSP_L1_GREY_SET(grey, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_grey_rgb_u) = *(uint32_t*)(&(grey))


#define GSP_L0_ENDIAN_SET(endian_mode, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_endian_u) = \
(((endian_mode).y_word_endn & 0x03) << 0)\
|(((endian_mode).y_lng_wrd_endn & 0x01) << 2)\
|(((endian_mode).uv_word_endn & 0x03) << 3)\
|(((endian_mode).uv_lng_wrd_endn & 0x01) << 5)\
|(((endian_mode).va_word_endn & 0x03) << 6)\
|(((endian_mode).va_lng_wrd_endn & 0x01) << 8)\
|(((endian_mode).rgb_swap_mode & 0x07) << 9)\
|(((endian_mode).a_swap_mode & 0x01) << 12)
#define GSP_L1_ENDIAN_SET(endian_mode, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_endian_u) = \
(((endian_mode).y_word_endn & 0x03) << 0)\
|(((endian_mode).y_lng_wrd_endn & 0x01) << 2)\
|(((endian_mode).uv_word_endn & 0x03) << 3)\
|(((endian_mode).uv_lng_wrd_endn & 0x01) << 5)\
|(((endian_mode).va_word_endn & 0x03) << 6)\
|(((endian_mode).va_lng_wrd_endn & 0x01) << 8)\
|(((endian_mode).rgb_swap_mode & 0x07) << 9)\
|(((endian_mode).a_swap_mode & 0x01) << 12)
#define GSP_Ld_ENDIAN_SET(endian_mode, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_des_data_endian_u) = \
(((endian_mode).y_word_endn & 0x03) << 0)\
|(((endian_mode).y_lng_wrd_endn & 0x01) << 2)\
|(((endian_mode).uv_word_endn & 0x03) << 3)\
|(((endian_mode).uv_lng_wrd_endn & 0x01) << 5)\
|(((endian_mode).va_word_endn & 0x03) << 6)\
|(((endian_mode).va_lng_wrd_endn & 0x01) << 8)\
|(((endian_mode).rgb_swap_mode & 0x07) << 9)\
|(((endian_mode).a_swap_mode & 0x01) << 12)

#define GSP_L0_ALPHA_SET(alpha, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_alpha_u) = (alpha)
#define GSP_L1_ALPHA_SET(alpha, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_alpha_u) = (alpha)

#define GSP_L0_COLORKEY_SET(colorkey, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer0_ck_u) = *(uint32_t*)(&(colorkey))
#define GSP_L1_COLORKEY_SET(colorkey, addr)\
	*(volatile uint32_t*)(&((GSP_REG_T*)(addr))->gsp_layer1_ck_u) = *(uint32_t*)(&(colorkey))

#define GSP_L0_IMGFORMAT_SET(format, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.img_format_l0 = (format)
#define GSP_L1_IMGFORMAT_SET(format, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_cfg_u.mBits.img_format_l1 = (format)
#define GSP_Ld_IMGFORMAT_SET(format, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_des_data_cfg_u.mBits.des_img_format = (format)
#define GSP_Ld_COMPRESSRGB888_SET(enable, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_des_data_cfg_u.mBits.compress_r8 = (enable)

#define GSP_L0_ROTMODE_SET(mode, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.rot_mod_l0 = (mode)
#define GSP_L1_ROTMODE_SET(mode, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_cfg_u.mBits.rot_mod_l1 = (mode)

#define GSP_L0_COLORKEYENABLE_SET(colorkey_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.ck_en_l0 = (colorkey_en)
#define GSP_L1_COLORKEYENABLE_SET(colorkey_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_cfg_u.mBits.ck_en_l1 = (colorkey_en)

#define GSP_L0_PALLETENABLE_SET(pallet_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.pallet_en_l0 = (pallet_en)
#define GSP_L1_PALLETENABLE_SET(pallet_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer1_cfg_u.mBits.pallet_en_l1 = (pallet_en)

#define GSP_L0_SCALETAPMODE_SET(row_tap, col_tap, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.row_tap_mod = (row_tap);\
((volatile GSP_REG_T*)(addr))->gsp_layer0_cfg_u.mBits.col_tap_mod = (col_tap)

#define GSP_IRQMODE_SET(mode, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_int_cfg_u.mBits.int_mod = (mode)
#define GSP_IRQENABLE_SET(int_enable, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_int_cfg_u.mBits.int_en = (int_enable)

//Level-interrupt clear signal, internal GSP HW module

#define GSP_IRQSTATUS_CLEAR(addr)\
	((volatile GSP_REG_T*)(addr))->gsp_int_cfg_u.mBits.int_clr = 1;\
udelay(10);\
((volatile GSP_REG_T*)(addr))->gsp_int_cfg_u.mBits.int_clr = 0

#define GSP_WORKSTATUS_GET(addr)   (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.gsp_busy)
#define GSP_ERRFLAG_GET(addr)   (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.error_flag)
#define GSP_ERRCODE_GET(addr)   (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.error_code)

#define GSP_DITHER_ENABLE_SET(dith_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.dither_en = (dith_en)
#define GSP_PMARGB_ENABLE_SET(pm_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.pmargb_en = (pm_en)
#define GSP_L0_PMARGBMODE_SET(mode, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.pmargb_mod0 = (mode)
#define GSP_L1_PMARGBMODE_SET(mode, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.pmargb_mod1 = (mode)
#define GSP_SCALE_ENABLE_SET(scal_en, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.scale_en = (scal_en)
#define GSP_SCALE_ENABLE_GET(addr)  (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.scale_en)

//v==0:allowed a burst stride 4K boarder; v==1:split a burst into 2 request , when sride 4K boarder.
//gsp_cfg_u.mBits.split 0:split  1:don't split
#define GSP_PAGES_BOARDER_SPLIT_SET(v, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.no_split = (!(v))

#define GSP_Y2R_OPT_SET(v, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.y2r_opt = (v)


#define GSP_PAGES_BOARDER_SPLIT_GET(addr)\
	(!((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.no_split)

#define GSP_SCALESTATUS_RESET(addr)\
	do {\
		((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.scale_status_clr = 1;\
		udelay(10);\
		((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.scale_status_clr = 0;\
	} while(0)

#define GSP_L0_ENABLE_SET(enable, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.l0_en = (enable)
#define GSP_L1_ENABLE_SET(enable, addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.l1_en = (enable)
#define GSP_ENGINE_TRIGGER(addr)\
	((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.gsp_run = 1

#define GSP_L0_ENABLE_GET(addr)     (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.l0_en)
#define GSP_L1_ENABLE_GET(addr)     (((volatile GSP_REG_T*)(addr))->gsp_cfg_u.mBits.l1_en)

#define GSP_CFG_L1_PARAM(param)\
	*(volatile GSP_LAYER1_REG_T *)GSP_L1_BASE = (param)
/**---------------------------------------------------------------------------*
 **                         Data Definition                              *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 **                         Function Propertype                                *
 **---------------------------------------------------------------------------*/

int gsp_enable(struct gsp_context *gsp_ctx);
void gsp_disable(struct gsp_context *ctx);
void gsp_config_layer(GSP_MODULE_ID_E layer_id, struct gsp_kcfg_info *kcfg,
		unsigned long gsp_reg_addr);
uint32_t gsp_trigger(unsigned long gsp_reg_addr, unsigned long mmu_reg_addr);
void gsp_wait_finish(void);
void gsp_module_enable(struct gsp_context *gsp_ctx);
void gsp_module_disable(struct gsp_context *gsp_ctx);
int gsp_clock_check_phase0(unsigned long gsp_reg_addr);
int gsp_clock_check_phase1(unsigned long gsp_reg_addr, unsigned long mmu_reg_addr);
int gsp_try_to_recover(struct gsp_context *ctx);


#endif
