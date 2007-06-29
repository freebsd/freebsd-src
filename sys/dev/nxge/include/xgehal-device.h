/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 *  FileName :    xgehal-device.h
 *
 *  Description:  HAL device object functionality
 *
 *  Created:      14 May 2004
 */

#ifndef XGE_HAL_DEVICE_H
#define XGE_HAL_DEVICE_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xge-queue.h>
#include <dev/nxge/include/xgehal-event.h>
#include <dev/nxge/include/xgehal-config.h>
#include <dev/nxge/include/xgehal-regs.h>
#include <dev/nxge/include/xgehal-channel.h>
#include <dev/nxge/include/xgehal-stats.h>
#include <dev/nxge/include/xgehal-ring.h>
#ifdef XGEHAL_RNIC
#include "xgehal-common-regs.h"
#include "xgehal-pcicfg-mgmt-regs.h"
#include "xgehal-mrpcim-regs.h"
#include "xgehal-srpcim-regs.h"
#include "xgehal-vpath-regs.h"
#include "xgehal-bitmap.h"
#include "xgehal-virtualpath.h"
#include "xgehal-lbwrapper.h"
#include "xgehal-blockpool.h"
#include "xgehal-regpool.h"
#endif

__EXTERN_BEGIN_DECLS

#define XGE_HAL_VPD_LENGTH                              80
#define XGE_HAL_CARD_XENA_VPD_ADDR                      0x50
#define XGE_HAL_CARD_HERC_VPD_ADDR                      0x80
#define XGE_HAL_VPD_READ_COMPLETE                       0x80
#define XGE_HAL_VPD_BUFFER_SIZE                         128
#define XGE_HAL_DEVICE_XMSI_WAIT_MAX_MILLIS		500
#define XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS		500
#define XGE_HAL_DEVICE_QUIESCENT_WAIT_MAX_MILLIS	500
#define XGE_HAL_DEVICE_FAULT_WAIT_MAX_MILLIS		50
#define XGE_HAL_DEVICE_RESET_WAIT_MAX_MILLIS		250
#define XGE_HAL_DEVICE_SPDM_READY_WAIT_MAX_MILLIS	250  /* TODO */

#define XGE_HAL_MAGIC					0x12345678
#define XGE_HAL_DEAD					0xDEADDEAD
#define XGE_HAL_DUMP_BUF_SIZE                           0x4000

#define XGE_HAL_LRO_MAX_BUCKETS				32

/**
 * enum xge_hal_card_e - Xframe adapter type.
 * @XGE_HAL_CARD_UNKNOWN: Unknown device.
 * @XGE_HAL_CARD_XENA: Xframe I device.
 * @XGE_HAL_CARD_HERC: Xframe II (PCI-266Mhz) device.
 * @XGE_HAL_CARD_TITAN: Xframe ER (PCI-266Mhz) device.
 *
 * Enumerates Xframe adapter types. The corresponding PCI device
 * IDs are listed in the file xgehal-defs.h.
 * (See XGE_PCI_DEVICE_ID_XENA_1, etc.)
 *
 * See also: xge_hal_device_check_id().
 */
typedef enum xge_hal_card_e {
	XGE_HAL_CARD_UNKNOWN	= 0,
	XGE_HAL_CARD_XENA	= 1,
	XGE_HAL_CARD_HERC	= 2,
	XGE_HAL_CARD_TITAN	= 3,
} xge_hal_card_e;

/**
 * struct xge_hal_device_attr_t - Device memory spaces.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *         (Linux and the rest.)
 * @regh1: BAR1 mapped memory handle. Same comment as above.
 * @bar0: BAR0 virtual address.
 * @bar1: BAR1 virtual address.
 * @irqh: IRQ handle (Solaris).
 * @cfgh: Configuration space handle (Solaris), or PCI device @pdev (Linux).
 * @pdev: PCI device object.
 *
 * Device memory spaces. Includes configuration, BAR0, BAR1, etc. per device
 * mapped memories. Also, includes a pointer to OS-specific PCI device object.
 */
typedef struct xge_hal_device_attr_t {
	pci_reg_h		regh0;
	pci_reg_h		regh1;
	pci_reg_h		regh2;
	char			*bar0;
	char			*bar1;
	char			*bar2;
	pci_irq_h		irqh;
	pci_cfg_h		cfgh;
	pci_dev_h		pdev;
} xge_hal_device_attr_t;

/**
 * enum xge_hal_device_link_state_e - Link state enumeration.
 * @XGE_HAL_LINK_NONE: Invalid link state.
 * @XGE_HAL_LINK_DOWN: Link is down.
 * @XGE_HAL_LINK_UP: Link is up.
 *
 */
typedef enum xge_hal_device_link_state_e {
	XGE_HAL_LINK_NONE,
	XGE_HAL_LINK_DOWN,
	XGE_HAL_LINK_UP
} xge_hal_device_link_state_e;


/**
 * enum xge_hal_pci_mode_e - PIC bus speed and mode specific enumeration.
 * @XGE_HAL_PCI_33MHZ_MODE:		33 MHZ pci mode.
 * @XGE_HAL_PCI_66MHZ_MODE:		66 MHZ pci mode.
 * @XGE_HAL_PCIX_M1_66MHZ_MODE:		PCIX M1 66MHZ mode.
 * @XGE_HAL_PCIX_M1_100MHZ_MODE:	PCIX M1 100MHZ mode.
 * @XGE_HAL_PCIX_M1_133MHZ_MODE:	PCIX M1 133MHZ mode.
 * @XGE_HAL_PCIX_M2_66MHZ_MODE:		PCIX M2 66MHZ mode.
 * @XGE_HAL_PCIX_M2_100MHZ_MODE:	PCIX M2 100MHZ mode.
 * @XGE_HAL_PCIX_M2_133MHZ_MODE:	PCIX M3 133MHZ mode.
 * @XGE_HAL_PCIX_M1_RESERVED:		PCIX M1 reserved mode.
 * @XGE_HAL_PCIX_M1_66MHZ_NS:		PCIX M1 66MHZ mode not supported.
 * @XGE_HAL_PCIX_M1_100MHZ_NS:		PCIX M1 100MHZ mode not supported.
 * @XGE_HAL_PCIX_M1_133MHZ_NS:		PCIX M1 133MHZ not supported.
 * @XGE_HAL_PCIX_M2_RESERVED:		PCIX M2 reserved.
 * @XGE_HAL_PCIX_533_RESERVED:		PCIX 533 reserved.
 * @XGE_HAL_PCI_BASIC_MODE:		PCI basic mode, XENA specific value.
 * @XGE_HAL_PCIX_BASIC_MODE:		PCIX basic mode, XENA specific value.
 * @XGE_HAL_PCI_INVALID_MODE:		Invalid PCI or PCIX mode.
 *
 */
typedef enum xge_hal_pci_mode_e {
	XGE_HAL_PCI_33MHZ_MODE		= 0x0,
	XGE_HAL_PCI_66MHZ_MODE		= 0x1,
	XGE_HAL_PCIX_M1_66MHZ_MODE	= 0x2,
	XGE_HAL_PCIX_M1_100MHZ_MODE	= 0x3,
	XGE_HAL_PCIX_M1_133MHZ_MODE	= 0x4,
	XGE_HAL_PCIX_M2_66MHZ_MODE	= 0x5,
	XGE_HAL_PCIX_M2_100MHZ_MODE	= 0x6,
	XGE_HAL_PCIX_M2_133MHZ_MODE	= 0x7,
	XGE_HAL_PCIX_M1_RESERVED	= 0x8,
	XGE_HAL_PCIX_M1_66MHZ_NS	= 0xA,
	XGE_HAL_PCIX_M1_100MHZ_NS	= 0xB,
	XGE_HAL_PCIX_M1_133MHZ_NS	= 0xC,
	XGE_HAL_PCIX_M2_RESERVED	= 0xD,
	XGE_HAL_PCIX_533_RESERVED	= 0xE,
	XGE_HAL_PCI_BASIC_MODE		= 0x10,
	XGE_HAL_PCIX_BASIC_MODE		= 0x11,
	XGE_HAL_PCI_INVALID_MODE	= 0x12,
} xge_hal_pci_mode_e;

/**
 * enum xge_hal_pci_bus_frequency_e - PCI bus frequency enumeration.
 * @XGE_HAL_PCI_BUS_FREQUENCY_33MHZ:	PCI bus frequency 33MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_66MHZ:	PCI bus frequency 66MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_100MHZ:	PCI bus frequency 100MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_133MHZ:	PCI bus frequency 133MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_200MHZ:	PCI bus frequency 200MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_250MHZ:	PCI bus frequency 250MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_266MHZ:	PCI bus frequency 266MHZ
 * @XGE_HAL_PCI_BUS_FREQUENCY_UNKNOWN:	Unrecognized PCI bus frequency value.
 *
 */
typedef enum xge_hal_pci_bus_frequency_e {
	XGE_HAL_PCI_BUS_FREQUENCY_33MHZ		= 33,
	XGE_HAL_PCI_BUS_FREQUENCY_66MHZ		= 66,
	XGE_HAL_PCI_BUS_FREQUENCY_100MHZ	= 100,
	XGE_HAL_PCI_BUS_FREQUENCY_133MHZ	= 133,
	XGE_HAL_PCI_BUS_FREQUENCY_200MHZ	= 200,
	XGE_HAL_PCI_BUS_FREQUENCY_250MHZ	= 250,
	XGE_HAL_PCI_BUS_FREQUENCY_266MHZ	= 266,
	XGE_HAL_PCI_BUS_FREQUENCY_UNKNOWN	= 0
} xge_hal_pci_bus_frequency_e;

/**
 * enum xge_hal_pci_bus_width_e - PCI bus width enumeration.
 * @XGE_HAL_PCI_BUS_WIDTH_64BIT:	64 bit bus width.
 * @XGE_HAL_PCI_BUS_WIDTH_32BIT:	32 bit bus width.
 * @XGE_HAL_PCI_BUS_WIDTH_UNKNOWN:  unknown bus width.
 *
 */
typedef enum xge_hal_pci_bus_width_e {
	XGE_HAL_PCI_BUS_WIDTH_64BIT	= 0,
	XGE_HAL_PCI_BUS_WIDTH_32BIT	= 1,
	XGE_HAL_PCI_BUS_WIDTH_UNKNOWN	= 2,
} xge_hal_pci_bus_width_e;

#if defined (XGE_HAL_CONFIG_LRO)

#define IP_TOTAL_LENGTH_OFFSET			2
#define IP_FAST_PATH_HDR_MASK			0x45
#define TCP_FAST_PATH_HDR_MASK1			0x50
#define TCP_FAST_PATH_HDR_MASK2			0x10
#define TCP_FAST_PATH_HDR_MASK3			0x18
#define IP_SOURCE_ADDRESS_OFFSET		12
#define IP_DESTINATION_ADDRESS_OFFSET		16
#define TCP_DESTINATION_PORT_OFFSET		2
#define TCP_SOURCE_PORT_OFFSET			0
#define TCP_DATA_OFFSET_OFFSET			12
#define TCP_WINDOW_OFFSET			14
#define TCP_SEQUENCE_NUMBER_OFFSET		4
#define TCP_ACKNOWLEDGEMENT_NUMBER_OFFSET	8

typedef struct tcplro {
	u16   source;
	u16   dest;
	u32   seq;
	u32   ack_seq;
	u8    doff_res;
	u8    ctrl;
	u16   window;
	u16   check;
	u16   urg_ptr;
} tcplro_t;

typedef struct iplro {
	u8    version_ihl;
	u8    tos;
	u16   tot_len;
	u16   id;
	u16   frag_off;
	u8    ttl;
	u8    protocol;
	u16   check;
	u32   saddr;
	u32   daddr;
	/*The options start here. */
} iplro_t;

/*
 * LRO object, one per each LRO session.
*/
typedef struct lro {
	/* non-linear: contains scatter-gather list of
	xframe-mapped received buffers */
	OS_NETSTACK_BUF		os_buf;
	OS_NETSTACK_BUF		os_buf_end;

	/* link layer header of the first frame;
	remains intack throughout the processing */
	u8			*ll_hdr;

	/* IP header - gets _collapsed_ */
	iplro_t			*ip_hdr;

	/* transport header - gets _collapsed_ */
	tcplro_t		*tcp_hdr;

	/* Next tcp sequence number */
	u32			tcp_next_seq_num;
	/* Current tcp seq & ack */
	u32			tcp_seq_num;
	u32			tcp_ack_num;

	/* total number of accumulated (so far) frames */
	int			sg_num;

	/* total data length */
	int			total_length;

	/* receive side hash value, available from Hercules */
	u32			rth_value;

	/* In use */
	u8			in_use;

	/* Total length of the fragments clubbed with the inital frame */
	u32			frags_len;

	/* LRO frame contains time stamp, if (ts_off != -1) */
	int 			ts_off;
		
} lro_t;
#endif

/*
 * xge_hal_spdm_entry_t
 *
 * Represents a single spdm entry in the SPDM table.
 */
typedef struct xge_hal_spdm_entry_t {
	xge_hal_ipaddr_t  src_ip;
	xge_hal_ipaddr_t  dst_ip;
	u32 jhash_value;
	u16 l4_sp;
	u16 l4_dp;
	u16 spdm_entry;
	u8  in_use;
	u8  is_tcp;
	u8  is_ipv4;
	u8  tgt_queue;
} xge_hal_spdm_entry_t;

#if defined(XGE_HAL_CONFIG_LRO)
typedef struct {
	lro_t			lro_pool[XGE_HAL_LRO_MAX_BUCKETS];
	int			lro_next_idx;
	lro_t			*lro_recent;
} xge_hal_lro_desc_t;
#endif
/*
 * xge_hal_vpd_data_t
 * 
 * Represents vpd capabilty structure
 */
typedef struct xge_hal_vpd_data_t {
        u8      product_name[XGE_HAL_VPD_LENGTH];
        u8      serial_num[XGE_HAL_VPD_LENGTH];
} xge_hal_vpd_data_t;

/*
 * xge_hal_device_t
 *
 * HAL device object. Represents Xframe.
 */
typedef struct {
	unsigned int		magic;
	pci_reg_h		regh0;
	pci_reg_h		regh1;
	pci_reg_h		regh2;
	char			*bar0;
	char			*isrbar0;
	char			*bar1;
	char			*bar2;
	pci_irq_h		irqh;
	pci_cfg_h		cfgh;
	pci_dev_h		pdev;
	xge_hal_pci_config_t	pci_config_space;
	xge_hal_pci_config_t	pci_config_space_bios;
	xge_hal_device_config_t	config;
	xge_list_t		free_channels;
	xge_list_t		fifo_channels;
	xge_list_t		ring_channels;
#ifdef XGEHAL_RNIC
	__hal_bitmap_entry_t	bitmap_table[XGE_HAL_MAX_BITMAP_BITS];
	__hal_virtualpath_t	virtual_paths[XGE_HAL_MAX_VIRTUAL_PATHS];
	__hal_blockpool_t	block_pool;
	__hal_regpool_t		reg_pool;
#endif
	volatile int		is_initialized;
	volatile int		terminating;
	xge_hal_stats_t		stats;
	macaddr_t		macaddr[1];
	xge_queue_h		queueh;
	volatile int		mcast_refcnt;
	int			is_promisc;
	volatile xge_hal_device_link_state_e	link_state;
	void			*upper_layer_info;
	xge_hal_device_attr_t	orig_attr;
	u16			device_id;
	u8			revision;
	int			msi_enabled;
	int			hw_is_initialized;
	u64			inject_serr;
	u64			inject_ecc;
	u8			inject_bad_tcode;
	int			inject_bad_tcode_for_chan_type;
        int                     reset_needed_after_close;
	int			tti_enabled;
	xge_hal_tti_config_t	bimodal_tti[XGE_HAL_MAX_RING_NUM];
	int			bimodal_timer_val_us;
	int			bimodal_urange_a_en;
	int			bimodal_intr_cnt;
	char			*spdm_mem_base;
	u16			spdm_max_entries;
	xge_hal_spdm_entry_t	**spdm_table;
	spinlock_t		spdm_lock;
	u32			msi_mask;
#if defined(XGE_HAL_CONFIG_LRO)
        xge_hal_lro_desc_t      lro_desc[XGE_HAL_MAX_RING_NUM];
#endif
	spinlock_t		xena_post_lock;

	/* bimodal workload stats */
	int			irq_workload_rxd[XGE_HAL_MAX_RING_NUM];
	int			irq_workload_rxcnt[XGE_HAL_MAX_RING_NUM];
	int			irq_workload_rxlen[XGE_HAL_MAX_RING_NUM];
	int			irq_workload_txd[XGE_HAL_MAX_FIFO_NUM];
	int			irq_workload_txcnt[XGE_HAL_MAX_FIFO_NUM];
	int			irq_workload_txlen[XGE_HAL_MAX_FIFO_NUM];

	int			mtu_first_time_set;
	u64			rxufca_lbolt;
	u64			rxufca_lbolt_time;
	u64			rxufca_intr_thres;
	char*                   dump_buf;
	xge_hal_pci_mode_e	pci_mode;
	xge_hal_pci_bus_frequency_e bus_frequency;
	xge_hal_pci_bus_width_e	bus_width;
	xge_hal_vpd_data_t      vpd_data;
	volatile int		in_poll;
	u64			msix_vector_table[XGE_HAL_MAX_MSIX_MESSAGES_WITH_ADDR];
} xge_hal_device_t;


/* ========================== PRIVATE API ================================= */

void
__hal_device_event_queued(void *data, int event_type);

xge_hal_status_e
__hal_device_set_swapper(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_rth_it_configure(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_rth_spdm_configure(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_verify_pcc_idle(xge_hal_device_t *hldev, u64 adp_status);

xge_hal_status_e
__hal_device_handle_pic(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_read_spdm_entry_line(xge_hal_device_t *hldev, u8 spdm_line,
                        u16 spdm_entry, u64 *spdm_line_val);

void __hal_pio_mem_write32_upper(pci_dev_h pdev, pci_reg_h regh, u32 val,
			void *addr);

void __hal_pio_mem_write32_lower(pci_dev_h pdev, pci_reg_h regh, u32 val,
			void *addr);
void __hal_device_get_vpd_data(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_handle_txpic(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_txdma(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_txmac(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_txxgxs(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_rxpic(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_rxdma(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_rxmac(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_rxxgxs(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_handle_mc(xge_hal_device_t *hldev, u64 reason);

xge_hal_status_e
__hal_device_register_poll(xge_hal_device_t *hldev, u64 *reg, int op, u64 mask,
			int max_millis);
xge_hal_status_e
__hal_device_rts_mac_configure(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_rts_qos_configure(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_rts_port_configure(xge_hal_device_t *hldev);

xge_hal_status_e
__hal_device_rti_configure(xge_hal_device_t *hldev, int runtime);

void
__hal_device_msi_intr_endis(xge_hal_device_t *hldev, int flag);

void
__hal_device_msix_intr_endis(xge_hal_device_t *hldev,
			      xge_hal_channel_t *channel, int flag);

/* =========================== PUBLIC API ================================= */

unsigned int
__hal_fix_time_ival_herc(xge_hal_device_t *hldev,
			 unsigned int time_ival);
xge_hal_status_e
xge_hal_rts_rth_itable_set(xge_hal_device_t *hldev, u8 *itable,
		u32 itable_size);

void
xge_hal_rts_rth_set(xge_hal_device_t *hldev, u8 def_q, u64 hash_type,
		u16 bucket_size);

void
xge_hal_rts_rth_init(xge_hal_device_t *hldev);

void
xge_hal_rts_rth_clr(xge_hal_device_t *hldev);

void
xge_hal_rts_rth_start(xge_hal_device_t *hldev);

void
xge_hal_rts_rth_stop(xge_hal_device_t *hldev);

void
xge_hal_device_rts_rth_key_set(xge_hal_device_t *hldev, u8 KeySize, u8 *Key);

xge_hal_status_e
xge_hal_device_rts_mac_enable(xge_hal_device_h devh, int index, macaddr_t macaddr);

xge_hal_status_e
xge_hal_device_rts_mac_disable(xge_hal_device_h devh, int index);

int xge_hal_reinitialize_hw(xge_hal_device_t * hldev);

/**
 * xge_hal_device_rti_reconfigure
 * @hldev: Hal Device
 */
static inline xge_hal_status_e
xge_hal_device_rti_reconfigure(xge_hal_device_t *hldev)
{
	return __hal_device_rti_configure(hldev, 1);
}

/**
 * xge_hal_device_rts_port_reconfigure
 * @hldev: Hal Device
 */
static inline xge_hal_status_e
xge_hal_device_rts_port_reconfigure(xge_hal_device_t *hldev)
{
	return __hal_device_rts_port_configure(hldev);
}

/**
 * xge_hal_device_is_initialized - Returns 0 if device is not
 * initialized, non-zero otherwise.
 * @devh: HAL device handle.
 *
 * Returns 0 if device is not initialized, non-zero otherwise.
 */
static inline int
xge_hal_device_is_initialized(xge_hal_device_h devh)
{
	return ((xge_hal_device_t*)devh)->is_initialized;
}


/**
 * xge_hal_device_in_poll - non-zero, if xge_hal_device_poll() is executing.
 * @devh: HAL device handle.
 *
 * Returns non-zero if xge_hal_device_poll() is executing, and 0 - otherwise.
 */
static inline int
xge_hal_device_in_poll(xge_hal_device_h devh)
{
	return ((xge_hal_device_t*)devh)->in_poll;
}


/**
 * xge_hal_device_inject_ecc - Inject ECC error.
 * @devh: HAL device, pointer to xge_hal_device_t structure.
 * @err_reg: Contains the error register.
 *
 * This function is used to inject ECC error into the driver flow.
 * This facility can be used to test the driver flow in the
 * case of ECC error is reported by the firmware.
 *
 * Returns: void
 * See also: xge_hal_device_inject_serr(),
 * xge_hal_device_inject_bad_tcode()
 */
static inline void
xge_hal_device_inject_ecc(xge_hal_device_h devh, u64 err_reg)
{
        ((xge_hal_device_t*)devh)->inject_ecc = err_reg;
}


/**
 * xge_hal_device_inject_serr - Inject SERR error.
 * @devh: HAL device, pointer to xge_hal_device_t structure.
 * @err_reg: Contains the error register.
 *
 * This function is used to inject SERR error into the driver flow.
 * This facility can be used to test the driver flow in the
 * case of SERR error is reported by firmware.
 *
 * Returns: void
 * See also: xge_hal_device_inject_ecc(),
 * xge_hal_device_inject_bad_tcode()
 */
static inline void
xge_hal_device_inject_serr(xge_hal_device_h devh, u64 err_reg)
{
        ((xge_hal_device_t*)devh)->inject_serr = err_reg;
}


/**
 * xge_hal_device_inject_bad_tcode - Inject  Bad transfer code.
 * @devh: HAL device, pointer to xge_hal_device_t structure.
 * @chan_type: Channel type (fifo/ring).
 * @t_code: Transfer code.
 *
 * This function is used to inject bad (Tx/Rx Data)transfer code
 * into the driver flow.
 *
 * This facility can be used to test the driver flow in the
 * case of bad transfer code reported by firmware for a Tx/Rx data
 * transfer.
 *
 * Returns: void
 * See also: xge_hal_device_inject_ecc(), xge_hal_device_inject_serr()
 */
static inline void
xge_hal_device_inject_bad_tcode(xge_hal_device_h devh, int chan_type, u8 t_code)
{
        ((xge_hal_device_t*)devh)->inject_bad_tcode_for_chan_type = chan_type;
        ((xge_hal_device_t*)devh)->inject_bad_tcode = t_code;
}

void xge_hal_device_msi_enable(xge_hal_device_h	devh);

/*
 * xge_hal_device_msi_mode - Is MSI enabled?
 * @devh: HAL device handle.
 *
 * Returns 0 if MSI is enabled for the specified device,
 * non-zero otherwise.
 */
static inline int
xge_hal_device_msi_mode(xge_hal_device_h devh)
{
	return ((xge_hal_device_t*)devh)->msi_enabled;
}

/**
 * xge_hal_device_queue - Get per-device event queue.
 * @devh: HAL device handle.
 *
 * Returns: event queue associated with the specified HAL device.
 */
static inline xge_queue_h
xge_hal_device_queue (xge_hal_device_h devh)
{
	return ((xge_hal_device_t*)devh)->queueh;
}

/**
 * xge_hal_device_attr - Get original (user-specified) device
 * attributes.
 * @devh: HAL device handle.
 *
 * Returns: original (user-specified) device attributes.
 */
static inline xge_hal_device_attr_t*
xge_hal_device_attr(xge_hal_device_h devh)
{
	return &((xge_hal_device_t*)devh)->orig_attr;
}

/**
 * xge_hal_device_private_set - Set ULD context.
 * @devh: HAL device handle.
 * @data: pointer to ULD context
 *
 * Use HAL device to set upper-layer driver (ULD) context.
 *
 * See also: xge_hal_device_from_private(), xge_hal_device_private()
 */
static inline void
xge_hal_device_private_set(xge_hal_device_h devh, void *data)
{
	((xge_hal_device_t*)devh)->upper_layer_info = data;
}

/**
 * xge_hal_device_private - Get ULD context.
 * @devh: HAL device handle.
 *
 * Use HAL device to get upper-layer driver (ULD) context.
 *
 * Returns:  ULD context.
 *
 * See also: xge_hal_device_from_private(), xge_hal_device_private_set()
 */
static inline void*
xge_hal_device_private(xge_hal_device_h devh)
{
	return ((xge_hal_device_t*)devh)->upper_layer_info;
}

/**
 * xge_hal_device_from_private - Get HAL device object from private.
 * @info_ptr: ULD context.
 *
 * Use ULD context to get HAL device.
 *
 * Returns:  Device handle.
 *
 * See also: xge_hal_device_private(), xge_hal_device_private_set()
 */
static inline xge_hal_device_h
xge_hal_device_from_private(void *info_ptr)
{
	return xge_container_of((void ** ) info_ptr, xge_hal_device_t,
	upper_layer_info);
}

/**
 * xge_hal_device_mtu_check - check MTU value for ranges
 * @hldev: the device
 * @new_mtu: new MTU value to check
 *
 * Will do sanity check for new MTU value.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_MTU_SIZE - MTU is invalid.
 *
 * See also: xge_hal_device_mtu_set()
 */
static inline xge_hal_status_e
xge_hal_device_mtu_check(xge_hal_device_t *hldev, int new_mtu)
{
	if ((new_mtu < XGE_HAL_MIN_MTU) || (new_mtu > XGE_HAL_MAX_MTU)) {
		return XGE_HAL_ERR_INVALID_MTU_SIZE;
	}

	return XGE_HAL_OK;
}

void xge_hal_device_bcast_enable(xge_hal_device_h devh);

void xge_hal_device_bcast_disable(xge_hal_device_h devh);

void xge_hal_device_terminating(xge_hal_device_h devh);

xge_hal_status_e xge_hal_device_initialize(xge_hal_device_t *hldev,
		xge_hal_device_attr_t *attr, xge_hal_device_config_t *config);

void xge_hal_device_terminate(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_reset(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_macaddr_get(xge_hal_device_t *hldev,
		int index,  macaddr_t *macaddr);

xge_hal_status_e xge_hal_device_macaddr_set(xge_hal_device_t *hldev,
		int index,  macaddr_t macaddr);

xge_hal_status_e xge_hal_device_macaddr_clear(xge_hal_device_t *hldev,
		int index);

int xge_hal_device_macaddr_find(xge_hal_device_t *hldev, macaddr_t wanted);

xge_hal_status_e xge_hal_device_mtu_set(xge_hal_device_t *hldev, int new_mtu);

xge_hal_status_e xge_hal_device_status(xge_hal_device_t *hldev, u64 *hw_status);

void xge_hal_device_intr_enable(xge_hal_device_t *hldev);

void xge_hal_device_intr_disable(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_mcast_enable(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_mcast_disable(xge_hal_device_t *hldev);

void xge_hal_device_promisc_enable(xge_hal_device_t *hldev);

void xge_hal_device_promisc_disable(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_disable(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_enable(xge_hal_device_t *hldev);

xge_hal_status_e xge_hal_device_handle_tcode(xge_hal_channel_h channelh,
					     xge_hal_dtr_h dtrh,
					     u8 t_code);

xge_hal_status_e xge_hal_device_link_state(xge_hal_device_h devh,
			xge_hal_device_link_state_e *ls);

void xge_hal_device_sched_timer(xge_hal_device_h devh, int interval_us,
			int one_shot);

void xge_hal_device_poll(xge_hal_device_h devh);

xge_hal_card_e xge_hal_device_check_id(xge_hal_device_h devh);

int xge_hal_device_is_slot_freeze(xge_hal_device_h devh);

xge_hal_status_e
xge_hal_device_pci_info_get(xge_hal_device_h devh, xge_hal_pci_mode_e *pci_mode,
			xge_hal_pci_bus_frequency_e *bus_frequency,
			xge_hal_pci_bus_width_e *bus_width);

xge_hal_status_e
xge_hal_spdm_entry_add(xge_hal_device_h devh, xge_hal_ipaddr_t *src_ip,
			xge_hal_ipaddr_t *dst_ip, u16 l4_sp, u16 l4_dp,
			u8 is_tcp, u8 is_ipv4, u8 tgt_queue);

xge_hal_status_e
xge_hal_spdm_entry_remove(xge_hal_device_h devh, xge_hal_ipaddr_t *src_ip,
			xge_hal_ipaddr_t *dst_ip, u16 l4_sp, u16 l4_dp,
			u8 is_tcp, u8 is_ipv4);

xge_hal_status_e
xge_hal_device_rts_section_enable(xge_hal_device_h devh, int index);

int
xge_hal_device_is_closed (xge_hal_device_h devh);

/* private functions, don't use them in ULD */

void __hal_serial_mem_write64(xge_hal_device_t *hldev, u64 value, u64 *reg);

u64 __hal_serial_mem_read64(xge_hal_device_t *hldev, u64 *reg);


/* Some function protoypes for MSI implementation. */
xge_hal_status_e
xge_hal_channel_msi_set (xge_hal_channel_h channelh, int msi,
			 u32 msg_val);
void
xge_hal_mask_msi(xge_hal_device_t *hldev);

void
xge_hal_unmask_msi(xge_hal_channel_h channelh);

xge_hal_status_e
xge_hal_channel_msix_set(xge_hal_channel_h channelh, int msix_idx);

xge_hal_status_e
xge_hal_mask_msix(xge_hal_device_h devh, int msi_id);

xge_hal_status_e
xge_hal_unmask_msix(xge_hal_device_h devh, int msi_id);

#if defined(XGE_HAL_CONFIG_LRO)
xge_hal_status_e
xge_hal_lro_init(u32 lro_scale, xge_hal_device_t *hldev);
#endif

#if defined(XGE_DEBUG_FP) && (XGE_DEBUG_FP & XGE_DEBUG_FP_DEVICE)
#define __HAL_STATIC_DEVICE
#define __HAL_INLINE_DEVICE

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE int
xge_hal_device_rev(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_begin_irq(xge_hal_device_t *hldev, u64 *reason);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_clear_rx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_clear_tx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_continue_irq(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_handle_irq(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_bar0(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_isrbar0(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_bar1(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_bar0_set(xge_hal_device_t *hldev, char *bar0);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_isrbar0_set(xge_hal_device_t *hldev, char *isrbar0);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_bar1_set(xge_hal_device_t *hldev, xge_hal_channel_h channelh,
		char *bar1);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_tx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_rx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_all(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_tx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_rx(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_all(xge_hal_device_t *hldev);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_tx_channels(xge_hal_device_t *hldev, int *got_tx);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_rx_channels(xge_hal_device_t *hldev, int *got_rx);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_rx_channel(xge_hal_channel_t *channel, int *got_rx);

__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_tx_channel(xge_hal_channel_t *channel, int *got_tx);

#if defined (XGE_HAL_CONFIG_LRO)
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL u8
__hal_header_parse_token_u8(u8 *string,u16 offset);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL u16
__hal_header_parse_token_u16(u8 *string,u16 offset);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL u32
__hal_header_parse_token_u32(u8 *string,u16 offset);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_header_update_u8(u8 *string, u16 offset, u8 val);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_header_update_u16(u8 *string, u16 offset, u16 val);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_header_update_u32(u8 *string, u16 offset, u32 val);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL u16
__hal_tcp_seg_len(iplro_t *ip, tcplro_t *tcp);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_ip_lro_capable(iplro_t *ip, xge_hal_dtr_info_t *ext_info);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_tcp_lro_capable(iplro_t *ip, tcplro_t *tcp, lro_t *lro, int *ts_off);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_lro_capable(u8 *buffer, iplro_t **ip, tcplro_t **tcp,
		xge_hal_dtr_info_t *ext_info);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_get_lro_session(u8 *eth_hdr, iplro_t *ip, tcplro_t *tcp, lro_t **lro,
		xge_hal_dtr_info_t *ext_info, xge_hal_device_t *hldev,
		xge_hal_lro_desc_t *ring_lro, lro_t **lro_end3);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_lro_under_optimal_thresh(iplro_t *ip, tcplro_t *tcp, lro_t *lro,
		xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_collapse_ip_hdr(iplro_t *ip, tcplro_t *tcp, lro_t *lro,
		xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_collapse_tcp_hdr(iplro_t *ip, tcplro_t *tcp, lro_t *lro,
		xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_append_lro(iplro_t *ip, tcplro_t **tcp, u32 *seg_len, lro_t *lro,
		xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
xge_hal_lro_process_rx(int ring, u8 *eth_hdr, u8 *ip_hdr, tcplro_t **tcp, 
                       u32 *seglen, lro_t **p_lro,
                       xge_hal_dtr_info_t *ext_info, xge_hal_device_t *hldev,
                       lro_t **lro_end3);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
xge_hal_accumulate_large_rx(u8 *buffer, tcplro_t **tcp, u32 *seglen,
		lro_t **lro, xge_hal_dtr_info_t *ext_info,
		xge_hal_device_t *hldev, lro_t **lro_end3);

void
xge_hal_lro_terminate(u32 lro_scale, xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL lro_t	*
xge_hal_lro_next_session (xge_hal_device_t *hldev, int ring);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL lro_t *
xge_hal_lro_get_next_session(xge_hal_device_t *hldev);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_open_lro_session (u8 *buffer, iplro_t *ip, tcplro_t *tcp, lro_t **lro,
                        xge_hal_device_t *hldev, xge_hal_lro_desc_t *ring_lro,
                        int slot, u32 tcp_seg_len, int ts_off);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
__hal_lro_get_free_slot (xge_hal_lro_desc_t	*ring_lro);
#endif

#else /* XGE_FASTPATH_EXTERN */
#define __HAL_STATIC_DEVICE static
#define __HAL_INLINE_DEVICE inline
#include <dev/nxge/xgehal/xgehal-device-fp.c>
#endif /* XGE_FASTPATH_INLINE */


__EXTERN_END_DECLS

#endif /* XGE_HAL_DEVICE_H */
