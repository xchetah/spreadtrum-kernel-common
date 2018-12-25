/* drivers/video/sc8825/lcd_rm68172_ld_mipi.c
 *
 * Support for rm68172_ld mipi LCD device
 *
 * Copyright (C) 2010 Spreadtrum
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

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"
#include <soc/sprd/adc.h>

//#define LCD_Delay(ms)  uDelay(ms*1000)

#define MAX_DATA   64

typedef struct LCM_Init_Code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
}LCM_Init_Code;

typedef struct LCM_force_cmd_code_tag{
	unsigned int datatype;
	LCM_Init_Code real_cmd_code;
}LCM_Force_Cmd_Code;

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)
//#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)


static LCM_Init_Code init_data[] = {

	{LCM_SEND(8), {6, 0, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},// PAGE 2

	{LCM_SEND(5), {3, 0, 0xF6, 0x60, 0x40}},

	{LCM_SEND(4), {2, 0, 0xEB, 0x05}},

	{LCM_SEND(7), {5, 0, 0xFE, 0x01, 0x80, 0x09, 0x09}},

	{LCM_SEND(8), {6, 0, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01}},// PAGE 1

	{LCM_SEND(6), {4, 0, 0xB0, 0x0A, 0x0A, 0x0A}},

	{LCM_SEND(6), {4, 0, 0xB1, 0x0A, 0x0A, 0x0A}},

	{LCM_SEND(6), {4, 0, 0xB5, 0x06, 0x06, 0x06}},

	{LCM_SEND(6), {4, 0, 0xB6, 0x34, 0x34, 0x34}},

	{LCM_SEND(6), {4, 0, 0xB7, 0x34, 0x34, 0x34}},

	{LCM_SEND(6), {4, 0, 0xB8, 0x24, 0x24, 0x24}},

	{LCM_SEND(6), {4, 0, 0xB9, 0x34, 0x34, 0x34}},

	{LCM_SEND(6), {4, 0, 0xBA, 0x14, 0x14, 0x14}},

	{LCM_SEND(6), {4, 0, 0xBC, 0x00, 0x78, 0x00}},//13

	{LCM_SEND(6), {4, 0, 0xBD, 0x00, 0x78, 0x00}},//13

	{LCM_SEND(5), {3, 0, 0xBE, 0x00, 0x8E}},// 88 VCOM FLICKER 5f

	{LCM_SEND(55), {53, 0, 0xD1, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	{LCM_SEND(55), {53, 0, 0xD2, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	{LCM_SEND(55), {53, 0, 0xD3, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	{LCM_SEND(55), {53, 0, 0xD4, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	{LCM_SEND(55), {53, 0, 0xD5, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	{LCM_SEND(55), {53, 0, 0xD6, 0x00, 0x00, 0x00, 0x06, 0x00, 0x26, 0x00, 0x4C, 0x00, 0x6C, 0x00, 0xA1, 0x00, 0xCA, 0x01, 0x06, 0x01, 0x32, 0x01, 0x70, 0x01, 0x9D, 0x01, 0xDD, 0x02, 0x0D, 0x02, 0x0F, 0x02, 0x3B, 0x02, 0x66, 0x02, 0x7F, 0x02, 0x9D, 0x02, 0xB0, 0x02, 0xC7, 0x02, 0xD6, 0x02, 0xEB, 0x02, 0xFA, 0x03, 0x10, 0x03, 0x44, 0x03, 0xFF}},

	//{LCM_SEND(55), {53, 0, 0xD6, 0x00, 0x00, 0x00, 0x10, 0x00, 0x2D, 0x00, 0x45, 0x00, 0x5B, 0x00, 0x80, 0x00, 0x9F, 0x00, 0xD3, 0x00, 0xFE, 0x01, 0x43, 0x01, 0x7B, 0x01, 0xD4, 0x02, 0x1F, 0x02, 0x21, 0x02, 0x66, 0x02, 0xB5, 0x02, 0xE9, 0x03, 0x2F, 0x03, 0x5E, 0x03, 0x98, 0x03, 0xB8, 0x03, 0xD9, 0x03, 0xE7, 0x03, 0xF2, 0x03, 0xF6, 0x03, 0xFF}},
	
	{LCM_SEND(8), {6, 0, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03}},// PAGE 3

	{LCM_SEND(10), {8, 0, 0xB0, 0x04, 0x15, 0xF6, 0xF5, 0x2C, 0x32, 0x70}},

	{LCM_SEND(12), {10, 0, 0xB2, 0xFA, 0xFB, 0xFC, 0xFD, 0xF0, 0x2C, 0x00, 0xC4, 0x08}},

	{LCM_SEND(9), {7, 0, 0xB3, 0x5B, 0x00, 0xFA, 0x29, 0x2A, 0x00}},

	{LCM_SEND(14), {12, 0, 0xB4, 0xFE, 0xFF, 0x00, 0x01, 0xF0, 0x40, 0x04, 0x08, 0x2C, 0x00, 0x00}},

	{LCM_SEND(14), {12, 0, 0xB5, 0x40, 0x00, 0xFE, 0x83, 0x2D, 0x2C, 0x27, 0x28, 0x33, 0x33, 0x00}},

	{LCM_SEND(10), {8, 0, 0xB6, 0xBC, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00}},

	{LCM_SEND(11), {9, 0, 0xB7, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00}},

	{LCM_SEND(6), {4, 0, 0xB8, 0x00, 0x00, 0x00}},

	{LCM_SEND(4), {2, 0, 0xB9, 0x90}},

	{LCM_SEND(19), {17, 0, 0xBA, 0x54, 0x01, 0xB9, 0xFD, 0x13, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0xCE, 0x8A, 0x10, 0x45}},

	{LCM_SEND(19), {17, 0, 0xBB, 0x45, 0x01, 0xCE, 0x8A, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x13, 0xB9, 0xFD, 0x10, 0x54}},

	{LCM_SEND(7), {5, 0, 0xBC, 0xF0, 0x3F, 0xFC, 0x0F}},

	{LCM_SEND(7), {5, 0, 0xBD, 0xF0, 0x3F, 0xFC, 0x0F}},

	{LCM_SEND(8), {6, 0, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},// PAGE 0

	{LCM_SEND(6), {4, 0, 0xB0, 0x00, 0x10, 0x10}},

	{LCM_SEND(4), {2, 0, 0xB1, 0xFC}},

	{LCM_SEND(4), {2, 0, 0xBA, 0x01}},

//	{LCM_SEND(4), {2, 0, 0xB4, 0x10}},

	{LCM_SEND(6), {4, 0, 0xBC, 0x02, 0x02, 0x02}},

	{LCM_SEND(7), {5, 0, 0xB8, 0x01, 0x03, 0x03, 0x03}},

	{LCM_SEND(2), {0x35,0x00}},

	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1), {0x29}},                  // Display On
	{LCM_SLEEP(20)}
};


static LCM_Init_Code sleep_in[] =  {
{LCM_SEND(1), {0x28}},
{LCM_SLEEP(10)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(120)},
{LCM_SEND(2), {0x4f, 0x01}},
{LCM_SLEEP(100)}
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)}
};

static LCM_Force_Cmd_Code rd_prep_code[]={
	{0x39, {LCM_SEND(8), {0x6, 0, 0xF0, 0x55, 0xaa, 0x52, 0x08, 0x01}}},
	{0x37, {LCM_SEND(2), {0x2, 0}}}
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}}
};

static int32_t rm68172_ld_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	pr_debug(KERN_DEBUG "rm68172_ld_mipi_init\n");

	mipi_set_cmd_mode();

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			msleep((init->tag & LCM_TAG_MASK));
		}
		init++;
	}
	return 0;
}

static int32_t rm68172_ld_get_adc_id(void)
{
#define SAMPLE_COUNT	16
	int32_t adc_value;
	int32_t ret, i, j, temp;
	int adc_val[SAMPLE_COUNT];
	struct adc_sample_data data = {
		.channel_id = 3,
		.channel_type = 0,	/*sw */
		.hw_channel_delay = 0,	/*reserved */
		.scale = 1,	/*small scale */
		.pbuf = &adc_val[0],
		.sample_num = SAMPLE_COUNT,
		.sample_bits = 1,
		.sample_speed = 0,	/*quick mode */
		.signal_mode = 0,	/*resistance path */
	};

	ret = sci_adc_get_values(&data);
	WARN_ON(0 != ret);
	for (j = 1; j <= SAMPLE_COUNT - 1; j++)
	{
		for (i = 0; i < SAMPLE_COUNT - j; i++)
		{
			if (adc_val[i] > adc_val[i + 1])
			{
				temp = adc_val[i];
				adc_val[i] = adc_val[i + 1];
				adc_val[i + 1] = temp;
			}
		}
	}

	adc_value = adc_val[SAMPLE_COUNT / 2];

	printk("__rm68172_ld_get_adc_id()__: adc_value = %d\r\n", adc_value);

	if((adc_value>=200) && (adc_value<1200))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static uint32_t rm68172_ld_readid(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;

	printk("lcd_rm68172_ld_mipi read id!\n");

#if 0
	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);
//	mipi_set_lp_mode();
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0xc5, 2, (uint8_t *)&read_data[0]);
		printk("lcd_rm68172_ld_mipi read id 0xc5 value is 0x%x, 0x%x!\n", read_data[0], read_data[1]);

		if((0x72==read_data[0]) && (0x81==read_data[1]))
		{
			printk("lcd_rm68172_ld_mipi read id success!\n");
			break;
		}
	}
//	mipi_set_hs_mode();
	mipi_eotp_set(1,1);
#endif

	if(rm68172_ld_get_adc_id())
	{
		printk("lcd_rm68172_ld_mipi read adc id sucess!\n");
		return 0x8172;
	}
	else
	{
		printk("lcd_rm68172_ld_mipi read adc id fail!\n");
		return 0;
	}

	return 0;
}

static int32_t rm68172_ld_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	printk("rm68172_ld_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep)
	{
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);

		mipi_set_cmd_mode();

		for(i = 0; i < size; i++)
		{
			tag = (sleep_in_out->tag >>24);
			if(tag & LCM_TAG_SEND)
			{
				mipi_gen_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
				udelay(20);
			}
			else if(tag & LCM_TAG_SLEEP)
			{
				msleep((sleep_in_out->tag & LCM_TAG_MASK));
			}
			sleep_in_out++;
		}

//		self->ops->panel_reset(self);
	}
	else
	{
		//sleep_in_out = sleep_out;
		//size = ARRAY_SIZE(sleep_out);
		printk("rm68172_ld_mipi_init out(20141218)\n");
		self->ops->panel_reset(self);
		rm68172_ld_mipi_init(self);
	}

	return 0;
}

static uint32_t rm68172_ld_readpowermode(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code_1;
	uint8_t read_data[1] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;

	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug("lcd_rm68172_ld_mipi read power mode!\n");
	mipi_eotp_set(0,0);
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code_1;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code_1); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
		//printk("lcd_rm68172_ld mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
		if((0x9c == read_data[0])  && (0 == read_rtn)){
			pr_debug("lcd_rm68172_ld_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	printk("lcd_rm68172_ld mipi read power mode fail!0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t rm68172_ld_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	return 1;

	pr_debug("rm68172_ld_check_esd!\n");
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
#endif
	power_mode = rm68172_ld_readpowermode(self);
	//power_mode = 0x0;
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
#endif
	if(power_mode == 0x9c){
		pr_debug("rm68172_ld_check_esd OK!\n");
		return 1;
	}else{
		printk("rm68172_ld_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}


static int32_t rm68172_after_suspend(struct panel_spec *self)
{
	printk("%s\r\n", __func__);
	return 0;
}

static struct panel_operations lcd_rm68172_ld_mipi_operations = {
	.panel_init = rm68172_ld_mipi_init,
	.panel_readid = rm68172_ld_readid,
	.panel_enter_sleep = rm68172_ld_enter_sleep,
	.panel_esd_check = rm68172_ld_check_esd,
	.panel_after_suspend = rm68172_after_suspend,
};

static struct timing_rgb lcd_rm68172_ld_mipi_timing = {
	.hfp = 24,  /* unit: pixel */
	.hbp = 16,
	.hsync = 8,
	.vfp = 16, /*unit: line*/
	.vbp = 14,
	.vsync =2,
};

static struct info_mipi lcd_rm68172_ld_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 2,
	.phy_feq = 461200,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_rm68172_ld_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_rm68172_ld_mipi_spec = {
	.width = 480,
	.height = 800,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_rm68172_ld_mipi_info
	},
	.ops = &lcd_rm68172_ld_mipi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};

struct panel_cfg lcd_rm68172_ld_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x8172,
	.lcd_name = "lcd_rm68172_ld_mipi",
	.panel = &lcd_rm68172_ld_mipi_spec,
};

static int __init lcd_rm68172_ld_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_rm68172_ld_mipi);
}

subsys_initcall(lcd_rm68172_ld_mipi_init);
