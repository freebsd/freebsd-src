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
 * $FreeBSD: src/sys/dev/nxge/if_nxge.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _IF_XGE_H
#define _IF_XGE_H

#include <dev/nxge/include/xgehal.h>
#include <dev/nxge/xge-osdep.h>

/* Printing description, Copyright */
#define XGE_DRIVER_VERSION                                                     \
	XGELL_VERSION_MAJOR"."XGELL_VERSION_MINOR"."                           \
	XGELL_VERSION_FIX"."XGELL_VERSION_BUILD
#define XGE_COPYRIGHT "Copyright(c) 2002-2007 Neterion Inc."

/* Printing */
#define xge_trace(trace, fmt, args...) xge_debug_ll(trace, fmt, ## args);

#define XGE_ALIGN_TO(buffer_length, to) {                                      \
	if((buffer_length % to) != 0) {                                        \
	    buffer_length += (to - (buffer_length % to));                      \
	}                                                                      \
}

#define XGE_EXIT_ON_ERR(text, label, return_value) {                           \
	xge_trace(XGE_ERR, "%s (Status: %d)", text, return_value);             \
	status = return_value;                                                 \
	goto label;                                                            \
}

#define XGE_SET_BUFFER_MODE_IN_RINGS(mode) {                                   \
	for(index = 0; index < XGE_RING_COUNT; index++)                        \
	    ring_config->queue[index].buffer_mode = mode;                      \
}

#define XGE_DEFAULT_USER_HARDCODED      -1
#define XGE_MAX_SEGS                     100  /* Maximum number of segments  */
#define XGE_TX_LEVEL_LOW                 16
#define XGE_FIFO_COUNT                   XGE_HAL_MIN_FIFO_NUM
#define XGE_RING_COUNT                   XGE_HAL_MIN_RING_NUM
#define XGE_BUFFER_SIZE                  20
#define XGE_LRO_DEFAULT_ENTRIES          12
#define XGE_BAUDRATE                     1000000000

/* Default values to configuration parameters */
#define XGE_DEFAULT_ENABLED_TSO                    1
#define XGE_DEFAULT_ENABLED_LRO                    1
#define XGE_DEFAULT_ENABLED_MSI                    1
#define XGE_DEFAULT_BUFFER_MODE                    1
#define XGE_DEFAULT_INITIAL_MTU                    1500
#define XGE_DEFAULT_LATENCY_TIMER                  -1
#define XGE_DEFAULT_MAX_SPLITS_TRANS               -1
#define XGE_DEFAULT_MMRB_COUNT                     -1
#define XGE_DEFAULT_SHARED_SPLITS                  0
#define XGE_DEFAULT_ISR_POLLING_CNT                8
#define XGE_DEFAULT_STATS_REFRESH_TIME_SEC         4
#define XGE_DEFAULT_MAC_RMAC_BCAST_EN              1
#define XGE_DEFAULT_MAC_TMAC_UTIL_PERIOD           5
#define XGE_DEFAULT_MAC_RMAC_UTIL_PERIOD           5
#define XGE_DEFAULT_MAC_RMAC_PAUSE_GEN_EN          1
#define XGE_DEFAULT_MAC_RMAC_PAUSE_RCV_EN          1
#define XGE_DEFAULT_MAC_RMAC_PAUSE_TIME            65535
#define XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q0Q3    187
#define XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q4Q7    187
#define XGE_DEFAULT_FIFO_MEMBLOCK_SIZE             PAGE_SIZE
#define XGE_DEFAULT_FIFO_RESERVE_THRESHOLD         0
#define XGE_DEFAULT_FIFO_MAX_FRAGS                 64
#define XGE_DEFAULT_FIFO_QUEUE_INTR                0
#define XGE_DEFAULT_FIFO_QUEUE_MAX                 2048
#define XGE_DEFAULT_FIFO_QUEUE_INITIAL             2048
#define XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_A        5
#define XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_B        10
#define XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_C        20
#define XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_A           15
#define XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_B           30
#define XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_C           45
#define XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_D           60
#define XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_CI_EN     1
#define XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_AC_EN     1
#define XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_VAL_US    8000
#define XGE_DEFAULT_FIFO_ALIGNMENT_SIZE            sizeof(u64)
#define XGE_DEFAULT_RING_MEMBLOCK_SIZE             PAGE_SIZE
#define XGE_DEFAULT_RING_STRIP_VLAN_TAG            1
#define XGE_DEFAULT_RING_QUEUE_MAX                 16
#define XGE_DEFAULT_RING_QUEUE_INITIAL             16
#define XGE_DEFAULT_RING_QUEUE_DRAM_SIZE_MB        32
#define XGE_DEFAULT_RING_QUEUE_INDICATE_MAX_PKTS   16
#define XGE_DEFAULT_RING_QUEUE_BACKOFF_INTERVAL_US 1000
#define XGE_DEFAULT_RING_QUEUE_RTI_URANGE_A        5
#define XGE_DEFAULT_RING_QUEUE_RTI_URANGE_B        10
#define XGE_DEFAULT_RING_QUEUE_RTI_URANGE_C        50
#define XGE_DEFAULT_RING_QUEUE_RTI_UFC_A           1
#define XGE_DEFAULT_RING_QUEUE_RTI_UFC_B           8
#define XGE_DEFAULT_RING_QUEUE_RTI_UFC_C           16
#define XGE_DEFAULT_RING_QUEUE_RTI_UFC_D           32
#define XGE_DEFAULT_RING_QUEUE_RTI_TIMER_AC_EN     1
#define XGE_DEFAULT_RING_QUEUE_RTI_TIMER_VAL_US    250

#define XGE_DRV_STATS(param) (lldev->driver_stats.param++)

#define XGE_SAVE_PARAM(to, what, value) to.what = value;

#define XGE_GET_PARAM(str_kenv, to, param, hardcode) {                         \
	static int param##__LINE__;                                            \
	if(testenv(str_kenv) == 1) {                                           \
	    getenv_int(str_kenv, &param##__LINE__);                            \
	}                                                                      \
	else {                                                                 \
	    param##__LINE__ = hardcode;                                        \
	}                                                                      \
	XGE_SAVE_PARAM(to, param, param##__LINE__);                            \
}

#define XGE_GET_PARAM_MAC(str_kenv, param, hardcode)                           \
	XGE_GET_PARAM(str_kenv, ((*dconfig).mac), param, hardcode);

#define XGE_GET_PARAM_FIFO(str_kenv, param, hardcode)                          \
	XGE_GET_PARAM(str_kenv, ((*dconfig).fifo), param, hardcode);

#define XGE_GET_PARAM_FIFO_QUEUE(str_kenv, param, qindex, hardcode)            \
	XGE_GET_PARAM(str_kenv, ((*dconfig).fifo.queue[qindex]), param,        \
	    hardcode);

#define XGE_GET_PARAM_FIFO_QUEUE_TTI(str_kenv, param, qindex, tindex, hardcode)\
	XGE_GET_PARAM(str_kenv, ((*dconfig).fifo.queue[qindex].tti[tindex]),   \
	    param, hardcode);

#define XGE_GET_PARAM_RING(str_kenv, param, hardcode)                          \
	XGE_GET_PARAM(str_kenv, ((*dconfig).ring), param, hardcode);

#define XGE_GET_PARAM_RING_QUEUE(str_kenv, param, qindex, hardcode)            \
	XGE_GET_PARAM(str_kenv, ((*dconfig).ring.queue[qindex]), param,        \
	    hardcode);

#define XGE_GET_PARAM_RING_QUEUE_RTI(str_kenv, param, qindex, hardcode)        \
	XGE_GET_PARAM(str_kenv, ((*dconfig).ring.queue[qindex].rti), param,    \
	    hardcode);

/* Values to identify the requests from getinfo tool in ioctl */
#define XGE_QUERY_STATS       1
#define XGE_QUERY_PCICONF     2
#define XGE_QUERY_DEVSTATS    3
#define XGE_QUERY_DEVCONF     4
#define XGE_READ_VERSION      5
#define XGE_QUERY_SWSTATS     6
#define XGE_QUERY_DRIVERSTATS 7
#define XGE_SET_BUFFER_MODE_1 8
#define XGE_SET_BUFFER_MODE_2 9
#define XGE_SET_BUFFER_MODE_5 10
#define XGE_QUERY_BUFFER_MODE 11

#define XGE_OFFSET_OF_LAST_REG           0x3180

#define VENDOR_ID_AMD                    0x1022
#define DEVICE_ID_8131_PCI_BRIDGE        0x7450

typedef struct mbuf *mbuf_t;

typedef enum xge_lables {
	xge_free_all                  = 0,
	xge_free_mutex                = 1,
	xge_free_terminate_hal_driver = 2,
	xge_free_hal_device           = 3,
	xge_free_pci_info             = 4,
	xge_free_bar0                 = 5,
	xge_free_bar0_resource        = 6,
	xge_free_bar1                 = 7,
	xge_free_bar1_resource        = 8,
	xge_free_irq_resource         = 9,
	xge_free_terminate_hal_device = 10,
	xge_free_media_interface      = 11,
} xge_lables_e;

typedef enum xge_option {
	XGE_CHANGE_LRO = 0,
	XGE_SET_MTU = 1
} xge_option_e;

typedef enum xge_event_e {
	XGE_LL_EVENT_TRY_XMIT_AGAIN   = XGE_LL_EVENT_BASE + 1,
	XGE_LL_EVENT_DEVICE_RESETTING = XGE_LL_EVENT_BASE + 2
} xge_event_e;

typedef struct xge_msi_info {
	u16 msi_control;                     /* MSI control 0x42              */
	u32 msi_lower_address;               /* MSI lower address 0x44        */
	u32 msi_higher_address;              /* MSI higher address 0x48       */
	u16 msi_data;                        /* MSI data                      */
} xge_msi_info_t;

typedef struct xge_driver_stats_t {
	/* ISR statistics */
	u64 isr_filter;
	u64 isr_line;
	u64 isr_msi;

	/* Tx statistics */
	u64 tx_calls;
	u64 tx_completions;
	u64 tx_desc_compl;
	u64 tx_tcode;
	u64 tx_defrag;
	u64 tx_no_txd;
	u64 tx_map_fail;
	u64 tx_max_frags;
	u64 tx_tso;
	u64 tx_posted;
	u64 tx_again;
	u64 tx_lock_fail;

	/* Rx statistics */
	u64 rx_completions;
	u64 rx_desc_compl;
	u64 rx_tcode;
	u64 rx_no_buf;
	u64 rx_map_fail;

	/* LRO statistics */
	u64 lro_uncapable;
	u64 lro_begin;
	u64 lro_end1;
	u64 lro_end2;
	u64 lro_end3;
	u64 lro_append;
	u64 lro_session_exceeded;
	u64 lro_close;
} xge_driver_stats_t;

typedef struct xge_lro_entry_t {
	SLIST_ENTRY(xge_lro_entry_t) next;
	struct mbuf *m_head;
	struct mbuf *m_tail;
	struct ip *lro_header_ip;
	int timestamp;
	u32 tsval;
	u32 tsecr;
	u32 source_ip;
	u32 dest_ip;
	u32 next_seq;
	u32 ack_seq;
	u32 len;
	u32 data_csum;
	u16 window;
	u16 source_port;
	u16 dest_port;
	u16 append_cnt;
	u16 mss;
} xge_lro_entry_t;

SLIST_HEAD(lro_head, xge_lro_entry_t);

/* Adapter structure */
typedef struct xge_lldev_t {
	device_t             device;         /* Device                        */
	struct ifnet         *ifnetp;        /* Interface ifnet structure     */
	struct resource      *irq;           /* Resource structure for IRQ    */
	void                 *irqhandle;     /* IRQ handle                    */
	xge_pci_info_t       *pdev;          /* PCI info                      */
	xge_hal_device_t     *devh;          /* HAL: Device Handle            */
	struct mtx           mtx_drv;        /* Mutex - Driver                */
	struct mtx           mtx_tx[XGE_FIFO_COUNT];
	                                     /* Mutex - Tx                    */
	char                 mtx_name_drv[16];/*Mutex Name - Driver           */
	char                 mtx_name_tx[16][XGE_FIFO_COUNT];
	                                     /* Mutex Name - Tx               */
	struct callout       timer;          /* Timer for polling             */
	struct ifmedia       media;          /* In-kernel representation of a */
	                                     /* single supported media type   */
	xge_hal_channel_h    fifo_channel[XGE_FIFO_COUNT];
	                                     /* FIFO channels                 */
	xge_hal_channel_h    ring_channel[XGE_RING_COUNT];
	                                     /* Ring channels                 */
	bus_dma_tag_t        dma_tag_tx;     /* Tag for dtr dma mapping (Tx)  */
	bus_dma_tag_t        dma_tag_rx;     /* Tag for dtr dma mapping (Rx)  */
	bus_dmamap_t         extra_dma_map;  /* Extra DMA map for Rx          */
	xge_msi_info_t       msi_info;       /* MSI info                      */
	xge_driver_stats_t   driver_stats;   /* Driver statistics             */
	int                  initialized;    /* Flag: Initialized or not      */
	int                  all_multicast;  /* All multicast flag            */
	int                  macaddr_count;  /* Multicast address count       */
	int                  in_detach;      /* To avoid ioctl during detach  */
	int                  buffer_mode;    /* Buffer Mode                   */
	int                  rxd_mbuf_cnt;   /* Number of buffers used        */
	int                  rxd_mbuf_len[5];/* Buffer lengths                */
	int                  enabled_tso;    /* Flag: TSO Enabled             */
	int                  enabled_lro;    /* Flag: LRO Enabled             */
	int                  enabled_msi;    /* Flag: MSI Enabled             */
	int                  mtu;            /* Interface MTU                 */
	int                  lro_num;        /* Number of LRO sessions        */
	struct lro_head      lro_active;     /* Active LRO sessions           */
	struct lro_head      lro_free;       /* Free LRO sessions             */
} xge_lldev_t;

/* Rx descriptor private structure */
typedef struct xge_rx_priv_t {
	mbuf_t        *bufferArray;
	xge_dma_mbuf_t dmainfo[5];
} xge_rx_priv_t;

/* Tx descriptor private structure */
typedef struct xge_tx_priv_t {
	mbuf_t       buffer;
	bus_dmamap_t dma_map;
} xge_tx_priv_t;

/* BAR0 Register */
typedef struct xge_register_t {
	char option[2];
	u64 offset;
	u64 value;
}xge_register_t;

void xge_init_params(xge_hal_device_config_t *, device_t);
void xge_init(void *);
void xge_device_init(xge_lldev_t *, xge_hal_channel_reopen_e);
void xge_device_stop(xge_lldev_t *, xge_hal_channel_reopen_e);
void xge_stop(xge_lldev_t *);
void xge_resources_free(device_t, xge_lables_e);
void xge_callback_link_up(void *);
void xge_callback_link_down(void *);
void xge_callback_crit_err(void *, xge_hal_event_e, u64);
void xge_callback_event(xge_queue_item_t *);
int  xge_ifmedia_change(struct ifnet *);
void xge_ifmedia_status(struct ifnet *, struct ifmediareq *);
int  xge_ioctl(struct ifnet *, unsigned long, caddr_t);
int  xge_ioctl_stats(xge_lldev_t *, struct ifreq *);
int  xge_ioctl_registers(xge_lldev_t *, struct ifreq *);
void xge_timer(void *);
int  xge_isr_filter(void *);
void xge_isr_line(void *);
void xge_isr_msi(void *);
void xge_enable_msi(xge_lldev_t *);
int  xge_rx_open(int, xge_lldev_t *, xge_hal_channel_reopen_e);
int  xge_tx_open(xge_lldev_t *, xge_hal_channel_reopen_e);
void xge_channel_close(xge_lldev_t *, xge_hal_channel_reopen_e);
int  xge_channel_open(xge_lldev_t *, xge_hal_channel_reopen_e);
xge_hal_status_e xge_rx_compl(xge_hal_channel_h, xge_hal_dtr_h, u8, void *);
xge_hal_status_e xge_tx_compl(xge_hal_channel_h, xge_hal_dtr_h, u8, void *);
xge_hal_status_e xge_tx_initial_replenish(xge_hal_channel_h, xge_hal_dtr_h,
	int, void *, xge_hal_channel_reopen_e);
xge_hal_status_e xge_rx_initial_replenish(xge_hal_channel_h, xge_hal_dtr_h,
	int, void *, xge_hal_channel_reopen_e);
void xge_rx_term(xge_hal_channel_h, xge_hal_dtr_h, xge_hal_dtr_state_e,
	void *, xge_hal_channel_reopen_e);
void xge_tx_term(xge_hal_channel_h, xge_hal_dtr_h, xge_hal_dtr_state_e,
	void *, xge_hal_channel_reopen_e);
void xge_set_mbuf_cflags(mbuf_t);
void xge_send(struct ifnet *);
static void inline xge_send_locked(struct ifnet *, int);
int  xge_get_buf(xge_hal_dtr_h, xge_rx_priv_t *, xge_lldev_t *, int);
int  xge_ring_dtr_get(mbuf_t, xge_hal_channel_h, xge_hal_dtr_h, xge_lldev_t *,
	xge_rx_priv_t *);
int  xge_get_buf_3b_5b(xge_hal_dtr_h, xge_rx_priv_t *, xge_lldev_t *);
void dmamap_cb(void *, bus_dma_segment_t *, int, int);
void xge_reset(xge_lldev_t *);
void xge_setmulti(xge_lldev_t *);
void xge_enable_promisc(xge_lldev_t *);
void xge_disable_promisc(xge_lldev_t *);
int  xge_change_mtu(xge_lldev_t *, int);
void xge_buffer_mode_init(xge_lldev_t *, int);
void xge_initialize(device_t, xge_hal_channel_reopen_e);
void xge_terminate(device_t, xge_hal_channel_reopen_e);
int  xge_probe(device_t);
int  xge_driver_initialize(void);
void xge_media_init(device_t);
void xge_pci_space_save(device_t);
void xge_pci_space_restore(device_t);
void xge_msi_info_save(xge_lldev_t *);
void xge_msi_info_restore(xge_lldev_t *);
int  xge_attach(device_t);
int  xge_interface_setup(device_t);
int  xge_detach(device_t);
int  xge_shutdown(device_t);
void xge_mutex_init(xge_lldev_t *);
void xge_mutex_destroy(xge_lldev_t *);
void xge_print_info(xge_lldev_t *);
void xge_lro_flush_sessions(xge_lldev_t *);
void xge_rx_buffer_sizes_set(xge_lldev_t *, int, int);
void xge_accumulate_large_rx(xge_lldev_t *, struct mbuf *, int,
	xge_rx_priv_t *);
xge_hal_status_e xge_create_dma_tags(device_t);
void xge_add_sysctl_handlers(xge_lldev_t *);
void xge_confirm_changes(xge_lldev_t *, xge_option_e);
static int xge_lro_accumulate(xge_lldev_t *, struct mbuf *);
static void xge_lro_flush(xge_lldev_t *, xge_lro_entry_t *);

#endif // _IF_XGE_H

