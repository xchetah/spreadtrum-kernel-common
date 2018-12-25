/*
 * sound/soc/sprd/sprd-asoc-common.h
 *
 * SPRD ASoC Common include -- SpreadTrum ASOC Common.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
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
#ifndef __SPRD_ASOC_COMMON_H
#define __SPRD_ASOC_COMMON_H

#define STR_ON_OFF(on) (on ? "On" : "Off")

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define SP_AUDIO_DEBUG_BASIC	(1<<0)
#define SP_AUDIO_DEBUG_MORE	(1<<1)
#define SP_AUDIO_DEBUG_REG	(1<<2)
#define SP_AUDIO_DEBUG_DEFAULT (SP_AUDIO_DEBUG_BASIC)

#define SP_AUDIO_CODEC_NUM	(4)
#define AUDIO_CODEC_2713	(0)
#define AUDIO_CODEC_2723	(1)
#define AUDIO_CODEC_2723E	(1)
#define AUDIO_CODEC_2723T	(2)
#define AUDIO_CODEC_2723M	(2)

#define AUDIO_2723_VER_AA	(0)
#define AUDIO_2723_VER_S	(0x000)
#define AUDIO_2723_VER_E	(0x090)
#define AUDIO_2723_VER_T	(0x0C0)
#define AUDIO_2723_VER_M	(0x0C1)


static const char *codec_hw_info[] = {
	"2713", "2723", "2723E", "2723T"
};

struct snd_card;
int sprd_audio_debug_init(struct snd_card *card);
int get_sp_audio_debug_flag(void);
#define sp_audio_debug(mask) (get_sp_audio_debug_flag() & (mask))

#define sp_asoc_pr_info(fmt, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_BASIC)) {	\
		pr_info(fmt, ##__VA_ARGS__);		\
	}						\
} while(0)

#define sp_asoc_pr_reg(fmt, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_REG)) {	\
		pr_info(fmt, ##__VA_ARGS__);		\
	}						\
} while(0)

#ifdef CONFIG_SND_SOC_SPRD_AUDIO_DEBUG
#define sp_asoc_dev_dbg(dev, format, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_MORE)) {		\
		dev_dbg(dev, pr_fmt(format), ##__VA_ARGS__);	\
	}							\
} while(0)

#define sp_asoc_pr_dbg(fmt, ...) do {				\
	if (sp_audio_debug(SP_AUDIO_DEBUG_MORE)) {		\
		pr_debug(fmt, ##__VA_ARGS__);			\
	}							\
} while(0)
#if !defined(CONFIG_DYNAMIC_DEBUG)
#define DEBUG
#endif
#else /* !CONFIG_SND_SOC_SPRD_AUDIO_DEBUG */
#define sp_asoc_dev_dbg(...)
#define sp_asoc_pr_dbg(...)
#endif

#endif /* __SPRD_ASOC_COMMON_H */
