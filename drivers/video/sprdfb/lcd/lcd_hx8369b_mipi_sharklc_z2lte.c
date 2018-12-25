/* drivers/video/sprdfb/lcd_hx8369b_mipi.c
 *
 * Support for hx8369b mipi LCD device
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
#include <mach/gpio.h>

#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   150

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
	{LCM_SEND(6),{4,0,0xB9,0xFF,0x83,0x69}},
	{LCM_SEND(18),{16, 0, 0xBA ,0x31 ,0x00 ,0x16 ,0xCA ,0xB1 ,0x0A ,0x00, 0x10, 0x28, 0x02, 0x21, 0x21, 0x9A, 0x1A, 0x8F}},

	//set GIP timing control
	{LCM_SEND(95),{93, 0, 0xD5, 
		0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x12, 0x30, 0x00,\
		0x00, 0x00, 0x01, 0x4C, 0x34, 0x3D, 0x15, 0x09, 0x01, 0x48,\
		0x37, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x03, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x88, 0x99,\
		0xAA, 0xBB, 0x57, 0x13, 0x31, 0x00, 0x00, 0x99, 0x88, 0xAA,\
		0xBB, 0x20, 0x64, 0x02, 0x00, 0x00, 0x88, 0x99, 0xAA, 0xBB,\
		0x46, 0x02, 0x20, 0x00, 0x00, 0x99, 0x88, 0xAA, 0xBB, 0x31,\
		0x75, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x0F, 0xFF, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0x00,\
		0x01, 0x5A}},
	{LCM_SEND(2),{0x3A, 0x70}},
	{LCM_SEND(2),{0x36, 0x00}},
	{LCM_SEND(15),{13, 0x00, 0xB1, 0x12, 0x83, 0x77, 0x00, 0x10, 0x10, 0x1B, 0x1B, 0x0C, 0x1A, 0x20, 0xD6}},
	{LCM_SEND(7),{5, 0x00, 0xB3, 0x83, 0x00, 0x3A, 0x17}},
	{LCM_SEND(2),{0xB4, 0x00}},
	{LCM_SEND(5),{3, 0, 0xB6, 0xA6, 0xA6}},
	{LCM_SEND(2),{0xCC, 0x0C}},
	{LCM_SEND(9),{0x07, 0x00, 0xC0, 0x73, 0x50, 0x00, 0x20, 0xC4, 0x00}},
	{LCM_SEND(130),{128, 0x00,
		0xC1, 0x01, 0x00, 0x07, 0x0F, 0x17, 0x1F, 0x26, 0x2E, 0x35, 0x3C, 0x45, 0x4C, 0x55, 0x5C, 0x66,\
		0x6D, 0x77, 0x7F, 0x87, 0x8F, 0x97, 0x9F, 0xA7, 0xAF, 0xB7, 0xBF, 0xC7, 0xCF, 0xD7, 0xDF, 0xE7,\
		0xEF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00, 0x07, 0x0F, 0x17,\
		0x1F, 0x27, 0x2F, 0x37, 0x3F, 0x47, 0x4F, 0x57, 0x5F, 0x67, 0x6F, 0x77, 0x7F, 0x87, 0x8F, 0x97,\
		0x9F, 0xA7, 0xAF, 0xB7, 0xBF, 0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x0F, 0x17, 0x1F, 0x26, 0x2E, 0x35, 0x3C, 0x45,\
		0x4C, 0x55, 0x5C, 0x66, 0x6D, 0x77, 0x7F, 0x87, 0x8F, 0x97, 0x9F, 0xA7, 0xAF, 0xB7, 0xBF, 0xC7,\
		0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{LCM_SEND(7),{5, 0x00, 0xE3, 0x07, 0x07, 0x07, 0x07}},
	{LCM_SEND(2),{0xEA, 0x7A}},
	{LCM_SEND(6),{4, 0x00, 0xC6, 0x41, 0xFF, 0x7C}},

	//gamma setting

	{LCM_SEND(0x26),{0x24, 0x00, 0xE0, 0x00, 0x01, 0x04, 0x0C, 0x0C, 0x3F, 0x1A, 0x30, 0x00, 0x0D, 0x11, 0x16, 0x18, 0x16, 0x17, 0x12, 0x16, 0x00, 0x01, 0x04, 0x0B, 0x0B, 0x3F, 0x1A, 0x30, 0x00, 0x0D, 0x11, 0x16, 0x18, 0x16, 0x16, 0x12, 0x17, 0x01}},
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(180)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(100)},
};


static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
	{LCM_SEND(1), {0x28}},
	{LCM_SLEEP(100)},
	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(100)},
};

static LCM_Init_Code sleep_out[] =  {
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(180)},
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(100)},
};
static int32_t hx8369b_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel hx8369b_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
	mipi_eotp_set(0,0);

	return 0;
}

static uint32_t hx8369b_readid(struct panel_spec *self)
{
	int32_t j =0;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("kernel lcd_hx8369b_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x03;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0x04, 3, &read_data);

		if((0x06 == read_data[0]) && (0x11 == read_data[1]) && (0x38 == read_data[2]))
		{
			printk("lcd_hx8369b_mipi read id success!\n");
			return 0x8369;
		}
	}
	mipi_eotp_set(0,0);
	LCD_PRINT("lcd_hx8369b_mipi read id fail! 0xda,0xdb,0xdc is 0x%x,0x%x,0x%x!\n",read_data[0],read_data[1],read_data[2]);
	return 0;
}


static int32_t hx8369b_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel hx8369b_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);
	}else{
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i <size ; i++){
		tag = (sleep_in_out->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(sleep_in_out->tag & LCM_TAG_MASK);
		}
		sleep_in_out++;
	}
	mipi_eotp_set(0,0);

	return 0;
}

static struct panel_operations lcd_hx8369b_mipi_operations = {
	.panel_init = hx8369b_mipi_init,
	.panel_readid = hx8369b_readid,
	.panel_enter_sleep = hx8369b_enter_sleep,
};

static struct timing_rgb lcd_hx8369b_mipi_timing = {
	.hfp = 115,  /* unit: pixel */
	.hbp = 72,
	.hsync = 4,
	.vfp = 16, /*unit: line*/
	.vbp = 12,
	.vsync = 4,
};


static struct info_mipi lcd_hx8369b_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	2,
	.phy_feq =420*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_hx8369b_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_hx8369b_mipi_spec = {
	.width = 480,
	.height = 800,
	.fps	= 62,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_hx8369b_mipi_info
	},
	.ops = &lcd_hx8369b_mipi_operations,
};
struct panel_cfg lcd_hx8369b_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x8369,
	.lcd_name = "lcd_hx8369b_mipi",
	.panel = &lcd_hx8369b_mipi_spec,
};

//temp code
static int __init lcd_hx8369b_mipi_init(void)
{
/*
	printk("hx8369b_mipi_init: set lcd backlight.\n");

		 if (gpio_request(214, "LCD_BL")) {
			   printk("Failed ro request LCD_BL GPIO_%d \n",
					   214);
			   return -ENODEV;
		 }
		 gpio_direction_output(214, 1);
		 gpio_set_value(214, 1);
*/

	return sprdfb_panel_register(&lcd_hx8369b_mipi);
}

subsys_initcall(lcd_hx8369b_mipi_init);

