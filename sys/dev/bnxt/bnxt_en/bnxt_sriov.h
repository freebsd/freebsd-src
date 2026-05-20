#ifndef _BNXT_SRIOV_H_
#define _BNXT_SRIOV_H_

#include <sys/iov_schema.h>
#include <linux/pci.h>
#include <dev/pci/pci_iov.h>

#include "opt_global.h"
#include "bnxt.h"

#ifndef PCI_IOV
#define PCI_IOV 1
#endif

/* macro definations */

#define BNXT_MAX_VFS 4
#define BNXT_HWRM_REQ_MAX_SIZE	128
#define BNXT_MAX_VF_CMD_FWD_PAGES  4
#define BNXT_VF_QOS		0x1
#define BNXT_VF_SPOOFCHK	0x2
#define BNXT_VF_LINK_FORCED	0x4
#define BNXT_VF_LINK_UP		0x8
#define BNXT_VF_TRUST		0x10
#define BNXT_VLAN_VID_MASK	0x0fff

#define BNXT_EXEC_FWD_RESP_SIZE_ERR(n)					\
	((offsetof(struct hwrm_exec_fwd_resp_input, encap_request) + n) >\
	 offsetof(struct hwrm_exec_fwd_resp_input, encap_resp_target_id))

#define BNXT_VF_RESV_STRATEGY_MAXIMAL   0
#define BNXT_VF_RESV_STRATEGY_MINIMAL   1
#define BNXT_VF_RESV_STRATEGY_MINIMAL_STATIC    2
#define FUNC_RESOURCE_QCAPS_RESP_FLAGS_MIN_GUARANTEED     0x1UL

#define BNXT_SRIOV_LOCK_INIT(sc, _name)	\
	mtx_init(&(sc)->sriov_lock, _name, "sriov_lock", MTX_DEF | MTX_NOWITNESS)
#define BNXT_SRIOV_LOCK(sc)		mtx_lock(&(sc)->sriov_lock)
#define BNXT_SRIOV_UNLOCK(sc)		mtx_unlock(&(sc)->sriov_lock)
#define BNXT_SRIOV_LOCK_DESTROY(sc)				\
	do {							\
		if (mtx_initialized(&(sc)->sriov_lock))		\
			mtx_destroy(&(sc)->sriov_lock);		\
} while (0)


/* structure declartions/definations */

struct bnxt_softc;

struct bnxt_vf_info {
	uint8_t		vfnum;
	uint16_t	fw_fid;
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	uint8_t		vf_mac_addr[ETHER_ADDR_LEN];
	uint32_t	vlan;
	uint32_t	flags;
	uint32_t	func_qcfg_flags;
	uint32_t	min_tx_rate;
	uint32_t	max_tx_rate;
	uint16_t	min_tx_rings;
	uint16_t	max_tx_rings;
	uint16_t	min_rx_rings;
	uint16_t	max_rx_rings;
	uint16_t	min_cp_rings;
	uint16_t	max_cp_rings;
	uint16_t	min_rsscos_ctxs;
	uint16_t	max_rsscos_ctxs;
	uint16_t	min_stat_ctxs;
	uint16_t	max_stat_ctxs;
	uint16_t	min_ring_grps;
	uint16_t	max_hw_ring_grps;
	uint16_t	min_vnics;
	uint16_t	max_vnics;
	uint16_t	min_irqs;
	uint16_t	max_irqs;
	uint16_t	min_l2_ctxs;
	uint16_t	max_l2_ctxs;
	void		*hwrm_cmd_req_addr;
	bus_addr_t	hwrm_cmd_req_dma_addr;
	struct		iflib_dma_info	hwrm_cmd_req;
	uint16_t	trusted;
	bool		spoofchk;
};

struct bnxt_resc_map {
	uint16_t *min_field;
	uint16_t *max_field;
	uint16_t hw_max;
	uint16_t pf_alloc;
};

/* function prototypes */

void bnxt_sriov_attach(struct bnxt_softc *softc);
int bnxt_iov_init(if_ctx_t ctx, uint16_t num_vfs, const nvlist_t *params);
void bnxt_iov_uninit(if_ctx_t ctx);
int bnxt_iov_vf_add(if_ctx_t ctx, uint16_t vfnum, const nvlist_t *params);
int bnxt_hwrm_func_vf_resource_free(struct bnxt_softc *softc, int num_vfs);
void bnxt_free_vf_resources(struct bnxt_softc *softc);
int bnxt_create_trusted_vf_sysctls(struct bnxt_softc *softc, uint16_t num_vfs);
int bnxt_create_spoofchk_vf_sysctls(struct bnxt_softc *softc, uint16_t num_vfs);
bool bnxt_is_trusted_vf(struct bnxt_softc *bp, struct bnxt_vf_info *vf);
void bnxt_hwrm_exec_fwd_req(struct bnxt_softc *bp);
void bnxt_destroy_trusted_vf_sysctls(struct bnxt_softc *softc);
int bnxt_set_vf_trust(struct bnxt_softc *softc, int vf_id, bool trusted);
int bnxt_approve_mac(struct bnxt_softc *sc);
void bnxt_update_vf_mac(struct bnxt_softc *sc);
bool bnxt_promisc_ok(struct bnxt_softc *softc);
int bnxt_set_vf_spoofchk(struct bnxt_softc *sc, int vf_id, bool enable);
int bnxt_cfg_hw_sriov(struct bnxt_softc *softc, uint16_t *num_vfs, bool reset);
void bnxt_reenable_sriov(struct bnxt_softc *bp);


#endif /* _BNXT_SRIOV_H_ */
