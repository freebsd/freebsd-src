/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_DEVICE_H
#define	VXGE_HAL_DEVICE_H

__EXTERN_BEGIN_DECLS

struct __hal_mrpcim_t;
struct __hal_srpcim_t;

/*
 * vxge_hal_vpd_data_t
 *
 * Represents vpd capabilty structure
 */
typedef struct vxge_hal_vpd_data_t {
	u8	product_name[VXGE_HAL_VPD_LEN];
	u8	serial_num[VXGE_HAL_VPD_LEN];
} vxge_hal_vpd_data_t;

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
/*
 * __hal_tracebuf_t
 *
 * HAL trace buffer object.
 */
typedef struct __hal_tracebuf_t {
	u8		*data;
	u64		wrapped_count;
	volatile u32	offset;
	u32		size;
} __hal_tracebuf_t;
#endif

/*
 * __hal_msix_map_t
 *
 * HAL msix to vpath map.
 */
typedef struct __hal_msix_map_t {
	u32	vp_id;
	u32	int_num;
} __hal_msix_map_t;

/*
 * __hal_device_t
 *
 * HAL device object. Represents X3100.
 */
typedef struct __hal_device_t {
	vxge_hal_device_t			header;
	u32					host_type;
	u32					vh_id;
	u32					func_id;
	u32					srpcim_id;
	u32					access_rights;
#define	VXGE_HAL_DEVICE_ACCESS_RIGHT_VPATH	0x1
#define	VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM	0x2
#define	VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM	0x4
	u32					ifmsg_seqno;
	u32					manager_up;
	vxge_hal_pci_config_t			pci_config_space;
	vxge_hal_pci_config_t			pci_config_space_bios;
	vxge_hal_pci_caps_offset_t		pci_caps;
	vxge_hal_pci_e_caps_offset_t		pci_e_caps;
	vxge_hal_pci_e_ext_caps_offset_t	pci_e_ext_caps;
	vxge_hal_legacy_reg_t			*legacy_reg;
	vxge_hal_toc_reg_t			*toc_reg;
	vxge_hal_common_reg_t			*common_reg;
	vxge_hal_memrepair_reg_t		*memrepair_reg;
	vxge_hal_pcicfgmgmt_reg_t
	    *pcicfgmgmt_reg[VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES];
	vxge_hal_mrpcim_reg_t			*mrpcim_reg;
	vxge_hal_srpcim_reg_t
	    *srpcim_reg[VXGE_HAL_TITAN_SRPCIM_REG_SPACES];
	vxge_hal_vpmgmt_reg_t
	    *vpmgmt_reg[VXGE_HAL_TITAN_VPMGMT_REG_SPACES];
	vxge_hal_vpath_reg_t
	    *vpath_reg[VXGE_HAL_TITAN_VPATH_REG_SPACES];
	u8					*kdfc;
	u8					*usdc;
	__hal_virtualpath_t
	    virtual_paths[VXGE_HAL_MAX_VIRTUAL_PATHS];
	u64					vpath_assignments;
	u64					vpaths_deployed;
	u32					first_vp_id;
	u64					tim_int_mask0[4];
	u32					tim_int_mask1[4];
	__hal_msix_map_t
	    msix_map[VXGE_HAL_MAX_VIRTUAL_PATHS * VXGE_HAL_VPATH_MSIX_MAX];
	struct __hal_srpcim_t			*srpcim;
	struct __hal_mrpcim_t			*mrpcim;
	__hal_blockpool_t			block_pool;
	vxge_list_t				pending_channel_list;
	spinlock_t				pending_channel_lock;
	vxge_hal_device_stats_t			stats;
	volatile u32				msix_enabled;
	volatile u32				hw_is_initialized;
	volatile int				device_resetting;
	volatile int				is_promisc;
	int					tti_enabled;
	spinlock_t				titan_post_lock;
	u32					mtu_first_time_set;
	char					*dump_buf;
#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	__hal_tracebuf_t			trace_buf;
#endif
	volatile u32				in_poll;
	u32					d_err_mask;
	u32					d_info_mask;
	u32					d_trace_mask;
} __hal_device_t;

/*
 * I2C device id. Used in I2C control register for accessing EEPROM device
 * memory.
 */
#define	VXGE_DEV_ID				5

#define	VXGE_HAL_DEVICE_MANAGER_STATE_SET(hldev, wmsg) {	\
	((__hal_device_t *)hldev)->manager_up =			\
		__hal_ifmsg_is_manager_up(wmsg);		\
}

#define	VXGE_HAL_DEVICE_LINK_STATE_SET(hldev, ls) {	\
	((vxge_hal_device_t *)hldev)->link_state = ls;	\
}

#define	VXGE_HAL_DEVICE_DATA_RATE_SET(hldev, dr) {	\
	((vxge_hal_device_t *)hldev)->data_rate = dr;	\
}

#define	VXGE_HAL_DEVICE_TIM_INT_MASK_SET(hldev, i) {			\
	if (i < 16) {							\
	    ((__hal_device_t *)hldev)->tim_int_mask0[0] |=		\
						vBIT(0x8, (i*4), 4);	\
	    ((__hal_device_t *)hldev)->tim_int_mask0[1] |=		\
						vBIT(0x4, (i*4), 4);	\
	    ((__hal_device_t *)hldev)->tim_int_mask0[3] |=		\
						vBIT(0x1, (i*4), 4);	\
	} else {							\
	    ((__hal_device_t *)hldev)->tim_int_mask1[0] = 0x80000000;	\
	    ((__hal_device_t *)hldev)->tim_int_mask1[1] = 0x40000000;	\
	    ((__hal_device_t *)hldev)->tim_int_mask1[3] = 0x10000000;	\
	}								\
}

#define	VXGE_HAL_DEVICE_TIM_INT_MASK_RESET(hldev, i) {			\
	if (i < 16) {							\
	    ((__hal_device_t *)hldev)->tim_int_mask0[0] &=		\
						~vBIT(0x8, (i*4), 4);	\
	    ((__hal_device_t *)hldev)->tim_int_mask0[1] &=		\
						~vBIT(0x4, (i*4), 4);	\
	    ((__hal_device_t *)hldev)->tim_int_mask0[3] &=		\
						~vBIT(0x1, (i*4), 4);	\
	} else {							\
	    ((__hal_device_t *)hldev)->tim_int_mask1[0] = 0;		\
	    ((__hal_device_t *)hldev)->tim_int_mask1[1] = 0;		\
	    ((__hal_device_t *)hldev)->tim_int_mask1[3] = 0;		\
	}								\
}

/* ========================== PRIVATE API ================================= */

void
vxge_hal_pio_mem_write32_upper(pci_dev_h pdev,
    pci_reg_h regh,
    u32 val,
    void *addr);

void
vxge_hal_pio_mem_write32_lower(pci_dev_h pdev,
    pci_reg_h regh,
    u32 val,
    void *addr);

void
__hal_device_event_queued(void *data,
    u32 event_type);

void
__hal_device_pci_caps_list_process(__hal_device_t *hldev);

void
__hal_device_pci_e_init(__hal_device_t *hldev);

vxge_hal_status_e
vxge_hal_device_register_poll(pci_dev_h pdev,
    pci_reg_h regh,
    u64 *reg,
    u32 op,
    u64 mask,
    u32 max_millis);

vxge_hal_status_e
__hal_device_register_stall(pci_dev_h pdev,
    pci_reg_h regh,
    u64 *reg,
    u32 op,
    u64 mask,
    u32 max_millis);

vxge_hal_status_e
__hal_device_reg_addr_get(__hal_device_t *hldev);

void
__hal_device_id_get(__hal_device_t *hldev);

u32
__hal_device_access_rights_get(u32 host_type, u32 func_id);

void
__hal_device_host_info_get(__hal_device_t *hldev);

vxge_hal_status_e
__hal_device_hw_initialize(__hal_device_t *hldev);

vxge_hal_status_e
__hal_device_reset(__hal_device_t *hldev);

vxge_hal_status_e
__hal_device_handle_link_up_ind(__hal_device_t *hldev);

vxge_hal_status_e
__hal_device_handle_link_down_ind(__hal_device_t *hldev);

void
__hal_device_handle_error(
    __hal_device_t *hldev,
    u32 vp_id,
    vxge_hal_event_e type);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_DEVICE_H */
