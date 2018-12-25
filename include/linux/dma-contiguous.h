#ifndef __LINUX_CMA_H
#define __LINUX_CMA_H

/*
 * Contiguous Memory Allocator for DMA mapping framework
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */

/*
 * Contiguous Memory Allocator
 *
 *   The Contiguous Memory Allocator (CMA) makes it possible to
 *   allocate big contiguous chunks of memory after the system has
 *   booted.
 *
 * Why is it needed?
 *
 *   Various devices on embedded systems have no scatter-getter and/or
 *   IO map support and require contiguous blocks of memory to
 *   operate.  They include devices such as cameras, hardware video
 *   coders, etc.
 *
 *   Such devices often require big memory buffers (a full HD frame
 *   is, for instance, more then 2 mega pixels large, i.e. more than 6
 *   MB of memory), which makes mechanisms such as kmalloc() or
 *   alloc_page() ineffective.
 *
 *   At the same time, a solution where a big memory region is
 *   reserved for a device is suboptimal since often more memory is
 *   reserved then strictly required and, moreover, the memory is
 *   inaccessible to page system even if device drivers don't use it.
 *
 *   CMA tries to solve this issue by operating on memory regions
 *   where only movable pages can be allocated from.  This way, kernel
 *   can use the memory for pagecache and when device driver requests
 *   it, allocated pages can be migrated.
 *
 * Driver usage
 *
 *   CMA should not be used by the device drivers directly. It is
 *   only a helper framework for dma-mapping subsystem.
 *
 *   For more information, see kernel-docs in drivers/base/dma-contiguous.c
 */

#ifdef __KERNEL__

struct cma;
struct page;
struct device;

#ifdef CONFIG_CMA

/*
 * There is always at least global CMA area and a few optional device
 * private areas configured in kernel .config.
 */
#define MAX_CMA_AREAS	(1 + CONFIG_CMA_AREAS)

extern struct cma *dma_contiguous_default_area;

static inline struct cma *dev_get_cma_area(struct device *dev)
{
	if (dev && dev->cma_area)
		return dev->cma_area;
	return dma_contiguous_default_area;
}

static inline void dev_set_cma_area(struct device *dev, struct cma *cma)
{
	if (dev)
		dev->cma_area = cma;
}

static inline void dma_contiguous_set_default(struct cma *cma)
{
	dma_contiguous_default_area = cma;
}

void dma_contiguous_reserve(phys_addr_t addr_limit);
int dma_contiguous_reserve_area(struct device *dev, phys_addr_t size,
				  phys_addr_t base, phys_addr_t limit,
				  phys_addr_t reserve_size, phys_addr_t threshold_size);
static inline int dma_declare_contiguous(struct device *dev, phys_addr_t size,
			   phys_addr_t base, phys_addr_t limit)
{
	int ret;
	ret = dma_contiguous_reserve_area(dev, size,
				  base, limit, 0, 0);
	return ret;
}

static inline int dma_declare_contiguous_reserved(struct device *dev,
					 phys_addr_t size, phys_addr_t base,
					 phys_addr_t limit, phys_addr_t reserve_size,
					 phys_addr_t threshold_size)
{
	int ret;
	ret = dma_contiguous_reserve_area(dev, size,
				  base, limit, reserve_size, threshold_size);
	return ret;
}

struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int order);
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count);

#else

#define MAX_CMA_AREAS	(0)

static inline struct cma *dev_get_cma_area(struct device *dev)
{
	return NULL;
}

static inline void dev_set_cma_area(struct device *dev, struct cma *cma) { }

static inline void dma_contiguous_set_default(struct cma *cma) { }

static inline void dma_contiguous_reserve(phys_addr_t limit) { }

static inline
int dma_declare_contiguous(struct device *dev, phys_addr_t size,
			   phys_addr_t base, phys_addr_t limit)
{
	return -ENOSYS;
}

static inline
struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int order)
{
	return NULL;
}

static inline
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count)
{
	return false;
}

#endif

#endif

#endif
