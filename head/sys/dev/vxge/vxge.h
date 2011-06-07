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

#ifndef	_VXGE_H_
#define	__VXGE_H_

#include <dev/vxge/vxgehal/vxgehal.h>
#include <dev/vxge/vxge-osdep.h>
#include "vxge-firmware.h"

#define	VXGE_GET_PARAM(str_kenv, to, param, hardcode) {	\
	static int __CONCAT(param, __LINE__);		\
	if (testenv(str_kenv) == 1)			\
		TUNABLE_INT_FETCH(str_kenv,		\
		    &__CONCAT(param, __LINE__));	\
	else						\
		__CONCAT(param, __LINE__) = hardcode;	\
							\
	to.param = __CONCAT(param, __LINE__);		\
}

#define	VXGE_BUFFER_ALIGN(buffer_length, to) {		\
	if (buffer_length % to)				\
		buffer_length +=			\
		    (to - (buffer_length % to));	\
}

#define	VXGE_HAL_VPATH_MSIX_ACTIVE		4
#define	VXGE_HAL_VPATH_MSIX_ALARM_ID		2
#define	VXGE_MSIX_ALARM_ID(hldev, i)			\
	((__hal_device_t *) hldev)->first_vp_id *	\
	    VXGE_HAL_VPATH_MSIX_ACTIVE + i;

#define	VXGE_DUAL_PORT_MODE			2
#define	VXGE_DUAL_PORT_MAP			0xAAAAULL
#define	VXGE_BAUDRATE				1000000000
#define	VXGE_MAX_SEGS				VXGE_HAL_MAX_FIFO_FRAGS
#define	VXGE_TSO_SIZE				65600
#define	VXGE_STATS_BUFFER_SIZE			65536
#define	VXGE_PRINT_BUF_SIZE			128
#define	VXGE_PMD_INFO_LEN			24
#define	VXGE_RXD_REPLENISH_COUNT		4
#define	VXGE_TX_LOW_THRESHOLD			32

/* Default configuration parameters */
#define	VXGE_DEFAULT_USER_HARDCODED		-1
#define	VXGE_DEFAULT_CONFIG_VALUE		0xFF
#define	VXGE_DEFAULT_CONFIG_ENABLE		1
#define	VXGE_DEFAULT_CONFIG_DISABLE		0

#if __FreeBSD_version >= 800000
#define	VXGE_DEFAULT_CONFIG_MQ_ENABLE		1
#else
#define	VXGE_DEFAULT_CONFIG_MQ_ENABLE		0
#endif

#define	VXGE_DEFAULT_CONFIG_IFQ_MAXLEN		1024

#define	VXGE_DEFAULT_BR_SIZE			4096
#define	VXGE_DEFAULT_RTH_BUCKET_SIZE		8
#define	VXGE_DEFAULT_RING_BLOCK			2
#define	VXGE_DEFAULT_SUPPORTED_DEVICES		1
#define	VXGE_DEFAULT_DEVICE_POLL_MILLIS		2000
#define	VXGE_DEFAULT_FIFO_ALIGNED_FRAGS		1
#define	VXGE_DEFAULT_VPATH_PRIORITY_LOW		3
#define	VXGE_DEFAULT_VPATH_PRIORITY_HIGH	0
#define	VXGE_DEFAULT_ALL_VID_ENABLE		\
	VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE

#define	VXGE_DEFAULT_STRIP_VLAN_TAG		\
	VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_ENABLE

#define	VXGE_DEFAULT_TTI_BTIMER_VAL		250000
#define	VXGE_DEFAULT_TTI_LTIMER_VAL		80
#define	VXGE_DEFAULT_TTI_RTIMER_VAL		0

#define	VXGE_DEFAULT_RTI_BTIMER_VAL		250
#define	VXGE_DEFAULT_RTI_LTIMER_VAL		100
#define	VXGE_DEFAULT_RTI_RTIMER_VAL		0

#define	VXGE_TTI_RTIMER_ADAPT_VAL		10
#define	VXGE_RTI_RTIMER_ADAPT_VAL		15

#define	VXGE_DEFAULT_TTI_TX_URANGE_A		5
#define	VXGE_DEFAULT_TTI_TX_URANGE_B		15
#define	VXGE_DEFAULT_TTI_TX_URANGE_C		40

#define	VXGE_DEFAULT_RTI_RX_URANGE_A		5
#define	VXGE_DEFAULT_RTI_RX_URANGE_B		15
#define	VXGE_DEFAULT_RTI_RX_URANGE_C		40

#define	VXGE_DEFAULT_TTI_TX_UFC_A		1
#define	VXGE_DEFAULT_TTI_TX_UFC_B		5
#define	VXGE_DEFAULT_TTI_TX_UFC_C		15
#define	VXGE_DEFAULT_TTI_TX_UFC_D		40

#define	VXGE_DEFAULT_RTI_RX_UFC_A		1
#define	VXGE_DEFAULT_RTI_RX_UFC_B		20
#define	VXGE_DEFAULT_RTI_RX_UFC_C		40
#define	VXGE_DEFAULT_RTI_RX_UFC_D		100

#define	VXGE_MAX_RX_INTERRUPT_COUNT		100
#define	VXGE_MAX_TX_INTERRUPT_COUNT		200

#define	is_multi_func(func_mode) \
	((func_mode == VXGE_HAL_PCIE_FUNC_MODE_MF8_VP2) || \
	(func_mode == VXGE_HAL_PCIE_FUNC_MODE_MF2_VP8) || \
	(func_mode == VXGE_HAL_PCIE_FUNC_MODE_MF4_VP4) || \
	(func_mode == VXGE_HAL_PCIE_FUNC_MODE_MF8P_VP2))

#define	is_single_func(func_mode) \
	(func_mode == VXGE_HAL_PCIE_FUNC_MODE_SF1_VP17)

#define	VXGE_DRV_STATS(v, x)		v->driver_stats.x++
#define	VXGE_MAX_MSIX_MESSAGES		(VXGE_HAL_MAX_VIRTUAL_PATHS * 2 + 2)

#define	VXGE_DRV_LOCK(x)		mtx_lock(&(x)->mtx_drv)
#define	VXGE_DRV_UNLOCK(x)		mtx_unlock(&(x)->mtx_drv)
#define	VXGE_DRV_LOCK_DESTROY(x)	mtx_destroy(&(x)->mtx_drv)
#define	VXGE_DRV_LOCK_ASSERT(x)		mtx_assert(&(x)->mtx_drv, MA_OWNED)

#define	VXGE_TX_LOCK(x)			mtx_lock(&(x)->mtx_tx)
#define	VXGE_TX_TRYLOCK(x)		mtx_trylock(&(x)->mtx_tx)
#define	VXGE_TX_UNLOCK(x)		mtx_unlock(&(x)->mtx_tx)
#define	VXGE_TX_LOCK_DESTROY(x)		mtx_destroy(&(x)->mtx_tx)
#define	VXGE_TX_LOCK_ASSERT(x)		mtx_assert(&(x)->mtx_tx, MA_OWNED)

const char *
vxge_port_mode[6] =
{
	"Default",
	"Reserved",
	"Active/Passive",
	"Single Port",
	"Dual Port",
	"Disabled"
};

const char *
vxge_port_failure[3] =
{
	"No Failover",
	"Failover only",
	"Failover & Failback"
};

/* IOCTLs to identify vxge-manage requests */
typedef enum _vxge_query_device_info_e {

	VXGE_GET_PCI_CONF = 100,
	VXGE_GET_MRPCIM_STATS = 101,
	VXGE_GET_DEVICE_STATS = 102,
	VXGE_GET_DEVICE_HWINFO = 103,
	VXGE_GET_DRIVER_STATS = 104,
	VXGE_GET_INTR_STATS = 105,
	VXGE_GET_VERSION = 106,
	VXGE_GET_TCODE = 107,
	VXGE_GET_VPATH_COUNT = 108,
	VXGE_GET_BANDWIDTH = 109,
	VXGE_SET_BANDWIDTH = 110,
	VXGE_GET_PORT_MODE = 111,
	VXGE_SET_PORT_MODE = 112

} vxge_query_device_info_e;

typedef enum _vxge_firmware_upgrade_e {

	VXGE_FW_UPGRADE_NONE = 0,
	VXGE_FW_UPGRADE_ALL = 1,
	VXGE_FW_UPGRADE_FORCE = 2

} vxge_firmware_upgrade_e;

typedef enum _vxge_free_resources_e {

	VXGE_FREE_NONE = 0,
	VXGE_FREE_MUTEX = 1,
	VXGE_FREE_PCI_INFO = 2,
	VXGE_FREE_BAR0 = 3,
	VXGE_FREE_BAR1 = 4,
	VXGE_FREE_BAR2 = 5,
	VXGE_FREE_ISR_RESOURCE = 6,
	VXGE_FREE_MEDIA = 7,
	VXGE_FREE_INTERFACE = 8,
	VXGE_FREE_DEVICE_CONFIG = 9,
	VXGE_FREE_TERMINATE_DEVICE = 10,
	VXGE_FREE_TERMINATE_DRIVER = 11,
	VXGE_DISABLE_PCI_BUSMASTER = 12,
	VXGE_FREE_VPATH = 13,
	VXGE_FREE_ALL = 14

} vxge_free_resources_e;

typedef enum _vxge_device_attributes_e {

	VXGE_PRINT_DRV_VERSION = 0,
	VXGE_PRINT_PCIE_INFO = 1,
	VXGE_PRINT_SERIAL_NO = 2,
	VXGE_PRINT_PART_NO = 3,
	VXGE_PRINT_FW_VERSION = 4,
	VXGE_PRINT_FW_DATE = 5,
	VXGE_PRINT_FUNC_MODE = 6,
	VXGE_PRINT_INTR_MODE = 7,
	VXGE_PRINT_VPATH_COUNT = 8,
	VXGE_PRINT_MTU_SIZE = 9,
	VXGE_PRINT_LRO_MODE = 10,
	VXGE_PRINT_RTH_MODE = 11,
	VXGE_PRINT_TSO_MODE = 12,
	VXGE_PRINT_PMD_PORTS_0 = 13,
	VXGE_PRINT_PMD_PORTS_1 = 14,
	VXGE_PRINT_ADAPTER_TYPE = 15,
	VXGE_PRINT_PORT_MODE = 16,
	VXGE_PRINT_PORT_FAILURE = 17,
	VXGE_PRINT_ACTIVE_PORT = 18,
	VXGE_PRINT_L2SWITCH_MODE = 19

} vxge_device_attribute_e;

typedef struct _vxge_isr_info_t {

	int	irq_rid;
	void   *irq_handle;
	struct resource *irq_res;

} vxge_isr_info_t;

typedef struct _vxge_drv_stats_t {

	u64	isr_msix;

	u64	tx_xmit;
	u64	tx_posted;
	u64	tx_compl;
	u64	tx_tso;
	u64	tx_tcode;
	u64	tx_low_dtr_cnt;
	u64	tx_reserve_failed;
	u64	tx_no_dma_setup;
	u64	tx_max_frags;
	u64	tx_again;

	u64	rx_compl;
	u64	rx_tcode;
	u64	rx_no_buf;
	u64	rx_map_fail;
	u64	rx_lro_queued;
	u64	rx_lro_flushed;

} vxge_drv_stats_t;

typedef struct vxge_dev_t vxge_dev_t;

/* Rx descriptor private structure */
typedef struct _vxge_rxd_priv_t {

	mbuf_t	mbuf_pkt;
	bus_size_t dma_sizes[1];
	bus_addr_t dma_addr[1];
	bus_dmamap_t dma_map;

} vxge_rxd_priv_t;

/* Tx descriptor private structure */
typedef struct _vxge_txdl_priv_t {

	mbuf_t	mbuf_pkt;
	bus_dmamap_t dma_map;
	bus_dma_segment_t dma_buffers[VXGE_MAX_SEGS];

} vxge_txdl_priv_t;

typedef struct _vxge_vpath_t {

	u32		vp_id;
	u32		vp_index;
	u32		is_open;
	u32		lro_enable;
	int		msix_vec;

	int		msix_vec_alarm;
	u32		is_configured;
	u64		rxd_posted;
	macaddr_t	mac_addr;
	macaddr_t	mac_mask;

	int		tx_ticks;
	int		rx_ticks;

	u32		tti_rtimer_val;
	u32		rti_rtimer_val;

	u64		tx_interrupts;
	u64		rx_interrupts;

	int		tx_intr_coalesce;
	int		rx_intr_coalesce;

	vxge_dev_t	*vdev;
	vxge_hal_vpath_h handle;
	char		mtx_tx_name[16];

	bus_dma_tag_t	dma_tag_tx;
	bus_dma_tag_t	dma_tag_rx;
	bus_dmamap_t	extra_dma_map;

	vxge_drv_stats_t driver_stats;
	struct		mtx mtx_tx;
	struct		lro_ctrl lro;

#if __FreeBSD_version >= 800000
	struct		buf_ring *br;
#endif

} vxge_vpath_t;

typedef struct _vxge_bw_info_t {

	char	query;
	u64	func_id;
	int	priority;
	int	bandwidth;

} vxge_bw_info_t;

typedef struct _vxge_port_info_t {

	char	query;
	int	port_mode;
	int	port_failure;

} vxge_port_info_t;

typedef struct _vxge_device_hw_info_t {

	vxge_hal_device_hw_info_t hw_info;
	vxge_hal_xmac_nwif_dp_mode port_mode;
	vxge_hal_xmac_nwif_behavior_on_failure port_failure;

} vxge_device_hw_info_t;

typedef struct _vxge_config_t {

	u32	intr_mode;
	int	lro_enable;
	int	rth_enable;
	int	tso_enable;
	int	tx_steering;
	int	rth_bkt_sz;
	int	ifq_maxlen;
	int	no_of_vpath;
	int	ifq_multi;
	int	intr_coalesce;
	int	low_latency;
	int	l2_switch;
	int	port_mode;
	int	function_mode;
	char	nic_attr[20][128];

	vxge_hal_device_hw_info_t	hw_info;
	vxge_firmware_upgrade_e		fw_option;
	vxge_hal_xmac_nwif_behavior_on_failure	port_failure;

	vxge_bw_info_t		bw_info[VXGE_HAL_MAX_FUNCTIONS];
	vxge_isr_info_t		isr_info[VXGE_MAX_MSIX_MESSAGES];

} vxge_config_t;

struct vxge_dev_t {

	device_t ndev;

	bool	is_privilaged;
	bool	is_initialized;
	bool	is_active;
	int	intr_count;
	bool	fw_upgrade;
	int	no_of_vpath;
	u64	active_port;
	u32	no_of_func;
	u32	hw_fw_version;
	u32	max_supported_vpath;
	int	rx_mbuf_sz;
	int	if_flags;
	int	ifm_optics;
	ifnet_t	ifp;

	vxge_hal_xmac_nwif_dp_mode		port_mode;
	vxge_hal_xmac_nwif_l2_switch_status	l2_switch;
	vxge_hal_xmac_nwif_behavior_on_failure	port_failure;

	char	ndev_name[16];
	char	mtx_drv_name[16];

	struct mtx mtx_drv;
	struct ifmedia media;

	vxge_pci_info_t		*pdev;
	vxge_hal_device_t	*devh;
	vxge_vpath_t		*vpaths;
	vxge_config_t		config;
	vxge_hal_device_config_t *device_config;
	vxge_hal_vpath_h	vpath_handles[VXGE_HAL_MAX_VIRTUAL_PATHS];
};

int	vxge_probe(device_t);
int	vxge_attach(device_t);
int	vxge_detach(device_t);
int	vxge_shutdown(device_t);

int	vxge_alloc_resources(vxge_dev_t *);
int	vxge_alloc_isr_resources(vxge_dev_t *);
int	vxge_alloc_bar_resources(vxge_dev_t *, int);
void	vxge_free_resources(device_t, vxge_free_resources_e);
void	vxge_free_isr_resources(vxge_dev_t *);
void	vxge_free_bar_resources(vxge_dev_t *, int);

int	vxge_device_hw_info_get(vxge_dev_t *);
int	vxge_firmware_verify(vxge_dev_t *);

vxge_hal_status_e
vxge_driver_init(vxge_dev_t *);

vxge_hal_status_e
vxge_firmware_upgrade(vxge_dev_t *);

vxge_hal_status_e
vxge_func_mode_set(vxge_dev_t *);

vxge_hal_status_e
vxge_port_mode_set(vxge_dev_t *);

vxge_hal_status_e
vxge_port_behavior_on_failure_set(vxge_dev_t *);

vxge_hal_status_e
vxge_l2switch_mode_set(vxge_dev_t *);

void	vxge_init(void *);
void	vxge_init_locked(vxge_dev_t *);

void	vxge_stop(vxge_dev_t *);
void	vxge_stop_locked(vxge_dev_t *);

void	vxge_reset(vxge_dev_t *);
int	vxge_ifp_setup(device_t);
int	vxge_isr_setup(vxge_dev_t *);

void	vxge_media_init(vxge_dev_t *);
int	vxge_media_change(ifnet_t);
void	vxge_media_status(ifnet_t, struct ifmediareq *);

void	vxge_mutex_init(vxge_dev_t *);
void	vxge_mutex_destroy(vxge_dev_t *);
void	vxge_link_up(vxge_hal_device_h, void *);
void	vxge_link_down(vxge_hal_device_h, void *);
void	vxge_crit_error(vxge_hal_device_h, void *, vxge_hal_event_e, u64);

int	vxge_ioctl(ifnet_t, u_long, caddr_t);
int	vxge_ioctl_regs(vxge_dev_t *, struct ifreq *);
int	vxge_ioctl_stats(vxge_dev_t *, struct ifreq *);
void	vxge_promisc_set(vxge_dev_t *);

void	vxge_vpath_config(vxge_dev_t *);
int	vxge_vpath_open(vxge_dev_t *);
void	vxge_vpath_close(vxge_dev_t *);
void	vxge_vpath_reset(vxge_dev_t *);

int	vxge_change_mtu(vxge_dev_t *, unsigned long);

u32	vxge_ring_length_get(u32);

void	vxge_isr_line(void *);
int	vxge_isr_filter(void *);
void	vxge_isr_msix(void *);
void	vxge_isr_msix_alarm(void *);

void
vxge_intr_coalesce_tx(vxge_vpath_t *);

void
vxge_intr_coalesce_rx(vxge_vpath_t *);

vxge_hal_status_e
vxge_msix_enable(vxge_dev_t *);

vxge_hal_status_e
vxge_rth_config(vxge_dev_t *);

int	vxge_dma_tags_create(vxge_vpath_t *);
void	vxge_device_hw_info_print(vxge_dev_t *);
int	vxge_driver_config(vxge_dev_t *);

#if __FreeBSD_version >= 800000

int
vxge_mq_send(ifnet_t, mbuf_t);

static inline int
vxge_mq_send_locked(ifnet_t, vxge_vpath_t *, mbuf_t);

void
vxge_mq_qflush(ifnet_t);

#endif

void
vxge_send(ifnet_t);

static inline void
vxge_send_locked(ifnet_t, vxge_vpath_t *);

static inline int
vxge_xmit(ifnet_t, vxge_vpath_t *, mbuf_t *);

static inline int
vxge_dma_mbuf_coalesce(bus_dma_tag_t, bus_dmamap_t,
    mbuf_t *, bus_dma_segment_t *, int *);

static inline void
vxge_rx_checksum(vxge_hal_ring_rxd_info_t, mbuf_t);

static inline void
vxge_rx_input(ifnet_t, mbuf_t, vxge_vpath_t *);

static inline vxge_hal_vpath_h
vxge_vpath_handle_get(vxge_dev_t *, int);

static inline int
vxge_vpath_get(vxge_dev_t *, mbuf_t);

void
vxge_tso_config(vxge_dev_t *);

vxge_hal_status_e
vxge_tx_replenish(vxge_hal_vpath_h, vxge_hal_txdl_h, void *,
    u32, void *, vxge_hal_reopen_e);

vxge_hal_status_e
vxge_tx_compl(vxge_hal_vpath_h, vxge_hal_txdl_h, void *,
    vxge_hal_fifo_tcode_e, void *);

void
vxge_tx_term(vxge_hal_vpath_h, vxge_hal_txdl_h, void *,
    vxge_hal_txdl_state_e, void *, vxge_hal_reopen_e);

vxge_hal_status_e
vxge_rx_replenish(vxge_hal_vpath_h, vxge_hal_rxd_h, void *,
    u32, void *, vxge_hal_reopen_e);

vxge_hal_status_e
vxge_rx_compl(vxge_hal_vpath_h, vxge_hal_rxd_h, void *, u8, void *);

void
vxge_rx_term(vxge_hal_vpath_h, vxge_hal_rxd_h, void *,
    vxge_hal_rxd_state_e, void *, vxge_hal_reopen_e);

void
vxge_rx_rxd_1b_get(vxge_vpath_t *, vxge_hal_rxd_h, void *);

int
vxge_rx_rxd_1b_set(vxge_vpath_t *, vxge_hal_rxd_h, void *);

int
vxge_bw_priority_config(vxge_dev_t *);

vxge_hal_status_e
vxge_bw_priority_get(vxge_dev_t *, vxge_bw_info_t *);

int
vxge_bw_priority_set(vxge_dev_t *, struct ifreq *);

int
vxge_bw_priority_update(vxge_dev_t *, u32, bool);

int
vxge_port_mode_update(vxge_dev_t *);

vxge_hal_status_e
vxge_port_mode_get(vxge_dev_t *, vxge_port_info_t *);

void
vxge_pmd_port_type_get(vxge_dev_t *, u32, char *, u8);

void
vxge_active_port_update(vxge_dev_t *);

static inline void
vxge_null_terminate(char *, size_t);

#endif	/* _VXGE_H_ */
