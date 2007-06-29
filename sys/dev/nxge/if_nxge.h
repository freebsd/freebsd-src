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
 *  if_xge.h
 */

#ifndef _IF_XGE_H
#define _IF_XGE_H

#include <dev/nxge/include/xgehal.h>
#include <dev/nxge/xge-osdep.h>

#if defined(XGE_FEATURE_TSO) && (__FreeBSD_version < 700026)
#undef XGE_FEATURE_TSO
#endif

#if defined(XGE_FEATURE_LRO)
#if __FreeBSD_version < 700047
#undef XGE_FEATURE_LRO
#undef XGE_HAL_CONFIG_LRO
#else
#define XGE_HAL_CONFIG_LRO
#endif
#endif

#ifdef  FUNC_PRINT
#define ENTER_FUNCTION  xge_os_printf("Enter\t==>[%s]\n", __FUNCTION__);
#define LEAVE_FUNCTION  xge_os_printf("Leave\t<==[%s]\n", __FUNCTION__);
#else
#define ENTER_FUNCTION
#define LEAVE_FUNCTION
#endif

/* Printing description, Copyright */
#define DRIVER_VERSION                   XGELL_VERSION_MAJOR"."               \
	                                 XGELL_VERSION_MINOR"."               \
	                                 XGELL_VERSION_FIX"."                 \
	                                 XGELL_VERSION_BUILD
#define COPYRIGHT_STRING                 "Copyright(c) 2002-2007 Neterion Inc."
#define PRINT_COPYRIGHT                  xge_os_printf("%s", COPYRIGHT_STRING)

/* Printing */
#define xge_trace(trace, fmt, args...) xge_debug_ll(trace, fmt, ## args);
#define xge_ctrace(trace, fmt...)        xge_debug_ll(trace, fmt);

#define BUFALIGN(buffer_length)                                               \
	if((buffer_length % 128) != 0) {                                      \
	    buffer_length += (128 - (buffer_length % 128));                   \
	}

static inline void *
xge_malloc(unsigned long size) {
	void *vaddr = malloc(size, M_DEVBUF, M_NOWAIT);
	bzero(vaddr, size);
	return vaddr;
}

#define SINGLE_ALLOC                     0
#define MULTI_ALLOC                      1 
#define SAVE                             0
#define RESTORE                          1
#define UP                               1
#define DOWN                             0
#define XGE_DEFAULT_USER_HARDCODED      -1
#define MAX_MBUF_FRAGS                   20   /* Maximum number of fragments */
#define MAX_SEGS                         100  /* Maximum number of segments  */
#define XGELL_TX_LEVEL_LOW               16
#define XGE_RING_COUNT                   XGE_HAL_MIN_RING_NUM
#define BUFFER_SIZE			 20

/* Default values to configuration parameters */
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

/* Values to identify the requests from getinfo tool in ioctl */
#define XGE_QUERY_STATS       1
#define XGE_QUERY_PCICONF     2
#define XGE_QUERY_INTRSTATS   3
#define XGE_QUERY_DEVCONF     4
#define XGE_READ_VERSION      5
#define XGE_QUERY_TCODE	      6
#define XGE_SET_BUFFER_MODE_1 7
#define XGE_SET_BUFFER_MODE_2 8
#define XGE_SET_BUFFER_MODE_3 9
#define XGE_SET_BUFFER_MODE_5 10
#define XGE_QUERY_BUFFER_MODE 11

#define XGE_OFFSET_OF_LAST_REG           0x3180

#define VENDOR_ID_AMD                    0x1022
#define DEVICE_ID_8131_PCI_BRIDGE        0x7450

typedef struct mbuf *mbuf_t;

typedef enum xgell_event_e {
	XGE_LL_EVENT_TRY_XMIT_AGAIN   = XGE_LL_EVENT_BASE + 1,
	XGE_LL_EVENT_DEVICE_RESETTING = XGE_LL_EVENT_BASE + 2,
} xgell_event_e;

/* Adapter structure */
typedef struct xgelldev {
	device_t             device;         /* Device                        */
	struct ifnet         *ifnetp;        /* Interface ifnet structure     */
	struct resource      *irq;           /* Resource structure for IRQ    */
	void                 *irqhandle;     /* IRQ handle                    */
	pci_info_t           *pdev;
	struct ifmedia       xge_media;      /* In-kernel representation of a */
	                                     /* single supported media type   */
	xge_hal_device_t     *devh;          /* HAL: Device Handle            */
	xge_hal_channel_h    ring_channel[XGE_HAL_MAX_FIFO_NUM];
	                                     /* Ring channel                  */
	xge_hal_channel_h    fifo_channel_0; /* FIFO channel                  */
	struct mtx           xge_lock;       /* Mutex - Default               */
	struct callout       timer;          /* Timer for polling             */
	struct xge_hal_stats_hw_info_t *hwstats; /* Hardware Statistics       */
	int                  saved_regs[16]; /* To save register space        */
	int                  xge_mtu;        /* MTU                           */
	int                  initialized;    /* Flag: Initialized or not      */
	bus_dma_tag_t        dma_tag_tx;     /* Tag for dtr dma mapping (Tx)  */
	bus_dma_tag_t        dma_tag_rx;     /* Tag for dtr dma mapping (Rx)  */
	int                  all_multicast;  /* All multicast flag            */
	int                  macaddr_count;  /* Multicast address count       */
	int                  in_detach;      /* To avoid ioctl during detach  */
	int                  buffer_mode;    /* Buffer Mode                   */
	int                  rxd_mbuf_cnt;   /* Number of buffers used        */
	int                  rxd_mbuf_len[5];/* Buffer lengths                */
} xgelldev_t;

/* Rx descriptor private structure */
typedef struct {
	mbuf_t      *bufferArray;
	struct      xge_dma_mbuf dmainfo[5];
} xgell_rx_priv_t;

/* Tx descriptor private structure */
typedef struct {
	mbuf_t       buffer;
	bus_dmamap_t dma_map;
} xgell_tx_priv_t;

/* BAR0 Register */
typedef struct barregister {  
	char option[2]; 
	u64 offset;
	u64 value;
}bar0reg_t;

void xge_init_params(xge_hal_device_config_t *dconfig, device_t dev);
void xge_init(void *);
void xge_init_locked(void *);
void xge_stop(xgelldev_t *);
void freeResources(device_t, int);
void xgell_callback_link_up(void *);
void xgell_callback_link_down(void *);
void xgell_callback_crit_err(void *, xge_hal_event_e, u64);
void xgell_callback_event(xge_queue_item_t *);
int  xge_ifmedia_change(struct ifnet *);
void xge_ifmedia_status(struct ifnet *, struct ifmediareq *);
int  xge_ioctl(struct ifnet *, unsigned long, caddr_t);
void xge_timer(void *);
int  xge_intr_filter(void *);
void xge_intr(void *);
int  xgell_rx_open(int, xgelldev_t *, xge_hal_channel_reopen_e);
int  xgell_tx_open(xgelldev_t *, xge_hal_channel_reopen_e);
int  xgell_channel_close(xgelldev_t *, xge_hal_channel_reopen_e);
int  xgell_channel_open(xgelldev_t *, xge_hal_channel_reopen_e);
xge_hal_status_e xgell_rx_compl(xge_hal_channel_h, xge_hal_dtr_h, u8, void *);
xge_hal_status_e xgell_tx_compl(xge_hal_channel_h, xge_hal_dtr_h, u8, void *);
xge_hal_status_e xgell_tx_initial_replenish(xge_hal_channel_h, xge_hal_dtr_h,
        int, void *, xge_hal_channel_reopen_e);
xge_hal_status_e xgell_rx_initial_replenish(xge_hal_channel_h, xge_hal_dtr_h,
        int, void *, xge_hal_channel_reopen_e);
void xgell_rx_term(xge_hal_channel_h, xge_hal_dtr_h, xge_hal_dtr_state_e,
        void *, xge_hal_channel_reopen_e);
void xgell_tx_term(xge_hal_channel_h, xge_hal_dtr_h, xge_hal_dtr_state_e,
        void *, xge_hal_channel_reopen_e);
void xgell_set_mbuf_cflags(mbuf_t);
void xge_send(struct ifnet *);
void xge_send_locked(struct ifnet *);
int  xgell_get_multimode_normalbuf(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
        xgelldev_t *lldev);
int  xgell_get_multimode_jumbobuf(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
        xgelldev_t *lldev, int lock);
int  xgell_get_second_buffer(xgell_rx_priv_t *rxd_priv, xgelldev_t *lldev);
int  xgell_get_buf(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
	xgelldev_t *lldev, int index);
int  xge_ring_dtr_get(mbuf_t mbuf_up, xge_hal_channel_h channelh, xge_hal_dtr_h dtr,
	xgelldev_t *lldev, xgell_rx_priv_t *rxd_priv);
int  xgell_get_buf_3b_5b(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
        xgelldev_t *lldev);
void dmamap_cb(void *, bus_dma_segment_t *, int, int);
void xgell_reset(xgelldev_t *);
void xge_setmulti(xgelldev_t *);
void xge_enable_promisc(xgelldev_t *);
void xge_disable_promisc(xgelldev_t *);
int  changeMtu(xgelldev_t *, int);
int  changeBufmode(xgelldev_t *, int);
void xge_initialize(device_t, xge_hal_channel_reopen_e);
void xge_terminate(device_t, xge_hal_channel_reopen_e);
void if_up_locked(xgelldev_t *);
void if_down_locked(xgelldev_t *);
int  xge_probe(device_t);
int  xge_driver_initialize(void);
void xge_media_init(device_t);
void xge_pci_space_save(device_t);
void xge_pci_space_restore(device_t);
int  xge_attach(device_t);
int  xge_interface_setup(device_t);
int  xge_detach(device_t);
int  xge_shutdown(device_t);
int  xge_suspend(device_t);
int  xge_resume(device_t);

#endif // _IF_XGE_H

