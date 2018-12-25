#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <soc/sprd/sci_glb_regs.h>

#ifndef CONFIG_64BIT
#include <mach/irqs.h>
#include <soc/sprd/globalregs.h>
#include <soc/sprd/hardware.h>
#else
#include <soc/sprd/irqs.h>
#endif
#include "parse_hwinfo.h"
#include <soc/sprd/sci.h>

#include "csi_api.h"
#include "csi_log.h"

#if  defined(CONFIG_ARCH_WHALE)

#define CSI2_EB              (SPRD_MMAHB_BASE + 0)
#define CSI2_EB_BIT         (1 << 3)
#define CSI2_RST             (SPRD_MMAHB_BASE + 4)
#define CSI2_RST_BIT        (1 << 4)

#define CSI2_EB2_BIT          (1 << 4)
#define CSI2_RST2_BIT         (1 << 5)
#else
#define CSI2_EB              (SPRD_MMAHB_BASE)
#define CSI2_EB_BIT          (1 << 4)
#define CSI2_RST             (SPRD_MMAHB_BASE + 0x4)
#define CSI2_RST_BIT         (1 << 5)
#endif

#if defined(CONFIG_SC_FPGA) && (defined(CONFIG_ARCH_SCX20))
extern void usc28c_csi_init(void);
#endif
void csi_api_event1_handler(int irq, void *handle);
void csi_api_event2_handler(int irq, void *handle);


static void csi_phy_power_down(u32 phy_id, u32 is_eb)
{
#if IS_ENABLED(VERSION3T) || IS_ENABLED(VERSION3D)
	/*bit0: phya ; bit1: phyb*/
	if (is_eb) {
		if (0x03 == (phy_id & 0x03)) {
			sci_glb_set(REG_AON_APB_PWR_CTRL, (BIT_CSI0_PHY_PD | BIT_CSI1_PHY_PD));
		} else {
			if (0x01 == (phy_id & 0x01)) {
				sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_CSI0_PHY_PD);
			}

			if (0x02 == (phy_id & 0x02)) {
				sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_CSI1_PHY_PD);
			}
		}
	} else {
		if (0x03 == (phy_id & 0x03)) {
			sci_glb_clr(REG_AON_APB_PWR_CTRL, (BIT_CSI0_PHY_PD | BIT_CSI1_PHY_PD));
		} else {
			if (0x01 == (phy_id & 0x01)) {
				sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_CSI0_PHY_PD);
			}

			if (0x02 == (phy_id & 0x02)) {
				sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_CSI1_PHY_PD);
			}
		}
	}
#elif IS_ENABLED(VERSION3L)
	/*bit0: phya ; bit1: phyb*/
	if (is_eb) {
		if (0x03 == (phy_id & 0x03)) {
			printk("csi phy erro: scx35l does not support the combination of phya & phyb\n");
		} else {
			if (0x01 == (phy_id & 0x01)) {
				sci_glb_set(REG_AON_APB_PWR_CTRL, (3 << 12));
			}

			if (0x02 == (phy_id & 0x02)) {
				sci_glb_set(REG_AON_APB_PWR_CTRL, (3 << 10));
			}
		}
	} else {
		if (0x03 == (phy_id & 0x03)) {
			printk("csi phy erro: scx35l does not support the combination of phya & phyb\n");
		} else {
			if (0x01 == (phy_id & 0x01)) {
				sci_glb_clr(REG_AON_APB_PWR_CTRL, (1 << 13));
				udelay(500);
				sci_glb_clr(REG_AON_APB_PWR_CTRL, (1 << 12));
			}

			if (0x02 == (phy_id & 0x02)) {
				sci_glb_clr(REG_AON_APB_PWR_CTRL, (1 << 11));
				udelay(500);
				sci_glb_clr(REG_AON_APB_PWR_CTRL, (1 << 10));
			}
		}
	}
#endif
}

static void csi_enable(void)
{
#if  defined(CONFIG_ARCH_WHALE)
	if(get_module_selectindex(MODULE_CSI) == 0) {
		sci_glb_set(CSI2_EB, CSI2_EB_BIT);
		sci_glb_set(CSI2_RST, CSI2_RST_BIT);
		udelay(1);
		sci_glb_clr(CSI2_RST, CSI2_RST_BIT);
	} else {
		sci_glb_set(CSI2_EB, CSI2_EB2_BIT);
		sci_glb_set(CSI2_RST, CSI2_RST2_BIT);
		udelay(1);
		sci_glb_clr(CSI2_RST, CSI2_RST2_BIT);
	}
#else
		sci_glb_set(CSI2_EB, CSI2_EB_BIT);
		sci_glb_set(CSI2_RST, CSI2_RST_BIT);
		udelay(1);
		sci_glb_clr(CSI2_RST, CSI2_RST_BIT);
#endif
}

int csi_api_malloc(void **handle)
{
	int ret = 0;
	struct csi_context *csi_handle = NULL;
	csi_handle = (struct csi_context *)vzalloc(sizeof(struct csi_context));
	if (NULL == handle) {
		printk("vzalloc fail,no mem");
		return -1;
	}
	csi_handle->g_csi2_irq = 0x12000034;
	spin_lock_init(&csi_handle->csi2_lock);
	*handle = csi_handle;

	return ret;
}

void csi_api_free(void *handle)
{
	if (handle != NULL) {
		vfree(handle);
		handle = NULL;
	}
}

u8 csi_api_init(u32 bps_per_lane, u32 phy_id)
{
	csi_error_t e = SUCCESS;
    u64 base_address = CSI2_BASE;

#if defined(CONFIG_SC_FPGA) && (defined(CONFIG_ARCH_SCX20))
	usc28c_csi_init();
#endif

    do {
		csi_phy_power_down(phy_id, 0);
		csi_enable();
		e = csi_init(base_address);
		if(e != SUCCESS) {
			LOG_ERROR("Unable to initialise driver");
			break;
		}
		dphy_init(bps_per_lane, phy_id);
    }while(0);

    return e;
}

u8 csi_api_start(void *handle)
{
	csi_error_t e = SUCCESS;
	int         ret = 0;
	struct csi_context *csi_handle = handle;

	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}
	do {
		/* set only one lane (lane 0) as active (ON) */
		e = csi_set_on_lanes(1);
		if(e != SUCCESS) {
			LOG_ERROR("Unable to set lanes");
			csi_close();
			break;
		}
		LOG_DEBUG("Lane set OK");
		e = csi_shut_down_phy(0);
		if(e != SUCCESS) {
			LOG_ERROR("Unable to bring up PHY");
			csi_close();
			break;
		}
		LOG_DEBUG("PHY power up OK");

		e = csi_reset_phy();
		if(e != SUCCESS) {
			LOG_ERROR("Unable to reset PHY");
			csi_close();
			break;
		}
		LOG_DEBUG("PHY reset OK");

		e = csi_reset_controller();
		if(e != SUCCESS) {
			LOG_ERROR("Unable to reset controller");
			csi_close();
			break;
		}
		/* MASK all interrupts */
		csi_event_disable(0xffffffff, 1);
		csi_event_disable(0xffffffff, 2);
#if  defined(CONFIG_ARCH_WHALE)
		ret = request_irq(csi_parseinfo[get_module_selectindex(MODULE_CSI)].irq,
				(irq_handler_t)csi_api_event1_handler,
				IRQF_SHARED,
				"CSI2_0",
				(void *)csi_handle);
#else
		ret = request_irq(IRQ_CSI_INT0,
				(irq_handler_t)csi_api_event1_handler,
				IRQF_SHARED,
				"CSI2_0",
				(void *)csi_handle);
#endif
		if (ret) {
			e = ERR_UNDEFINED;
			break;
		}
#if  defined(CONFIG_ARCH_WHALE)
		ret = request_irq(csi_parseinfo[get_module_selectindex(MODULE_CSI)].irq2,
				(irq_handler_t)csi_api_event2_handler,
				IRQF_SHARED,
				"CSI2_1",
				(void *)csi_handle);
#else
		ret = request_irq(IRQ_CSI_INT1,
				(irq_handler_t)csi_api_event2_handler,
				IRQF_SHARED,
				"CSI2_1",
				(void *)csi_handle);
#endif
		if (ret) {
			e = ERR_UNDEFINED;
			break;
		}

	} while(0);

	return e;
}

u8 csi_api_close(void *handle, u32 phy_id)
{
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}
	LOG_DEBUG("exit");
	csi_api_unregister_all_events(handle);
	csi_shut_down_phy(1);
#if  defined(CONFIG_ARCH_WHALE)
	free_irq(csi_parseinfo[get_module_selectindex(MODULE_CSI)].irq, &csi_handle->g_csi2_irq);
	free_irq(csi_parseinfo[get_module_selectindex(MODULE_CSI)].irq2, &csi_handle->g_csi2_irq);
#else
	free_irq(IRQ_CSI_INT0, &csi_handle->g_csi2_irq);
	free_irq(IRQ_CSI_INT1, &csi_handle->g_csi2_irq);
#endif
	csi_close();
	csi_phy_power_down(phy_id, 1);

	return SUCCESS;
}

u8 csi_api_set_on_lanes(u8 no_of_lanes)
{
    return csi_set_on_lanes(no_of_lanes);
}

u8 csi_api_get_on_lanes()
{
    return csi_get_on_lanes();
}

csi_lane_state_t csi_api_get_clk_state()
{
    return csi_clk_state();
}

csi_lane_state_t csi_api_get_lane_state(u8 lane)
{
    return csi_lane_module_state(lane);
}

static u32 csi_api_event_map(u8 event, u8 vc_lane)
{
	switch((csi_event_t)(event)) {
	case ERR_PHY_TX_START:
		return (0x1 << (event + vc_lane));
	case ERR_FRAME_BOUNDARY_MATCH:
		return (0x1 << (event + vc_lane));
	case ERR_FRAME_SEQUENCE:
		return (0x1 << (event + vc_lane));
	case ERR_CRC_DURING_FRAME:
		return (0x1 << (event + vc_lane));
	case ERR_LINE_CRC:
		return (0x1 << (24 + vc_lane));
	case ERR_DOUBLE_ECC:
		return (0x1 << 28);
	case ERR_PHY_ESCAPE_ENTRY:
		return (0x1 << (0 + vc_lane));
	case ERR_ECC_SINGLE:
		return (0x1 << (8 + vc_lane));
	case ERR_UNSUPPORTED_DATA_TYPE:
		return (0x1 << (12 + vc_lane));
	case MAX_EVENT:
		break;
	default:
		break;
	}
	switch((csi_line_event_t)(event)) {
	case ERR_LINE_BOUNDARY_MATCH:
		return (0x1 << (16 + (vc_lane % 4)));
	case ERR_LINE_SEQUENCE:
		return (0x1 << (20 + (vc_lane % 4)));
	default:
		break;
	}
	/* writing the following value affects nothing */
	return 0x80000000;
}

u8 csi_api_register_event(void *handle, csi_event_t event, u8 vc_lane, handler_t handler)
{
	/* the VC_LANE value is the lane number ONLY in PHY ERRORS
	  * e rest are all virtual channels number except for
	  * double ECC, where VC is unknown
	  */
	u8 e = SUCCESS;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}
	/* the maximum */
	if (vc_lane < 4) {
		switch(event) {
		case ERR_PHY_TX_START:
				LOG_WARNING("the lane value depends on the synthesis defines");
				LOG_WARNING("make sure this value does not exceed these defines");
		case ERR_FRAME_BOUNDARY_MATCH:
		case ERR_FRAME_SEQUENCE:
		case ERR_CRC_DURING_FRAME:
		case ERR_LINE_CRC:
		case ERR_DOUBLE_ECC:
			e = csi_event_enable(csi_api_event_map(event, vc_lane), 1);
			break;

		case ERR_PHY_ESCAPE_ENTRY:
			LOG_WARNING("the lane value depends on the synthesis defines");
			LOG_WARNING("make sure this value does not exceed these defines");
		case ERR_ECC_SINGLE:
		case ERR_UNSUPPORTED_DATA_TYPE:
			e = csi_event_enable(csi_api_event_map(event, vc_lane), 2);
			break;

		default:
			e = ERROR_EVENT_TYPE_INVALID;
			break;
		}
	} else {
		e = ERROR_VC_LANE_OUT_OF_BOUND;
	}
	if (e == SUCCESS) {
		csi_handle->csi_api_event_registry[event + vc_lane] = handler;
	}

	return e;
}

u8 csi_api_unregister_event(void *handle, csi_event_t event, u8 vc_lane)
{
	csi_error_t e = SUCCESS;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}
	/* the maximum */
    if (vc_lane < 4) {
		switch(event) {
		case ERR_PHY_TX_START:
			LOG_WARNING("the lane value depends on the synthesis defines");
			LOG_WARNING("make sure this value does not exceed these defines");
		case ERR_FRAME_BOUNDARY_MATCH:
		case ERR_FRAME_SEQUENCE:
		case ERR_CRC_DURING_FRAME:
		case ERR_LINE_CRC:
		case ERR_DOUBLE_ECC:
			e = csi_event_disable(csi_api_event_map(event, vc_lane), 1);
			break;

		case ERR_PHY_ESCAPE_ENTRY:
			LOG_WARNING("the lane value depends on the synthesis defines");
			LOG_WARNING("make sure this value does not exceed these defines");
		case ERR_ECC_SINGLE:
		case ERR_UNSUPPORTED_DATA_TYPE:
			e = csi_event_disable(csi_api_event_map(event, vc_lane), 2);
			break;

		default:
			e = ERROR_EVENT_TYPE_INVALID;
			break;

		}
    } else {
        e = ERROR_VC_LANE_OUT_OF_BOUND;
    }
    if (e == SUCCESS) {
        csi_handle->csi_api_event_registry[event + vc_lane] = NULL;
    }

    return e;
}

u8 csi_api_register_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event, handler_t handler)
{
	u8 id = 0;
	int counter = 0;
	int first_slot = -1;
	int already_registered = 0;
	u8 e = SUCCESS;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}

	if ((data_type < NULL_PACKET)
	|| ((data_type > EMBEDDED_8BIT_NON_IMAGE_DATA) && (data_type < YUV420_8BIT))
	|| ((data_type > RGB888) && (data_type < RAW6))
	|| ((data_type > RAW14) && (data_type < USER_DEFINED_8BIT_DATA_TYPE_1 ))
	|| (data_type > USER_DEFINED_8BIT_DATA_TYPE_8)) {
		/* wrong data type - not long packet */
		return ERROR_DATA_TYPE_INVALID;
	}
	/* the maximum */
	if (vc > 3) {
		return ERROR_VC_LANE_OUT_OF_BOUND;
	}
	id = (u8)((vc << 6)) | (u8)(data_type);

	for (counter = 0; counter < 8; counter++) {
		if (csi_get_registered_line_event(counter) == 0) {
			/* first empty slot to register event */
			first_slot = counter;
			break;
		}
		if (id == csi_get_registered_line_event(counter)) {
			LOG_WARNING("already registered line/datatype");
			already_registered = 1;
			first_slot = counter; /* no need to register it again */
								/* but NOT that there are no slots!*/
			break;
		}
	}
	/* if not all slots are taken */
	if (first_slot != -1) {
		/* un-mask (whether registered or not) */
		e = csi_event_enable(csi_api_event_map(line_event, first_slot), (first_slot / 4 == 1)? 2: 1);
		if (e == SUCCESS) {
			/* if not already registered */
			if (already_registered != 1) {
				e = csi_register_line_event(vc, data_type, first_slot);
				if (csi_handle->csi_api_event_registry[line_event + first_slot] != NULL) {
					LOG_WARNING("already registered event and callback (overwriting!)");
				}
			}
			/* register callback */
			csi_handle->csi_api_event_registry[line_event + first_slot] = handler;
		}
	} else {
		LOG_WARNING("all slots are taken");
		e = ERROR_SLOTS_FULL;
	}

	return e;
}

u8 csi_api_unregister_line_event(void *handle, u8 vc, csi_data_type_t data_type, csi_line_event_t line_event)
{
	u8 id = 0;
	int counter = 0;
	int replace_slot = -1;
	int last_slot = -1;
	u8 vc_new = 0;
	csi_data_type_t dt_new;
	csi_line_event_t other_line_event;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}

	if ((data_type < NULL_PACKET)
	|| ((data_type > EMBEDDED_8BIT_NON_IMAGE_DATA) && (data_type < YUV420_8BIT))
	|| ((data_type > RGB888) && (data_type < RAW6))
	|| ((data_type > RAW14) && (data_type < USER_DEFINED_8BIT_DATA_TYPE_1 ))
	|| (data_type > USER_DEFINED_8BIT_DATA_TYPE_8)) {
		/* wrong data type - not long packet */
		return ERROR_DATA_TYPE_INVALID;
	}
	/* the maximum */
	if (vc > 3) {
		return ERROR_VC_LANE_OUT_OF_BOUND;
	}
	if (line_event == ERR_LINE_BOUNDARY_MATCH) {
		other_line_event = ERR_LINE_SEQUENCE;
	} else if (line_event == ERR_LINE_SEQUENCE) {
		other_line_event = ERR_LINE_BOUNDARY_MATCH;
	} else {
		return ERROR_DATA_TYPE_INVALID;
	}
	id = (vc << 6) | data_type;
	for (counter = 0; counter < 8; counter++) {
		if (id == csi_get_registered_line_event(counter)) {
			replace_slot = counter;
		}
		if (csi_get_registered_line_event(counter) == 0) {
			last_slot = counter - 1;
			break;
		}
	}
	if (replace_slot == -1) {
		return ERROR_NOT_REGISTERED;
	}
	if (last_slot == -1) {
		last_slot = 7; /* the very last entry */
	}
	vc_new = csi_get_registered_line_event(last_slot) >> 6;
	dt_new = (csi_get_registered_line_event(last_slot) << 2) >> 2;

	if ((csi_handle->csi_api_event_registry[other_line_event + replace_slot] == NULL) && (last_slot != replace_slot)) {
		/* swap IDI with last registered if its NOT the last entry*/
		/* copy the last to (over) the one to be deleted*/
		csi_register_line_event(vc_new, dt_new, replace_slot);
		/* now that last registered is copied to a new location, unregister old location */
		csi_event_disable(csi_api_event_map(line_event, last_slot), (last_slot / 4 == 1)? 2: 1);
		/* swap event registry */
		csi_handle->csi_api_event_registry[line_event + replace_slot] = csi_handle->csi_api_event_registry[line_event + last_slot];
		/* mask last slot */
		if (csi_handle->csi_api_event_registry[other_line_event + last_slot] != NULL) {
			/* swap event registry - the other possible line event of the last slot */
			csi_handle->csi_api_event_registry[other_line_event + replace_slot] = csi_handle->csi_api_event_registry[other_line_event + last_slot];
			/* mask last slot, other line event!*/
			csi_event_disable(csi_api_event_map(other_line_event, last_slot), (last_slot / 4 == 1)? 2: 1);
			/* un mask other line event */
			csi_event_enable(csi_api_event_map(other_line_event, replace_slot), (replace_slot / 4 == 1)? 2: 1);
		}
		csi_unregister_line_event(last_slot);
	} else {
		/* remove from event registry */
		csi_handle->csi_api_event_registry[line_event + replace_slot] = NULL;
		/* mask */
		csi_event_disable(csi_api_event_map(line_event, replace_slot), (replace_slot / 4 == 1)? 2: 1);

		if ((csi_handle->csi_api_event_registry[other_line_event + replace_slot] == NULL) && (last_slot == replace_slot)) {  /* if it is last entry */
			csi_unregister_line_event(last_slot);
		}
	}

	return SUCCESS;
}

void csi_api_event1_handler(int irq, void *handle)
{
	u32 source = 0;
	unsigned long flag;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return;
	}

	source = csi_event_get_source(1);
	spin_lock_irqsave(&csi_handle->csi2_lock,flag);
	if (NULL != csi_handle->isr_cb) {
		(*csi_handle->isr_cb)(1, source, csi_handle->u_data);
	}
	spin_unlock_irqrestore(&csi_handle->csi2_lock, flag);

	return;
#if 0
	if (source > 0) {   /* map to enumerator csi_event_t or csi_line_event_t */
		if ((source & (0xf << 0)) > 0) {
			tmp = ERR_PHY_TX_START + ((source & 0xf) >> 1);
		}
		if ((source & (0xf << 4)) > 0) {
			tmp = ERR_FRAME_BOUNDARY_MATCH + ((source & 0xf0) >> 5);
		}
		if ((source & (0xf << 8)) > 0) {
			tmp = ERR_FRAME_SEQUENCE + ((source & (0xf << 8)) >> 9);
		}
		if ((source & (0xf << 12)) > 0) {
			tmp = ERR_CRC_DURING_FRAME + ((source & (0xf << 12)) >> 13);
		}
		if ((source & (0xf << 16)) > 0) {
			tmp = ERR_LINE_BOUNDARY_MATCH + ((source & (0xf << 16)) >> 17);
		}
		if ((source & (0xf << 20)) > 0) {
			tmp = ERR_LINE_SEQUENCE + ((source & (0xf << 20)) >> 21);
		}
		if ((source & (0xf << 24)) > 0) {
			tmp = ERR_LINE_CRC + ((source & (0xf << 24)) >> 25);
		}
		if ((source & (0xf << 28)) > 0) {
			tmp = ERR_DOUBLE_ECC;
		}
		/* call callback if valid */
		if (tmp != 0x80000000) {
			if (csi_api_event_registry[tmp] != NULL) {
				csi_api_event_registry[tmp](param);
			}
		}
	}
#endif
}

void csi_api_event2_handler(int irq, void *handle)
{
	u32 source = 0;
	unsigned long flag;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ;
	}

	source = csi_event_get_source(2);
	spin_lock_irqsave(&csi_handle->csi2_lock, flag);
	if (csi_handle->isr_cb) {
		(*csi_handle->isr_cb)(2, source, csi_handle->u_data);
	}
	spin_unlock_irqrestore(&csi_handle->csi2_lock, flag);

	return;

#if 0
	source = csi_event_get_source(2);

	if (source > 0) {
		/* map to enumerator csi_event_t or csi_line_event_t */
		if ((source & (0xf << 0)) > 0) {
			tmp = ERR_PHY_ESCAPE_ENTRY + ((source & 0xf) >> 1);
		}

		if ((source & (0xf << 4)) > 0) {
			; /* error discarded */
		}

		if ((source & (0xf << 8)) > 0) {
			tmp = ERR_ECC_SINGLE + ((source & (0xf << 8)) >> 9);
		}
		if ((source & (0xf << 12)) > 0) {
			tmp = ERR_UNSUPPORTED_DATA_TYPE + ((source & (0xf << 12)) >> 13);
		}
		if ((source & (0xf << 16)) > 0) {
			tmp = ERR_LINE_BOUNDARY_MATCH + 4 + ((source & (0xf << 16)) >> 17);
			/* 4 added because its in ERR2. ERR1 already has 4 */
		}
		if ((source & (0xf << 20)) > 0) {
			tmp = ERR_LINE_SEQUENCE + 4 + ((source & (0xf << 20)) >> 21);
		}
		/* call callback if valid */
		if (tmp != 0x80000000) {
			if (csi_api_event_registry[tmp] != NULL) {
				csi_api_event_registry[tmp](param);
			}
		}
	}
#endif
}

u8 csi_api_unregister_all_events(void *handle)
{
	u8 e = SUCCESS;
	u8 counter = 0;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return -1;
	}

	e = csi_event_disable(0xffffffff, 1);
	e = csi_event_disable(0xffffffff, 2);
	if (e == SUCCESS) {
		for (counter = 0; counter < 8; counter++) {
			e = csi_unregister_line_event(counter);
		}
		for (counter = (u8)(ERR_PHY_TX_START); counter < (u8)(MAX_EVENT); counter++) {
			csi_handle->csi_api_event_registry[counter] = NULL;
		}
	}

	return e;
}

u8 csi_api_shut_down_phy(u8 shutdown)
{
	return csi_shut_down_phy(shutdown);
}

u8 csi_api_reset_phy()
{
	return csi_reset_phy();
}

u8 csi_api_reset_controller()
{
	return csi_reset_controller();
}

u8 csi_api_core_write(csi_registers_t address, u32 data)
{
	return csi_core_write(address, data);
}

u32 csi_api_core_read(csi_registers_t address)
{
	return csi_core_read(address);
}

int csi_reg_isr(void *handle, csi2_isr_func user_func, void* user_data)
{
	unsigned long                flag;
	struct csi_context *csi_handle = handle;
	if (NULL == handle) {
		printk("handle null\n");
		return ERR_UNDEFINED;
	}

	spin_lock_irqsave(&csi_handle->csi2_lock, flag);
	csi_handle->isr_cb = user_func;
	csi_handle->u_data = user_data;
	spin_unlock_irqrestore(&csi_handle->csi2_lock, flag);

	return 0;
}

int csi_read_registers(u32* reg_buf, u32 *buf_len)
{
	u32                *reg_addr = (u32*)CSI2_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && reg_addr < (u64)(CSI2_BASE + SPRD_CSI2_REG_SIZE)) {
		*reg_buf++ = *(volatile u32*)reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = SPRD_CSI2_REG_SIZE;

	return 0;
}
