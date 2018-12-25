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

#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"

static int32_t isp_k_contrast_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_contrast_info contrast_info;

	memset(&contrast_info, 0x00, sizeof(contrast_info));

	ret = copy_from_user((void *)&contrast_info, param->property_param, sizeof(contrast_info));
	if (0 != ret) {
		printk("isp_k_contrast_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_CONTRAST_PARAM, 0x1FE, (contrast_info.factor << 1));

	if (contrast_info.bypass) {
		REG_OWR(ISP_CONTRAST_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CONTRAST_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_contrast(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_contrast: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_contrast: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CONTRAST_BLOCK:
		ret = isp_k_contrast_block(param);
		break;
	default:
		printk("isp_k_cfg_contrast: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
