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

#ifndef	VXGE_HAL_VIRTUALPATH_H
#define	VXGE_HAL_VIRTUALPATH_H

__EXTERN_BEGIN_DECLS

struct __hal_device_t;


/*
 * struct __hal_virtualpath_t - Virtual Path
 *
 * @vp_id: Virtual path id
 * @vp_open: This flag specifies if vxge_hal_vp_open is called from LL Driver
 * @hldev: Hal device
 * @vp_config: Virtual Path Config
 * @vp_reg: VPATH Register map address in BAR0
 * @vpmgmt_reg: VPATH_MGMT register map address
 * @is_first_vpath: 1 if this first vpath in this vfunc, 0 otherwise
 * @promisc_en: Promisc mode state flag.
 * @min_bandwidth: Guaranteed Band Width in Mbps
 * @max_bandwidth: Maximum Band Width in Mbps
 * @max_mtu: Max mtu that can be supported
 * @sess_grps_available: The mask of available session groups for this vpath
 * @bmap_root_assigned: The bitmap root for this vpath
 * @vsport_choices: The mask of vsports that are available for this vpath
 * @vsport_number: vsport attached to this vpath
 * @sess_grp_start: Session oid start
 * @sess_grp_end: session oid end
 * @max_kdfc_db: Maximum kernel mode doorbells
 * @max_nofl_db: Maximum non offload doorbells
 * @max_ofl_db: Maximum offload doorbells
 * @max_msg_db: Maximum message doorbells
 * @rxd_mem_size: Maximum RxD memory size
 * @tx_intr_num: Interrupt Number associated with the TX
 * @rx_intr_num: Interrupt Number associated with the RX
 * @einta_intr_num: Interrupt Number associated with Emulated MSIX DeAssert IntA
 * @bmap_intr_num: Interrupt Number associated with the bitmap
 * @nce_oid_db: NCE ID database
 * @session_oid_db: Session Object Id database
 * @active_lros: Active LRO session list
 * @active_lro_count: Active LRO count
 * @free_lros: Free LRO session list
 * @free_lro_count: Free LRO count
 * @lro_lock: LRO session lists' lock
 * @sqs: List of send queues
 * @sq_lock: Lock for operations on sqs
 * @srqs: List of SRQs
 * @srq_lock: Lock for operations on srqs
 * @srq_oid_db: DRQ object id database
 * @cqrqs: CQRQs
 * @cqrq_lock: Lock for operations on cqrqs
 * @cqrq_oid_db: CQRQ object id database
 * @umqh: UP Message Queue
 * @dmqh: Down Message Queue
 * @umq_dmq_ir: The adapter will overwrite and update this location as Messages
 *		are read from DMQ and written into UMQ.
 * @umq_dmq_ir_reg_entry: Reg entry of umq_dmq_ir_t
 * @ringh: Ring Queue
 * @fifoh: FIFO Queue
 * @vpath_handles: Virtual Path handles list
 * @vpath_handles_lock: Lock for operations on Virtual Path handles list
 * @stats_block: Memory for DMAing stats
 * @stats: Vpath statistics
 *
 * Virtual path structure to encapsulate the data related to a virtual path.
 * Virtual paths are allocated by the HAL upon getting configuration from the
 * driver and inserted into the list of virtual paths.
 */
typedef struct __hal_virtualpath_t {
	u32				vp_id;

	u32				vp_open;
#define	VXGE_HAL_VP_NOT_OPEN		0
#define	VXGE_HAL_VP_OPEN		1

	struct __hal_device_t		*hldev;
	vxge_hal_vp_config_t		*vp_config;
	vxge_hal_vpath_reg_t		*vp_reg;
	vxge_hal_vpmgmt_reg_t		*vpmgmt_reg;
	__hal_non_offload_db_wrapper_t	*nofl_db;
	__hal_messaging_db_wrapper_t	*msg_db;
	u32				is_first_vpath;

	u32				promisc_en;
#define	VXGE_HAL_VP_PROMISC_ENABLE	1
#define	VXGE_HAL_VP_PROMISC_DISABLE	0

	u32				min_bandwidth;
	u32				max_bandwidth;

	u32				max_mtu;
	u64				sess_grps_available;
	u32				bmap_root_assigned;
	u32				vsport_choices;
	u32				vsport_number;
	u32				sess_grp_start;
	u32				sess_grp_end;
	u32				max_kdfc_db;
	u32				max_nofl_db;
	u32				max_ofl_db;
	u32				max_msg_db;
	u32				rxd_mem_size;
	u32				tx_intr_num;
	u32				rx_intr_num;
	u32				einta_intr_num;
	u32				bmap_intr_num;

	u64				tim_tti_cfg1_saved;
	u64				tim_tti_cfg3_saved;
	u64				tim_rti_cfg1_saved;
	u64				tim_rti_cfg3_saved;


	vxge_hal_ring_h			ringh;
	vxge_hal_fifo_h			fifoh;
	vxge_list_t			vpath_handles;
	spinlock_t			vpath_handles_lock;
	__hal_blockpool_entry_t		*stats_block;
	vxge_hal_vpath_stats_hw_info_t	*hw_stats;
	vxge_hal_vpath_stats_hw_info_t	*hw_stats_sav;
	vxge_hal_vpath_stats_sw_info_t	*sw_stats;
} __hal_virtualpath_t;

/*
 * struct __hal_vpath_handle_t - List item to store callback information
 * @item: List head to keep the item in linked list
 * @vpath: Virtual path to which this item belongs
 * @cb_fn: Callback function to be called
 * @client_handle: Client handle to be returned with the callback
 *
 * This structure is used to store the callback information.
 */
typedef struct __hal_vpath_handle_t {
	vxge_list_t			item;
	__hal_virtualpath_t		*vpath;
	vxge_hal_vpath_callback_f	cb_fn;
	vxge_hal_client_h		client_handle;
} __hal_vpath_handle_t;


#define	VXGE_HAL_VIRTUAL_PATH_HANDLE(vpath)				\
		((vxge_hal_vpath_h)(vpath)->vpath_handles.next)

#define	VXGE_HAL_VPATH_STATS_PIO_READ(offset) {				\
	status = __hal_vpath_stats_access(vpath,			\
			VXGE_HAL_STATS_OP_READ,				\
			offset,						\
			&val64);					\
	if (status != VXGE_HAL_OK) {					\
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",	\
		    __FILE__, __func__, __LINE__, status);		\
		return (status);					\
	}								\
}

vxge_hal_status_e
__hal_vpath_size_quantum_set(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_mgmt_read(
    struct __hal_device_t *hldev,
    __hal_virtualpath_t *vpath);

vxge_hal_status_e
__hal_vpath_pci_read(
    struct __hal_device_t *hldev,
    u32 vp_id,
    u32 offset,
    u32 length,
    void *val);

vxge_hal_status_e
__hal_vpath_reset_check(
    __hal_virtualpath_t *vpath);

vxge_hal_status_e
__hal_vpath_fw_memo_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u32 action,
    u64 param_index,
    u64 *data0,
    u64 *data1);

vxge_hal_status_e
__hal_vpath_fw_flash_ver_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    vxge_hal_device_version_t *fw_version,
    vxge_hal_device_date_t *fw_date,
    vxge_hal_device_version_t *flash_version,
    vxge_hal_device_date_t *flash_date);

vxge_hal_status_e
__hal_vpath_card_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u8 *serial_number,
    u8 *part_number,
    u8 *product_description);

vxge_hal_status_e
__hal_vpath_pmd_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u32 *ports,
    vxge_hal_device_pmd_info_t *pmd_port0,
    vxge_hal_device_pmd_info_t *pmd_port1);

u64
__hal_vpath_pci_func_mode_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg);

vxge_hal_device_lag_mode_e
__hal_vpath_lag_mode_get(
    __hal_virtualpath_t *vpath);

u64
__hal_vpath_vpath_map_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    u32 vh,
    u32 func,
    vxge_hal_vpath_reg_t *vpath_reg);

vxge_hal_status_e
__hal_vpath_fw_upgrade(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u8 *buffer,
    u32 length);

vxge_hal_status_e
__hal_vpath_pcie_func_mode_set(
    struct __hal_device_t *hldev,
    u32 vp_id,
    u32 func_mode);

vxge_hal_status_e
__hal_vpath_flick_link_led(
    struct __hal_device_t *hldev,
    u32 vp_id,
    u32 port,
    u32 on_off);

vxge_hal_status_e
__hal_vpath_udp_rth_set(
    struct __hal_device_t *hldev,
    u32 vp_id,
    u32 on_off);

vxge_hal_status_e
__hal_vpath_rts_table_get(
    vxge_hal_vpath_h vpath_handle,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 *data1,
    u64 *data2);

vxge_hal_status_e
__hal_vpath_rts_table_set(
    vxge_hal_vpath_h vpath_handle,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 data1,
    u64 data2);


vxge_hal_status_e
__hal_vpath_hw_reset(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_sw_reset(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_prc_configure(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_kdfc_configure(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_mac_configure(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_tim_configure(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_hw_initialize(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vp_initialize(
    vxge_hal_device_h devh,
    u32 vp_id,
    vxge_hal_vp_config_t *config);

void
__hal_vp_terminate(
    vxge_hal_device_h devh,
    u32 vp_id);

vxge_hal_status_e
__hal_vpath_hw_addr_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    macaddr_t macaddr,
    macaddr_t macaddr_mask);


vxge_hal_status_e
__hal_vpath_intr_enable(
    __hal_virtualpath_t *vpath);

vxge_hal_status_e
__hal_vpath_intr_disable(
    __hal_virtualpath_t *vpath);

vxge_hal_device_link_state_e
__hal_vpath_link_state_test(
    __hal_virtualpath_t *vpath);

vxge_hal_device_link_state_e
__hal_vpath_link_state_poll(
    __hal_virtualpath_t *vpath);

vxge_hal_device_data_rate_e
__hal_vpath_data_rate_poll(
    __hal_virtualpath_t *vpath);

vxge_hal_status_e
__hal_vpath_alarm_process(
    __hal_virtualpath_t *vpath,
    u32 skip_alarms);

vxge_hal_status_e
__hal_vpath_stats_access(
    __hal_virtualpath_t *vpath,
    u32 operation,
    u32 offset,
    u64 *stat);

vxge_hal_status_e
__hal_vpath_xmac_tx_stats_get(
    __hal_virtualpath_t *vpath,
    vxge_hal_xmac_vpath_tx_stats_t *vpath_tx_stats);

vxge_hal_status_e
__hal_vpath_xmac_rx_stats_get(
    __hal_virtualpath_t *vpath,
    vxge_hal_xmac_vpath_rx_stats_t *vpath_rx_stats);


vxge_hal_status_e
__hal_vpath_hw_stats_get(
    __hal_virtualpath_t *vpath,
    vxge_hal_vpath_stats_hw_info_t *hw_stats);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_VIRTUALPATH_H */
