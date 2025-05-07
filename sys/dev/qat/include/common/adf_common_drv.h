/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_DRV_H
#define ADF_DRV_H

#include <dev/pci/pcivar.h>
#include "adf_accel_devices.h"
#include "icp_qat_fw_loader_handle.h"
#include "icp_qat_hal.h"
#include "adf_cfg_user.h"
#include "adf_uio.h"
#include "adf_uio_control.h"

#define QAT_UIO_IOC_MAGIC 'b'
#define ADF_MAJOR_VERSION 0
#define ADF_MINOR_VERSION 6
#define ADF_BUILD_VERSION 0
#define ADF_DRV_VERSION                                                        \
	__stringify(ADF_MAJOR_VERSION) "." __stringify(                        \
	    ADF_MINOR_VERSION) "." __stringify(ADF_BUILD_VERSION)

#define IOCTL_GET_BUNDLE_SIZE _IOR(QAT_UIO_IOC_MAGIC, 0, int32_t)
#define IOCTL_ALLOC_BUNDLE _IOW(QAT_UIO_IOC_MAGIC, 1, int)
#define IOCTL_GET_ACCEL_TYPE _IOR(QAT_UIO_IOC_MAGIC, 2, uint32_t)
#define IOCTL_ADD_MEM_FD _IOW(QAT_UIO_IOC_MAGIC, 3, int)
#define ADF_STATUS_RESTARTING 0
#define ADF_STATUS_STARTING 1
#define ADF_STATUS_CONFIGURED 2
#define ADF_STATUS_STARTED 3
#define ADF_STATUS_AE_INITIALISED 4
#define ADF_STATUS_AE_UCODE_LOADED 5
#define ADF_STATUS_AE_STARTED 6
#define ADF_STATUS_PF_RUNNING 7
#define ADF_STATUS_IRQ_ALLOCATED 8
#define ADF_PCIE_FLR_ATTEMPT 10
#define ADF_STATUS_SYSCTL_CTX_INITIALISED 9

#define PCI_EXP_AERUCS 0x104

/* PMISC BAR upper and lower offsets in PCIe config space */
#define ADF_PMISC_L_OFFSET 0x18
#define ADF_PMISC_U_OFFSET 0x1c

enum adf_dev_reset_mode { ADF_DEV_RESET_ASYNC = 0, ADF_DEV_RESET_SYNC };

enum adf_event {
	ADF_EVENT_INIT = 0,
	ADF_EVENT_START,
	ADF_EVENT_STOP,
	ADF_EVENT_SHUTDOWN,
	ADF_EVENT_RESTARTING,
	ADF_EVENT_RESTARTED,
	ADF_EVENT_ERROR,
};

struct adf_state {
	enum adf_event dev_state;
	int dev_id;
};

struct service_hndl {
	int (*event_hld)(struct adf_accel_dev *accel_dev, enum adf_event event);
	unsigned long init_status[ADF_DEVS_ARRAY_SIZE];
	unsigned long start_status[ADF_DEVS_ARRAY_SIZE];
	char *name;
	struct list_head list;
};

static inline int
get_current_node(void)
{
	return PCPU_GET(domain);
}

int adf_service_register(struct service_hndl *service);
int adf_service_unregister(struct service_hndl *service);

int adf_dev_init(struct adf_accel_dev *accel_dev);
int adf_dev_start(struct adf_accel_dev *accel_dev);
int adf_dev_stop(struct adf_accel_dev *accel_dev);
void adf_dev_shutdown(struct adf_accel_dev *accel_dev);
int adf_dev_autoreset(struct adf_accel_dev *accel_dev);
int adf_dev_reset(struct adf_accel_dev *accel_dev,
		  enum adf_dev_reset_mode mode);
int adf_dev_aer_schedule_reset(struct adf_accel_dev *accel_dev,
			       enum adf_dev_reset_mode mode);
void adf_error_notifier(uintptr_t arg);
int adf_init_fatal_error_wq(void);
void adf_exit_fatal_error_wq(void);
int adf_notify_fatal_error(struct adf_accel_dev *accel_dev);
void adf_devmgr_update_class_index(struct adf_hw_device_data *hw_data);
void adf_clean_vf_map(bool);
int adf_sysctl_add_fw_versions(struct adf_accel_dev *accel_dev);
int adf_sysctl_remove_fw_versions(struct adf_accel_dev *accel_dev);

int adf_ctl_dev_register(void);
void adf_ctl_dev_unregister(void);
int adf_register_ctl_device_driver(void);
void adf_unregister_ctl_device_driver(void);
int adf_processes_dev_register(void);
void adf_processes_dev_unregister(void);
void adf_state_init(void);
void adf_state_destroy(void);
int adf_devmgr_add_dev(struct adf_accel_dev *accel_dev,
		       struct adf_accel_dev *pf);
void adf_devmgr_rm_dev(struct adf_accel_dev *accel_dev,
		       struct adf_accel_dev *pf);
struct list_head *adf_devmgr_get_head(void);
struct adf_accel_dev *adf_devmgr_get_dev_by_id(uint32_t id);
struct adf_accel_dev *adf_devmgr_get_first(void);
struct adf_accel_dev *adf_devmgr_pci_to_accel_dev(device_t pci_dev);
int adf_devmgr_verify_id(uint32_t *id);
void adf_devmgr_get_num_dev(uint32_t *num);
int adf_devmgr_in_reset(struct adf_accel_dev *accel_dev);
int adf_dev_started(struct adf_accel_dev *accel_dev);
int adf_dev_restarting_notify(struct adf_accel_dev *accel_dev);
int adf_dev_restarting_notify_sync(struct adf_accel_dev *accel_dev);
int adf_dev_restarted_notify(struct adf_accel_dev *accel_dev);
int adf_dev_stop_notify_sync(struct adf_accel_dev *accel_dev);
int adf_ae_init(struct adf_accel_dev *accel_dev);
int adf_ae_shutdown(struct adf_accel_dev *accel_dev);
int adf_ae_fw_load(struct adf_accel_dev *accel_dev);
void adf_ae_fw_release(struct adf_accel_dev *accel_dev);
int adf_ae_start(struct adf_accel_dev *accel_dev);
int adf_ae_stop(struct adf_accel_dev *accel_dev);

int adf_aer_store_ppaerucm_reg(device_t pdev,
			       struct adf_hw_device_data *hw_data);

int adf_enable_aer(struct adf_accel_dev *accel_dev, device_t *adf);
void adf_disable_aer(struct adf_accel_dev *accel_dev);
void adf_reset_sbr(struct adf_accel_dev *accel_dev);
void adf_reset_flr(struct adf_accel_dev *accel_dev);
void adf_dev_pre_reset(struct adf_accel_dev *accel_dev);
void adf_dev_post_reset(struct adf_accel_dev *accel_dev);
void adf_dev_restore(struct adf_accel_dev *accel_dev);
int adf_init_aer(void);
void adf_exit_aer(void);
int adf_put_admin_msg_sync(struct adf_accel_dev *accel_dev,
			   u32 ae,
			   void *in,
			   void *out);
struct icp_qat_fw_init_admin_req;
struct icp_qat_fw_init_admin_resp;
int adf_send_admin(struct adf_accel_dev *accel_dev,
		   struct icp_qat_fw_init_admin_req *req,
		   struct icp_qat_fw_init_admin_resp *resp,
		   u32 ae_mask);
int adf_config_device(struct adf_accel_dev *accel_dev);

int adf_init_admin_comms(struct adf_accel_dev *accel_dev);
void adf_exit_admin_comms(struct adf_accel_dev *accel_dev);
int adf_send_admin_init(struct adf_accel_dev *accel_dev);
int adf_get_fw_timestamp(struct adf_accel_dev *accel_dev, u64 *timestamp);
int adf_get_fw_pke_stats(struct adf_accel_dev *accel_dev,
			 u64 *suc_count,
			 u64 *unsuc_count);
int adf_dev_measure_clock(struct adf_accel_dev *accel_dev,
			  u32 *frequency,
			  u32 min,
			  u32 max);
int adf_clock_debugfs_add(struct adf_accel_dev *accel_dev);
u64 adf_clock_get_current_time(void);
int adf_init_arb(struct adf_accel_dev *accel_dev);
int adf_init_gen2_arb(struct adf_accel_dev *accel_dev);
void adf_exit_arb(struct adf_accel_dev *accel_dev);
void adf_disable_arb(struct adf_accel_dev *accel_dev);
void adf_update_ring_arb(struct adf_etr_ring_data *ring);
void adf_enable_ring_arb(struct adf_accel_dev *accel_dev,
			 void *csr_addr,
			 unsigned int bank_nr,
			 unsigned int mask);
void adf_disable_ring_arb(struct adf_accel_dev *accel_dev,
			  void *csr_addr,
			  unsigned int bank_nr,
			  unsigned int mask);
int adf_set_ssm_wdtimer(struct adf_accel_dev *accel_dev);
void adf_update_uio_ring_arb(struct adf_uio_control_bundle *bundle);
struct adf_accel_dev *adf_devmgr_get_dev_by_bdf(struct adf_pci_address *addr);
struct adf_accel_dev *adf_devmgr_get_dev_by_pci_bus(u8 bus);
int adf_get_vf_nr(struct adf_pci_address *vf_pci_addr, int *vf_nr);
u32 adf_get_slices_for_svc(struct adf_accel_dev *accel_dev,
			   enum adf_svc_type svc);
bool adf_is_bdf_equal(struct adf_pci_address *bdf1,
		      struct adf_pci_address *bdf2);
int adf_is_vf_nr_valid(struct adf_accel_dev *accel_dev, int vf_nr);
void adf_dev_get(struct adf_accel_dev *accel_dev);
void adf_dev_put(struct adf_accel_dev *accel_dev);
int adf_dev_in_use(struct adf_accel_dev *accel_dev);
int adf_init_etr_data(struct adf_accel_dev *accel_dev);
void adf_cleanup_etr_data(struct adf_accel_dev *accel_dev);

struct qat_crypto_instance *qat_crypto_get_instance_node(int node);
void qat_crypto_put_instance(struct qat_crypto_instance *inst);
void qat_alg_callback(void *resp);
void qat_alg_asym_callback(void *resp);
int qat_algs_register(void);
void qat_algs_unregister(void);
int qat_asym_algs_register(void);
void qat_asym_algs_unregister(void);

int adf_isr_resource_alloc(struct adf_accel_dev *accel_dev);
void adf_isr_resource_free(struct adf_accel_dev *accel_dev);
int adf_vf_isr_resource_alloc(struct adf_accel_dev *accel_dev);
void adf_vf_isr_resource_free(struct adf_accel_dev *accel_dev);
int adf_pfvf_comms_disabled(struct adf_accel_dev *accel_dev);
int qat_hal_init(struct adf_accel_dev *accel_dev);
void qat_hal_deinit(struct icp_qat_fw_loader_handle *handle);
int qat_hal_start(struct icp_qat_fw_loader_handle *handle);
void qat_hal_stop(struct icp_qat_fw_loader_handle *handle,
		  unsigned char ae,
		  unsigned int ctx_mask);
void qat_hal_reset(struct icp_qat_fw_loader_handle *handle);
int qat_hal_clr_reset(struct icp_qat_fw_loader_handle *handle);
void qat_hal_set_live_ctx(struct icp_qat_fw_loader_handle *handle,
			  unsigned char ae,
			  unsigned int ctx_mask);
int qat_hal_check_ae_active(struct icp_qat_fw_loader_handle *handle,
			    unsigned int ae);
int qat_hal_set_ae_lm_mode(struct icp_qat_fw_loader_handle *handle,
			   unsigned char ae,
			   enum icp_qat_uof_regtype lm_type,
			   unsigned char mode);
void qat_hal_set_ae_tindex_mode(struct icp_qat_fw_loader_handle *handle,
				unsigned char ae,
				unsigned char mode);
void qat_hal_set_ae_scs_mode(struct icp_qat_fw_loader_handle *handle,
			     unsigned char ae,
			     unsigned char mode);
int qat_hal_set_ae_ctx_mode(struct icp_qat_fw_loader_handle *handle,
			    unsigned char ae,
			    unsigned char mode);
int qat_hal_set_ae_nn_mode(struct icp_qat_fw_loader_handle *handle,
			   unsigned char ae,
			   unsigned char mode);
void qat_hal_set_pc(struct icp_qat_fw_loader_handle *handle,
		    unsigned char ae,
		    unsigned int ctx_mask,
		    unsigned int upc);
void qat_hal_wr_uwords(struct icp_qat_fw_loader_handle *handle,
		       unsigned char ae,
		       unsigned int uaddr,
		       unsigned int words_num,
		       const uint64_t *uword);
void qat_hal_wr_coalesce_uwords(struct icp_qat_fw_loader_handle *handle,
				unsigned char ae,
				unsigned int uaddr,
				unsigned int words_num,
				uint64_t *uword);

void qat_hal_wr_umem(struct icp_qat_fw_loader_handle *handle,
		     unsigned char ae,
		     unsigned int uword_addr,
		     unsigned int words_num,
		     unsigned int *data);
int qat_hal_get_ins_num(void);
int qat_hal_batch_wr_lm(struct icp_qat_fw_loader_handle *handle,
			unsigned char ae,
			struct icp_qat_uof_batch_init *lm_init_header);
int qat_hal_init_gpr(struct icp_qat_fw_loader_handle *handle,
		     unsigned char ae,
		     unsigned long ctx_mask,
		     enum icp_qat_uof_regtype reg_type,
		     unsigned short reg_num,
		     unsigned int regdata);
int qat_hal_init_wr_xfer(struct icp_qat_fw_loader_handle *handle,
			 unsigned char ae,
			 unsigned long ctx_mask,
			 enum icp_qat_uof_regtype reg_type,
			 unsigned short reg_num,
			 unsigned int regdata);
int qat_hal_init_rd_xfer(struct icp_qat_fw_loader_handle *handle,
			 unsigned char ae,
			 unsigned long ctx_mask,
			 enum icp_qat_uof_regtype reg_type,
			 unsigned short reg_num,
			 unsigned int regdata);
int qat_hal_init_nn(struct icp_qat_fw_loader_handle *handle,
		    unsigned char ae,
		    unsigned long ctx_mask,
		    unsigned short reg_num,
		    unsigned int regdata);
int qat_hal_wr_lm(struct icp_qat_fw_loader_handle *handle,
		  unsigned char ae,
		  unsigned short lm_addr,
		  unsigned int value);
int qat_uclo_wr_all_uimage(struct icp_qat_fw_loader_handle *handle);
void qat_uclo_del_obj(struct icp_qat_fw_loader_handle *handle);
void qat_uclo_del_mof(struct icp_qat_fw_loader_handle *handle);
int qat_uclo_wr_mimage(struct icp_qat_fw_loader_handle *handle,
		       const void *addr_ptr,
		       int mem_size);
int qat_uclo_map_obj(struct icp_qat_fw_loader_handle *handle,
		     const void *addr_ptr,
		     u32 mem_size,
		     const char *obj_name);

void qat_hal_get_scs_neigh_ae(unsigned char ae, unsigned char *ae_neigh);
int qat_uclo_set_cfg_ae_mask(struct icp_qat_fw_loader_handle *handle,
			     unsigned int cfg_ae_mask);
int adf_init_vf_wq(void);
void adf_exit_vf_wq(void);
void adf_flush_vf_wq(struct adf_accel_dev *accel_dev);
int adf_pf2vf_handle_pf_restarting(struct adf_accel_dev *accel_dev);
int adf_pf2vf_handle_pf_rp_reset(struct adf_accel_dev *accel_dev,
				 struct pfvf_message msg);
int adf_pf2vf_handle_pf_error(struct adf_accel_dev *accel_dev);
bool adf_recv_and_handle_pf2vf_msg(struct adf_accel_dev *accel_dev);
static inline int
adf_sriov_configure(device_t *pdev, int numvfs)
{
	return 0;
}

static inline void
adf_disable_sriov(struct adf_accel_dev *accel_dev)
{
}

static inline void
adf_vf2pf_handler(struct adf_accel_vf_info *vf_info)
{
}

static inline int
adf_init_pf_wq(void)
{
	return 0;
}

static inline void
adf_exit_pf_wq(void)
{
}
#endif
