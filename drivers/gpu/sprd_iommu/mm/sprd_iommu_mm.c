/*
 * drivers/gpu/iommu/iommu.c *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include"../sprd_iommu_common.h"
#include <linux/clk-provider.h>

static void sprd_iommu_mm_prepare(struct sprd_iommu_dev *dev)
{
#ifndef CONFIG_SC_FPGA
	pr_info("%s, axi clock:%p\n", __func__, dev->mmu_axiclock);
	if (dev->mmu_axiclock)
		clk_prepare(dev->mmu_axiclock);
#endif
}

static void sprd_iommu_mm_unprepare(struct sprd_iommu_dev *dev)
{
#ifndef CONFIG_SC_FPGA
	pr_info("%s, axi clock:%p\n", __func__, dev->mmu_axiclock);
	if (dev->mmu_axiclock)
		clk_unprepare(dev->mmu_axiclock);
#endif
}

void sprd_iommu_mm_clk_enable(struct sprd_iommu_dev *dev)
{
	if (!dev->light_sleep_en)
		pr_info("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_prepare_enable(dev->mmu_mclock);
	clk_prepare_enable(dev->mmu_clock);
#endif
}

void sprd_iommu_mm_clk_disable(struct sprd_iommu_dev *dev)
{
	if (!dev->light_sleep_en)
		pr_info("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_disable_unprepare(dev->mmu_clock);
	clk_disable_unprepare(dev->mmu_mclock);
#endif
}

void sprd_iommu_mm_open(struct sprd_iommu_dev *dev)
{
}

void sprd_iommu_mm_release(struct sprd_iommu_dev *dev)
{
}

int sprd_iommu_mm_init(struct sprd_iommu_dev *dev,
			struct sprd_iommu_init_data *data)
{
	int err = -1;
#ifndef CONFIG_SC_FPGA
	struct device_node *np;

	np = dev->misc_dev.this_device->of_node;
	if (!np)
		return -1;

	dev->mmu_clock = of_clk_get(np, 0);
	dev->mmu_mclock = of_clk_get(np, 1);
	dev->mmu_axiclock = of_clk_get(np, 2);
	if (IS_ERR(dev->mmu_axiclock))
		pr_info("%s, can't get mm axi clock:%p\n", __func__,
			dev->mmu_axiclock);
	else
		dev->light_sleep_en = true;

	if (IS_ERR(dev->mmu_clock) || IS_ERR(dev->mmu_mclock)) {
		pr_info("%s, can't get clock:%p, %p\n", __func__,
			dev->mmu_clock, dev->mmu_mclock);
		goto errorout;
	}
#endif
	sprd_iommu_mm_clk_enable(dev);
	err = sprd_iommu_init(dev, data);
	sprd_iommu_mm_clk_disable(dev);

	return err;

#ifndef CONFIG_SC_FPGA
errorout:
	if (dev->mmu_clock)
		clk_put(dev->mmu_clock);

	if (dev->mmu_mclock)
		clk_put(dev->mmu_mclock);

	if (dev->mmu_axiclock)
		clk_put(dev->mmu_axiclock);

	return -1;
#endif
}

int sprd_iommu_mm_exit(struct sprd_iommu_dev *dev)
{
	int err = -1;

	sprd_iommu_mm_clk_enable(dev);
	err = sprd_iommu_exit(dev);
	sprd_iommu_mm_clk_disable(dev);

	return err;
}

unsigned long sprd_iommu_mm_iova_alloc(struct sprd_iommu_dev *dev,
					size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev, iova_length);
}

void sprd_iommu_mm_iova_free(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	sprd_iommu_iova_free(dev, iova, iova_length);
}

int sprd_iommu_mm_iova_map(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length, struct sg_table *table)
{
	int err = -1;

	if (dev->light_sleep_en) {
		mutex_lock(&dev->mutex_map);
		if (0 == dev->map_count)
			sprd_iommu_mm_prepare(dev);

		sprd_iommu_mm_clk_enable(dev);

		if (0 == dev->map_count)
			sprd_iommu_reset_enable(dev);
		dev->map_count++;

		err = sprd_iommu_iova_map(dev, iova, iova_length, table);
		sprd_iommu_mm_clk_disable(dev);
		mutex_unlock(&dev->mutex_map);
	} else {
		mutex_lock(&dev->mutex_map);
		if (0 == dev->map_count) {
			sprd_iommu_mm_clk_enable(dev);
			sprd_iommu_reset_enable(dev);
		}
		dev->map_count++;
		err = sprd_iommu_iova_map(dev, iova, iova_length, table);
		mutex_unlock(&dev->mutex_map);
	}
	return err;
}

int sprd_iommu_mm_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	int err = -1;

	if (dev->light_sleep_en) {
		mutex_lock(&dev->mutex_map);
		sprd_iommu_mm_clk_enable(dev);
		err = sprd_iommu_iova_unmap(dev, iova, iova_length);

		dev->map_count--;
		if (0 == dev->map_count)
			sprd_iommu_disable(dev);

		sprd_iommu_mm_clk_disable(dev);

		if (0 == dev->map_count)
			sprd_iommu_mm_unprepare(dev);
		mutex_unlock(&dev->mutex_map);
	} else {
		mutex_lock(&dev->mutex_map);
		err = sprd_iommu_iova_unmap(dev, iova, iova_length);
		dev->map_count--;
		if (0 == dev->map_count) {
			sprd_iommu_disable(dev);
			sprd_iommu_mm_clk_disable(dev);
		}
		mutex_unlock(&dev->mutex_map);
	}
	return err;
}

int sprd_iommu_mm_backup(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);

	if (dev->light_sleep_en) {
		if (dev->map_count > 0) {
			sprd_iommu_mm_clk_enable(dev);
			err = sprd_iommu_backup(dev);
			sprd_iommu_mm_clk_disable(dev);
			sprd_iommu_mm_unprepare(dev);
		}
	} else {
		if (dev->map_count > 0) {
			err = sprd_iommu_backup(dev);
			sprd_iommu_mm_clk_disable(dev);
		}
	}
	mutex_unlock(&dev->mutex_map);
	return err;
}

int sprd_iommu_mm_restore(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);

	if (dev->light_sleep_en) {
		if (dev->map_count > 0) {
			sprd_iommu_mm_prepare(dev);
			sprd_iommu_mm_clk_enable(dev);
			err = sprd_iommu_restore(dev);
			sprd_iommu_mm_clk_disable(dev);
		}
	} else {
		if (dev->map_count > 0) {
			sprd_iommu_mm_clk_enable(dev);
			err = sprd_iommu_restore(dev);
		}
	}
	mutex_unlock(&dev->mutex_map);
	return err;
}

int sprd_iommu_mm_dump(struct sprd_iommu_dev *dev, void *data)
{
	int ret = 0;

	mutex_lock(&dev->mutex_map);
	if (dev->light_sleep_en) {
		if (dev->map_count > 0) {
			sprd_iommu_mm_clk_enable(dev);
			mutex_lock(&dev->mutex_pgt);
			memcpy(data, (void *)dev->init_data->pgt_base,
					dev->init_data->pgt_size);
			mutex_unlock(&dev->mutex_pgt);
			sprd_iommu_mm_clk_disable(dev);
			ret = dev->init_data->pgt_size;
		}
	} else {
		if (dev->map_count > 0) {
			mutex_lock(&dev->mutex_pgt);
			memcpy(data, (void *)dev->init_data->pgt_base,
					dev->init_data->pgt_size);
			mutex_unlock(&dev->mutex_pgt);
			ret = dev->init_data->pgt_size;
		}
	}
	mutex_unlock(&dev->mutex_map);

	return ret;
}

void sprd_iommu_mm_pgt_show(struct sprd_iommu_dev *dev)
{
	return iommu_pgt_show(dev);
}

struct sprd_iommu_ops iommu_mm_ops = {
	.init = sprd_iommu_mm_init,
	.exit = sprd_iommu_mm_exit,
	.iova_alloc = sprd_iommu_mm_iova_alloc,
	.iova_free = sprd_iommu_mm_iova_free,
	.iova_map = sprd_iommu_mm_iova_map,
	.iova_unmap = sprd_iommu_mm_iova_unmap,
	.backup = sprd_iommu_mm_backup,
	.restore = sprd_iommu_mm_restore,
	.enable = sprd_iommu_mm_clk_enable,
	.disable = sprd_iommu_mm_clk_disable,
	.dump = sprd_iommu_mm_dump,
	.open = sprd_iommu_mm_open,
	.release = sprd_iommu_mm_release,
	.pgt_show = sprd_iommu_mm_pgt_show,
};

