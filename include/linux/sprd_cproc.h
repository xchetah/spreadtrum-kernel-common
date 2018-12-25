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

#ifndef _SPRD_CPROC_H
#define _SPRD_CPROC_H
#define INVALID_REG (0xff)
enum {
	CPROC_CTRL_SHUT_DOWN = 0,
	CPROC_CTRL_DEEP_SLEEP = 1,
	CPROC_CTRL_RESET = 2,
	CPROC_CTRL_GET_STATUS = 3,
	CPROC_CTRL_IRAM_PW = 4,
	CPROC_CTRL_EXT0,
	CPROC_CTRL_EXT1,
	CPROC_CTRL_EXT2,
	CPROC_CTRL_EXT3,
	CPROC_CTRL_EXT4,
	CPROC_CTRL_EXT5,
	CPROC_CTRL_EXT6,
	CPROC_CTRL_NR,
};

#define CPROC_IRAM_DATA_NR 3

enum {
	CPROC_REGION_CP_MEM = 0,
	CPROC_REGION_IRAM_MEM = 1,
	CPROC_REGION_CTRL_REG = 1,
	CPROC_REGION_NR,
};

struct cproc_segments {
	char			*name;
	uint32_t		base;		/* segment addr */
	uint32_t		maxsz;		/* segment size */
};

#define MAX_CPROC_NODE_NAME_LEN	0x20
#define MAX_IRAM_DATA_NUM	0x40

struct load_node {
	char name[MAX_CPROC_NODE_NAME_LEN];
	uint32_t size;
};

struct cproc_ctrl {
	unsigned long iram_addr;
	uint32_t iram_size;
	uint32_t iram_data[MAX_IRAM_DATA_NUM];
	unsigned long ctrl_reg[CPROC_CTRL_NR];
	uint32_t ctrl_mask[CPROC_CTRL_NR];
};

struct cproc_init_data {
	char			*devname;
	unsigned long		base;		/* CP base addr */
	uint32_t		maxsz;		/* CP max size */
	int			(*start)(void *arg);
	int			(*stop)(void *arg);
	int			(*suspend)(void *arg);
	int			(*resume)(void *arg);

	struct cproc_ctrl	*ctrl;
	void * 		shmem;
	int			wdtirq;
	uint32_t		type;	/* bit0-15: ic type, bit16-31: modlue type*/
	uint32_t		segnr;
	struct cproc_segments	segs[];
};

#endif
