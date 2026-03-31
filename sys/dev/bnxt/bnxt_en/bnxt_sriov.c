#include <linux/delay.h>
#include <linux/etherdevice.h>

#include "opt_global.h"
#include "bnxt.h"
#include "hsi_struct_def.h"
#include "bnxt_hwrm.h"
#include "bnxt_sriov.h"


static int
bnxt_set_vf_admin_mac(struct bnxt_softc *softc, struct bnxt_vf_info *vf,
		      const uint8_t *mac)
{
	struct hwrm_func_cfg_input req = {0};
	int rc;

	if (!BNXT_PF(softc))
		return (EOPNOTSUPP);

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);

	req.fid = htole16(vf->fw_fid);
	req.enables = htole32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
	memcpy(req.dflt_mac_addr, mac, ETHER_ADDR_LEN);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);

	return (rc);
}

static void
bnxt_vf_parse_schema(struct bnxt_softc *softc, struct bnxt_vf_info *vf,
		     const nvlist_t *params)
{
	const void *mac;
	size_t maclen;

	memset(vf->mac_addr, 0, ETHER_ADDR_LEN);
	memset(vf->vf_mac_addr, 0, ETHER_ADDR_LEN);

	if (params == NULL)
		return;

	if (nvlist_exists(params, "mac-anti-spoof"))
		vf->spoofchk = nvlist_get_bool(params, "mac-anti-spoof");
	if (nvlist_exists(params, "trust"))
		vf->trusted = nvlist_get_bool(params, "trust");

	if (!nvlist_exists(params, "mac-addr"))
		return;

	mac = nvlist_get_binary(params, "mac-addr", &maclen);

	if (maclen != ETHER_ADDR_LEN)
		return;

	if (!is_valid_ether_addr(mac))
		return;

	memcpy(vf->mac_addr, mac, ETHER_ADDR_LEN);
	vf->has_admin_mac = true;
}

/* Add a Virtual Functions */
int
bnxt_iov_vf_add(if_ctx_t ctx, uint16_t vfnum, const nvlist_t *params)
{
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	struct bnxt_vf_info *vf = &softc->pf.vf[vfnum];
	int rc;

	vf->fw_fid = softc->pf.first_vf_id + vfnum;
	vf->vfnum = vfnum;

	/* Parse schema */
	bnxt_vf_parse_schema(softc, vf, params);

	/*
	 * If user provided MAC, program it into firmware.
	 */
	if (vf->has_admin_mac) {
		rc = bnxt_set_vf_admin_mac(softc, vf, vf->mac_addr);
		if (rc)
			device_printf(softc->dev,
				      "vf%u: PF-assigned MAC programming failed (rc=%d), falling back to firmware/default MAC\n",
				      vfnum, rc);
	}

	return 0;
}

/* Free driver-side VF resources (called after hwrm_vf_resc_free) */
void bnxt_free_vf_resources(struct bnxt_softc *softc)
{
	int i;
	size_t page_size = 1UL << softc->pf.vf_hwrm_cmd_req_page_shift;

	softc->pf.active_vfs = 0;

	if (softc->pf.vf) {
		kfree(softc->pf.vf);
		softc->pf.vf = NULL;
	}
	if (softc->pf.vf_event_bmap) {
		kfree(softc->pf.vf_event_bmap);
		softc->pf.vf_event_bmap = NULL;
	}
	for (i = 0; i < softc->pf.hwrm_cmd_req_pages; i++) {
		if (softc->pf.hwrm_cmd_req_addr[i]) {
			dma_free_coherent(&softc->pdev->dev, page_size,
					  softc->pf.hwrm_cmd_req_addr[i],
					  softc->pf.hwrm_cmd_req_dma_addr[i]);
					  softc->pf.hwrm_cmd_req_addr[i] = NULL;
		}
	}
}

/* Free firmware-side VF resources */
int
bnxt_hwrm_func_vf_resource_free(struct bnxt_softc *softc, int num_vfs)
{
	int i, rc;
	struct hwrm_func_vf_resc_free_input req;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_VF_RESC_FREE);

	BNXT_HWRM_LOCK(softc);
	for (i = softc->pf.first_vf_id; i < softc->pf.first_vf_id + num_vfs; i++) {
		req.vf_id = cpu_to_le16(i);
		rc = _hwrm_send_message(softc, &req, sizeof(req));
		if (rc)
			break;
	}
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

/* Free all VF resources */
void bnxt_iov_uninit(if_ctx_t ctx)
{
	int rc;
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	int num_vfs = softc->pf.num_vfs;

	if (!num_vfs)
		return;

	BNXT_SRIOV_LOCK(softc);
	softc->pf.num_vfs = 0;
	BNXT_SRIOV_UNLOCK(softc);

	rc = bnxt_hwrm_func_vf_resource_free(softc, num_vfs);
	if (rc)
		device_printf(softc->dev, "VF resource free HWRM failed: %d\n", rc);

	bnxt_free_vf_resources(softc);
	BNXT_SRIOV_LOCK_DESTROY(softc);
}

static inline int
bnxt_set_vf_resc_field(uint16_t *min_field, uint16_t *max_field,
		       uint16_t hw_max, uint16_t pf_alloc, int num_vfs)
{
	uint16_t val = 0;

	if (num_vfs <= 0)
		return -EINVAL;

	if (hw_max > pf_alloc)
		val = (hw_max - pf_alloc) / num_vfs;

	*min_field = *max_field = cpu_to_le16(val);

	return 0;
}

static int bnxt_set_vf_params(struct bnxt_softc *softc, int vf_id)
{
	struct hwrm_func_cfg_input req = {0};
	struct bnxt_vf_info *vf;
	int rc;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_CFG);

	vf = &softc->pf.vf[vf_id];
	req.fid = cpu_to_le16(vf->fw_fid);


	if (is_valid_ether_addr(vf->mac_addr)) {
		req.enables |= cpu_to_le32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
		memcpy(req.dflt_mac_addr, vf->mac_addr, ETHER_ADDR_LEN);
	}

	if (vf->vlan) {
		req.enables |= cpu_to_le32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN);
		req.dflt_vlan = cpu_to_le16(vf->vlan);
	}

	if (vf->flags & BNXT_VF_TRUST)
		req.flags = cpu_to_le32(HWRM_FUNC_CFG_INPUT_FLAGS_TRUSTED_VF_ENABLE);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);
	if (rc)
		device_printf(softc->dev, "hwrm_func_cfg failed (error:%d)\n", rc);

	return rc;
}

static int
bnxt_hwrm_func_vf_resc_cfg(struct bnxt_softc *softc, int num_vfs, bool reset)
{
	struct hwrm_func_vf_resource_cfg_input req = {0};
	struct bnxt_pf_info *pf = &softc->pf;
	struct bnxt_func_qcfg *fn_qcfg = &softc->fn_qcfg;
	struct bnxt_hw_resc *hw_resc = &softc->hw_resc;
	int i, rc;
	uint16_t msix_val = 0;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_VF_RESOURCE_CFG);
	struct bnxt_resc_map resc_table[] = {
		{ &req.min_tx_rings,		&req.max_tx_rings,	hw_resc->max_tx_rings,		fn_qcfg->alloc_tx_rings },
		{ &req.min_rx_rings,		&req.max_rx_rings,	hw_resc->max_rx_rings,		fn_qcfg->alloc_rx_rings },
		{ &req.min_cmpl_rings,		&req.max_cmpl_rings,	hw_resc->max_cp_rings,		fn_qcfg->alloc_completion_rings },
		{ &req.min_stat_ctx,		&req.max_stat_ctx,	hw_resc->max_stat_ctxs,		fn_qcfg->alloc_stat_ctx },
		{ &req.min_vnics,		&req.max_vnics,		hw_resc->max_vnics,		fn_qcfg->alloc_vnics },
		{ &req.min_hw_ring_grps,	&req.max_hw_ring_grps,	hw_resc->max_hw_ring_grps,	fn_qcfg->alloc_hw_ring_grps },
		{ &req.min_rsscos_ctx,		&req.max_rsscos_ctx,	hw_resc->max_rsscos_ctxs,	fn_qcfg->alloc_rss_ctx },
		{ &req.min_l2_ctxs,		&req.max_l2_ctxs,	hw_resc->max_l2_ctxs,		fn_qcfg->alloc_l2_ctx },
	};

	for (i = 0; i < sizeof(resc_table) / sizeof(resc_table[0]); i++) {
		rc = bnxt_set_vf_resc_field(resc_table[i].min_field,
					    resc_table[i].max_field,
					    resc_table[i].hw_max,
					    resc_table[i].pf_alloc,
					    num_vfs);
		if (rc)
			return rc;
	}

	if (hw_resc->max_irqs > fn_qcfg->alloc_msix && num_vfs > 0)
		msix_val = (hw_resc->max_irqs - fn_qcfg->alloc_msix) / num_vfs;

	req.max_msix = cpu_to_le16(msix_val);

	for (i = 0; i < num_vfs; i++) {
		struct bnxt_vf_info *vf = &pf->vf[i];

		vf->fw_fid = pf->first_vf_id + i;
		if (reset) {
			rc = bnxt_set_vf_params(softc, i);
			if (rc)
				break;
		}

		req.vf_id = cpu_to_le16(vf->fw_fid);

		BNXT_HWRM_LOCK(softc);
		rc = _hwrm_send_message(softc, &req, sizeof(req));
		BNXT_HWRM_UNLOCK(softc);

		if (rc) {
			device_printf(softc->dev, "HWRM_FUNC_VF_RESOURCE_CFG req dump:\n");
			break;
		}

		pf->active_vfs = i + 1;

		vf->min_tx_rings    = le16_to_cpu(req.min_tx_rings);
		vf->min_rx_rings    = le16_to_cpu(req.min_rx_rings);
		vf->min_cp_rings    = le16_to_cpu(req.min_cmpl_rings);
		vf->min_stat_ctxs   = le16_to_cpu(req.min_stat_ctx);
		vf->min_ring_grps   = le16_to_cpu(req.min_hw_ring_grps);
		vf->min_vnics       = le16_to_cpu(req.min_vnics);
	}

	if (pf->active_vfs)
		memcpy(&softc->vf_resc_cfg_input, &req,
		       sizeof(struct hwrm_func_vf_resource_cfg_input));

	return rc;
}

static int
bnxt_hwrm_func_buf_rgtr(struct bnxt_softc *softc)
{
	int rc;
	struct hwrm_func_buf_rgtr_input req;

	bnxt_hwrm_cmd_hdr_init(softc, &req, HWRM_FUNC_BUF_RGTR);

	req.req_buf_num_pages = cpu_to_le16(softc->pf.hwrm_cmd_req_pages);
	req.req_buf_page_size = cpu_to_le16(softc->pf.vf_hwrm_cmd_req_page_shift);
	req.req_buf_len = cpu_to_le16(BNXT_HWRM_REQ_MAX_SIZE);
	req.req_buf_page_addr0 = cpu_to_le64(softc->pf.hwrm_cmd_req_dma_addr[0]);
	req.req_buf_page_addr1 = cpu_to_le64(softc->pf.hwrm_cmd_req_dma_addr[1]);
	req.req_buf_page_addr2 = cpu_to_le64(softc->pf.hwrm_cmd_req_dma_addr[2]);
	req.req_buf_page_addr3 = cpu_to_le64(softc->pf.hwrm_cmd_req_dma_addr[3]);

	BNXT_HWRM_LOCK(softc);
	rc = _hwrm_send_message(softc, &req, sizeof(req));
	BNXT_HWRM_UNLOCK(softc);

	return rc;
}

static void
bnxt_set_vf_attr(struct bnxt_softc *softc, int num_vfs)
{
	int i;
	struct bnxt_vf_info *vf;

	for (i = 0; i < num_vfs; i++) {
		vf = &softc->pf.vf[i];
		memset(vf, 0, sizeof(*vf));
	}
}

static int
bnxt_alloc_vf_resources(struct bnxt_softc *softc, int num_vfs)
{
	struct pci_dev *pdev = softc->pdev;
	u32 nr_pages, size, i, j, k = 0;
	u32 page_size, reqs_per_page;
	void *p;

	p = kcalloc(num_vfs, sizeof(struct bnxt_vf_info), GFP_KERNEL);
	if (!p)
		return ENOMEM;

	rcu_assign_pointer(softc->pf.vf, p);
	bnxt_set_vf_attr(softc, num_vfs);

	size = num_vfs * BNXT_HWRM_REQ_MAX_SIZE;
	page_size = BNXT_PAGE_SIZE;
	softc->pf.vf_hwrm_cmd_req_page_shift = BNXT_PAGE_SHIFT;
	while (size > page_size * BNXT_MAX_VF_CMD_FWD_PAGES) {
		page_size *= 2;
		softc->pf.vf_hwrm_cmd_req_page_shift++;
	}
	nr_pages = DIV_ROUND_UP(size, page_size);
	reqs_per_page = page_size / BNXT_HWRM_REQ_MAX_SIZE;

	for (i = 0; i < nr_pages; i++) {
		softc->pf.hwrm_cmd_req_addr[i] =
			dma_alloc_coherent(&pdev->dev, page_size,
					   &softc->pf.hwrm_cmd_req_dma_addr[i],
					   GFP_ATOMIC);

		if (!softc->pf.hwrm_cmd_req_addr[i])
			return ENOMEM;

		for (j = 0; j < reqs_per_page && k < num_vfs; j++) {
			struct bnxt_vf_info *vf = &softc->pf.vf[k];

			vf->hwrm_cmd_req_addr = (char *)softc->pf.hwrm_cmd_req_addr[i] +
						j * BNXT_HWRM_REQ_MAX_SIZE;
			vf->hwrm_cmd_req_dma_addr =
				softc->pf.hwrm_cmd_req_dma_addr[i] + j *
				BNXT_HWRM_REQ_MAX_SIZE;
			k++;
		}
	}

	softc->pf.vf_event_bmap = kzalloc(ALIGN(DIV_ROUND_UP(num_vfs, 8),
					  sizeof(long)), GFP_ATOMIC);
	if (!softc->pf.vf_event_bmap)
		return ENOMEM;

	softc->pf.hwrm_cmd_req_pages = nr_pages;

	return 0;
}

int bnxt_cfg_hw_sriov(struct bnxt_softc *softc, uint16_t *num_vfs, bool reset)
{
	int rc;

	rc = bnxt_hwrm_func_buf_rgtr(softc);
	if (rc) {
		device_printf(softc->dev, "hwrm func buf rgtr failed (error=%d)\n", rc);
		return (EIO);
	}

	rc = bnxt_hwrm_func_vf_resc_cfg(softc, *num_vfs, reset);
	if (rc) {
		device_printf(softc->dev, "hwrm func VF resc config failed (error=%d)\n", rc);
		return (EIO);
	}

	return (0);
}

int
bnxt_iov_init(if_ctx_t ctx, uint16_t num_vfs, const nvlist_t *params)
{
	int rc;
	if_t ifp = iflib_get_ifp(ctx);
	struct bnxt_softc *softc = iflib_get_softc(ctx);
	bool admin_up = !!(if_getflags(ifp) & IFF_UP);
	bool running  = !!(if_getdrvflags(ifp) & IFF_DRV_RUNNING);

	if (!admin_up || !running) {
		device_printf(softc->dev, "PF is down, rejecting VF creation\n");
		return ENETDOWN;
	}

	if (num_vfs > BNXT_MAX_VFS) {
		device_printf(softc->dev, "Requested %u VFs exceeds maximum supported (%u)\n",
			      num_vfs, BNXT_MAX_VFS);
		return ERANGE;
	}

	/*
	 * Initialize SR-IOV lock before creating any SR-IOV state, so sysctl/VF
	 * paths can safely synchronize and error paths can always destroy it.
	 */
	BNXT_SRIOV_LOCK_INIT(softc, device_get_nameunit(softc->dev));

	rc = bnxt_alloc_vf_resources(softc, num_vfs);
	if (rc) {
		device_printf(softc->dev, "VF resource alloc failed (error=%d)\n", rc);
		goto fail_lock;
	}

	rc = bnxt_cfg_hw_sriov(softc, &num_vfs, false);
	if (rc)
		goto fail_free_vf_resc;


	BNXT_SRIOV_LOCK(softc);
	softc->pf.num_vfs = num_vfs;
	BNXT_SRIOV_UNLOCK(softc);

	return 0;

fail_free_vf_resc:
	bnxt_free_vf_resources(softc);
fail_lock:
	BNXT_SRIOV_LOCK_DESTROY(softc);
	return rc;
}

void bnxt_sriov_attach(struct bnxt_softc *softc)
{
	int rc;
	device_t dev = softc->dev;
	nvlist_t *pf_schema, *vf_schema;

	pf_schema = pci_iov_schema_alloc_node();
	vf_schema = pci_iov_schema_alloc_node();

	/* Optionally add VF-specific attributes to the VF schema */
	pci_iov_schema_add_unicast_mac(vf_schema, "mac-addr", 0, NULL);
	pci_iov_schema_add_bool(vf_schema, "mac-anti-spoof", IOV_SCHEMA_HASDEFAULT, FALSE);
	pci_iov_schema_add_bool(vf_schema, "trust", IOV_SCHEMA_HASDEFAULT, FALSE);

	/* Attach SR-IOV schemas to the device */
	rc = pci_iov_attach(dev, pf_schema, vf_schema);
	if (rc)
		device_printf(dev, "Failed to initialize SR-IOV (error=%d)\n", rc);
}

