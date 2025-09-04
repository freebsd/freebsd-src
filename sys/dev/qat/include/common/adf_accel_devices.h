/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_ACCEL_DEVICES_H_
#define ADF_ACCEL_DEVICES_H_

#include "qat_freebsd.h"
#include "adf_cfg_common.h"
#include "adf_pfvf_msg.h"

#include "opt_qat.h"

#define ADF_CFG_NUM_SERVICES 4

#define ADF_DH895XCC_DEVICE_NAME "dh895xcc"
#define ADF_DH895XCCVF_DEVICE_NAME "dh895xccvf"
#define ADF_C62X_DEVICE_NAME "c6xx"
#define ADF_C62XVF_DEVICE_NAME "c6xxvf"
#define ADF_C3XXX_DEVICE_NAME "c3xxx"
#define ADF_C3XXXVF_DEVICE_NAME "c3xxxvf"
#define ADF_200XX_DEVICE_NAME "200xx"
#define ADF_200XXVF_DEVICE_NAME "200xxvf"
#define ADF_C4XXX_DEVICE_NAME "c4xxx"
#define ADF_C4XXXVF_DEVICE_NAME "c4xxxvf"
#define ADF_4XXX_DEVICE_NAME "4xxx"
#define ADF_4XXXVF_DEVICE_NAME "4xxxvf"
#define ADF_DH895XCC_PCI_DEVICE_ID 0x435
#define ADF_DH895XCCIOV_PCI_DEVICE_ID 0x443
#define ADF_C62X_PCI_DEVICE_ID 0x37c8
#define ADF_C62XIOV_PCI_DEVICE_ID 0x37c9
#define ADF_C3XXX_PCI_DEVICE_ID 0x19e2
#define ADF_C3XXXIOV_PCI_DEVICE_ID 0x19e3
#define ADF_200XX_PCI_DEVICE_ID 0x18ee
#define ADF_200XXIOV_PCI_DEVICE_ID 0x18ef
#define ADF_D15XX_PCI_DEVICE_ID 0x6f54
#define ADF_D15XXIOV_PCI_DEVICE_ID 0x6f55
#define ADF_C4XXX_PCI_DEVICE_ID 0x18a0
#define ADF_C4XXXIOV_PCI_DEVICE_ID 0x18a1
#define ADF_4XXX_PCI_DEVICE_ID 0x4940
#define ADF_4XXXIOV_PCI_DEVICE_ID 0x4941
#define ADF_401XX_PCI_DEVICE_ID 0x4942
#define ADF_401XXIOV_PCI_DEVICE_ID 0x4943
#define ADF_402XX_PCI_DEVICE_ID 0x4944
#define ADF_402XXIOV_PCI_DEVICE_ID 0x4945

#define IS_QAT_GEN3(ID) ({ (ID == ADF_C4XXX_PCI_DEVICE_ID); })
static inline bool
IS_QAT_GEN4(const unsigned int id)
{
	return (id == ADF_4XXX_PCI_DEVICE_ID || id == ADF_401XX_PCI_DEVICE_ID ||
		id == ADF_402XX_PCI_DEVICE_ID ||
		id == ADF_402XXIOV_PCI_DEVICE_ID ||
		id == ADF_4XXXIOV_PCI_DEVICE_ID ||
		id == ADF_401XXIOV_PCI_DEVICE_ID);
}

#define IS_QAT_GEN3_OR_GEN4(ID) (IS_QAT_GEN3(ID) || IS_QAT_GEN4(ID))
#define ADF_VF2PF_SET_SIZE 32
#define ADF_MAX_VF2PF_SET 4
#define ADF_VF2PF_SET_OFFSET(set_nr) ((set_nr)*ADF_VF2PF_SET_SIZE)
#define ADF_VF2PF_VFNR_TO_SET(vf_nr) ((vf_nr) / ADF_VF2PF_SET_SIZE)
#define ADF_VF2PF_VFNR_TO_MASK(vf_nr)                                          \
	({                                                                     \
		u32 vf_nr_ = (vf_nr);                                          \
		BIT((vf_nr_)-ADF_VF2PF_SET_SIZE *ADF_VF2PF_VFNR_TO_SET(        \
		    vf_nr_));                                                  \
	})

#define ADF_DEVICE_FUSECTL_OFFSET 0x40
#define ADF_DEVICE_LEGFUSE_OFFSET 0x4C
#define ADF_DEVICE_FUSECTL_MASK 0x80000000
#define ADF_PCI_MAX_BARS 3
#define ADF_DEVICE_NAME_LENGTH 32
#define ADF_ETR_MAX_RINGS_PER_BANK 16
#define ADF_MAX_MSIX_VECTOR_NAME 32
#define ADF_DEVICE_NAME_PREFIX "qat_"
#define ADF_STOP_RETRY 50
#define ADF_NUM_THREADS_PER_AE (8)
#define ADF_AE_ADMIN_THREAD (7)
#define ADF_NUM_PKE_STRAND (2)
#define ADF_AE_STRAND0_THREAD (8)
#define ADF_AE_STRAND1_THREAD (9)
#define ADF_CFG_NUM_SERVICES 4
#define ADF_SRV_TYPE_BIT_LEN 3
#define ADF_SRV_TYPE_MASK 0x7
#define ADF_RINGS_PER_SRV_TYPE 2
#define ADF_THRD_ABILITY_BIT_LEN 4
#define ADF_THRD_ABILITY_MASK 0xf
#define ADF_VF_OFFSET 0x8
#define ADF_MAX_FUNC_PER_DEV 0x7
#define ADF_PCI_DEV_OFFSET 0x3

#define ADF_SRV_TYPE_BIT_LEN 3
#define ADF_SRV_TYPE_MASK 0x7

#define GET_SRV_TYPE(ena_srv_mask, srv)                                        \
	(((ena_srv_mask) >> (ADF_SRV_TYPE_BIT_LEN * (srv))) & ADF_SRV_TYPE_MASK)

#define GET_CSR_OPS(accel_dev) (&(accel_dev)->hw_device->csr_info.csr_ops)
#define GET_PFVF_OPS(accel_dev) (&(accel_dev)->hw_device->csr_info.pfvf_ops)
#define ADF_DEFAULT_RING_TO_SRV_MAP                                            \
	(CRYPTO | CRYPTO << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                   \
	 NA << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                                \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

enum adf_accel_capabilities {
	ADF_ACCEL_CAPABILITIES_NULL = 0,
	ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC = 1,
	ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC = 2,
	ADF_ACCEL_CAPABILITIES_CIPHER = 4,
	ADF_ACCEL_CAPABILITIES_AUTHENTICATION = 8,
	ADF_ACCEL_CAPABILITIES_COMPRESSION = 32,
	ADF_ACCEL_CAPABILITIES_DEPRECATED = 64,
	ADF_ACCEL_CAPABILITIES_RANDOM_NUMBER = 128
};

struct adf_bar {
	rman_res_t base_addr;
	struct resource *virt_addr;
	rman_res_t size;
} __packed;

struct adf_accel_msix {
	struct msix_entry *entries;
	u32 num_entries;
} __packed;

struct adf_accel_pci {
	device_t pci_dev;
	struct adf_accel_msix msix_entries;
	struct adf_bar pci_bars[ADF_PCI_MAX_BARS];
	uint8_t revid;
	uint8_t sku;
	int node;
} __packed;

enum dev_state { DEV_DOWN = 0, DEV_UP };

enum dev_sku_info {
	DEV_SKU_1 = 0,
	DEV_SKU_2,
	DEV_SKU_3,
	DEV_SKU_4,
	DEV_SKU_VF,
	DEV_SKU_1_CY,
	DEV_SKU_2_CY,
	DEV_SKU_3_CY,
	DEV_SKU_UNKNOWN
};

static inline const char *
get_sku_info(enum dev_sku_info info)
{
	switch (info) {
	case DEV_SKU_1:
		return "SKU1";
	case DEV_SKU_1_CY:
		return "SKU1CY";
	case DEV_SKU_2:
		return "SKU2";
	case DEV_SKU_2_CY:
		return "SKU2CY";
	case DEV_SKU_3:
		return "SKU3";
	case DEV_SKU_3_CY:
		return "SKU3CY";
	case DEV_SKU_4:
		return "SKU4";
	case DEV_SKU_VF:
		return "SKUVF";
	case DEV_SKU_UNKNOWN:
	default:
		break;
	}
	return "Unknown SKU";
}

enum adf_accel_unit_services {
	ADF_ACCEL_SERVICE_NULL = 0,
	ADF_ACCEL_INLINE_CRYPTO = 1,
	ADF_ACCEL_CRYPTO = 2,
	ADF_ACCEL_COMPRESSION = 4,
	ADF_ACCEL_ASYM = 8,
	ADF_ACCEL_ADMIN = 16
};

struct adf_ae_info {
	u32 num_asym_thd;
	u32 num_sym_thd;
	u32 num_dc_thd;
} __packed;

struct adf_accel_unit {
	u8 au_mask;
	u32 accel_mask;
	u64 ae_mask;
	u64 comp_ae_mask;
	u32 num_ae;
	enum adf_accel_unit_services services;
} __packed;

struct adf_accel_unit_info {
	u32 inline_ingress_msk;
	u32 inline_egress_msk;
	u32 sym_ae_msk;
	u32 asym_ae_msk;
	u32 dc_ae_msk;
	u8 num_cy_au;
	u8 num_dc_au;
	u8 num_asym_au;
	u8 num_inline_au;
	struct adf_accel_unit *au;
	const struct adf_ae_info *ae_info;
} __packed;

struct adf_hw_aram_info {
	/* Inline Egress mask. "1" = AE is working with egress traffic */
	u32 inline_direction_egress_mask;
	/* Inline congestion managmenet profiles set in config file */
	u32 inline_congest_mngt_profile;
	/* Initialise CY AE mask, "1" = AE is used for CY operations */
	u32 cy_ae_mask;
	/* Initialise DC AE mask, "1" = AE is used for DC operations */
	u32 dc_ae_mask;
	/* Number of long words used to define the ARAM regions */
	u32 num_aram_lw_entries;
	/* ARAM region definitions */
	u32 mmp_region_size;
	u32 mmp_region_offset;
	u32 skm_region_size;
	u32 skm_region_offset;
	/*
	 * Defines size and offset of compression intermediate buffers stored
	 * in ARAM (device's on-chip memory).
	 */
	u32 inter_buff_aram_region_size;
	u32 inter_buff_aram_region_offset;
	u32 sadb_region_size;
	u32 sadb_region_offset;
} __packed;

struct adf_hw_device_class {
	const char *name;
	const enum adf_device_type type;
	uint32_t instances;
} __packed;

struct arb_info {
	u32 arbiter_offset;
	u32 wrk_thd_2_srv_arb_map;
	u32 wrk_cfg_offset;
} __packed;

struct admin_info {
	u32 admin_msg_ur;
	u32 admin_msg_lr;
	u32 mailbox_offset;
} __packed;

struct adf_hw_csr_ops {
	u64 (*build_csr_ring_base_addr)(bus_addr_t addr, u32 size);
	u32 (*read_csr_ring_head)(struct resource *csr_base_addr,
				  u32 bank,
				  u32 ring);
	void (*write_csr_ring_head)(struct resource *csr_base_addr,
				    u32 bank,
				    u32 ring,
				    u32 value);
	u32 (*read_csr_ring_tail)(struct resource *csr_base_addr,
				  u32 bank,
				  u32 ring);
	void (*write_csr_ring_tail)(struct resource *csr_base_addr,
				    u32 bank,
				    u32 ring,
				    u32 value);
	u32 (*read_csr_e_stat)(struct resource *csr_base_addr, u32 bank);
	void (*write_csr_ring_config)(struct resource *csr_base_addr,
				      u32 bank,
				      u32 ring,
				      u32 value);
	bus_addr_t (*read_csr_ring_base)(struct resource *csr_base_addr,
					 u32 bank,
					 u32 ring);
	void (*write_csr_ring_base)(struct resource *csr_base_addr,
				    u32 bank,
				    u32 ring,
				    bus_addr_t addr);
	void (*write_csr_int_flag)(struct resource *csr_base_addr,
				   u32 bank,
				   u32 value);
	void (*write_csr_int_srcsel)(struct resource *csr_base_addr, u32 bank);
	void (*write_csr_int_col_en)(struct resource *csr_base_addr,
				     u32 bank,
				     u32 value);
	void (*write_csr_int_col_ctl)(struct resource *csr_base_addr,
				      u32 bank,
				      u32 value);
	void (*write_csr_int_flag_and_col)(struct resource *csr_base_addr,
					   u32 bank,
					   u32 value);
	u32 (*read_csr_ring_srv_arb_en)(struct resource *csr_base_addr,
					u32 bank);
	void (*write_csr_ring_srv_arb_en)(struct resource *csr_base_addr,
					  u32 bank,
					  u32 value);
	u32 (*get_src_sel_mask)(void);
	u32 (*get_int_col_ctl_enable_mask)(void);
	u32 (*get_bank_irq_mask)(u32 irq_mask);
};

struct adf_cfg_device_data;
struct adf_accel_dev;
struct adf_etr_data;
struct adf_etr_ring_data;

struct adf_pfvf_ops {
	int (*enable_comms)(struct adf_accel_dev *accel_dev);
	u32 (*get_pf2vf_offset)(u32 i);
	u32 (*get_vf2pf_offset)(u32 i);
	void (*enable_vf2pf_interrupts)(struct resource *pmisc_addr,
					u32 vf_mask);
	void (*disable_all_vf2pf_interrupts)(struct resource *pmisc_addr);
	u32 (*disable_pending_vf2pf_interrupts)(struct resource *pmisc_addr);
	int (*send_msg)(struct adf_accel_dev *accel_dev,
			struct pfvf_message msg,
			u32 pfvf_offset,
			struct mutex *csr_lock);
	struct pfvf_message (*recv_msg)(struct adf_accel_dev *accel_dev,
					u32 pfvf_offset,
					u8 compat_ver);
};

struct adf_hw_csr_info {
	struct adf_hw_csr_ops csr_ops;
	struct adf_pfvf_ops pfvf_ops;
	u32 csr_addr_offset;
	u32 ring_bundle_size;
	u32 bank_int_flag_clear_mask;
	u32 num_rings_per_int_srcsel;
	u32 arb_enable_mask;
};

struct adf_hw_device_data {
	struct adf_hw_device_class *dev_class;
	uint32_t (*get_accel_mask)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_ae_mask)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_sram_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_misc_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_etr_bar_id)(struct adf_hw_device_data *self);
	uint32_t (*get_num_aes)(struct adf_hw_device_data *self);
	uint32_t (*get_num_accels)(struct adf_hw_device_data *self);
	void (*notify_and_wait_ethernet)(struct adf_accel_dev *accel_dev);
	bool (*get_eth_doorbell_msg)(struct adf_accel_dev *accel_dev);
	void (*get_arb_info)(struct arb_info *arb_csrs_info);
	void (*get_admin_info)(struct admin_info *admin_csrs_info);
	void (*get_errsou_offset)(u32 *errsou3, u32 *errsou5);
	uint32_t (*get_num_accel_units)(struct adf_hw_device_data *self);
	int (*init_accel_units)(struct adf_accel_dev *accel_dev);
	void (*exit_accel_units)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_clock_speed)(struct adf_hw_device_data *self);
	enum dev_sku_info (*get_sku)(struct adf_hw_device_data *self);
	bool (*check_prod_sku)(struct adf_accel_dev *accel_dev);
	int (*alloc_irq)(struct adf_accel_dev *accel_dev);
	void (*free_irq)(struct adf_accel_dev *accel_dev);
	void (*enable_error_correction)(struct adf_accel_dev *accel_dev);
	int (*check_uncorrectable_error)(struct adf_accel_dev *accel_dev);
	void (*print_err_registers)(struct adf_accel_dev *accel_dev);
	void (*disable_error_interrupts)(struct adf_accel_dev *accel_dev);
	int (*init_ras)(struct adf_accel_dev *accel_dev);
	void (*exit_ras)(struct adf_accel_dev *accel_dev);
	void (*disable_arb)(struct adf_accel_dev *accel_dev);
	void (*update_ras_errors)(struct adf_accel_dev *accel_dev, int error);
	bool (*ras_interrupts)(struct adf_accel_dev *accel_dev,
			       bool *reset_required);
	int (*init_admin_comms)(struct adf_accel_dev *accel_dev);
	void (*exit_admin_comms)(struct adf_accel_dev *accel_dev);
	int (*send_admin_init)(struct adf_accel_dev *accel_dev);
	void (*set_asym_rings_mask)(struct adf_accel_dev *accel_dev);
	int (*get_ring_to_svc_map)(struct adf_accel_dev *accel_dev,
				   u16 *ring_to_svc_map);
	uint32_t (*get_accel_cap)(struct adf_accel_dev *accel_dev);
	int (*init_arb)(struct adf_accel_dev *accel_dev);
	void (*exit_arb)(struct adf_accel_dev *accel_dev);
	void (*get_arb_mapping)(struct adf_accel_dev *accel_dev,
				const uint32_t **cfg);
	int (*init_device)(struct adf_accel_dev *accel_dev);
	int (*get_heartbeat_status)(struct adf_accel_dev *accel_dev);
	int (*int_timer_init)(struct adf_accel_dev *accel_dev);
	void (*int_timer_exit)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_ae_clock)(struct adf_hw_device_data *self);
	uint32_t (*get_hb_clock)(struct adf_hw_device_data *self);
	void (*disable_iov)(struct adf_accel_dev *accel_dev);
	void (*configure_iov_threads)(struct adf_accel_dev *accel_dev,
				      bool enable);
	void (*enable_ints)(struct adf_accel_dev *accel_dev);
	bool (*check_slice_hang)(struct adf_accel_dev *accel_dev);
	int (*set_ssm_wdtimer)(struct adf_accel_dev *accel_dev);
	void (*enable_pf2vf_interrupt)(struct adf_accel_dev *accel_dev);
	void (*disable_pf2vf_interrupt)(struct adf_accel_dev *accel_dev);
	int (*interrupt_active_pf2vf)(struct adf_accel_dev *accel_dev);
	int (*get_int_active_bundles)(struct adf_accel_dev *accel_dev);
	void (*reset_device)(struct adf_accel_dev *accel_dev);
	void (*reset_hw_units)(struct adf_accel_dev *accel_dev);
	int (*measure_clock)(struct adf_accel_dev *accel_dev);
	void (*restore_device)(struct adf_accel_dev *accel_dev);
	uint32_t (*get_obj_cfg_ae_mask)(struct adf_accel_dev *accel_dev,
					enum adf_accel_unit_services services);
	enum adf_accel_unit_services (
	    *get_service_type)(struct adf_accel_dev *accel_dev, s32 obj_num);
	int (*add_pke_stats)(struct adf_accel_dev *accel_dev);
	void (*remove_pke_stats)(struct adf_accel_dev *accel_dev);
	int (*add_misc_error)(struct adf_accel_dev *accel_dev);
	int (*count_ras_event)(struct adf_accel_dev *accel_dev,
			       u32 *ras_event,
			       char *aeidstr);
	void (*remove_misc_error)(struct adf_accel_dev *accel_dev);
	int (*configure_accel_units)(struct adf_accel_dev *accel_dev);
	int (*ring_pair_reset)(struct adf_accel_dev *accel_dev,
			       u32 bank_number);
	void (*config_ring_irq)(struct adf_accel_dev *accel_dev,
				u32 bank_number,
				u16 ring_mask);
	uint32_t (*get_objs_num)(struct adf_accel_dev *accel_dev);
	const char *(*get_obj_name)(struct adf_accel_dev *accel_dev,
				    enum adf_accel_unit_services services);
	void (*pre_reset)(struct adf_accel_dev *accel_dev);
	void (*post_reset)(struct adf_accel_dev *accel_dev);
	void (*set_msix_rttable)(struct adf_accel_dev *accel_dev);
	void (*get_ring_svc_map_data)(int ring_pair_index,
				      u16 ring_to_svc_map,
				      u8 *serv_type,
				      int *ring_index,
				      int *num_rings_per_srv,
				      int bundle_num);
	struct adf_hw_csr_info csr_info;
	const char *fw_name;
	const char *fw_mmp_name;
	bool reset_ack;
	uint32_t fuses;
	uint32_t accel_capabilities_mask;
	uint32_t instance_id;
	uint16_t accel_mask;
	u32 aerucm_mask;
	u32 ae_mask;
	u32 admin_ae_mask;
	u32 service_mask;
	u32 service_to_load_mask;
	u32 heartbeat_ctr_num;
	uint16_t tx_rings_mask;
	uint8_t tx_rx_gap;
	uint8_t num_banks;
	u8 num_rings_per_bank;
	uint8_t num_accel;
	uint8_t num_logical_accel;
	uint8_t num_engines;
	bool get_ring_to_svc_done;
	int (*get_storage_enabled)(struct adf_accel_dev *accel_dev,
				   uint32_t *storage_enabled);
	u8 query_storage_cap;
	u32 clock_frequency;
	u8 storage_enable;
	u32 extended_dc_capabilities;
	int (*config_device)(struct adf_accel_dev *accel_dev);
	u32 asym_ae_active_thd_mask;
	u16 asym_rings_mask;
	int (*get_fw_image_type)(struct adf_accel_dev *accel_dev,
				 enum adf_cfg_fw_image_type *fw_image_type);
	u16 ring_to_svc_map;
} __packed;

/* helper enum for performing CSR operations */
enum operation {
	AND,
	OR,
};

/* 32-bit CSR write macro */
#define ADF_CSR_WR(csr_base, csr_offset, val)                                  \
	bus_write_4(csr_base, csr_offset, val)

/* 64-bit CSR write macro */
#ifdef __x86_64__
#define ADF_CSR_WR64(csr_base, csr_offset, val)                                \
	bus_write_8(csr_base, csr_offset, val)
#else
static __inline void
adf_csr_wr64(struct resource *csr_base, bus_size_t offset, uint64_t value)
{
	bus_write_4(csr_base, offset, (uint32_t)value);
	bus_write_4(csr_base, offset + 4, (uint32_t)(value >> 32));
}
#define ADF_CSR_WR64(csr_base, csr_offset, val)                                \
	adf_csr_wr64(csr_base, csr_offset, val)
#endif

/* 32-bit CSR read macro */
#define ADF_CSR_RD(csr_base, csr_offset) bus_read_4(csr_base, csr_offset)

/* 64-bit CSR read macro */
#ifdef __x86_64__
#define ADF_CSR_RD64(csr_base, csr_offset) bus_read_8(csr_base, csr_offset)
#else
static __inline uint64_t
adf_csr_rd64(struct resource *csr_base, bus_size_t offset)
{
	return (((uint64_t)bus_read_4(csr_base, offset)) |
		(((uint64_t)bus_read_4(csr_base, offset + 4)) << 32));
}
#define ADF_CSR_RD64(csr_base, csr_offset) adf_csr_rd64(csr_base, csr_offset)
#endif

#define GET_DEV(accel_dev) ((accel_dev)->accel_pci_dev.pci_dev)
#define GET_BARS(accel_dev) ((accel_dev)->accel_pci_dev.pci_bars)
#define GET_HW_DATA(accel_dev) (accel_dev->hw_device)
#define GET_MAX_BANKS(accel_dev) (GET_HW_DATA(accel_dev)->num_banks)
#define GET_DEV_SKU(accel_dev) (accel_dev->accel_pci_dev.sku)
#define GET_NUM_RINGS_PER_BANK(accel_dev)                                      \
	(GET_HW_DATA(accel_dev)->num_rings_per_bank)
#define GET_MAX_ACCELENGINES(accel_dev) (GET_HW_DATA(accel_dev)->num_engines)
#define accel_to_pci_dev(accel_ptr) accel_ptr->accel_pci_dev.pci_dev
#define GET_SRV_TYPE(ena_srv_mask, srv)                                        \
	(((ena_srv_mask) >> (ADF_SRV_TYPE_BIT_LEN * (srv))) & ADF_SRV_TYPE_MASK)
#define SET_ASYM_MASK(asym_mask, srv)                                          \
	({                                                                     \
		typeof(srv) srv_ = (srv);                                      \
		(asym_mask) |= ((1 << (srv_)*ADF_RINGS_PER_SRV_TYPE) |         \
				(1 << ((srv_)*ADF_RINGS_PER_SRV_TYPE + 1)));   \
	})

#define GET_NUM_RINGS_PER_BANK(accel_dev)                                      \
	(GET_HW_DATA(accel_dev)->num_rings_per_bank)
#define GET_MAX_PROCESSES(accel_dev)                                           \
	({                                                                     \
		typeof(accel_dev) dev = (accel_dev);                           \
		(GET_MAX_BANKS(dev) * (GET_NUM_RINGS_PER_BANK(dev) / 2));      \
	})
#define GET_DU_TABLE(accel_dev) (accel_dev->du_table)

static inline void
adf_csr_fetch_and_and(struct resource *csr, size_t offs, unsigned long mask)
{
	unsigned int val = ADF_CSR_RD(csr, offs);

	val &= mask;
	ADF_CSR_WR(csr, offs, val);
}

static inline void
adf_csr_fetch_and_or(struct resource *csr, size_t offs, unsigned long mask)
{
	unsigned int val = ADF_CSR_RD(csr, offs);

	val |= mask;
	ADF_CSR_WR(csr, offs, val);
}

static inline void
adf_csr_fetch_and_update(enum operation op,
			 struct resource *csr,
			 size_t offs,
			 unsigned long mask)
{
	switch (op) {
	case AND:
		adf_csr_fetch_and_and(csr, offs, mask);
		break;
	case OR:
		adf_csr_fetch_and_or(csr, offs, mask);
		break;
	}
}

struct pfvf_stats {
	struct dentry *stats_file;
	/* Messages put in CSR */
	unsigned int tx;
	/* Messages read from CSR */
	unsigned int rx;
	/* Interrupt fired but int bit was clear */
	unsigned int spurious;
	/* Block messages sent */
	unsigned int blk_tx;
	/* Block messages received */
	unsigned int blk_rx;
	/* Blocks received with CRC errors */
	unsigned int crc_err;
	/* CSR in use by other side */
	unsigned int busy;
	/* Receiver did not acknowledge */
	unsigned int no_ack;
	/* Collision detected */
	unsigned int collision;
	/* Couldn't send a response */
	unsigned int tx_timeout;
	/* Didn't receive a response */
	unsigned int rx_timeout;
	/* Responses received */
	unsigned int rx_rsp;
	/* Messages re-transmitted */
	unsigned int retry;
	/* Event put timeout */
	unsigned int event_timeout;
};

#define NUM_PFVF_COUNTERS 14

void adf_get_admin_info(struct admin_info *admin_csrs_info);
struct adf_admin_comms {
	bus_addr_t phy_addr;
	bus_addr_t const_tbl_addr;
	bus_addr_t aram_map_phys_addr;
	bus_addr_t phy_hb_addr;
	bus_dmamap_t aram_map;
	bus_dmamap_t const_tbl_map;
	bus_dmamap_t hb_map;
	char *virt_addr;
	char *virt_hb_addr;
	struct resource *mailbox_addr;
	struct sx lock;
	struct bus_dmamem dma_mem;
	struct bus_dmamem dma_hb;
};

struct icp_qat_fw_loader_handle;
struct adf_fw_loader_data {
	struct icp_qat_fw_loader_handle *fw_loader;
	const struct firmware *uof_fw;
	const struct firmware *mmp_fw;
};

struct adf_accel_vf_info {
	struct adf_accel_dev *accel_dev;
	struct mutex pf2vf_lock; /* protect CSR access for PF2VF messages */
	u32 vf_nr;
	bool init;
	u8 compat_ver;
	struct pfvf_stats pfvf_counters;
};

struct adf_fw_versions {
	u8 fw_version_major;
	u8 fw_version_minor;
	u8 fw_version_patch;
	u8 mmp_version_major;
	u8 mmp_version_minor;
	u8 mmp_version_patch;
};

struct adf_int_timer {
	struct adf_accel_dev *accel_dev;
	struct workqueue_struct *timer_irq_wq;
	struct timer_list timer;
	u32 timeout_val;
	u32 int_cnt;
	bool enabled;
};

#define ADF_COMPAT_CHECKER_MAX 8
typedef int (*adf_iov_compat_checker_t)(struct adf_accel_dev *accel_dev,
					u8 vf_compat_ver);
struct adf_accel_compat_manager {
	u8 num_chker;
	adf_iov_compat_checker_t iov_compat_checkers[ADF_COMPAT_CHECKER_MAX];
};

struct adf_heartbeat;
struct adf_accel_dev {
	struct adf_hw_aram_info *aram_info;
	struct adf_accel_unit_info *au_info;
	struct adf_etr_data *transport;
	struct adf_hw_device_data *hw_device;
	struct adf_cfg_device_data *cfg;
	struct adf_fw_loader_data *fw_loader;
	struct adf_admin_comms *admin;
	struct adf_uio_control_accel *accel;
	struct adf_heartbeat *heartbeat;
	struct adf_int_timer *int_timer;
	struct adf_fw_versions fw_versions;
	unsigned int autoreset_on_error;
	struct adf_fw_counters_data *fw_counters_data;
	struct sysctl_oid *debugfs_ae_config;
	struct list_head crypto_list;
	atomic_t *ras_counters;
	unsigned long status;
	atomic_t ref_count;
	bus_dma_tag_t dma_tag;
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *ras_correctable;
	struct sysctl_oid *ras_uncorrectable;
	struct sysctl_oid *ras_fatal;
	struct sysctl_oid *ras_reset;
	struct sysctl_oid *pke_replay_dbgfile;
	struct sysctl_oid *misc_error_dbgfile;
	struct sysctl_oid *fw_version_oid;
	struct sysctl_oid *mmp_version_oid;
	struct sysctl_oid *hw_version_oid;
	struct sysctl_oid *cnv_error_oid;
	struct list_head list;
	struct adf_accel_pci accel_pci_dev;
	struct adf_accel_compat_manager *cm;
	u8 compat_ver;
#ifdef QAT_DISABLE_SAFE_DC_MODE
	struct sysctl_oid *safe_dc_mode;
	u8 disable_safe_dc_mode;
#endif /* QAT_DISABLE_SAFE_DC_MODE */
	union {
		struct {
			/* vf_info is non-zero when SR-IOV is init'ed */
			struct adf_accel_vf_info *vf_info;
			int num_vfs;
		} pf;
		struct {
			bool irq_enabled;
			struct resource *irq;
			void *cookie;
			struct task pf2vf_bh_tasklet;
			struct mutex vf2pf_lock; /* protect CSR access */
			struct completion msg_received;
			struct pfvf_message
			    response; /* temp field holding pf2vf response */
			enum ring_reset_result rpreset_sts;
			struct mutex rpreset_lock; /* protect rpreset_sts */
			struct pfvf_stats pfvf_counters;
			u8 pf_compat_ver;
		} vf;
	} u1;
	bool is_vf;
	u32 accel_id;
	void *lac_dev;
	struct mutex lock; /* protect accel_dev during start/stop e.t.c */
};
#endif
