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


#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/clk-provider.h>
#include <linux/sprd_iommu.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>

#include "vpp_deint_drv.h"

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define SPRD_MMAHB_BASE_DT		SPRD_MMAHB_BASE

#define VPP_MINOR MISC_DYNAMIC_MINOR
#define VPP_AQUIRE_TIMEOUT_MS 500
#define VPP_INIT_TIMEOUT_MS 200

#define DEFAULT_FREQ_DIV 0x0

static DEFINE_SEMAPHORE(s_vpp_sem);

struct deint_fh {
    int is_deint_aquired;
    int is_clock_enabled;

    wait_queue_head_t wait_queue_work;
    int condition_work;
    int deint_int_status;
};

struct deint_dev {
    unsigned int freq_div;

    struct semaphore deint_mutex;

    struct clk *vpp_clk;
    struct clk *vpp_parent_clk[DEINT_CLK_LEVEL_NUM];
    struct clk *vpp_ctrl_clk[MAX_VPP_CTRL_CLK_NUM];

    char* vpp_clk_node_name[MAX_VPP_CTRL_CLK_NUM];

    unsigned int irq;
    unsigned int ctrl_clk_num;
    unsigned int chip_version;

    struct deint_fh *deint_fp;
    struct device_node *dev_np;
};

static struct deint_dev deint_hw_dev;
static struct wake_lock deint_wakelock;
static atomic_t deint_instance_cnt = ATOMIC_INIT(0);

static unsigned long SPRD_VPP_PHYS = 0;
static unsigned long SPRD_VPP_BASE = 0;

struct clock_name_map_t {
    unsigned long freq;
    char *name;
};

static struct clock_name_map_t clock_name_map[DEINT_CLK_LEVEL_NUM];

static int max_freq_level = DEINT_CLK_LEVEL_NUM;

static char *deint_get_clk_src_name(unsigned int freq_level)
{
    if (freq_level >= max_freq_level ) {
        printk(KERN_ERR "set freq_level to 0");
        freq_level = 0;
    }

    return clock_name_map[freq_level].name;
}

static int find_deint_freq_level(unsigned long freq)
{
    int level = 0;
    int i;
    for (i = 0; i < max_freq_level; i++) {
        if (clock_name_map[i].freq == freq) {
            level = i;
            break;
        }
    }
    return level;
}

static int deint_alloc_dma_buffer(VPP_DRV_BUFFER_T *vb)
{
    if (!vb)
        return -1;

    //printk(KERN_ERR "deint_alloc_dma_buffer size=%d\n", vb->size);

    vb->base = (unsigned long)dma_alloc_coherent(NULL, PAGE_ALIGN(vb->size), (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);
    if ((void *)(vb->base) == NULL)	{
        printk(KERN_ERR "dynamic Physical memory allocation error size=%d\n", vb->size);
        return -1;
    }

    return 0;
}

static void deint_free_dma_buffer(VPP_DRV_BUFFER_T *vb)
{
    if (!vb)
        return;

    if (vb->base)
        dma_free_coherent(0, PAGE_ALIGN(vb->size), (void *)vb->base, vb->phys_addr);

    return;
}
static long deint_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct clk *clk_parent;
    char *name_parent;
    unsigned long frequency;
    struct deint_fh *deint_fp = filp->private_data;

    if (deint_fp == NULL) {
        printk(KERN_ERR "deint_ioctl error occured, deint_fp == NULL\n");
        return  -EINVAL;
    }

    switch (cmd) {
    case SPRD_VPP_ALLOCATE_PHYSICAL_MEMORY:
    {
        VPP_DRV_BUFFER_T *vb = NULL;

        down(&s_vpp_sem);

        vb = kzalloc(sizeof(VPP_DRV_BUFFER_T), GFP_KERNEL);
        if (!vb) {
            up(&s_vpp_sem);
            return -ENOMEM;
        }

        ret = copy_from_user(vb, (VPP_DRV_BUFFER_T *)arg, sizeof(VPP_DRV_BUFFER_T));
        if (ret) {
            kfree(vb);
            up(&s_vpp_sem);
            return -EFAULT;
        }

        ret = deint_alloc_dma_buffer(vb);
        if (ret == -1) {
            ret = -ENOMEM;
            kfree(vb);
            up(&s_vpp_sem);
            break;
        }
        ret = copy_to_user((void __user *)arg, vb, sizeof(VPP_DRV_BUFFER_T));
        if (ret) {
            kfree(vb);
            ret = -EFAULT;
            up(&s_vpp_sem);
            break;
        }

        up(&s_vpp_sem);
        break;
    }

    case SPRD_VPP_FREE_PHYSICAL_MEMORY:
    {
        VPP_DRV_BUFFER_T vb;

        down(&s_vpp_sem);

        ret = copy_from_user(&vb, (VPP_DRV_BUFFER_T *)arg, sizeof(VPP_DRV_BUFFER_T));
        if (ret) {
            up(&s_vpp_sem);
            return -EACCES;
        }

        printk(KERN_ERR "SPRD_VPP_FREE_PHYSICAL_MEMORY base: 0x%x, phys_addr: 0x%x, size: 0x%x, virt_addr: 0x%x\n",
               vb.base, vb.phys_addr, vb.size, vb.virt_addr);

        if (vb.base)
            deint_free_dma_buffer(&vb);

        up(&s_vpp_sem);
        break;
    }

    case SPRD_VPP_DEINT_COMPLETE:
    {
        ret = wait_event_interruptible_timeout(
                  deint_fp->wait_queue_work,
                  deint_fp->condition_work,
                  msecs_to_jiffies(VPP_INIT_TIMEOUT_MS));

        if (ret == -ERESTARTSYS) {
            printk(KERN_INFO "deint complete -ERESTARTSYS\n");
            put_user(ret, (int __user *)arg);
            ret = -EINVAL;
        }
        else {
            if (ret == 0) {
                printk(KERN_ERR "vpp complete  timeout\n");
                ret = -ETIMEDOUT;

                /*clear vpp int*/
                __raw_writel((1<<1), SPRD_VPP_BASE + VPP_INT_CLR);
            }

            put_user(ret, (int __user *)arg);
            deint_fp->condition_work = 0;
        }

        break;
    }

    case SPRD_VPP_DEINT_ACQUIRE:
    {
        pr_debug("deint ioctl SPRD_VPP_DEINT_ACQUIRE begin\n");
        wake_lock(&deint_wakelock);
        ret = down_timeout(&deint_hw_dev.deint_mutex, msecs_to_jiffies(VPP_AQUIRE_TIMEOUT_MS));
        if (ret) {
            printk(KERN_ERR "deint error timeout\n");
            //up(&deint_hw_dev.deint_mutex);
            return ret;
        }

#ifdef ENABLE_VPP_FRAME_BY_FRAME
        ret = clk_prepare_enable(deint_hw_dev.vpp_clk);
        if (ret) {
            printk(KERN_ERR "###: vpp_clk: clk_prepare_enable() failed!\n");
            return ret;
        }
#endif

        deint_hw_dev.deint_fp= deint_fp;
        deint_fp->is_deint_aquired = 1;

        break;
    }

    case SPRD_VPP_DEINT_RELEASE:
    {
        pr_debug("SPRD_VPP_DEINT_RELEASE\n");
        deint_fp->is_deint_aquired = 0;
        deint_hw_dev.deint_fp = NULL;

#ifdef ENABLE_VPP_FRAME_BY_FRAME
        if (deint_hw_dev.vpp_clk) {
            clk_disable_unprepare(deint_hw_dev.vpp_clk);
        }
#endif

        up(&deint_hw_dev.deint_mutex);
        wake_unlock(&deint_wakelock);

        break;
    }

    case VPP_RESET:
        pr_debug("vsp ioctl VSP_RESET\n");
        sci_glb_set(SPRD_MMAHB_BASE_DT+0x04, BIT(15));
        sci_glb_clr(SPRD_MMAHB_BASE_DT+0x04, BIT(15));
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static irqreturn_t deint_isr(int irq, void *data)
{
    int ret = 0xff; // 0xff : invalid
    struct deint_fh *deint_fp = ((struct deint_dev *)data)->deint_fp;

    if(NULL == deint_fp) {
        //printk(KERN_ERR "This interrupt signal is not for Deint\n");
        return  IRQ_NONE;
    }

    ret =  __raw_readl(SPRD_VPP_BASE + VPP_INT_STS);
    if((ret >> 1) & 0x1)
    {
        __raw_writel((1<<1), SPRD_VPP_BASE + VPP_INT_CLR);
    }
    else
    {
        return  IRQ_NONE;
    }

    deint_fp->condition_work = 1;
    wake_up_interruptible(&deint_fp->wait_queue_work);

    return IRQ_HANDLED;
}

static int deint_clock_release(void)
{
    int i = 0;

#ifndef ENABLE_VPP_FRAME_BY_FRAME
    if (deint_hw_dev.vpp_clk) {
        clk_disable_unprepare(deint_hw_dev.vpp_clk);
    }
#endif

    for(i = deint_hw_dev.ctrl_clk_num-1; i >= 0; i--) {
        if (i == 1) continue; //vpp clock position which is dependent on dts file

        if (deint_hw_dev.vpp_ctrl_clk[i]) {
            clk_disable_unprepare(deint_hw_dev.vpp_ctrl_clk[i]);
        }
    }

#if defined(CONFIG_SPRD_IOMMU)
    if(deint_hw_dev.chip_version == WHALE)
        sprd_iommu_module_disable(IOMMU_VPP);
    else
        sprd_iommu_module_disable(IOMMU_MM);
#endif

    return 0;
}

static int vpp_set_mm_clk(void)
{
    int i, ret =0;
    struct clk *clk_tmp;
    struct clk *clk_vpp;
    struct clk *clk_parent;
    char *name_parent;
    int instance_cnt = atomic_read(&deint_instance_cnt);

    pr_debug(KERN_INFO "vsp_set_mm_clk: deint_instance_cnt %d\n", instance_cnt);

    for(i = 0; i < deint_hw_dev.ctrl_clk_num; i++) {
        if (i == 1) continue; //vpp clock position which is dependent on dts file

        printk(KERN_ERR "###:clk_ahb_disp clk_enable() failed!\n");

        if(deint_hw_dev.vpp_ctrl_clk[i]) {
            ret = clk_prepare_enable(deint_hw_dev.vpp_ctrl_clk[i]);
            if (ret) {
                printk(KERN_ERR "###:clk_ahb_disp clk_enable() failed!\n");
                goto errout;
            }
        }
    }

#ifndef ENABLE_VPP_FRAME_BY_FRAME
    if (deint_hw_dev.vpp_clk) {
        ret = clk_prepare_enable(deint_hw_dev.vpp_clk);
        if (ret) {
            printk(KERN_ERR "###: vpp_clk: clk_prepare_enable() failed!\n");
            ret = -EINVAL;
            goto errout;
        }
    } else {
        printk(KERN_ERR "deint clock is NULL \n");
        ret = -EINVAL;
        goto errout;
    }
#endif


    ret = clk_set_parent(deint_hw_dev.vpp_clk, deint_hw_dev.vpp_parent_clk[DEFAULT_FREQ_DIV]);
    if (ret) {
        printk(KERN_ERR "clock[%s]: clk_set_parent() failed!", "clk_vpp");
        ret = -EINVAL;
        goto errout;
    }

    ret = clk_set_parent(deint_hw_dev.vpp_clk, deint_hw_dev.vpp_parent_clk[DEINT_CLK_LEVEL_NUM-1]);
    if (ret) {
        printk(KERN_ERR "clock[%s]: clk_set_parent() failed!", "clk_vpp");
        ret = -EINVAL;
        goto errout;
    }

    printk("vpp_freq %d Hz \n", (int)clk_get_rate(deint_hw_dev.vpp_clk));

#if defined(CONFIG_SPRD_IOMMU)
    if(deint_hw_dev.chip_version == WHALE)
        sprd_iommu_module_enable(IOMMU_VPP);
    else
        sprd_iommu_module_enable(IOMMU_MM);
#endif

    return 0;

errout:

    deint_clock_release();
    return ret;
}

static int deint_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    struct deint_fh *deint_fp = kmalloc(sizeof(struct deint_fh), GFP_KERNEL);

    if (deint_fp == NULL) {
        printk(KERN_ERR "deint open error occured\n");
        return  -EINVAL;
    }
    filp->private_data = deint_fp;
    deint_fp->is_clock_enabled = 0;
    deint_fp->is_deint_aquired = 0;
    deint_fp->condition_work = 0;
    //deint_hw_dev.deint_fp = deint_fp;

    init_waitqueue_head(&deint_fp->wait_queue_work);

    ret = vpp_set_mm_clk();

    atomic_inc_return(&deint_instance_cnt);

    printk("deint_open: ret %d \n", ret);

    return ret;


}

static int deint_release (struct inode *inode, struct file *filp)
{
    struct deint_fh *deint_fp = filp->private_data;
    int instance_cnt = atomic_read(&deint_instance_cnt);

    if (deint_fp == NULL) {
        printk(KERN_ERR "deint_release error occured, deint_fp == NULL\n");
        return  -EINVAL;
    }

    printk(KERN_INFO "deint_release: instance_cnt = %d\n", instance_cnt);

    if (deint_fp->is_deint_aquired) {
        printk(KERN_ERR "error occured and up deint_mutex \n");
        up(&deint_hw_dev.deint_mutex);
    }

    kfree(filp->private_data);
    filp->private_data=NULL;

    deint_clock_release();

    atomic_dec_return(&deint_instance_cnt);

    return 0;
}

static int deint_map_to_register(struct file *fp, struct vm_area_struct *vm)
{
    unsigned long pfn;

    printk("@deint[%s] \n", __FUNCTION__);

    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
    pfn = SPRD_VPP_PHYS >> PAGE_SHIFT;

    return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int deint_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
    printk("@deint[%s] \n", __FUNCTION__);

    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);

    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int deint_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret = 0;
    printk("@deint[%s], vm_pgoff = %ld\n", __FUNCTION__, vma->vm_pgoff);

    if (vma->vm_pgoff)
    {
        ret = deint_map_to_physical_memory(filp, vma);
    }
    else
    {
        ret = deint_map_to_register(filp, vma);
    }

    printk("@deint mmap start: %p,size: %ld\n",
           (void*)vma->vm_start,
           (unsigned long)(vma->vm_end - vma->vm_start));

    return 0;
}

static const struct file_operations deint_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = deint_ioctl,
    .mmap  = deint_mmap,
    .open = deint_open,
    .release = deint_release,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = deint_ioctl,
#endif
};

static struct miscdevice deint_dev = {
    .minor   = VPP_MINOR,
    .name   = "sprd_vpp",
    .fops   = &deint_fops,
};

static int deint_suspend(struct platform_device *pdev, pm_message_t state)
{
    int ret=-1;
    int cnt;
    int instance_cnt = atomic_read(&deint_instance_cnt);

    for (cnt = 0; cnt < instance_cnt; cnt++) {
        deint_clock_release();
        printk(KERN_INFO "deint_suspend, cnt: %d\n", cnt);
    }


    return 0;
}

static int deint_resume(struct platform_device *pdev)
{

    int ret = 0;
    int cnt;
    int instance_cnt = atomic_read(&deint_instance_cnt);

    for (cnt = 0; cnt < instance_cnt; cnt++) {
        ret = vpp_set_mm_clk();
        printk(KERN_INFO "deint_resume, cnt: %d\n", cnt);
    }

    return ret;
}
static int deint_remove(struct platform_device *pdev)
{

    misc_deregister(&deint_dev);

    free_irq(deint_hw_dev.irq, &deint_hw_dev);

    if (deint_hw_dev.vpp_clk) {
        clk_put(deint_hw_dev.vpp_clk);
    }

    if (deint_hw_dev.vpp_parent_clk) {
        clk_put(deint_hw_dev.vpp_parent_clk);
    }

    printk(KERN_INFO "deint_remove Success !\n");
    return 0;
}

static int deint_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    struct device_node *deint_clk_np = NULL;
    char *clk_node_name = NULL;
    struct resource res;
    int i, ret, clk_count = 0;
    int deint_clk_count = 0;

    ret = of_address_to_resource(np, 0, &res);
    if(ret < 0) {
        dev_err(dev, "no reg of property specified\n");
        printk(KERN_ERR "deint: failed to parse_dt!\n");
        return -EINVAL;
    }

    SPRD_VPP_PHYS = res.start;
    SPRD_VPP_BASE = (unsigned long)ioremap_nocache(res.start, resource_size(&res));
    if(!SPRD_VPP_BASE)
        BUG();

    deint_hw_dev.irq = irq_of_parse_and_map(np, 0);
    deint_hw_dev.dev_np = np;

    printk(KERN_INFO "deint_parse_dt ,  SPRD_VPP_PHYS = %p, SPRD_VPP_BASE = %p, irq = %d\n",
           (void*)SPRD_VPP_PHYS, (void*)SPRD_VPP_BASE, deint_hw_dev.irq);

    deint_hw_dev.ctrl_clk_num = of_clk_get_parent_count(np);
    for(i = 0; i < deint_hw_dev.ctrl_clk_num; i++) {
        clk_node_name  = of_clk_get_parent_name(np, i);
        deint_hw_dev.vpp_clk_node_name[i] = clk_node_name;
        deint_hw_dev.vpp_ctrl_clk[i] = of_clk_get_by_name(np, clk_node_name);

        printk(KERN_INFO "deint_parse_dt , clock num: %d, node_name: %s, clock: %p\n", i, clk_node_name, deint_hw_dev.vpp_ctrl_clk[i]);
        if (IS_ERR(deint_hw_dev.vpp_ctrl_clk[i]) || (!deint_hw_dev.vpp_ctrl_clk[i])) {
            printk(KERN_ERR "###: Failed : can't get clock [%s}!\n", clk_node_name);
            deint_hw_dev.vpp_ctrl_clk[i] = NULL;
            return -EINVAL;
        }
    }

    deint_hw_dev.vpp_clk = deint_hw_dev.vpp_ctrl_clk[1]; //This position is based on related dts file
    deint_clk_np = of_find_node_by_name(NULL, deint_hw_dev.vpp_clk_node_name[1]);
    if (!deint_clk_np) {
        printk(KERN_ERR "failed to get deint clk device node\n");
        return -EINVAL;
    }

    clk_count = of_clk_get_parent_count(deint_clk_np);
    if(clk_count != DEINT_CLK_LEVEL_NUM) {
        printk(KERN_ERR "failed to get deint clock count\n");
        return -EINVAL;
    }

    for(i = 0; i < clk_count; i++) {
        struct clk *clk_parent;
        char *name_parent;
        unsigned long frequency;

        name_parent = of_clk_get_parent_name(deint_clk_np,  i);
        clk_parent = clk_get(NULL, name_parent);
        frequency = clk_get_rate(clk_parent);
        printk(KERN_INFO "deint: clock_name_map[%d] = (%d, %s)\n", i, frequency, name_parent);

        clock_name_map[i].name = name_parent;
        clock_name_map[i].freq = frequency;
        deint_hw_dev.vpp_parent_clk[i] = clk_parent;
    }

    if(deint_hw_dev.ctrl_clk_num == 6) {
        deint_hw_dev.chip_version = WHALE;
    } else if (deint_hw_dev.ctrl_clk_num > 0) {
        deint_hw_dev.chip_version = TSHARK;
    } else {
        printk(KERN_ERR "deint_parse_dt: deint clock num is set error! \n");
        return -EINVAL;
    }

    return 0;
}

static int deint_probe(struct platform_device *pdev)
{
    int ret;
    unsigned long irq_flag;
    struct resource *res = NULL;

    printk(KERN_INFO "deint_probe called !\n");

    memset(&deint_hw_dev, 0, sizeof(deint_hw_dev));
    deint_hw_dev.freq_div = DEFAULT_FREQ_DIV;

    if (pdev->dev.of_node) {
        if(deint_parse_dt(&pdev->dev)) {
            printk(KERN_ERR "deint_parse_dt failed\n");
            return -EINVAL;
        }
    }

    sema_init(&deint_hw_dev.deint_mutex, 1);
    wake_lock_init(&deint_wakelock, WAKE_LOCK_SUSPEND, "pm_message_wakelock_vpp_deint");

    if (deint_hw_dev.chip_version == WHALE)
        irq_flag = IRQF_DISABLED;
    else
        irq_flag = IRQF_DISABLED|IRQF_SHARED;

    ret = misc_register(&deint_dev);
    if (ret) {
        printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n", VPP_MINOR, ret);
        goto errout;
    }

    /* register isr */
    ret = request_irq(deint_hw_dev.irq, deint_isr, irq_flag, "deinterlace", &deint_hw_dev);
    if (ret) {
        printk(KERN_ERR "vpp: failed to request deint irq!\n");
        ret = -EINVAL;
        goto errout;
    }

    return 0;

errout:
    misc_deregister(&deint_dev);

    return ret;
}

static const struct of_device_id  of_match_table_vpp[] = {
    { .compatible = "sprd,sprd_vpp", },
    { },
};

static struct platform_driver deint_driver = {
    .probe    = deint_probe,
    .remove   = deint_remove,
    .suspend = deint_suspend,
    .resume = deint_resume,
    .driver   = {
        .owner = THIS_MODULE,
        .name = "sprd_vpp",
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(of_match_table_vpp) ,
#endif
    },
};

static int __init deint_init(void)
{
    printk(KERN_INFO "deint_init called !\n");
    if (platform_driver_register(&deint_driver) != 0) {
        printk(KERN_ERR "platform device deint drv register Failed \n");
        return -1;
    }

    return 0;
}

static void __exit deint_exit(void)
{
    printk(KERN_INFO "deint_exit called !\n");
    platform_driver_unregister(&deint_driver);
}

module_init(deint_init);
module_exit(deint_exit);

MODULE_DESCRIPTION("SPRD VPP Driver");
MODULE_LICENSE("GPL");

