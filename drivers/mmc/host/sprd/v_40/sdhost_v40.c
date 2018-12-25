/*
 *  linux/drivers/mmc/host/sdhost.c - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2014-2014 Jason.Wu(Jishuang.Wu), All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_device.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>

#include "sdhost_v40.h"

#ifdef CONFIG_DEBUG_FS
#include "sdhost_debugfs.h"
#endif

#ifdef CONFIG_ARCH_SCX20

#include <soc/sprd/arch_lock.h>

#define  BIT_6			0x00000040
#define  BIT_7			0x00000080
#define  BIT_12			0x00001000
#define  BIT_16			0x00010000

#define  REG_PIN_SD0_D3                 ( 0x0224 )
#define  REG_PIN_CTRL4                  ( 0x0010 )
#define  REG_PIN_CTRL7                  ( 0x001c )

extern int pinmap_set(u32 offset, u32 value);
extern u32 pinmap_get(u32 offset);

static void mmc_pin_ctl_set(int powerup)
{
	u32 value = 0;

	value = pinmap_get(REG_PIN_CTRL7);

	if (!powerup) {
		value |= (1 << 16);
	} else {
		value &= ~(1 << 16);
        }
        pinmap_set(REG_PIN_CTRL7, value);
}

static void mmc_pin_set(int powerup)
{
	u32 value = 0;
	int i = 0;

        if (!powerup) {
		for (i = 0; i < 5; i++){
			value = pinmap_get(REG_PIN_SD0_D3 + i * 4);
			value = value | BIT_6;
			value = value & (~ (BIT_12 | BIT_7));
			pinmap_set(REG_PIN_SD0_D3 + i * 4, value);
		}
        } else {
		for (i = 0; i < 5; i++){
			value = pinmap_get(REG_PIN_SD0_D3 + i * 4);
			value = value | BIT_12 | BIT_7;
			value = value & (~BIT_6);
			pinmap_set(REG_PIN_SD0_D3 + i * 4, value);
		}
        }
}
#endif
//#include "linux/mmc/sdhost.h"

#define DRIVER_NAME "sdhost"

#define STATIC_FUNC

static const u8 tuning_blk_pattern_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const u8 tuning_blk_pattern_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

extern void mmc_power_cycle(struct mmc_host *host);

STATIC_FUNC void _resetIOS(struct sdhost_host *host)
{
	_sdhost_disableall_int(host->ioaddr);
	//---
	host->ios.clock = 0;
	host->ios.vdd = 0;
	//host->ios.bus_mode    = MMC_BUSMODE_OPENDRAIN;
	//host->ios.chip_select = MMC_CS_DONTCARE;
	host->ios.power_mode = MMC_POWER_OFF;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ios.signal_voltage = MMC_SIGNAL_VOLTAGE_330;
	//host->ios.drv_type    = MMC_SET_DRIVER_TYPE_B;
	//---
	_sdhost_reset(host->ioaddr, _RST_ALL);
	_sdhost_set_delay(host->ioaddr, host->writeDelay, host->readCmdDelay, host->readPosDelay, host->readNegDelay);
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM)
STATIC_FUNC int __local_pm_suspend(struct sdhost_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	_sdhost_disableall_int(host->ioaddr);
	_sdhost_AllClk_off(host->ioaddr);
	spin_unlock_irqrestore(&host->lock, flags);
	clk_disable_unprepare(host->clk);
	clk_disable_unprepare(host->clk_sdioenable);
/*
	unsigned int temp = 0;
	void __iomem *base_baseregs;
	base_baseregs =  ioremap(0x20210000, 0x100);
	temp = readl( base_baseregs);
	printk("+++++++sleep+++++++++%s++ahb register : 0x%x \n", host->deviceName, temp);
	iounmap(base_baseregs);
*/
	synchronize_irq(host->irq);

	return 0;
}

STATIC_FUNC int __local_pm_resume(struct sdhost_host *host)
{
	unsigned long flags;
	clk_prepare_enable(host->clk_sdioenable);
	clk_prepare_enable(host->clk);
	spin_lock_irqsave(&host->lock, flags);
	if (host->ios.clock) {
		_sdhost_SDClk_off(host->ioaddr);
		_sdhost_Clk_set_and_on(host->ioaddr);
		_sdhost_SDClk_on(host->ioaddr);
	}

	spin_unlock_irqrestore(&host->lock, flags);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
STATIC_FUNC void _pm_runtime_setting(struct platform_device *pdev, struct sdhost_host *host)
{
	printk("sdhost %s run PM init\n", host->deviceName);
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 100);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	return;
}

STATIC_FUNC int _runtime_get(struct sdhost_host *host)
{
	return pm_runtime_get_sync(host->mmc->parent);
}

STATIC_FUNC int _runtime_put(struct sdhost_host *host)
{
	pm_runtime_mark_last_busy(host->mmc->parent);
	return pm_runtime_put_autosuspend(host->mmc->parent);
}

STATIC_FUNC int _runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sdhost_host *host = platform_get_drvdata(pdev);

	return __local_pm_suspend(host);
}

STATIC_FUNC int _runtime_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sdhost_host *host = platform_get_drvdata(pdev);

	return __local_pm_resume(host);
}

STATIC_FUNC int _runtime_idle(struct device *dev)
{
	return 0;
}

#else
STATIC_FUNC void _pm_runtime_setting(struct platform_device *pdev, struct sdhost_host *host)
{
	return;
}

STATIC_FUNC int _runtime_get(struct sdhost_host *host)
{
	return 0;
}

STATIC_FUNC int _runtime_put(struct sdhost_host *host)
{
	return 0;
}

#endif

#ifdef CONFIG_PM
STATIC_FUNC int _pm_suspend(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sdhost_host *host = platform_get_drvdata(pdev);

	_runtime_get(host);

	host->mmc->pm_flags = host->mmc->pm_caps;
	err = mmc_suspend_host(host->mmc);
	if (err) {
		_runtime_put(host);
		printk("sdhost %s pm suspend fail %d\n", host->deviceName, err);
		return err;
	}
/*
		printk("sdhost %s:\n"
	       "sdhost clock = %d\n"
	       "sdhost vdd = %d\n"
	       "sdhost bus_mode = %d\n"
	       "sdhost chip_select = %d\n"
	       "sdhost power_mode = %d\n"
	       "sdhost bus_width = %d\n"
	       "sdhost timing = %d\n"
	       "sdhost signal_voltage = %d\n"
	       "sdhost drv_type = %d\n",
	       host->deviceName,
	       host->ios.clock,
	       host->ios.vdd,
	       host->ios.bus_mode,
	       host->ios.chip_select,
	       host->ios.power_mode,
	       host->ios.bus_width,
	       host->ios.timing,
	       host->ios.signal_voltage,
	       host->ios.drv_type);
*/
	return __local_pm_suspend(host);
}

STATIC_FUNC int _pm_resume(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct sdhost_host *host = platform_get_drvdata(pdev);
	struct mmc_ios ios;

	__local_pm_resume(host);

	ios = host->mmc->ios;
	_resetIOS(host);
	host->mmc->ops->set_ios(host->mmc, &ios);

/*		printk("sdhost %s:\n"
	       "sdhost clock = %d\n"
	       "sdhost vdd = %d\n"
	       "sdhost bus_mode = %d\n"
	       "sdhost chip_select = %d\n"
	       "sdhost power_mode = %d\n"
	       "sdhost bus_width = %d\n"
	       "sdhost timing = %d\n"
	       "sdhost signal_voltage = %d\n"
	       "sdhost drv_type = %d\n",
	       host->deviceName,
	       host->ios.clock,
	       host->ios.vdd,
	       host->ios.bus_mode,
	       host->ios.chip_select,
	       host->ios.power_mode,
	       host->ios.bus_width,
	       host->ios.timing,
	       host->ios.signal_voltage,
	       host->ios.drv_type);
*/
	err = mmc_resume_host(host->mmc);
	if (err) {
		printk("sdhost %s pm resume fail %d\n", host->deviceName, err);
		return err;
	}
	_runtime_put(host);
	return err;
}
#endif

STATIC_FUNC void __getRsp(struct sdhost_host *host)
{
	u32 i;
	if (host->cmd->flags & MMC_RSP_PRESENT) {
		if (host->cmd->flags & MMC_RSP_136) {
			/* CRC is stripped so we need to do some shifting. */
			for (i = 0; i < 4; i++) {
				host->cmd->resp[i] = _sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE + (3 - i) * 4) << 8;
				if (i != 3)
					host->cmd->resp[i] |= _sdhost_readb(host->ioaddr, SDHOST_32_RESPONSE + (3 - i) * 4 - 1);
			}
		} else {
			host->cmd->resp[0] = _sdhost_readl(host->ioaddr, SDHOST_32_RESPONSE);
		}
	}
}

STATIC_FUNC void _sendCmd(struct sdhost_host *host, struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	int sg_cnt;
	u32 flag = 0;
	u16 rspType = 0;
	int ifHasData = 0;
	int ifMulti = 0;
	int ifRd = 0;
	int ifDma = 0;
	uint16_t autoCmd = __ACMD_DIS;

//      printk("sdhost %s cmd %d, arg 0x%x, flag 0x%x\n", host->deviceName, cmd->opcode, cmd->arg, cmd->flags);
//      if (cmd->data) {
//            printk("sdhost %s blkSize %d, cnt %d\n", host->deviceName, cmd->data->blksz, cmd->data->blocks);
//      }

	_sdhost_disableall_int(host->ioaddr);

	//printk("CMD opcode : %d  \n", cmd->opcode);

	if (38 == cmd->opcode) {
		// if it is erase command , it's busy time will long, so we set long timeout value here.
		mod_timer(&host->timer, jiffies + 10 * HZ);
		_sdhost_writeb(host->ioaddr, __DATA_TIMEOUT_MAX_VAL, SDHOST_8_TIMEOUT);
	} else {
		mod_timer(&host->timer, jiffies + 5 * HZ);
		_sdhost_writeb(host->ioaddr, host->dataTimeOutVal, SDHOST_8_TIMEOUT);
	}

	host->cmd = cmd;
	if (data) {
		// set data param
		BUG_ON(data->blksz * data->blocks > 524288);
		BUG_ON(data->blksz > host->mmc->max_blk_size);
		BUG_ON(data->blocks > 65535);

		data->bytes_xfered = 0;

		ifHasData = 1;
		ifRd = (data->flags & MMC_DATA_READ);
		ifMulti = (mmc_op_multi(cmd->opcode) || data->blocks > 1);
		if (ifRd && !ifMulti) {
			flag = _DATA_FILTER_RD_SIGLE;
		} else if (ifRd && ifMulti) {
			flag = _DATA_FILTER_RD_MULTI;
		} else if (!ifRd && !ifMulti) {
			flag = _DATA_FILTER_WR_SIGLE;
		} else {
			flag = _DATA_FILTER_WR_MULT;
		}
		if (!host->autoCmdMode) {
			flag |= _INT_ERR_ACMD;
		}
		ifDma = 1;
		autoCmd = host->autoCmdMode;
//              _sdhost_set_trans_mode(host->ioaddr, ifMulti, ifRd, host->autoCmdMode, ifMulti, ifDma);
		_sdhost_set_blk_size(host->ioaddr, data->blksz);

		sg_cnt = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len, (data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		if (1 == sg_cnt) {
			_sdhost_set_DMA(host->ioaddr, __SDMA_MOD);
			_sdhost_set_32_blk_cnt(host->ioaddr, data->blocks);
			_sdhost_writel(host->ioaddr, ((uint64_t) (sg_dma_address(data->sg))) & 0xffffffff, SDHOST_DMA_ADDRESS_LOW);
			_sdhost_writel(host->ioaddr, ((uint64_t) (sg_dma_address(data->sg)) >> 32) & 0xffffffff, SDHOST_DMA_ADDRESS_HIGH);

		} else {
			//WARN_ON(1);
			BUG_ON(1);
			flag |= _INT_ERR_ADMA;
			_sdhost_set_DMA(host->ioaddr, __32ADMA_MOD);
			_sdhost_set_32_blk_cnt(host->ioaddr, data->blocks);
			_sdhost_writel(host->ioaddr, ((uint64_t) (sg_dma_address(data->sg))) & 0xffffffff, SDHOST_DMA_ADDRESS_LOW);
			_sdhost_writel(host->ioaddr, ((uint64_t) (sg_dma_address(data->sg)) >> 32) & 0xffffffff, SDHOST_DMA_ADDRESS_HIGH);
		}
	} else {
//              _sdhost_set_trans_mode(host->ioaddr, 0, 0, __ACMD_DIS, 0, 0);
	}
	_sdhost_writel(host->ioaddr, cmd->arg, SDHOST_32_ARG);
	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_R1B:
		rspType = _RSP1B_5B;
		flag |= _CMD_FILTER_R1B;
		break;
	case MMC_RSP_NONE:
		rspType = _RSP0;
		flag |= _CMD_FILTER_R0;
		break;
	case MMC_RSP_R2:
		rspType = _RSP2;
		flag |= _CMD_FILTER_R2;
		break;
//      case MMC_RSP_R3:
//              rspType = _RSP3_4;
//              flag |= _CMD_FILTER_R3;
//              break;
	case MMC_RSP_R4:
		rspType = _RSP3_4;
		flag |= _CMD_FILTER_R1_R4_R5_R6_R7;
		break;
	case MMC_RSP_R1:
//      case MMC_RSP_R5:
//      case MMC_RSP_R6:
//      case MMC_RSP_R7:
		rspType = _RSP1_5_6_7;
		flag |= _CMD_FILTER_R1_R4_R5_R6_R7;
		break;
	default:
		BUG_ON(1);
		break;
	}
	host->int_filter = flag;
	_sdhost_enable_int(host->ioaddr, flag);
//      printk("sdhost %s cmd:%d rsp:%d intflag:0x%x ifMulti:0x%x ifRd:0x%x autoCmd:0x%x ifDma:0x%x\n", host->deviceName, cmd->opcode, mmc_resp_type(cmd), flag,ifMulti,ifRd,autoCmd,ifDma);

//      _sdhost_set_cmd(host->ioaddr, cmd->opcode, ifHasData, rspType);
	_sdhost_set_trans_and_cmd(host->ioaddr, ifMulti, ifRd, autoCmd, ifMulti, ifDma, cmd->opcode, ifHasData, rspType);
}

STATIC_FUNC irqreturn_t _irq(int irq, void *param)
{
	u32 intmask;

	struct sdhost_host *host = (struct sdhost_host *)param;
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd;
	struct mmc_data *data;

	spin_lock(&host->lock);
	if ((!mrq) || (!cmd)) {	// bug 411922 : maybe _timeout run in one core and _irq run in another core, this will panic if access cmd->data. jason.wu
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}
	data = cmd->data;

	intmask = _sdhost_readl(host->ioaddr, SDHOST_32_INT_ST);
	if (!intmask) {
		spin_unlock(&host->lock);
		return IRQ_NONE;
	}
	//printk("sdhost %s int 0x%x\n", host->deviceName, intmask);

	// disable unused interrupt
	_sdhost_clear_int(host->ioaddr, intmask);
	// just care about the interrupt that we want
	intmask &= host->int_filter;
	while (intmask) {
		if (_INT_FILTER_ERR & intmask) {
			// some error happened in command
			if (_INT_FILTER_ERR_CMD & intmask) {
				if (_INT_ERR_CMD_TIMEOUT & intmask) {
					cmd->error = -ETIMEDOUT;
				} else {
					cmd->error = -EILSEQ;
				}
			}
			// some error happened in data token or command with R1B
			if (_INT_FILTER_ERR_DATA & intmask) {
				if (data) {
					// current error is happened in data token
					if (_INT_ERR_DATA_TIMEOUT & intmask) {
						data->error = -ETIMEDOUT;
					} else {
						data->error = -EILSEQ;
					}
				} else {
					// current error is happend in response with busy
					if (_INT_ERR_DATA_TIMEOUT & intmask) {
						cmd->error = -ETIMEDOUT;
					} else {
						cmd->error = -EILSEQ;
					}
				}
			}
			if (_INT_ERR_ACMD & intmask) {
				// Auto cmd12 and cmd23 error is belong to data token error
				data->error = -EILSEQ;
			}
			if (_INT_ERR_ADMA & intmask) {
				data->error = -EIO;
			}
			// for debug
			//printk("sdhost %s int 0x%x\n", host->deviceName, intmask);
			dumpSDIOReg(host);
			_sdhost_disableall_int(host->ioaddr);
			// if current error happened in data token, we send cmd12 to stop it.
			if ((mrq->cmd == cmd) && (mrq->stop)) {
				_sdhost_reset(host->ioaddr, _RST_CMD | _RST_DATA);
				_sendCmd(host, mrq->stop);
			} else {
				// request finish with error, so reset it and stop the request
				_sdhost_reset(host->ioaddr, _RST_CMD | _RST_DATA);
				tasklet_schedule(&host->finish_tasklet);
			}
			goto out;
		} else {
			// delete irq that wanted in filter
			//_sdhost_clear_int(host->ioaddr, _INT_FILTER_NORMAL & intmask);
			host->int_filter &= ~(_INT_FILTER_NORMAL & intmask);
			if (_INT_DMA_END & intmask) {
				_sdhost_writel(host->ioaddr, _sdhost_readl(host->ioaddr, SDHOST_DMA_ADDRESS_LOW), SDHOST_DMA_ADDRESS_LOW);
				_sdhost_writel(host->ioaddr, _sdhost_readl(host->ioaddr, SDHOST_DMA_ADDRESS_HIGH), SDHOST_DMA_ADDRESS_HIGH);
			}
			if (_INT_CMD_END & intmask) {
				cmd->error = 0;
				__getRsp(host);
			}
			if (_INT_TRAN_END & intmask) {
				if (data) {
					dma_unmap_sg(mmc_dev(host->mmc),
						     data->sg, data->sg_len, (data->flags & MMC_DATA_READ) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
					data->error = 0;
					data->bytes_xfered = data->blksz * data->blocks;
				} else {
					// R1B also can produce transferComplete interrupt
					cmd->error = 0;
				}
			}
			if (!(_INT_FILTER_NORMAL & host->int_filter)) {
				// current cmd finished
				_sdhost_disableall_int(host->ioaddr);
				_sdhost_reset(host->ioaddr, _RST_CMD | _RST_DATA);
				if (mrq->sbc == cmd) {
					_sendCmd(host, mrq->cmd);
				} else if ((mrq->cmd == host->cmd)
					   && (mrq->stop)) {
					_sendCmd(host, mrq->stop);
				} else {
					// finish with success and stop the request
					tasklet_schedule(&host->finish_tasklet);
					goto out;
				}
			}

		}

		intmask = _sdhost_readl(host->ioaddr, SDHOST_32_INT_ST);
		_sdhost_clear_int(host->ioaddr, intmask);
		intmask &= host->int_filter;
	};
out:
	spin_unlock(&host->lock);
	return IRQ_HANDLED;
}

STATIC_FUNC void _tasklet(unsigned long param)
{
	struct sdhost_host *host = (struct sdhost_host *)param;
	unsigned long flags;
	struct mmc_request *mrq;

	del_timer(&host->timer);

	spin_lock_irqsave(&host->lock, flags);
	if (!host->mrq) {
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}
	mrq = host->mrq;
	host->mrq = NULL;
	host->cmd = NULL;
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);

//      printk("sdhost %s cmd %d data %d\n", host->deviceName, mrq->cmd->error, ((! !mrq->cmd->data) ? mrq->cmd->data->error : 0));
	mmc_request_done(host->mmc, mrq);
	_runtime_put(host);
}

STATIC_FUNC void _timeout(unsigned long data)
{
	struct sdhost_host *host = (struct sdhost_host *)data;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	if (host->mrq) {
		printk("sdhost %s Timeout waiting for hardware interrupt!\n", host->deviceName);
		dumpSDIOReg(host);
		if (host->cmd->data) {
			host->cmd->data->error = -ETIMEDOUT;
		} else if (host->cmd) {
			host->cmd->error = -ETIMEDOUT;
		} else {
			host->mrq->cmd->error = -ETIMEDOUT;
		}
		_sdhost_disableall_int(host->ioaddr);
		_sdhost_reset(host->ioaddr, _RST_CMD | _RST_DATA);
		tasklet_schedule(&host->finish_tasklet);
	}
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	return;
}

STATIC_FUNC void sdhost_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;

	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);

	host->mrq = mrq;
	// 1 find whether card is still in slot
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
		if (!mmc_gpio_get_cd(host->mmc)) {
			mrq->cmd->error = -ENOMEDIUM;
			tasklet_schedule(&host->finish_tasklet);
			mmiowb();
			spin_unlock_irqrestore(&host->lock, flags);
			return;
		}
		// else asume sdcard is present
	}
	// in our control we can not use auto cmd12 and auto cmd23 together
	// so in following program we use auto cmd23 prior to auto cmd12
	//printk("sdhost %s request %d %d %d\n", host->deviceName, !!mrq->sbc,!!mrq->cmd,!!mrq->stop);
	host->autoCmdMode = __ACMD_DIS;
	if (!mrq->sbc && mrq->stop && SDHOST_FLAG_ENABLE_ACMD12) {
		host->autoCmdMode = __ACMD12;
		mrq->data->stop = NULL;
		mrq->stop = NULL;
	}
	// 3 send cmd list
	if ((mrq->sbc) && SDHOST_FLAG_ENABLE_ACMD23) {
		host->autoCmdMode = __ACMD23;
		mrq->data->stop = NULL;
		mrq->stop = NULL;
		_sendCmd(host, mrq->cmd);
	} else if (mrq->sbc) {
		mrq->data->stop = NULL;
		mrq->stop = NULL;
		_sendCmd(host, mrq->sbc);
	} else {
		_sendCmd(host, mrq->cmd);
	}

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	return;
}

STATIC_FUNC void _signalVoltageOnOff(struct sdhost_host *host, uint32_t onOff)
{
	if (!host->_1_8V_signal) {
		printk("sdhost %s there is no signal voltage!\n", host->deviceName);
		return;
	}
	if (onOff) {
		//if (!host->_1_8V_signal_enabled) {
		if (!regulator_is_enabled(host->_1_8V_signal)) {
			if (regulator_enable(host->_1_8V_signal)) {
				printk("sdhost %s signal voltage enable fail!\n", host->deviceName);
			} else if (regulator_is_enabled(host->_1_8V_signal)) {
				host->_1_8V_signal_enabled = true;
				printk("sdhost %s signal voltage enable success!\n", host->deviceName);
			} else {
				printk("sdhost %s signal voltage enable hw fail!\n", host->deviceName);
			}
		}
	} else {
		//if (host->_1_8V_signal_enabled) {
		if (regulator_is_enabled(host->_1_8V_signal)) {
			if (regulator_disable(host->_1_8V_signal)) {
				printk("sdhost %s signal voltage disable fail\n", host->deviceName);
			} else if (!regulator_is_enabled(host->_1_8V_signal)) {
				host->_1_8V_signal_enabled = false;
				printk("sdhost %s signal voltage disable success!\n", host->deviceName);
			} else {
				printk("sdhost %s signal voltage disable hw fail\n", host->deviceName);
			}
		}
	}
	return;
}

/*
	charpter:
	1 This votage is always poweron
	2 initial votage is 2.7v~3.6v
	3 It can be reconfig to 1.7v~1.95v
*/
STATIC_FUNC int sdhost_set_vqmmc(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;
	int err;
/*
	printk("*********************************sdhost %s vqmmc:\n"
	       "sdhost clock = %d-->%d\n"
	       "sdhost vdd = %d-->%d\n"
	       "sdhost bus_mode = %d-->%d\n"
	       "sdhost chip_select = %d-->%d\n"
	       "sdhost power_mode = %d-->%d\n"
	       "sdhost bus_width = %d-->%d\n"
	       "sdhost timing = %d-->%d\n"
	       "sdhost signal_voltage = %d-->%d\n"
	       "sdhost drv_type = %d-->%d\n",
	       host->deviceName,
	       host->ios.clock, ios->clock,
	       host->ios.vdd, ios->vdd,
	       host->ios.bus_mode, ios->bus_mode,
	       host->ios.chip_select, ios->chip_select,
	       host->ios.power_mode, ios->power_mode,
	       host->ios.bus_width, ios->bus_width,
	       host->ios.timing, ios->timing, host->ios.signal_voltage, ios->signal_voltage, host->ios.drv_type, ios->drv_type);
*/
	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);
	if (!host->_1_8V_signal) {
		// there are no 1.8v signal votage.
		spin_unlock_irqrestore(&host->lock, flags);
		_runtime_put(host);
		err = 0;
		printk("sdhost %s There is no signalling voltage\n", host->deviceName);
		return err;
	}
	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		//err = regulator_set_voltage(host->_1_8V_signal, 2700000, 3600000);
		err = regulator_set_voltage(host->_1_8V_signal, 3000000, 3000000);
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		//err = regulator_set_voltage(host->_1_8V_signal, 1700000, 1950000);
		err = regulator_set_voltage(host->_1_8V_signal, 1800000, 1800000);
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		err = regulator_set_voltage(host->_1_8V_signal, 1100000, 1300000);
		break;
	default:
		err = -EIO;
		BUG_ON(1);
		break;
	}
	if (likely(!err)) {
		host->ios.signal_voltage = ios->signal_voltage;
	}
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	_runtime_put(host);

	WARN(err, "Switching to signalling voltage  failed\n");
	return err;
}

STATIC_FUNC void _wait_mrq_done(struct mmc_request *mrq)
{
	complete(&mrq->completion);
}

STATIC_FUNC int _exec_tuning(struct mmc_host *host)
{
	struct mmc_request mrq = { NULL };
	struct mmc_command cmd = { 0 };
	struct mmc_data data = { 0 };
	struct scatterlist sg;
	struct mmc_ios *ios = &host->ios;
	const u8 *tuning_block_pattern;
	int size, err = 0;
	u8 *data_buf;
	u32 opcode;

	if (ios->bus_width == MMC_BUS_WIDTH_8) {
		tuning_block_pattern = tuning_blk_pattern_8bit;
		size = sizeof(tuning_blk_pattern_8bit);
		opcode = MMC_SEND_TUNING_BLOCK_HS200;
	} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
		tuning_block_pattern = tuning_blk_pattern_4bit;
		size = sizeof(tuning_blk_pattern_4bit);
		opcode = MMC_SEND_TUNING_BLOCK;
	} else
		return -EINVAL;

	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	/*
	 * According to the tuning specs, Tuning process
	 * is normally shorter 40 executions of CMD19,
	 * and timeout value should be shorter than 150 ms
	 */
	data.timeout_ns = 150 * NSEC_PER_MSEC;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, data_buf, size);

	init_completion(&(mrq.completion));
	mrq.done = _wait_mrq_done;

	mrq.cmd->error = 0;
	mrq.cmd->mrq = &mrq;

	if (mrq.data) {

		mrq.cmd->data = mrq.data;
		mrq.data->error = 0;
		mrq.data->mrq = &mrq;

		if (mrq.stop) {
			mrq.data->stop = mrq.stop;
			mrq.stop->error = 0;
			mrq.stop->mrq = &mrq;
		}
	}

	sdhost_request(host, &mrq);

	/* wait mrq finished */
	while (1) {

		wait_for_completion(&(mrq.completion));

		if (!mrq.cmd->error || !mrq.cmd->retries)
			break;

		mrq.cmd->retries--;
		mrq.cmd->error = 0;
		sdhost_request(host, &mrq);
	}

	if (cmd.error) {
		err = cmd.error;
		goto out;
	}

	if (data.error) {
		err = data.error;
		goto out;
	}

	if (memcmp(data_buf, tuning_block_pattern, size))
		err = -EIO;

out:
	kfree(data_buf);
	return err;
}

STATIC_FUNC int sdhost_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sdhost_host *host = mmc_priv(mmc);
	u32 try_phase = 0;
	u32 count = 0;
	u32 total = 0;

	_runtime_get(host);
	// tuning for DATA line
	do {
		_sdhost_writeb(host->ioaddr, try_phase, SDHOST_DLL_CLKRD_DLY);
		//printk("-----*EMMC data tuning dalay cycle : 0x%x \n ", _sdhost_readl(host->ioaddr, 0x84));
		if (!_exec_tuning(mmc)) {
			total = total + try_phase;
			count++;
			//printk("---**EMMC data line: Found  tuning phase %d  pass \n", try_phase);
		} else {
			//printk("---**EMMC data line: Found  tuning phase %d  fail \n", try_phase);
		}
	} while (++try_phase < 255);

	if (count > 0) {
		//tuning success
		try_phase = total / count;
		printk("*****tuning value : 0x%x \n", try_phase);
		_sdhost_writeb(host->ioaddr, try_phase, SDHOST_DLL_CLKRD_DLY);
		_runtime_put(host);
		return 0;
	} else {
		//tuning fail
		_runtime_put(host);
		return 1;
	}
}

STATIC_FUNC void _enable_Dpll(struct sdhost_host *host)
{
	u32 ulTemp = 0;

	//set bit18,bit21,bit24-27 as 0
	ulTemp = _sdhost_readl(host->ioaddr, SDHOST_DLL_CFG);
	ulTemp &= ~(DLL_EN | DLL_ALL_CPST_EN);
	_sdhost_writel(host->ioaddr, ulTemp, SDHOST_DLL_CFG);
	mdelay(10);

	//set bit18, bit24-27 as 1
	ulTemp = _sdhost_readl(host->ioaddr, SDHOST_DLL_CFG);
	ulTemp |= DLL_ALL_CPST_EN | DLL_SEARCH_MODE | DLL_INIT_COUNT | DLL_PHA_INTERNAL;
	_sdhost_writel(host->ioaddr, ulTemp, SDHOST_DLL_CFG);
	mdelay(10);

	//set bit21 as 1
	ulTemp = _sdhost_readl(host->ioaddr, SDHOST_DLL_CFG);
	ulTemp |= DLL_EN;
	_sdhost_writel(host->ioaddr, ulTemp, SDHOST_DLL_CFG);
	mdelay(10);

	//check DLL_LOCKED =1 and  DLL_ERROR = 0
	ulTemp = _sdhost_readl(host->ioaddr, SDHOST_DLL_STS0);
	while (!(((ulTemp & DLL_LOCKED) == DLL_LOCKED) && ((ulTemp & DLL_ERROR) != DLL_ERROR))) {
		//printk("------SDHOST_DLL_STS0 : 0x%x \n", ulTemp);
		ulTemp = _sdhost_readl(host->ioaddr, SDHOST_DLL_STS0);
	}
	//printk("------DPLL locked done \n");
}

STATIC_FUNC void _switch_AON_clksource(struct sdhost_host *host, struct mmc_ios *ios)
{
	unsigned int ulTemp = 0;

	ulTemp = (unsigned int)(host->base_clk / ios->clock);
	if ((host->base_clk % ios->clock) != 0) {
		ulTemp = ulTemp + 1;
	}

	if (ios->clock <= 400000) {
		clk_disable_unprepare(host->clk);
		clk_disable_unprepare(host->clk_sdioenable);

		clk_set_parent(host->clk, host->clk_init);
		clk_set_rate(host->clk, 200000);

		clk_prepare_enable(host->clk);
		clk_prepare_enable(host->clk_sdioenable);
	} else {
		clk_disable_unprepare(host->clk);
		clk_disable_unprepare(host->clk_sdioenable);

		clk_set_parent(host->clk, host->clk_runing);
		clk_set_rate(host->clk, (host->base_clk * 2) / ulTemp);

		clk_prepare_enable(host->clk);
		clk_prepare_enable(host->clk_sdioenable);

		if (ios->clock > 50000000) {
			_enable_Dpll(host);
		}

	}

	/*****************for debug freq_div******************/
	/*
	   unsigned int temp = 0;
	   void __iomem *AON_baseregs;
	   AON_baseregs =  ioremap(0x402d0200, 0x200);
	   temp = readl(AON_baseregs + 0x144);
	   printk("++++++++AON_CLK_EMMC : 0x%x \n",temp);
	   temp = readl(AON_baseregs + 0x128);
	   printk("++++++++AON_CLK_SD : 0x%x \n",temp);
	 */
}

STATIC_FUNC void sdhost_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;
/*
	printk("+++++++++++++++++++++++++++++++++++++sdhost %s ios:\n"
	       "sdhost clock = %d-->%d\n"
	       "sdhost vdd = %d-->%d\n"
	       "sdhost bus_mode = %d-->%d\n"
	       "sdhost chip_select = %d-->%d\n"
	       "sdhost power_mode = %d-->%d\n"
	       "sdhost bus_width = %d-->%d\n"
	       "sdhost timing = %d-->%d\n"
	       "sdhost signal_voltage = %d-->%d\n"
	       "sdhost drv_type = %d-->%d\n",
	       host->deviceName,
	       host->ios.clock, ios->clock,
	       host->ios.vdd, ios->vdd,
	       host->ios.bus_mode, ios->bus_mode,
	       host->ios.chip_select, ios->chip_select,
	       host->ios.power_mode, ios->power_mode,
	       host->ios.bus_width, ios->bus_width,
	       host->ios.timing, ios->timing, host->ios.signal_voltage, ios->signal_voltage, host->ios.drv_type, ios->drv_type);
*/
	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);

	do {
		if (0 == ios->clock) {
			_sdhost_AllClk_off(host->ioaddr);
			host->ios.clock = 0;
		} else if (ios->clock != host->ios.clock) {
			uint32_t div;
			div = _sdhost_calcDiv(host->base_clk, ios->clock);
			_sdhost_SDClk_off(host->ioaddr);
			/***********************************************/
			//for r6p0 r7p0 change AON clock source to change FREQ
			_switch_AON_clksource(host, ios);
			/*****************for debug freq_div******************/
			_sdhost_Clk_set_and_on(host->ioaddr);
			_sdhost_SDClk_on(host->ioaddr);
			host->ios.clock = ios->clock;
			host->dataTimeOutVal = _sdhost_calcTimeout(host->base_clk, div, 3);
		}
		if (ios->power_mode != host->ios.power_mode) {
			if (MMC_POWER_OFF == ios->power_mode) {
#ifdef CONFIG_ARCH_SCX20
                                if (!strcmp(host->deviceName, "sdio_sd")) {
					mmc_pin_set(0);
					mmc_pin_ctl_set(0);
                                }
#endif
				spin_unlock_irqrestore(&host->lock, flags);
				_signalVoltageOnOff(host, 0);
				if (host->SD_pwr) {
					//mmc_regulator_set_ocr(host->mmc, host->SD_pwr, 0);
					regulator_disable(host->SD_pwr);
				}
				spin_lock_irqsave(&host->lock, flags);
				_resetIOS(host);
				host->ios.power_mode = ios->power_mode;
			} else if ((MMC_POWER_ON == ios->power_mode)
				   || (MMC_POWER_UP == ios->power_mode)
			    ) {
				spin_unlock_irqrestore(&host->lock, flags);
				if (host->SD_pwr) {
					if (regulator_is_enabled(host->SD_pwr)) {
					} else {
						regulator_enable(host->SD_pwr);
						//mmc_regulator_set_ocr(host->mmc, host->SD_pwr, ios->vdd);
					}
				}
				_signalVoltageOnOff(host, 1);
				spin_lock_irqsave(&host->lock, flags);
#ifdef CONFIG_ARCH_SCX20
                                if (!strcmp(host->deviceName, "sdio_sd")) {
					mmc_pin_ctl_set(1);
					mmc_pin_set(1);
                                }
#endif
				host->ios.power_mode = ios->power_mode;
				host->ios.vdd = ios->vdd;
			} else {
				BUG_ON(1);
			}
		}
		// flash power voltage select
		if (ios->vdd != host->ios.vdd) {
			if (host->SD_pwr) {
				printk("sdhost %s 3.0 %d!\n", host->deviceName, ios->vdd);
				//mmc_regulator_set_ocr(host->mmc, host->SD_pwr, ios->vdd);
				spin_unlock_irqrestore(&host->lock, flags);
				regulator_enable(host->SD_pwr);
				spin_lock_irqsave(&host->lock, flags);
			}
			host->ios.vdd = ios->vdd;
		}

		if (ios->bus_width != host->ios.bus_width) {
			_sdhost_set_buswidth(host->ioaddr, ios->bus_width);
			host->ios.bus_width = ios->bus_width;
		}

		if (ios->timing != host->ios.timing) {
			// 1 first close SD clock, charpter:
			_sdhost_SDClk_off(host->ioaddr);
			// 2 set timing mode
			switch (ios->timing) {	/* timing specification used */
			case MMC_TIMING_LEGACY:{
					/*basic clock mode */
					//_sdhost_disable_HISPD(host->ioaddr);
					_sdhost_set_UHS_mode(host->ioaddr, __TIMING_MODE_SDR12);
				}
				break;
			case MMC_TIMING_MMC_HS:
			case MMC_TIMING_SD_HS:{
					//_sdhost_enable_HISPD(host->ioaddr);
					_sdhost_set_UHS_mode(host->ioaddr, __TIMING_MODE_SDR12);
				}
				break;
			case MMC_TIMING_UHS_SDR12:
			case MMC_TIMING_UHS_SDR25:
			case MMC_TIMING_UHS_SDR50:
			case MMC_TIMING_UHS_SDR104:
			case MMC_TIMING_UHS_DDR50:
			case MMC_TIMING_MMC_HS200:{
					//_sdhost_enable_HISPD(host->ioaddr);
					_sdhost_set_UHS_mode(host->ioaddr, ios->timing - MMC_TIMING_UHS_SDR12 + __TIMING_MODE_SDR12);
				}
				break;
			default:{
					BUG_ON(1);
				}
				break;
			}
			// 3 open SD clock
			_sdhost_SDClk_on(host->ioaddr);
			host->ios.timing = ios->timing;
		}

	} while (0);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	_runtime_put(host);
}

STATIC_FUNC int sdhost_get_ro(struct mmc_host *mmc)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;

	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);
	/*read & write */
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	_runtime_put(host);
	return 0;
}

STATIC_FUNC int sdhost_get_cd(struct mmc_host *mmc)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;
	int gpio_cd;

	//printk("sdhost %s, get cd\n",host->deviceName);
	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);

	if (host->mmc->caps & MMC_CAP_NONREMOVABLE) {
		spin_unlock_irqrestore(&host->lock, flags);
		_runtime_put(host);
		return 1;
	}
	gpio_cd = mmc_gpio_get_cd(host->mmc);
	if (IS_ERR_VALUE(gpio_cd)) {
		gpio_cd = 1;
	}
	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	_runtime_put(host);
	return ! !gpio_cd;
}

STATIC_FUNC int sdhost_card_busy(struct mmc_host *mmc)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 present_state;
	u32 temp_state;

	_runtime_get(host);
	spin_lock_irqsave(&host->lock, flags);

	/* Check whether DAT[3:0] is 0000 */
	present_state = _sdhost_readl(host->ioaddr, SDHOST_32_PRES_STATE);

	mmiowb();
	spin_unlock_irqrestore(&host->lock, flags);
	_runtime_put(host);

	if (4 == host->ios.bus_width) {
		return !(present_state & _DATA_LVL_MASK_4BIT);
	} else if (8 == host->ios.bus_width) {
		return !(present_state & _DATA_LVL_MASK_8BIT);
	} else {
		return !(present_state & _DATA_LVL_MASK_1BIT);
	}

}

STATIC_FUNC void sdhost_hw_reset(struct mmc_host *mmc)
{
	struct sdhost_host *host = mmc_priv(mmc);
	unsigned long flags;
	printk("sdhost %s: sdhost_hw_reset start.\n", host->deviceName);
	mmc_power_cycle(host->mmc);
	printk("sdhost %s: sdhost_hw_reset end.\n", host->deviceName);
	return;
}

STATIC_FUNC const struct mmc_host_ops sdhost_ops = {
	.enable = 0,
	.disable = 0,
	.post_req = 0,
	.pre_req = 0,

	.request = sdhost_request,
	.set_ios = sdhost_set_ios,
	.get_ro = sdhost_get_ro,
	.get_cd = sdhost_get_cd,

	.enable_sdio_irq = 0,
	.init_card = 0,

	.start_signal_voltage_switch = sdhost_set_vqmmc,
	.card_busy = sdhost_card_busy,
	.execute_tuning = sdhost_execute_tuning,
	.select_drive_strength = 0,

	.hw_reset = 0,		//sdhost_hw_reset,
	.card_event = 0		//sdhost_card_event
};

STATIC_FUNC int _getBasicResource(struct platform_device *pdev, struct sdhost_host *host)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;

#if 0
	host->ioaddr = (void __iomem *)0xf511c000;
	host->irq = (60 + 32);
	host->detect_gpio = -1;
	host->SD_Pwr_Name = "vddemmccore";
	host->_1_8V_signal_Name = "vddgen1";
	host->ocr_avail = MMC_VDD_30_31;
	host->clkName = "clk_emmc";
	host->clkParentName = "clk_384m";
	host->base_clk = 384000000;
	host->caps = 0xC00F8547;
	host->caps2 = 0x202;
	host->writeDelay = 0x04;
	host->readPosDelay = 0x04;
	host->readNegDelay = 0x04;
#else
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOENT;
	host->ioaddr = ioremap(res->start, resource_size(res));
	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0)
		return host->irq;
	of_property_read_string(np, "sprd,name", &host->deviceName);
	if (of_property_read_u32(np, "detect_gpio", &host->detect_gpio))
		host->detect_gpio = -1;
	of_property_read_string(np, "SD_Pwr_Name", &host->SD_Pwr_Name);
	of_property_read_string(np, "_1_8V_signal_Name", &host->_1_8V_signal_Name);

	of_property_read_u32(np, "ocr_avail", &host->ocr_avail);
	of_property_read_u32(np, "signal_default_Voltage", &host->signal_default_Voltage);

	host->clk = of_clk_get(np, 0);
	if (IS_ERR_OR_NULL(host->clk))
		return PTR_ERR(host->clk);

	//1M clock source
	host->clk_init = of_clk_get(np, 1);
	if (IS_ERR_OR_NULL(host->clk_init))
		return PTR_ERR(host->clk_init);

	//384M clock source
	host->clk_runing = of_clk_get(np, 2);
	if (IS_ERR_OR_NULL(host->clk_runing))
		return PTR_ERR(host->clk_runing);

	host->clk_sdioenable = of_clk_get(np, 3);
	if (IS_ERR_OR_NULL(host->clk_sdioenable)) {
		printk("++++++++++get sdioenableclock fail \n");
		return PTR_ERR(host->clk_runing);
	} else {
		printk("++++++++++get sdioenableclock success \n");
	}

	of_property_read_u32(np, "base_clk", &host->base_clk);
	of_property_read_u32(np, "caps", &host->caps);
	of_property_read_u32(np, "caps2", &host->caps2);
	of_property_read_u32(np, "pm_caps", &host->pm_caps);
	of_property_read_u32(np, "writeDelay", &host->writeDelay);
	of_property_read_u32(np, "readCmdDelay", &host->readCmdDelay);
	of_property_read_u32(np, "readPosDelay", &host->readPosDelay);
	of_property_read_u32(np, "readNegDelay", &host->readNegDelay);
#endif

	return 0;
}

STATIC_FUNC int _getExtResource(struct sdhost_host *host)
{
	int err;

	host->dma_mask = DMA_BIT_MASK(64);
	host->dataTimeOutVal = 0;
// 1 LDO
	host->SD_pwr = regulator_get(NULL, host->SD_Pwr_Name);
//      regulator_enable(host->SD_pwr);
	if (IS_ERR_OR_NULL(host->SD_pwr)) {
		printk("sdhost %s: no SD_pwr regulator found\n", host->deviceName);
		host->SD_pwr = NULL;
	}

	host->_1_8V_signal = regulator_get(NULL, host->_1_8V_signal_Name);

	if (IS_ERR_OR_NULL(host->_1_8V_signal)) {
		printk("sdhost %s: no _1_8V_signal regulator found\n", host->deviceName);
		host->_1_8V_signal = NULL;
	} else {
		regulator_is_supported_voltage(host->_1_8V_signal, host->signal_default_Voltage, host->signal_default_Voltage);
		regulator_set_voltage(host->_1_8V_signal, host->signal_default_Voltage, host->signal_default_Voltage);
	}
// 2 clock
	//enable AHB adio bit
	clk_prepare_enable(host->clk_sdioenable);
	//init clk FREQ 200k
	clk_set_parent(host->clk, host->clk_init);
	clk_set_rate(host->clk, 200000);
	clk_prepare_enable(host->clk);

// 3 reset sdio
	_resetIOS(host);
	err = request_irq(host->irq, _irq, IRQF_SHARED, mmc_hostname(host->mmc), host);
	if (err)
		return err;
	tasklet_init(&host->finish_tasklet, _tasklet, (unsigned long)host);
// 4 init timer
	setup_timer(&host->timer, _timeout, (unsigned long)host);
	return 0;
}

STATIC_FUNC int _setMmcStruct(struct sdhost_host *host, struct mmc_host *mmc)
{
	/* Add pm notify feature to sdio module*/
	host->caps2 |= MMC_CAP2_POWEROFF_NOTIFY;

	mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
	mmc->ops = &sdhost_ops;
	mmc->f_max = host->base_clk;
	mmc->f_min = (unsigned int)(host->base_clk / __CLK_MAX_DIV);
	mmc->max_discard_to = (1 << 27) / (host->base_clk / 1000);

	mmc->caps = host->caps;
	mmc->caps2 = host->caps2;
	mmc->pm_caps = host->pm_caps;
	mmc->pm_flags = host->pm_caps;
	mmc->ocr_avail = host->ocr_avail;
	mmc->ocr_avail_sdio = host->ocr_avail;
	mmc->ocr_avail_sd = host->ocr_avail;
	mmc->ocr_avail_mmc = host->ocr_avail;
	mmc->max_current_330 = SDHOST_MAX_CUR;
	mmc->max_current_300 = SDHOST_MAX_CUR;
	mmc->max_current_180 = SDHOST_MAX_CUR;

	mmc->max_segs = 1;
	mmc->max_req_size = 524288;	// 512k
	mmc->max_seg_size = mmc->max_req_size;

	mmc->max_blk_size = 512;
	mmc->max_blk_count = 65535;
	return 0;
}

STATIC_FUNC int sdhost_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct sdhost_host *host;

// 1 globe resource
	mmc = mmc_alloc_host(sizeof(struct sdhost_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;
	host = mmc_priv(mmc);
	host->mmc = mmc;
	spin_lock_init(&host->lock);
	platform_set_drvdata(pdev, host);
// 2 get sdio irq and sdio iomem
	_getBasicResource(pdev, host);

	_getExtResource(host);
	//set addr width as 64bit
	_sdhost_set_addr_width(host->ioaddr, __64BIT_ADDR_EN);
	_setMmcStruct(host, mmc);
	_pm_runtime_setting(pdev, host);
// 3 add host
	mmiowb();
	mmc_add_host(mmc);
	if (-1 != host->detect_gpio) {
		mmc->caps &= ~MMC_CAP_NONREMOVABLE;
		mmc_gpio_request_cd(mmc, host->detect_gpio);
	}
#ifdef CONFIG_DEBUG_FS
	sdhost_add_debugfs(host);
#endif
	return 0;
}

STATIC_FUNC void sdhost_shutdown(struct platform_device *pdev)
{
	return;
}

STATIC_FUNC const struct dev_pm_ops sdhost_dev_pm_ops = {
#ifdef CONFIG_PM
	.suspend = _pm_suspend,
	.resume = _pm_resume,
#endif
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(_runtime_suspend, _runtime_resume, _runtime_idle)
#endif
};

STATIC_FUNC const struct of_device_id sdhost_of_match[] = {
	{.compatible = "sprd,sdhost-3.0"},
	{}
};

MODULE_DEVICE_TABLE(of, sdhost_of_match);

STATIC_FUNC struct platform_driver sdhost_driver = {
	.probe = sdhost_probe,
	.shutdown = sdhost_shutdown,
	.driver = {
		   .owner = THIS_MODULE,
		   .pm = &sdhost_dev_pm_ops,
		   .name = DRIVER_NAME,
		   .of_match_table = of_match_ptr(sdhost_of_match),
		   },
};

STATIC_FUNC int __init sdhost_drv_init(void)
{

	struct device_node *np, *parent;
	const char *devName;
	parent = of_find_node_by_name(NULL, "sdios");
	for_each_child_of_node(parent, np) {
		of_property_read_string(np, "sprd,name", &devName);
		of_platform_device_create(np, devName, NULL);
		printk("%s deviceName:%s\n", __func__, devName);
	}

	return platform_driver_register(&sdhost_driver);
}

STATIC_FUNC void __exit sdhost_drv_exit(void)
{
	platform_driver_unregister(&sdhost_driver);
}

module_init(sdhost_drv_init);
module_exit(sdhost_drv_exit);
MODULE_AUTHOR("Jason.Wu(Jishuang.Wu) <jason.wu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum sdio host controller driver");
MODULE_LICENSE("GPL");
