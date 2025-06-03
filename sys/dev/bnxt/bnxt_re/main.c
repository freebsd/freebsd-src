/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Main component of the bnxt_re driver
 */

#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <dev/mlx5/port.h>
#include <dev/mlx5/vport.h>
#include <linux/list.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include <linux/in.h>
#include <linux/etherdevice.h>

#include "bnxt_re.h"
#include "ib_verbs.h"
#include "bnxt_re-abi.h"
#include "bnxt.h"

static char drv_version[] =
		"Broadcom NetXtreme-C/E RoCE Driver " ROCE_DRV_MODULE_NAME \
		" v" ROCE_DRV_MODULE_VERSION " (" ROCE_DRV_MODULE_RELDATE ")\n";

#define BNXT_RE_DESC	"Broadcom NetXtreme RoCE"
#define BNXT_ADEV_NAME "if_bnxt"

MODULE_DESCRIPTION("Broadcom NetXtreme-C/E RoCE Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEPEND(bnxt_re, linuxkpi, 1, 1, 1);
MODULE_DEPEND(bnxt_re, ibcore, 1, 1, 1);
MODULE_DEPEND(bnxt_re, if_bnxt, 1, 1, 1);
MODULE_VERSION(bnxt_re, 1);


DEFINE_MUTEX(bnxt_re_mutex); /* mutex lock for driver */

static unsigned int restrict_mrs = 0;
module_param(restrict_mrs, uint, 0);
MODULE_PARM_DESC(restrict_mrs, " Restrict the no. of MRs 0 = 256K , 1 = 64K");

unsigned int restrict_stats = 0;
module_param(restrict_stats, uint, 0);
MODULE_PARM_DESC(restrict_stats, "Restrict stats query frequency to ethtool coalesce value. Disabled by default");

unsigned int enable_fc = 1;
module_param(enable_fc, uint, 0);
MODULE_PARM_DESC(enable_fc, "Enable default PFC, CC,ETS during driver load. 1 - fc enable, 0 - fc disable - Default is 1");

unsigned int min_tx_depth = 1;
module_param(min_tx_depth, uint, 0);
MODULE_PARM_DESC(min_tx_depth, "Minimum TX depth - Default is 1");

static uint8_t max_msix_vec[BNXT_RE_MAX_DEVICES] = {0};
static unsigned int max_msix_vec_argc;
module_param_array(max_msix_vec, byte, &max_msix_vec_argc, 0444);
MODULE_PARM_DESC(max_msix_vec, "Max MSI-x vectors per PF (2 - 64) - Default is 64");

unsigned int cmdq_shadow_qd = RCFW_CMD_NON_BLOCKING_SHADOW_QD;
module_param_named(cmdq_shadow_qd, cmdq_shadow_qd, uint, 0644);
MODULE_PARM_DESC(cmdq_shadow_qd, "Perf Stat Debug: Shadow QD Range (1-64) - Default is 64");


/* globals */
struct list_head bnxt_re_dev_list = LINUX_LIST_HEAD_INIT(bnxt_re_dev_list);
static int bnxt_re_probe_count;

DEFINE_MUTEX(bnxt_re_dev_lock);
static u32 gmod_exit;
static u32 gadd_dev_inprogress;

static void bnxt_re_task(struct work_struct *work_task);
static struct workqueue_struct *bnxt_re_wq;
static int bnxt_re_query_hwrm_intf_version(struct bnxt_re_dev *rdev);
static int bnxt_re_hwrm_qcfg(struct bnxt_re_dev *rdev, u32 *db_len,
			     u32 *offset);
static int bnxt_re_ib_init(struct bnxt_re_dev *rdev);
static void bnxt_re_ib_init_2(struct bnxt_re_dev *rdev);
void _bnxt_re_remove(struct auxiliary_device *adev);
void writel_fbsd(struct bnxt_softc *bp, u32, u8, u32);
u32 readl_fbsd(struct bnxt_softc *bp, u32, u8);
static int bnxt_re_hwrm_dbr_pacing_qcfg(struct bnxt_re_dev *rdev);

int bnxt_re_register_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
	rc = register_netdevice_notifier(nb);
	return rc;
}

int bnxt_re_unregister_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
	rc = unregister_netdevice_notifier(nb);
	return rc;
}

void bnxt_re_set_dma_device(struct ib_device *ibdev, struct bnxt_re_dev *rdev)
{
	ibdev->dma_device = &rdev->en_dev->pdev->dev;
}

void bnxt_re_init_resolve_wq(struct bnxt_re_dev *rdev)
{
	rdev->resolve_wq = create_singlethread_workqueue("bnxt_re_resolve_wq");
	 INIT_LIST_HEAD(&rdev->mac_wq_list);
}

void bnxt_re_uninit_resolve_wq(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_resolve_dmac_work *tmp_work = NULL, *tmp_st;
	if (!rdev->resolve_wq)
		return;
	flush_workqueue(rdev->resolve_wq);
	list_for_each_entry_safe(tmp_work, tmp_st, &rdev->mac_wq_list, list) {
			list_del(&tmp_work->list);
			kfree(tmp_work);
	}
	destroy_workqueue(rdev->resolve_wq);
	rdev->resolve_wq = NULL;
}

u32 readl_fbsd(struct bnxt_softc *bp, u32 reg_off, u8 bar_idx)
{

	if (bar_idx)
		return bus_space_read_8(bp->doorbell_bar.tag, bp->doorbell_bar.handle, reg_off);
	else
		return bus_space_read_8(bp->hwrm_bar.tag, bp->hwrm_bar.handle, reg_off);
}

void writel_fbsd(struct bnxt_softc *bp, u32 reg_off, u8 bar_idx, u32 val)
{
	if (bar_idx)
		bus_space_write_8(bp->doorbell_bar.tag, bp->doorbell_bar.handle, reg_off, htole32(val));
	else
		bus_space_write_8(bp->hwrm_bar.tag, bp->hwrm_bar.handle, reg_off, htole32(val));
}

static void bnxt_re_update_fifo_occup_slabs(struct bnxt_re_dev *rdev,
					    u32 fifo_occup)
{
	if (fifo_occup > rdev->dbg_stats->dbq.fifo_occup_water_mark)
		rdev->dbg_stats->dbq.fifo_occup_water_mark = fifo_occup;

	if (fifo_occup > 8 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_4++;
	else if (fifo_occup > 4 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_3++;
	else if (fifo_occup > 2 * rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_2++;
	else if (fifo_occup > rdev->pacing_algo_th)
		rdev->dbg_stats->dbq.fifo_occup_slab_1++;
}

static void bnxt_re_update_do_pacing_slabs(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;

	if (pacing_data->do_pacing > rdev->dbg_stats->dbq.do_pacing_water_mark)
		rdev->dbg_stats->dbq.do_pacing_water_mark = pacing_data->do_pacing;

	if (pacing_data->do_pacing > 16 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_5++;
	else if (pacing_data->do_pacing > 8 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_4++;
	else if (pacing_data->do_pacing > 4 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_3++;
	else if (pacing_data->do_pacing > 2 * rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_2++;
	else if (pacing_data->do_pacing > rdev->dbr_def_do_pacing)
		rdev->dbg_stats->dbq.do_pacing_slab_1++;
}

static bool bnxt_re_is_qp1_qp(struct bnxt_re_qp *qp)
{
	return qp->ib_qp.qp_type == IB_QPT_GSI;
}

static struct bnxt_re_qp *bnxt_re_get_qp1_qp(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

	mutex_lock(&rdev->qp_lock);
	list_for_each_entry(qp, &rdev->qp_list, list) {
		if (bnxt_re_is_qp1_qp(qp)) {
			mutex_unlock(&rdev->qp_lock);
			return qp;
		}
	}
	mutex_unlock(&rdev->qp_lock);
	return NULL;
}

/* Set the maximum number of each resource that the driver actually wants
 * to allocate. This may be up to the maximum number the firmware has
 * reserved for the function. The driver may choose to allocate fewer
 * resources than the firmware maximum.
 */
static void bnxt_re_limit_pf_res(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_max_res dev_res = {};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_qplib_dev_attr *attr;
	struct bnxt_qplib_ctx *hctx;
	int i;

	attr = rdev->dev_attr;
	hctx = rdev->qplib_res.hctx;
	cctx = rdev->chip_ctx;

	bnxt_qplib_max_res_supported(cctx, &rdev->qplib_res, &dev_res, false);
	if (!_is_chip_gen_p5_p7(cctx)) {
		hctx->qp_ctx.max = min_t(u32, dev_res.max_qp, attr->max_qp);
		hctx->mrw_ctx.max = min_t(u32, dev_res.max_mr, attr->max_mr);
		/* To accommodate 16k MRs and 16k AHs,
		 * driver has to allocate 32k backing store memory
		 */
		hctx->mrw_ctx.max *= 2;
		hctx->srq_ctx.max = min_t(u32, dev_res.max_srq, attr->max_srq);
		hctx->cq_ctx.max = min_t(u32, dev_res.max_cq, attr->max_cq);
		for (i = 0; i < MAX_TQM_ALLOC_REQ; i++)
			hctx->tqm_ctx.qcount[i] = attr->tqm_alloc_reqs[i];
	} else {
		hctx->qp_ctx.max = attr->max_qp ? attr->max_qp : dev_res.max_qp;
		hctx->mrw_ctx.max = attr->max_mr ? attr->max_mr : dev_res.max_mr;
		hctx->srq_ctx.max = attr->max_srq ? attr->max_srq : dev_res.max_srq;
		hctx->cq_ctx.max = attr->max_cq ? attr->max_cq : dev_res.max_cq;
	}
}

static void bnxt_re_limit_vf_res(struct bnxt_re_dev *rdev,
				 struct bnxt_qplib_vf_res *vf_res,
				 u32 num_vf)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	struct bnxt_qplib_max_res dev_res = {};

	bnxt_qplib_max_res_supported(cctx, &rdev->qplib_res, &dev_res, true);
	vf_res->max_qp = dev_res.max_qp / num_vf;
	vf_res->max_srq = dev_res.max_srq / num_vf;
	vf_res->max_cq = dev_res.max_cq / num_vf;
	/*
	 * MR and AH shares the same backing store, the value specified
	 * for max_mrw is split into half by the FW for MR and AH
	 */
	vf_res->max_mrw = dev_res.max_mr * 2 / num_vf;
	vf_res->max_gid = BNXT_RE_MAX_GID_PER_VF;
}

static void bnxt_re_set_resource_limits(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;

	hctx = rdev->qplib_res.hctx;
	memset(&hctx->vf_res, 0, sizeof(struct bnxt_qplib_vf_res));
	bnxt_re_limit_pf_res(rdev);

	if (rdev->num_vfs)
		bnxt_re_limit_vf_res(rdev, &hctx->vf_res, rdev->num_vfs);
}

static void bnxt_re_dettach_irq(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_rcfw *rcfw = NULL;
	struct bnxt_qplib_nq *nq;
	int indx;

	rcfw = &rdev->rcfw;
	for (indx = 0; indx < rdev->nqr.max_init; indx++) {
		nq = &rdev->nqr.nq[indx];
		mutex_lock(&nq->lock);
		bnxt_qplib_nq_stop_irq(nq, false);
		mutex_unlock(&nq->lock);
	}

	bnxt_qplib_rcfw_stop_irq(rcfw, false);
}

static void bnxt_re_detach_err_device(struct bnxt_re_dev *rdev)
{
	/* Free the MSIx vectors only so that L2 can proceed with MSIx disable */
	bnxt_re_dettach_irq(rdev);

	/* Set the state as detached to prevent sending any more commands */
	set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
	set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
	wake_up_all(&rdev->rcfw.cmdq.waitq);
}

#define MAX_DSCP_PRI_TUPLE	64

struct bnxt_re_dcb_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	struct hwrm_async_event_cmpl cmpl;
};

static void bnxt_re_init_dcb_wq(struct bnxt_re_dev *rdev)
{
	rdev->dcb_wq = create_singlethread_workqueue("bnxt_re_dcb_wq");
}

static void bnxt_re_uninit_dcb_wq(struct bnxt_re_dev *rdev)
{
	if (!rdev->dcb_wq)
		return;
	flush_workqueue(rdev->dcb_wq);
	destroy_workqueue(rdev->dcb_wq);
	rdev->dcb_wq = NULL;
}

static void bnxt_re_init_aer_wq(struct bnxt_re_dev *rdev)
{
	rdev->aer_wq = create_singlethread_workqueue("bnxt_re_aer_wq");
}

static void bnxt_re_uninit_aer_wq(struct bnxt_re_dev *rdev)
{
	if (!rdev->aer_wq)
		return;
	flush_workqueue(rdev->aer_wq);
	destroy_workqueue(rdev->aer_wq);
	rdev->aer_wq = NULL;
}

static int bnxt_re_update_qp1_tos_dscp(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		return 0;

	qp = bnxt_re_get_qp1_qp(rdev);
	if (!qp)
		return 0;

	qp->qplib_qp.modify_flags = CMDQ_MODIFY_QP_MODIFY_MASK_TOS_DSCP;
	qp->qplib_qp.tos_dscp = rdev->cc_param.qp1_tos_dscp;

	return bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
}

static void bnxt_re_reconfigure_dscp(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param;
	struct bnxt_re_tc_rec *tc_rec;
	bool update_cc = false;
	u8 dscp_user;
	int rc;

	cc_param = &rdev->cc_param;
	tc_rec = &rdev->tc_rec[0];

	if (!(cc_param->roce_dscp_user || cc_param->cnp_dscp_user))
		return;

	if (cc_param->cnp_dscp_user) {
		dscp_user = (cc_param->cnp_dscp_user & 0x3f);
		if ((tc_rec->cnp_dscp_bv & (1ul << dscp_user)) &&
		    (cc_param->alt_tos_dscp != dscp_user)) {
			cc_param->alt_tos_dscp = dscp_user;
			cc_param->mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP;
			update_cc = true;
		}
	}

	if (cc_param->roce_dscp_user) {
		dscp_user = (cc_param->roce_dscp_user & 0x3f);
		if ((tc_rec->roce_dscp_bv & (1ul << dscp_user)) &&
		    (cc_param->tos_dscp != dscp_user)) {
			cc_param->tos_dscp = dscp_user;
			cc_param->mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
			update_cc = true;
		}
	}

	if (update_cc) {
		rc = bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param);
		if (rc)
			dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	}
}

static void bnxt_re_dcb_wq_task(struct work_struct *work)
{
	struct bnxt_qplib_cc_param *cc_param;
	struct bnxt_re_tc_rec *tc_rec;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_dcb_work *dcb_work =
			container_of(work, struct bnxt_re_dcb_work, work);
	int rc;

	rdev = dcb_work->rdev;
	if (!rdev)
		goto exit;

	mutex_lock(&rdev->cc_lock);

	cc_param = &rdev->cc_param;
	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query ccparam rc:%d", rc);
		goto fail;
	}
	tc_rec = &rdev->tc_rec[0];
	/*
	 * Upon the receival of DCB Async event:
	 *   If roce_dscp or cnp_dscp or both (which user configured using configfs)
	 *   is in the list, re-program the value using modify_roce_cc command
	 */
	bnxt_re_reconfigure_dscp(rdev);

	cc_param->roce_pri = tc_rec->roce_prio;
	if (cc_param->qp1_tos_dscp != cc_param->tos_dscp) {
		cc_param->qp1_tos_dscp = cc_param->tos_dscp;
		rc = bnxt_re_update_qp1_tos_dscp(rdev);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "%s:Failed to modify QP1 rc:%d",
				__func__, rc);
			goto fail;
		}
	}

fail:
	mutex_unlock(&rdev->cc_lock);
exit:
	kfree(dcb_work);
}

static int bnxt_re_hwrm_dbr_pacing_broadcast_event(struct bnxt_re_dev *rdev)
{
	struct hwrm_func_dbr_pacing_broadcast_event_output resp = {0};
	struct hwrm_func_dbr_pacing_broadcast_event_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;
	int rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_DBR_PACING_BROADCAST_EVENT, -1, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to send dbr pacing broadcast event rc:%d", rc);
		return rc;
	}
	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_nqlist_query(struct bnxt_re_dev *rdev)
{
	struct hwrm_func_dbr_pacing_nqlist_query_output resp = {0};
	struct hwrm_func_dbr_pacing_nqlist_query_input req = {0};
	struct bnxt_dbq_nq_list *nq_list = &rdev->nq_list;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	bool primary_found = false;
	struct bnxt_fw_msg fw_msg;
	struct bnxt_qplib_nq *nq;
	int rc, i, j = 1;
	u16 *nql_ptr;

	nq = &rdev->nqr.nq[0];

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_DBR_PACING_NQLIST_QUERY, -1, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to send dbr pacing nq list query rc:%d", rc);
		return rc;
	}
	nq_list->num_nql_entries = le32_to_cpu(resp.num_nqs);
	nql_ptr = &resp.nq_ring_id0;
	/* populate the nq_list of the primary function with list received
	 * from FW. Fill the NQ IDs of secondary functions from index 1 to
	 * num_nql_entries - 1. Fill the  nq_list->nq_id[0] with the
	 * nq_id of the primary pf
	 */
	for (i = 0; i < nq_list->num_nql_entries; i++) {
		u16 nq_id = *nql_ptr;

		dev_dbg(rdev_to_dev(rdev),
			"nq_list->nq_id[%d] = %d\n", i, nq_id);
		if (nq_id != nq->ring_id) {
			nq_list->nq_id[j] = nq_id;
			j++;
		} else {
			primary_found = true;
			nq_list->nq_id[0] = nq->ring_id;
		}
		nql_ptr++;
	}
	if (primary_found)
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 1);

	return 0;
}

static void __wait_for_fifo_occupancy_below_th(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;
	u32 read_val, fifo_occup;
	bool first_read = true;

	/* loop shouldn't run infintely as the occupancy usually goes
	 * below pacing algo threshold as soon as pacing kicks in.
	 */
	while (1) {
		read_val = readl_fbsd(rdev->en_dev->softc, rdev->dbr_db_fifo_reg_off, 0);
		fifo_occup = pacing_data->fifo_max_depth -
			     ((read_val & pacing_data->fifo_room_mask) >>
			      pacing_data->fifo_room_shift);
		/* Fifo occupancy cannot be greater the MAX FIFO depth */
		if (fifo_occup > pacing_data->fifo_max_depth)
			break;

		if (first_read) {
			bnxt_re_update_fifo_occup_slabs(rdev, fifo_occup);
			first_read = false;
		}
		if (fifo_occup < pacing_data->pacing_th)
			break;
	}
}

static void bnxt_re_set_default_pacing_data(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;

	pacing_data->do_pacing = rdev->dbr_def_do_pacing;
	pacing_data->pacing_th = rdev->pacing_algo_th;
	pacing_data->alarm_th =
		pacing_data->pacing_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE(rdev->chip_ctx);
}

#define CAG_RING_MASK 0x7FF
#define CAG_RING_SHIFT 17
#define WATERMARK_MASK 0xFFF
#define WATERMARK_SHIFT	0

static bool bnxt_re_check_if_dbq_intr_triggered(struct bnxt_re_dev *rdev)
{
	u32 read_val;
	int j;

	for (j = 0; j < 10; j++) {
		read_val = readl_fbsd(rdev->en_dev->softc, rdev->dbr_aeq_arm_reg_off, 0);
		dev_dbg(rdev_to_dev(rdev), "AEQ ARM status = 0x%x\n",
			read_val);
		if (!read_val)
			return true;
	}
	return false;
}

int bnxt_re_set_dbq_throttling_reg(struct bnxt_re_dev *rdev, u16 nq_id, u32 throttle)
{
	u32 cag_ring_water_mark = 0, read_val;
	u32 throttle_val;

	/* Convert throttle percentage to value */
	throttle_val = (rdev->qplib_res.pacing_data->fifo_max_depth * throttle) / 100;

	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		cag_ring_water_mark = (nq_id & CAG_RING_MASK) << CAG_RING_SHIFT |
				      (throttle_val & WATERMARK_MASK);
		writel_fbsd(rdev->en_dev->softc,  rdev->dbr_throttling_reg_off, 0, cag_ring_water_mark);
		read_val = readl_fbsd(rdev->en_dev->softc , rdev->dbr_throttling_reg_off, 0);
		dev_dbg(rdev_to_dev(rdev),
			"%s: dbr_throttling_reg_off read_val = 0x%x\n",
			__func__, read_val);
		if (read_val != cag_ring_water_mark) {
			dev_dbg(rdev_to_dev(rdev),
				"nq_id = %d write_val=0x%x read_val=0x%x\n",
				nq_id, cag_ring_water_mark, read_val);
			return 1;
		}
	}
	writel_fbsd(rdev->en_dev->softc,  rdev->dbr_aeq_arm_reg_off, 0, 1);
	return 0;
}

static void bnxt_re_set_dbq_throttling_for_non_primary(struct bnxt_re_dev *rdev)
{
	struct bnxt_dbq_nq_list *nq_list;
	struct bnxt_qplib_nq *nq;
	int i;

	nq_list = &rdev->nq_list;
	/* Run a loop for other Active functions if this is primary function */
	if (bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
		dev_dbg(rdev_to_dev(rdev), "%s:  nq_list->num_nql_entries= %d\n",
			__func__, nq_list->num_nql_entries);
		nq = &rdev->nqr.nq[0];
		for (i = nq_list->num_nql_entries - 1; i > 0; i--) {
			u16 nq_id = nq_list->nq_id[i];
			if (nq)
				dev_dbg(rdev_to_dev(rdev),
					"%s: nq_id = %d cur_fn_ring_id = %d\n",
					__func__, nq_id, nq->ring_id);
			if (bnxt_re_set_dbq_throttling_reg
					(rdev, nq_id, 0))
				break;
			bnxt_re_check_if_dbq_intr_triggered(rdev);
		}
	}
}

static void bnxt_re_handle_dbr_nq_pacing_notification(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;
	int rc = 0;

	nq = &rdev->nqr.nq[0];

	/* Query the NQ list*/
	rc = bnxt_re_hwrm_dbr_pacing_nqlist_query(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to Query NQ list rc= %d", rc);
		return;
	}
	/*Configure GRC access for Throttling and aeq_arm register */
	writel_fbsd(rdev->en_dev->softc,  BNXT_GRCPF_REG_WINDOW_BASE_OUT + 28, 0,
		    rdev->chip_ctx->dbr_aeq_arm_reg & BNXT_GRC_BASE_MASK);

	rdev->dbr_throttling_reg_off =
		(rdev->chip_ctx->dbr_throttling_reg &
		 BNXT_GRC_OFFSET_MASK) + 0x8000;
	rdev->dbr_aeq_arm_reg_off =
		(rdev->chip_ctx->dbr_aeq_arm_reg &
		 BNXT_GRC_OFFSET_MASK) + 0x8000;

	bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id, rdev->dbq_watermark);
}

static void bnxt_re_dbq_wq_task(struct work_struct *work)
{
	struct bnxt_re_dbq_work *dbq_work =
			container_of(work, struct bnxt_re_dbq_work, work);
	struct bnxt_re_dev *rdev;

	rdev = dbq_work->rdev;

	if (!rdev)
		goto exit;
	switch (dbq_work->event) {
	case BNXT_RE_DBQ_EVENT_SCHED:
		dev_dbg(rdev_to_dev(rdev), "%s: Handle DBQ Pacing event\n",
			__func__);
		if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			bnxt_re_hwrm_dbr_pacing_broadcast_event(rdev);
		else
			bnxt_re_pacing_alert(rdev);
		break;
	case BNXT_RE_DBR_PACING_EVENT:
		dev_dbg(rdev_to_dev(rdev), "%s: Sched interrupt/pacing worker\n",
			__func__);
		if (_is_chip_p7(rdev->chip_ctx))
			bnxt_re_pacing_alert(rdev);
		else if (!rdev->chip_ctx->modes.dbr_pacing_v0)
			bnxt_re_hwrm_dbr_pacing_qcfg(rdev);
		break;
	case BNXT_RE_DBR_NQ_PACING_NOTIFICATION:
		bnxt_re_handle_dbr_nq_pacing_notification(rdev);
		/* Issue a broadcast event to notify other functions
		 * that primary changed
		 */
		bnxt_re_hwrm_dbr_pacing_broadcast_event(rdev);
		break;
	}
exit:
	kfree(dbq_work);
}

static void bnxt_re_async_notifier(void *handle, struct hwrm_async_event_cmpl *cmpl)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_re_dcb_work *dcb_work;
	struct bnxt_re_dbq_work *dbq_work;
	struct bnxt_re_dev *rdev;
	u16 event_id;
	u32 data1;
	u32 data2 = 0;

	if (!cmpl) {
		pr_err("Async event, bad completion\n");
		return;
	}

	if (!en_info || !en_info->en_dev) {
		pr_err("Async event, bad en_info or en_dev\n");
		return;
	}
	rdev = en_info->rdev;

	event_id = le16_to_cpu(cmpl->event_id);
	data1 = le32_to_cpu(cmpl->event_data1);
	data2 = le32_to_cpu(cmpl->event_data2);

	if (!rdev || !rdev_to_dev(rdev)) {
		dev_dbg(NULL, "Async event, bad rdev or netdev\n");
		return;
	}

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags) ||
	    !test_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags)) {
		dev_dbg(NULL, "Async event, device already detached\n");
		return;
	}
	if (data2 >= 0)
		dev_dbg(rdev_to_dev(rdev), "Async event_id = %d data1 = %d data2 = %d",
			event_id, data1, data2);

	switch (event_id) {
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE:
		/* Not handling the event in older FWs */
		if (!is_qport_service_type_supported(rdev))
			break;
		if (!rdev->dcb_wq)
			break;
		dcb_work = kzalloc(sizeof(*dcb_work), GFP_ATOMIC);
		if (!dcb_work)
			break;

		dcb_work->rdev = rdev;
		memcpy(&dcb_work->cmpl, cmpl, sizeof(*cmpl));
		INIT_WORK(&dcb_work->work, bnxt_re_dcb_wq_task);
		queue_work(rdev->dcb_wq, &dcb_work->work);
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY:
		if (EVENT_DATA1_RESET_NOTIFY_FATAL(data1)) {
			/* Set rcfw flag to control commands send to Bono */
			set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
			/* Set bnxt_re flag to control commands send via L2 driver */
			set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
			wake_up_all(&rdev->rcfw.cmdq.waitq);
		}
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_THRESHOLD:
		if (!rdev->dbr_pacing)
			break;
		dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
		if (!dbq_work)
			goto unlock;
		dbq_work->rdev = rdev;
		dbq_work->event = BNXT_RE_DBR_PACING_EVENT;
		INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
		queue_work(rdev->dbq_wq, &dbq_work->work);
		rdev->dbr_sw_stats->dbq_int_recv++;
		break;
	case HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE:
		if (!rdev->dbr_pacing)
			break;

		dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
		if (!dbq_work)
			goto unlock;
		dbq_work->rdev = rdev;
		dbq_work->event = BNXT_RE_DBR_NQ_PACING_NOTIFICATION;
		INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
		queue_work(rdev->dbq_wq, &dbq_work->work);
		break;

	default:
		break;
	}
unlock:
	return;
}

static void bnxt_re_db_fifo_check(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						dbq_fifo_check_work);
	struct bnxt_qplib_db_pacing_data *pacing_data;
	u32 pacing_save;

	if (!mutex_trylock(&rdev->dbq_lock))
		return;
	pacing_data = rdev->qplib_res.pacing_data;
	pacing_save = rdev->do_pacing_save;
	__wait_for_fifo_occupancy_below_th(rdev);
	cancel_delayed_work_sync(&rdev->dbq_pacing_work);
	if (rdev->dbr_recovery_on)
		goto recovery_on;
	if (pacing_save > rdev->dbr_def_do_pacing) {
		/* Double the do_pacing value during the congestion */
		pacing_save = pacing_save << 1;
	} else {
		/*
		 * when a new congestion is detected increase the do_pacing
		 * by 8 times. And also increase the pacing_th by 4 times. The
		 * reason to increase pacing_th is to give more space for the
		 * queue to oscillate down without getting empty, but also more
		 * room for the queue to increase without causing another alarm.
		 */
		pacing_save = pacing_save << 3;
		pacing_data->pacing_th = rdev->pacing_algo_th * 4;
	}

	if (pacing_save > BNXT_RE_MAX_DBR_DO_PACING)
		pacing_save = BNXT_RE_MAX_DBR_DO_PACING;

	pacing_data->do_pacing = pacing_save;
	rdev->do_pacing_save = pacing_data->do_pacing;
	pacing_data->alarm_th =
		pacing_data->pacing_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE(rdev->chip_ctx);
recovery_on:
	schedule_delayed_work(&rdev->dbq_pacing_work,
			      msecs_to_jiffies(rdev->dbq_pacing_time));
	rdev->dbr_sw_stats->dbq_pacing_alerts++;
	mutex_unlock(&rdev->dbq_lock);
}

static void bnxt_re_pacing_timer_exp(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						dbq_pacing_work.work);
	struct bnxt_qplib_db_pacing_data *pacing_data;
	u32 read_val, fifo_occup;
	struct bnxt_qplib_nq *nq;

	if (!mutex_trylock(&rdev->dbq_lock))
		return;

	pacing_data = rdev->qplib_res.pacing_data;
	read_val = readl_fbsd(rdev->en_dev->softc , rdev->dbr_db_fifo_reg_off, 0);
	fifo_occup = pacing_data->fifo_max_depth -
		     ((read_val & pacing_data->fifo_room_mask) >>
		      pacing_data->fifo_room_shift);

	if (fifo_occup > pacing_data->pacing_th)
		goto restart_timer;

	/*
	 * Instead of immediately going back to the default do_pacing
	 * reduce it by 1/8 times and restart the timer.
	 */
	pacing_data->do_pacing = pacing_data->do_pacing - (pacing_data->do_pacing >> 3);
	pacing_data->do_pacing = max_t(u32, rdev->dbr_def_do_pacing, pacing_data->do_pacing);
	/*
	 * If the fifo_occup is less than the interrupt enable threshold
	 * enable the interrupt on the primary PF.
	 */
	if (rdev->dbq_int_disable && fifo_occup < rdev->pacing_en_int_th) {
		if (bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
			if (!rdev->chip_ctx->modes.dbr_pacing_v0) {
				nq = &rdev->nqr.nq[0];
				bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id,
							       rdev->dbq_watermark);
				rdev->dbr_sw_stats->dbq_int_en++;
				rdev->dbq_int_disable = false;
			}
		}
	}
	if (pacing_data->do_pacing <= rdev->dbr_def_do_pacing) {
		bnxt_re_set_default_pacing_data(rdev);
		rdev->dbr_sw_stats->dbq_pacing_complete++;
		goto dbq_unlock;
	}
restart_timer:
	schedule_delayed_work(&rdev->dbq_pacing_work,
			      msecs_to_jiffies(rdev->dbq_pacing_time));
	bnxt_re_update_do_pacing_slabs(rdev);
	rdev->dbr_sw_stats->dbq_pacing_resched++;
dbq_unlock:
	rdev->do_pacing_save = pacing_data->do_pacing;
	mutex_unlock(&rdev->dbq_lock);
}

void bnxt_re_pacing_alert(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data;

	if (!rdev->dbr_pacing)
		return;
	mutex_lock(&rdev->dbq_lock);
	pacing_data = rdev->qplib_res.pacing_data;

	/*
	 * Increase the alarm_th to max so that other user lib instances do not
	 * keep alerting the driver.
	 */
	pacing_data->alarm_th = pacing_data->fifo_max_depth;
	pacing_data->do_pacing = BNXT_RE_MAX_DBR_DO_PACING;
	cancel_work_sync(&rdev->dbq_fifo_check_work);
	schedule_work(&rdev->dbq_fifo_check_work);
	mutex_unlock(&rdev->dbq_lock);
}

void bnxt_re_schedule_dbq_event(struct bnxt_qplib_res *res)
{
	struct bnxt_re_dbq_work *dbq_work;
	struct bnxt_re_dev *rdev;

	rdev = container_of(res, struct bnxt_re_dev, qplib_res);

	atomic_set(&rdev->dbq_intr_running, 1);

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		goto exit;
	/* Run the loop to send dbq event to other functions
	 * for newer FW
	 */
	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) &&
	    !rdev->chip_ctx->modes.dbr_pacing_v0)
		bnxt_re_set_dbq_throttling_for_non_primary(rdev);

	dbq_work = kzalloc(sizeof(*dbq_work), GFP_ATOMIC);
	if (!dbq_work)
		goto exit;
	dbq_work->rdev = rdev;
	dbq_work->event = BNXT_RE_DBQ_EVENT_SCHED;
	INIT_WORK(&dbq_work->work, bnxt_re_dbq_wq_task);
	queue_work(rdev->dbq_wq, &dbq_work->work);
	rdev->dbr_sw_stats->dbq_int_recv++;
	rdev->dbq_int_disable = true;
exit:
	atomic_set(&rdev->dbq_intr_running, 0);
}

static void bnxt_re_free_msix(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int rc;

	rc = en_dev->en_ops->bnxt_free_msix(rdev->en_dev, BNXT_ROCE_ULP);
	if (rc)
		dev_err(rdev_to_dev(rdev), "netdev %p free_msix failed! rc = 0x%x",
			rdev->netdev, rc);
}

static int bnxt_re_request_msix(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int rc = 0, num_msix_want, num_msix_got;
	struct bnxt_msix_entry *entry;

	/*
	 * Request MSIx based on the function type. This is
	 * a temporory solution to enable max VFs when NPAR is
	 * enabled.
	 * TODO - change the scheme with an adapter specific check
	 * as the latest adapters can support more NQs. For now
	 * this change satisfy all adapter versions.
	 */

	if (rdev->is_virtfn)
		num_msix_want = BNXT_RE_MAX_MSIX_VF;
	else if (BNXT_EN_NPAR(en_dev))
		num_msix_want = BNXT_RE_MAX_MSIX_NPAR_PF;
	else if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		num_msix_want = rdev->num_msix_requested ?: BNXT_RE_MAX_MSIX_GEN_P5_PF;
	else
		num_msix_want = BNXT_RE_MAX_MSIX_PF;

	/*
	 * Since MSIX vectors are used for both NQs and CREQ, we should try to
	 * allocate num_online_cpus + 1 by taking into account the CREQ. This
	 * leaves the number of MSIX vectors for NQs match the number of CPUs
	 * and allows the system to be fully utilized
	 */
	num_msix_want = min_t(u32, num_msix_want, num_online_cpus() + 1);
	num_msix_want = min_t(u32, num_msix_want, BNXT_RE_MAX_MSIX);
	num_msix_want = max_t(u32, num_msix_want, BNXT_RE_MIN_MSIX);

	entry = rdev->nqr.msix_entries;

	num_msix_got = en_dev->en_ops->bnxt_request_msix(en_dev, BNXT_ROCE_ULP,
							 entry, num_msix_want);
	if (num_msix_got < BNXT_RE_MIN_MSIX) {
		rc = -EINVAL;
		goto done;
	}
	if (num_msix_got != num_msix_want)
		dev_warn(rdev_to_dev(rdev),
			 "bnxt_request_msix: wanted %d vectors, got %d\n",
			 num_msix_want, num_msix_got);

	rdev->nqr.num_msix = num_msix_got;
	return 0;
done:
	if (num_msix_got)
		bnxt_re_free_msix(rdev);
	return rc;
}

static int  __wait_for_ib_unregister(struct bnxt_re_dev *rdev,
				     struct bnxt_re_en_dev_info *en_info)
{
	u64 timeout = 0;
	u32 cur_prod = 0, cur_cons = 0;
	int retry = 0, rc = 0, ret = 0;

	cur_prod = rdev->rcfw.cmdq.hwq.prod;
	cur_cons = rdev->rcfw.cmdq.hwq.cons;
	timeout = msecs_to_jiffies(BNXT_RE_RECOVERY_IB_UNINIT_WAIT_TIME_MS);
	retry = BNXT_RE_RECOVERY_IB_UNINIT_WAIT_RETRY;
	/* During module exit, increase timeout ten-fold to 100 mins to wait
	 * as long as possible for ib_unregister() to complete
	 */
	if (rdev->mod_exit)
		retry *= 10;
	do {
		/*
		 * Since the caller of this function invokes with bnxt_re_mutex held,
		 * release it to avoid holding a lock while in wait / sleep mode.
		 */
		mutex_unlock(&bnxt_re_mutex);
		rc = wait_event_timeout(en_info->waitq,
					en_info->ib_uninit_done,
					timeout);
		mutex_lock(&bnxt_re_mutex);

		if (!bnxt_re_is_rdev_valid(rdev))
			break;

		if (rc)
			break;

		if (!RCFW_NO_FW_ACCESS(&rdev->rcfw)) {
			/* No need to check for cmdq stall during module exit,
			 * wait for ib unregister to complete
			 */
			if (!rdev->mod_exit)
				ret = __check_cmdq_stall(&rdev->rcfw, &cur_prod, &cur_cons);
			if (ret || en_info->ib_uninit_done)
				break;
		}
	} while (retry--);

	return rc;
}

static int bnxt_re_handle_start(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_re_dev *rdev = NULL;
	struct ifnet *real_dev;
	struct bnxt_en_dev *en_dev;
	struct ifnet *netdev;
	int rc = 0;

	if (!en_info || !en_info->en_dev) {
		pr_err("Start, bad en_info or en_dev\n");
		return -EINVAL;
	}
	netdev = en_info->en_dev->net;
	if (en_info->rdev) {
		dev_info(rdev_to_dev(en_info->rdev),
			 "%s: Device is already added adev %p rdev: %p\n",
			 __func__, adev, en_info->rdev);
		return 0;
	}

	en_dev = en_info->en_dev;
	real_dev = rdma_vlan_dev_real_dev(netdev);
	if (!real_dev)
		real_dev = netdev;
	rc = bnxt_re_add_device(&rdev, real_dev,
				en_info->gsi_mode,
				BNXT_RE_POST_RECOVERY_INIT,
				en_info->wqe_mode,
				en_info->num_msix_requested, adev);
	if (rc) {
		/* Add device failed. Unregister the device.
		 * This has to be done explicitly as
		 * bnxt_re_stop would not have unregistered
		 */
		rtnl_lock();
		en_dev->en_ops->bnxt_unregister_device(en_dev, BNXT_ROCE_ULP);
		rtnl_unlock();
		mutex_lock(&bnxt_re_dev_lock);
		gadd_dev_inprogress--;
		mutex_unlock(&bnxt_re_dev_lock);
		return rc;
	}
	rdev->adev = adev;
	rtnl_lock();
	bnxt_re_get_link_speed(rdev);
	rtnl_unlock();
	rc = bnxt_re_ib_init(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed ib_init\n");
		return rc;
	}
	bnxt_re_ib_init_2(rdev);

	return rc;
}

static void bnxt_re_stop(void *handle)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct ifnet *netdev;
	struct bnxt_re_dev *rdev;
	struct bnxt_en_dev *en_dev;
	int rc = 0;

	rtnl_unlock();
	mutex_lock(&bnxt_re_mutex);
	if (!en_info || !en_info->en_dev) {
		pr_err("Stop, bad en_info or en_dev\n");
		goto exit;
	}
	netdev = en_info->en_dev->net;
	rdev = en_info->rdev;
	if (!rdev)
		goto exit;

	if (!bnxt_re_is_rdev_valid(rdev))
		goto exit;

	/*
	 * Check if fw has undergone reset or is in a fatal condition.
	 * If so, set flags so that no further commands are sent down to FW
	 */
	en_dev = rdev->en_dev;
	if (en_dev->en_state & BNXT_STATE_FW_FATAL_COND ||
	    en_dev->en_state & BNXT_STATE_FW_RESET_DET) {
		/* Set rcfw flag to control commands send to Bono */
		set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);
		/* Set bnxt_re flag to control commands send via L2 driver */
		set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
		wake_up_all(&rdev->rcfw.cmdq.waitq);
	}

	if (test_bit(BNXT_RE_FLAG_STOP_IN_PROGRESS, &rdev->flags))
		goto exit;
	set_bit(BNXT_RE_FLAG_STOP_IN_PROGRESS, &rdev->flags);

	en_info->wqe_mode = rdev->chip_ctx->modes.wqe_mode;
	en_info->gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	en_info->num_msix_requested = rdev->num_msix_requested;
	en_info->ib_uninit_done = false;

	if (rdev->dbr_pacing)
		bnxt_re_set_pacing_dev_state(rdev);

	dev_info(rdev_to_dev(rdev), "%s: L2 driver notified to stop."
		 "Attempting to stop and Dispatching event "
		 "to inform the stack\n", __func__);
	init_waitqueue_head(&en_info->waitq);
	/* Schedule a work item to handle IB UNINIT for recovery */
	bnxt_re_schedule_work(rdev, NETDEV_UNREGISTER,
			      NULL, netdev, rdev->adev);
	rc = __wait_for_ib_unregister(rdev, en_info);
	if (!bnxt_re_is_rdev_valid(rdev))
		goto exit;
	if (!rc) {
		dev_info(rdev_to_dev(rdev), "%s: Attempt to stop failed\n",
			 __func__);
		bnxt_re_detach_err_device(rdev);
		goto exit;
	}
	bnxt_re_remove_device(rdev, BNXT_RE_PRE_RECOVERY_REMOVE, rdev->adev);
exit:
	mutex_unlock(&bnxt_re_mutex);
	/* Take rtnl_lock before return, bnxt_re_stop is called with rtnl_lock */
	rtnl_lock();

	return;
}

static void bnxt_re_start(void *handle)
{
	rtnl_unlock();
	mutex_lock(&bnxt_re_mutex);
	if (bnxt_re_handle_start((struct auxiliary_device *)handle))
		pr_err("Failed to start RoCE device");
	mutex_unlock(&bnxt_re_mutex);
	/* Take rtnl_lock before return, bnxt_re_start is called with rtnl_lock */
	rtnl_lock();
	return;
}

static void bnxt_re_shutdown(void *p)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(p);
	struct bnxt_re_dev *rdev;

	if (!en_info) {
		pr_err("Shutdown, bad en_info\n");
		return;
	}
	rtnl_unlock();
	mutex_lock(&bnxt_re_mutex);
	rdev = en_info->rdev;
	if (!rdev || !bnxt_re_is_rdev_valid(rdev))
		goto exit;

	/* rtnl_lock held by L2 before coming here */
	bnxt_re_stopqps_and_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, rdev->adev);
exit:
	mutex_unlock(&bnxt_re_mutex);
	rtnl_lock();
	return;
}

static void bnxt_re_stop_irq(void *handle)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_qplib_rcfw *rcfw = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_nq *nq;
	int indx;

	if (!en_info) {
		pr_err("Stop irq, bad en_info\n");
		return;
	}
	rdev = en_info->rdev;

	if (!rdev)
		return;

	rcfw = &rdev->rcfw;
	for (indx = 0; indx < rdev->nqr.max_init; indx++) {
		nq = &rdev->nqr.nq[indx];
		mutex_lock(&nq->lock);
		bnxt_qplib_nq_stop_irq(nq, false);
		mutex_unlock(&nq->lock);
	}

	if (test_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags))
		bnxt_qplib_rcfw_stop_irq(rcfw, false);
}

static void bnxt_re_start_irq(void *handle, struct bnxt_msix_entry *ent)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(handle);
	struct bnxt_msix_entry *msix_ent = NULL;
	struct bnxt_qplib_rcfw *rcfw = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_nq *nq;
	int indx, rc, vec;

	if (!en_info) {
		pr_err("Start irq, bad en_info\n");
		return;
	}
	rdev = en_info->rdev;
	if (!rdev)
		return;
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;
	msix_ent = rdev->nqr.msix_entries;
	rcfw = &rdev->rcfw;

	if (!ent) {
		/* Not setting the f/w timeout bit in rcfw.
		 * During the driver unload the first command
		 * to f/w will timeout and that will set the
		 * timeout bit.
		 */
		dev_err(rdev_to_dev(rdev), "Failed to re-start IRQs\n");
		return;
	}

	/* Vectors may change after restart, so update with new vectors
	 * in device structure.
	 */
	for (indx = 0; indx < rdev->nqr.num_msix; indx++)
		rdev->nqr.msix_entries[indx].vector = ent[indx].vector;

	if (test_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags)) {
		rc = bnxt_qplib_rcfw_start_irq(rcfw, msix_ent[BNXT_RE_AEQ_IDX].vector,
					       false);
		if (rc) {
			dev_warn(rdev_to_dev(rdev),
				 "Failed to reinit CREQ\n");
			return;
		}
	}
	for (indx = 0 ; indx < rdev->nqr.max_init; indx++) {
		nq = &rdev->nqr.nq[indx];
		vec = indx + 1;
		rc = bnxt_qplib_nq_start_irq(nq, indx, msix_ent[vec].vector,
					     false);
		if (rc) {
			dev_warn(rdev_to_dev(rdev),
				 "Failed to reinit NQ index %d\n", indx);
			return;
		}
	}
}

/*
 * Except for ulp_async_notifier, the remaining ulp_ops
 * below are called with rtnl_lock held
 */
static struct bnxt_ulp_ops bnxt_re_ulp_ops = {
	.ulp_async_notifier = bnxt_re_async_notifier,
	.ulp_stop = bnxt_re_stop,
	.ulp_start = bnxt_re_start,
	.ulp_shutdown = bnxt_re_shutdown,
	.ulp_irq_stop = bnxt_re_stop_irq,
	.ulp_irq_restart = bnxt_re_start_irq,
};

static inline const char *bnxt_re_netevent(unsigned long event)
{
	BNXT_RE_NETDEV_EVENT(event, NETDEV_UP);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_DOWN);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGE);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_REGISTER);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_UNREGISTER);
	BNXT_RE_NETDEV_EVENT(event, NETDEV_CHANGEADDR);
	return "Unknown";
}

/* RoCE -> Net driver */

/* Driver registration routines used to let the networking driver (bnxt_en)
 * to know that the RoCE driver is now installed */
static void bnxt_re_unregister_netdev(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int rc;

	rtnl_lock();
	rc = en_dev->en_ops->bnxt_unregister_device(rdev->en_dev,
						    BNXT_ROCE_ULP);
	rtnl_unlock();
	if (rc)
		dev_err(rdev_to_dev(rdev), "netdev %p unregister failed! rc = 0x%x",
			rdev->en_dev->net, rc);

	clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);
}

static int bnxt_re_register_netdev(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	int rc = 0;

	rtnl_lock();
	rc = en_dev->en_ops->bnxt_register_device(en_dev,
						  BNXT_ROCE_ULP,
						  &bnxt_re_ulp_ops,
						  rdev->adev);
	rtnl_unlock();
	if (rc) {
		dev_err(rdev_to_dev(rdev), "netdev %p register failed! rc = 0x%x",
			rdev->netdev, rc);
		return rc;
	}

	return rc;
}

static void bnxt_re_set_db_offset(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_en_dev *en_dev;
	struct bnxt_qplib_res *res;
	u32 l2db_len = 0;
	u32 offset = 0;
	u32 barlen;
	int rc;

	res = &rdev->qplib_res;
	en_dev = rdev->en_dev;
	cctx = rdev->chip_ctx;

	/* Issue qcfg */
	rc = bnxt_re_hwrm_qcfg(rdev, &l2db_len, &offset);
	if (rc)
		dev_info(rdev_to_dev(rdev),
			 "Couldn't get DB bar size, Low latency framework is disabled\n");
	/* set register offsets for both UC and WC */
	if (_is_chip_p7(cctx))
		res->dpi_tbl.ucreg.offset = offset;
	else
		res->dpi_tbl.ucreg.offset = res->is_vf ? BNXT_QPLIB_DBR_VF_DB_OFFSET :
							 BNXT_QPLIB_DBR_PF_DB_OFFSET;
	res->dpi_tbl.wcreg.offset = res->dpi_tbl.ucreg.offset;

	/* If WC mapping is disabled by L2 driver then en_dev->l2_db_size
	 * is equal to the DB-Bar actual size. This indicates that L2
	 * is mapping entire bar as UC-. RoCE driver can't enable WC mapping
	 * in such cases and DB-push will be disabled.
	 */
	barlen = pci_resource_len(res->pdev, RCFW_DBR_PCI_BAR_REGION);
	if (cctx->modes.db_push && l2db_len && en_dev->l2_db_size != barlen) {
		res->dpi_tbl.wcreg.offset = en_dev->l2_db_size;
		dev_info(rdev_to_dev(rdev),
			 "Low latency framework is enabled\n");
	}

	return;
}

static void bnxt_re_set_drv_mode(struct bnxt_re_dev *rdev, u8 mode)
{
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_en_dev *en_dev;

	en_dev = rdev->en_dev;
	cctx = rdev->chip_ctx;
	cctx->modes.wqe_mode = _is_chip_gen_p5_p7(rdev->chip_ctx) ?
					mode : BNXT_QPLIB_WQE_MODE_STATIC;
	cctx->modes.te_bypass = false;
	if (bnxt_re_hwrm_qcaps(rdev))
		dev_err(rdev_to_dev(rdev),
			"Failed to query hwrm qcaps\n");
	 /*
	  * TODO: Need a better mechanism for spreading of the
	  * 512 extended PPP pages in the presence of VF and
	  * NPAR, until then not enabling push
	  */
	if (_is_chip_p7(rdev->chip_ctx) && cctx->modes.db_push) {
		if (rdev->is_virtfn || BNXT_EN_NPAR(en_dev))
			cctx->modes.db_push = false;
	}

	rdev->roce_mode = en_dev->flags & BNXT_EN_FLAG_ROCE_CAP;
	dev_dbg(rdev_to_dev(rdev),
		"RoCE is supported on the device - caps:0x%x",
		rdev->roce_mode);
	if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
		rdev->roce_mode = BNXT_RE_FLAG_ROCEV2_CAP;
	cctx->hw_stats_size = en_dev->hw_ring_stats_size;
}

static void bnxt_re_destroy_chip_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_qplib_res *res;

	if (!rdev->chip_ctx)
		return;

	res = &rdev->qplib_res;
	bnxt_qplib_unmap_db_bar(res);

	kfree(res->hctx);
	res->rcfw = NULL;
	kfree(rdev->dev_attr);
	rdev->dev_attr = NULL;

	chip_ctx = rdev->chip_ctx;
	rdev->chip_ctx = NULL;
	res->cctx = NULL;
	res->hctx = NULL;
	res->pdev = NULL;
	res->netdev = NULL;
	kfree(chip_ctx);
}

static int bnxt_re_setup_chip_ctx(struct bnxt_re_dev *rdev, u8 wqe_mode)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_en_dev *en_dev;
	int rc;

	en_dev = rdev->en_dev;
	/* Supply pci device to qplib */
	rdev->qplib_res.pdev = en_dev->pdev;
	rdev->qplib_res.netdev = rdev->netdev;
	rdev->qplib_res.en_dev = en_dev;

	chip_ctx = kzalloc(sizeof(*chip_ctx), GFP_KERNEL);
	if (!chip_ctx)
		return -ENOMEM;
	rdev->chip_ctx = chip_ctx;
	rdev->qplib_res.cctx = chip_ctx;
	rc = bnxt_re_query_hwrm_intf_version(rdev);
	if (rc)
		goto fail;
	rdev->dev_attr = kzalloc(sizeof(*rdev->dev_attr), GFP_KERNEL);
	if (!rdev->dev_attr) {
		rc = -ENOMEM;
		goto fail;
	}
	rdev->qplib_res.dattr = rdev->dev_attr;
	rdev->qplib_res.rcfw = &rdev->rcfw;
	rdev->qplib_res.is_vf = rdev->is_virtfn;

	rdev->qplib_res.hctx = kzalloc(sizeof(*rdev->qplib_res.hctx),
				       GFP_KERNEL);
	if (!rdev->qplib_res.hctx) {
		rc = -ENOMEM;
		goto fail;
	}
	bnxt_re_set_drv_mode(rdev, wqe_mode);

	bnxt_re_set_db_offset(rdev);
	rc = bnxt_qplib_map_db_bar(&rdev->qplib_res);
	if (rc)
		goto fail;

	rc = bnxt_qplib_enable_atomic_ops_to_root(en_dev->pdev);
	if (rc)
		dev_dbg(rdev_to_dev(rdev),
			"platform doesn't support global atomics");

	return 0;
fail:
	kfree(rdev->chip_ctx);
	rdev->chip_ctx = NULL;

	kfree(rdev->dev_attr);
	rdev->dev_attr = NULL;

	kfree(rdev->qplib_res.hctx);
	rdev->qplib_res.hctx = NULL;
	return rc;
}

static u16 bnxt_re_get_rtype(struct bnxt_re_dev *rdev) {
	return _is_chip_gen_p5_p7(rdev->chip_ctx) ?
	       HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ :
	       HWRM_RING_ALLOC_INPUT_RING_TYPE_ROCE_CMPL;
}

static int bnxt_re_net_ring_free(struct bnxt_re_dev *rdev, u16 fw_ring_id)
{
	int rc = -EINVAL;
	struct hwrm_ring_free_input req = {0};
	struct hwrm_ring_free_output resp;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;

	if (!en_dev)
		return rc;

	/* To avoid unnecessary error messages during recovery.
	 * HW is anyway in error state. So dont send down the command */
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;

	/* allocation had failed, no need to issue hwrm */
	if (fw_ring_id == 0xffff)
		return 0;

	memset(&fw_msg, 0, sizeof(fw_msg));

	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_RING_FREE, -1, -1);
	req.ring_type = bnxt_re_get_rtype(rdev);
	req.ring_id = cpu_to_le16(fw_ring_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to free HW ring with rc = 0x%x", rc);
		return rc;
	}
	dev_dbg(rdev_to_dev(rdev), "HW ring freed with id = 0x%x\n",
		fw_ring_id);

	return rc;
}

static int bnxt_re_net_ring_alloc(struct bnxt_re_dev *rdev,
				  struct bnxt_re_ring_attr *ring_attr,
				  u16 *fw_ring_id)
{
	int rc = -EINVAL;
	struct hwrm_ring_alloc_input req = {0};
	struct hwrm_ring_alloc_output resp;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;

	if (!en_dev)
		return rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_RING_ALLOC, -1, -1);
	req.flags = cpu_to_le16(ring_attr->flags);
	req.enables = 0;
	req.page_tbl_addr =  cpu_to_le64(ring_attr->dma_arr[0]);
	if (ring_attr->pages > 1) {
		/* Page size is in log2 units */
		req.page_size = BNXT_PAGE_SHIFT;
		req.page_tbl_depth = 1;
	} else {
		req.page_size = 4;
		req.page_tbl_depth = 0;
	}

	req.fbo = 0;
	/* Association of ring index with doorbell index and MSIX number */
	req.logical_id = cpu_to_le16(ring_attr->lrid);
	req.length = cpu_to_le32(ring_attr->depth + 1);
	req.ring_type = ring_attr->type;
	req.int_mode = ring_attr->mode;
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to allocate HW ring with rc = 0x%x", rc);
		return rc;
	}
	*fw_ring_id = le16_to_cpu(resp.ring_id);
	dev_dbg(rdev_to_dev(rdev),
		"HW ring allocated with id = 0x%x at slot 0x%x",
		resp.ring_id, ring_attr->lrid);

	return rc;
}

static int bnxt_re_net_stats_ctx_free(struct bnxt_re_dev *rdev,
				      u32 fw_stats_ctx_id, u16 tid)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_stat_ctx_free_input req = {0};
	struct hwrm_stat_ctx_free_output resp;
	struct bnxt_fw_msg fw_msg;
	int rc = -EINVAL;

	if (!en_dev)
		return rc;

	/* To avoid unnecessary error messages during recovery.
	 * HW is anyway in error state. So dont send down the command */
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;
	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_STAT_CTX_FREE, -1, tid);
	req.stat_ctx_id = cpu_to_le32(fw_stats_ctx_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to free HW stats ctx with rc = 0x%x", rc);
		return rc;
	}
	dev_dbg(rdev_to_dev(rdev),
		"HW stats ctx freed with id = 0x%x", fw_stats_ctx_id);

	return rc;
}

static int bnxt_re_net_stats_ctx_alloc(struct bnxt_re_dev *rdev, u16 tid)
{
	struct hwrm_stat_ctx_alloc_output resp = {};
	struct hwrm_stat_ctx_alloc_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_qplib_stats *stat;
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_fw_msg fw_msg;
	int rc = 0;

	hctx = rdev->qplib_res.hctx;
	stat = (tid == 0xffff) ? &hctx->stats : &hctx->stats2;
	stat->fw_id = INVALID_STATS_CTX_ID;

	if (!en_dev)
		return -EINVAL;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_STAT_CTX_ALLOC, -1, tid);
	req.update_period_ms = cpu_to_le32(1000);
	req.stats_dma_length = rdev->chip_ctx->hw_stats_size;
	req.stats_dma_addr = cpu_to_le64(stat->dma_map);
	req.stat_ctx_flags = HWRM_STAT_CTX_ALLOC_INPUT_STAT_CTX_FLAGS_ROCE;
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to allocate HW stats ctx, rc = 0x%x", rc);
		return rc;
	}
	stat->fw_id = le32_to_cpu(resp.stat_ctx_id);
	dev_dbg(rdev_to_dev(rdev), "HW stats ctx allocated with id = 0x%x",
		stat->fw_id);

	return rc;
}

static void bnxt_re_net_unregister_async_event(struct bnxt_re_dev *rdev)
{
	const struct bnxt_en_ops *en_ops;

	if (rdev->is_virtfn ||
	    test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return;

	memset(rdev->event_bitmap, 0, sizeof(rdev->event_bitmap));
	en_ops = rdev->en_dev->en_ops;
	if (en_ops->bnxt_register_fw_async_events
	    (rdev->en_dev, BNXT_ROCE_ULP,
	     (unsigned long *)rdev->event_bitmap,
	      HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE))
		dev_err(rdev_to_dev(rdev),
			"Failed to unregister async event");
}

static void bnxt_re_net_register_async_event(struct bnxt_re_dev *rdev)
{
	const struct bnxt_en_ops *en_ops;

	if (rdev->is_virtfn)
		return;

	rdev->event_bitmap[0] |=
		BIT(HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE) |
		BIT(HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY);

	rdev->event_bitmap[2] |=
	   BIT(HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT - 64);
	rdev->event_bitmap[2] |=
		BIT(HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_THRESHOLD - 64) |
		BIT(HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE - 64);
	en_ops = rdev->en_dev->en_ops;
	if (en_ops->bnxt_register_fw_async_events
	    (rdev->en_dev, BNXT_ROCE_ULP,
	     (unsigned long *)rdev->event_bitmap,
	      HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DOORBELL_PACING_NQ_UPDATE))
		dev_err(rdev_to_dev(rdev),
			"Failed to reg Async event");
}

static int bnxt_re_query_hwrm_intf_version(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_ver_get_output resp = {0};
	struct hwrm_ver_get_input req = {0};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg;
	int rc = 0;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_VER_GET, -1, -1);
	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query HW version, rc = 0x%x", rc);
		return rc;
	}
	cctx = rdev->chip_ctx;
	cctx->hwrm_intf_ver = (u64) le16_to_cpu(resp.hwrm_intf_major) << 48 |
			      (u64) le16_to_cpu(resp.hwrm_intf_minor) << 32 |
			      (u64) le16_to_cpu(resp.hwrm_intf_build) << 16 |
				    le16_to_cpu(resp.hwrm_intf_patch);

	cctx->hwrm_cmd_max_timeout = le16_to_cpu(resp.max_req_timeout);

	if (!cctx->hwrm_cmd_max_timeout)
		cctx->hwrm_cmd_max_timeout = RCFW_FW_STALL_MAX_TIMEOUT;

	cctx->chip_num = le16_to_cpu(resp.chip_num);
	cctx->chip_rev = resp.chip_rev;
	cctx->chip_metal = resp.chip_metal;
	return 0;
}

/* Query device config using common hwrm */
static int bnxt_re_hwrm_qcfg(struct bnxt_re_dev *rdev, u32 *db_len,
			     u32 *offset)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_func_qcfg_output resp = {0};
	struct hwrm_func_qcfg_input req = {0};
	struct bnxt_fw_msg fw_msg;
	int rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_QCFG, -1, -1);
	req.fid = cpu_to_le16(0xffff);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query config, rc = %#x", rc);
		return rc;
	}

	*db_len = PAGE_ALIGN(le16_to_cpu(resp.l2_doorbell_bar_size_kb) * 1024);
	*offset = PAGE_ALIGN(le16_to_cpu(resp.legacy_l2_db_size_kb) * 1024);
	return 0;
}

/* Query function capabilities using common hwrm */
int bnxt_re_hwrm_qcaps(struct bnxt_re_dev *rdev)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_func_qcaps_output resp = {0};
	struct hwrm_func_qcaps_input req = {0};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg;
	u8 push_enable = false;
	int rc;

	cctx = rdev->chip_ctx;
	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_QCAPS, -1, -1);
	req.fid = cpu_to_le16(0xffff);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query capabilities, rc = %#x", rc);
		return rc;
	}
	if (_is_chip_p7(rdev->chip_ctx))
		push_enable =
			(resp.flags_ext &
			 HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_PPP_PUSH_MODE_SUPPORTED) ?
			 true : false;
	else
		push_enable =
			(resp.flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_WCB_PUSH_MODE) ?
			 true : false;
	cctx->modes.db_push = push_enable;

	cctx->modes.dbr_pacing =
		resp.flags_ext & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_DBR_PACING_SUPPORTED ?
			true : false;
	cctx->modes.dbr_pacing_ext =
		resp.flags_ext2 &
			HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_DBR_PACING_EXT_SUPPORTED ?
			true : false;
	cctx->modes.dbr_drop_recov =
		(resp.flags_ext2 &
		 HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_SW_DBR_DROP_RECOVERY_SUPPORTED) ?
			true : false;
	cctx->modes.dbr_pacing_v0 =
		(resp.flags_ext2 &
		 HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT2_DBR_PACING_V0_SUPPORTED) ?
			true : false;
	dev_dbg(rdev_to_dev(rdev),
		"%s: cctx->modes.dbr_pacing = %d cctx->modes.dbr_pacing_ext = %d, dbr_drop_recov %d\n",
		__func__, cctx->modes.dbr_pacing, cctx->modes.dbr_pacing_ext, cctx->modes.dbr_drop_recov);

	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_qcfg(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_db_pacing_data *pacing_data = rdev->qplib_res.pacing_data;
	struct hwrm_func_dbr_pacing_qcfg_output resp = {0};
	struct hwrm_func_dbr_pacing_qcfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_fw_msg fw_msg;
	u32 primary_nq_id;
	int rc;

	cctx = rdev->chip_ctx;
	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_DBR_PACING_QCFG, -1, -1);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to query dbr pacing config, rc = %#x", rc);
		return rc;
	}

	primary_nq_id = le32_to_cpu(resp.primary_nq_id);
	if (primary_nq_id == 0xffffffff &&
	    !bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		dev_err(rdev_to_dev(rdev), "%s:%d Invoke bnxt_qplib_dbr_pacing_set_primary_pf with 1\n",
			__func__, __LINE__);
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 1);
	}

	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		struct bnxt_qplib_nq *nq;

		nq = &rdev->nqr.nq[0];
		/* Reset the primary capability */
		if (nq->ring_id != primary_nq_id)
			bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 0);
	}

	if ((resp.dbr_stat_db_fifo_reg &
	     HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK) ==
	    HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_GRC)
		cctx->dbr_stat_db_fifo =
		resp.dbr_stat_db_fifo_reg &
		~HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK;

	if ((resp.dbr_throttling_aeq_arm_reg &
	    HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_THROTTLING_AEQ_ARM_REG_ADDR_SPACE_MASK)
	    == HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_THROTTLING_AEQ_ARM_REG_ADDR_SPACE_GRC) {
		cctx->dbr_aeq_arm_reg = resp.dbr_throttling_aeq_arm_reg &
			~HWRM_FUNC_DBR_PACING_QCFG_OUTPUT_DBR_STAT_DB_FIFO_REG_ADDR_SPACE_MASK;
		cctx->dbr_throttling_reg = cctx->dbr_aeq_arm_reg - 4;
	}
	pacing_data->fifo_max_depth = le32_to_cpu(resp.dbr_stat_db_max_fifo_depth);
	if (!pacing_data->fifo_max_depth)
		pacing_data->fifo_max_depth = BNXT_RE_MAX_FIFO_DEPTH(cctx);
	pacing_data->fifo_room_mask = le32_to_cpu(resp.dbr_stat_db_fifo_reg_fifo_room_mask);
	pacing_data->fifo_room_shift = resp.dbr_stat_db_fifo_reg_fifo_room_shift;
	dev_dbg(rdev_to_dev(rdev),
		"%s: nq:0x%x primary_pf:%d db_fifo:0x%x aeq_arm:0x%x i"
		"fifo_max_depth 0x%x , resp.dbr_stat_db_max_fifo_depth 0x%x);\n",
		__func__, resp.primary_nq_id, cctx->modes.dbr_primary_pf,
		 cctx->dbr_stat_db_fifo, cctx->dbr_aeq_arm_reg,
		 pacing_data->fifo_max_depth,
		le32_to_cpu(resp.dbr_stat_db_max_fifo_depth));
	return 0;
}

static int bnxt_re_hwrm_dbr_pacing_cfg(struct bnxt_re_dev *rdev, bool enable)
{
	struct hwrm_func_dbr_pacing_cfg_output resp = {0};
	struct hwrm_func_dbr_pacing_cfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;
	int rc;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		return 0;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_DBR_PACING_CFG, -1, -1);
	if (enable) {
		req.flags = HWRM_FUNC_DBR_PACING_CFG_INPUT_FLAGS_DBR_NQ_EVENT_ENABLE;
		req.enables =
		cpu_to_le32(HWRM_FUNC_DBR_PACING_CFG_INPUT_ENABLES_PRIMARY_NQ_ID_VALID |
			    HWRM_FUNC_DBR_PACING_CFG_INPUT_ENABLES_PACING_THRESHOLD_VALID);
	} else {
		req.flags = HWRM_FUNC_DBR_PACING_CFG_INPUT_FLAGS_DBR_NQ_EVENT_DISABLE;
	}
	req.primary_nq_id = cpu_to_le32(rdev->dbq_nq_id);
	req.pacing_threshold = cpu_to_le32(rdev->dbq_watermark);
	dev_dbg(rdev_to_dev(rdev), "%s: nq_id = 0x%x pacing_threshold = 0x%x",
		__func__, req.primary_nq_id, req.pacing_threshold);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to set dbr pacing config, rc = %#x", rc);
		return rc;
	}
	return 0;
}

/* Net -> RoCE driver */

/* Device */
struct bnxt_re_dev *bnxt_re_from_netdev(struct ifnet *netdev)
{
	struct bnxt_re_dev *rdev;

	rcu_read_lock();
	list_for_each_entry_rcu(rdev, &bnxt_re_dev_list, list) {
		if (rdev->netdev == netdev) {
			rcu_read_unlock();
			dev_dbg(rdev_to_dev(rdev),
				"netdev (%p) found, ref_count = 0x%x",
				netdev, atomic_read(&rdev->ref_count));
			return rdev;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", rdev->en_dev->pdev->vendor);
}


static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev.dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", rdev->ibdev.node_desc);
}

static DEVICE_ATTR(hw_rev, 0444, show_rev, NULL);
static DEVICE_ATTR(hca_type, 0444, show_hca, NULL);
static struct device_attribute *bnxt_re_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_hca_type
};

int ib_register_device_compat(struct bnxt_re_dev *rdev)
{
	struct ib_device *ibdev = &rdev->ibdev;
	char name[IB_DEVICE_NAME_MAX];

	memset(name, 0, IB_DEVICE_NAME_MAX);
	strlcpy(name, "bnxt_re%d", IB_DEVICE_NAME_MAX);

	strlcpy(ibdev->name, name, IB_DEVICE_NAME_MAX);

	return ib_register_device(ibdev, NULL);
}

static int bnxt_re_register_ib(struct bnxt_re_dev *rdev)
{
	struct ib_device *ibdev = &rdev->ibdev;
	int ret = 0;

	/* ib device init */
	ibdev->owner = THIS_MODULE;
	ibdev->uverbs_abi_ver = BNXT_RE_ABI_VERSION;
	ibdev->node_type = RDMA_NODE_IB_CA;
	strlcpy(ibdev->node_desc, BNXT_RE_DESC " HCA",
		strlen(BNXT_RE_DESC) + 5);
	ibdev->phys_port_cnt = 1;

	bnxt_qplib_get_guid(rdev->dev_addr, (u8 *)&ibdev->node_guid);

	/* Data path irqs is one less than the max msix vectors */
	ibdev->num_comp_vectors	= rdev->nqr.num_msix - 1;
	bnxt_re_set_dma_device(ibdev, rdev);
	ibdev->local_dma_lkey = BNXT_QPLIB_RSVD_LKEY;

	/* User space */
	ibdev->uverbs_cmd_mask =
			(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
			(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
			(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
			(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
			(1ull << IB_USER_VERBS_CMD_REG_MR)		|
			(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL) |
			(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
			(1ull << IB_USER_VERBS_CMD_REREG_MR)		|
			(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ)		|
			(1ull << IB_USER_VERBS_CMD_ALLOC_MW)		|
			(1ull << IB_USER_VERBS_CMD_DEALLOC_MW)		|
			(1ull << IB_USER_VERBS_CMD_CREATE_AH)		|
			(1ull << IB_USER_VERBS_CMD_MODIFY_AH)		|
			(1ull << IB_USER_VERBS_CMD_QUERY_AH)		|
			(1ull << IB_USER_VERBS_CMD_DESTROY_AH);

	ibdev->uverbs_ex_cmd_mask = (1ull << IB_USER_VERBS_EX_CMD_MODIFY_QP);
	ibdev->uverbs_cmd_mask |= (1ull << IB_USER_VERBS_CMD_POLL_CQ);

#define bnxt_re_ib_ah bnxt_re_ah
#define bnxt_re_ib_cq bnxt_re_cq
#define bnxt_re_ib_pd bnxt_re_pd
#define bnxt_re_ib_srq bnxt_re_srq
#define bnxt_re_ib_ucontext bnxt_re_ucontext
	INIT_IB_DEVICE_OPS(&ibdev->ops, bnxt_re, BNXT_RE);

	ibdev->query_device		= bnxt_re_query_device;
	ibdev->modify_device		= bnxt_re_modify_device;
	ibdev->query_port		= bnxt_re_query_port;
	ibdev->modify_port		= bnxt_re_modify_port;
	ibdev->get_port_immutable	= bnxt_re_get_port_immutable;
	ibdev->query_pkey		= bnxt_re_query_pkey;
	ibdev->query_gid		= bnxt_re_query_gid;
	ibdev->get_netdev		= bnxt_re_get_netdev;
	ibdev->add_gid			= bnxt_re_add_gid;
	ibdev->del_gid			= bnxt_re_del_gid;
	ibdev->get_link_layer		= bnxt_re_get_link_layer;
	ibdev->alloc_pd			= bnxt_re_alloc_pd;
	ibdev->dealloc_pd		= bnxt_re_dealloc_pd;
	ibdev->create_ah		= bnxt_re_create_ah;
	ibdev->modify_ah		= bnxt_re_modify_ah;
	ibdev->query_ah			= bnxt_re_query_ah;
	ibdev->destroy_ah		= bnxt_re_destroy_ah;
	ibdev->create_srq		= bnxt_re_create_srq;
	ibdev->modify_srq		= bnxt_re_modify_srq;
	ibdev->query_srq		= bnxt_re_query_srq;
	ibdev->destroy_srq		= bnxt_re_destroy_srq;
	ibdev->post_srq_recv		= bnxt_re_post_srq_recv;
	ibdev->create_qp		= bnxt_re_create_qp;
	ibdev->modify_qp		= bnxt_re_modify_qp;
	ibdev->query_qp			= bnxt_re_query_qp;
	ibdev->destroy_qp		= bnxt_re_destroy_qp;
	ibdev->post_send		= bnxt_re_post_send;
	ibdev->post_recv		= bnxt_re_post_recv;
	ibdev->create_cq		= bnxt_re_create_cq;
	ibdev->modify_cq		= bnxt_re_modify_cq;
	ibdev->destroy_cq		= bnxt_re_destroy_cq;
	ibdev->resize_cq		= bnxt_re_resize_cq;
	ibdev->poll_cq			= bnxt_re_poll_cq;
	ibdev->req_notify_cq		= bnxt_re_req_notify_cq;
	ibdev->get_dma_mr		= bnxt_re_get_dma_mr;
	ibdev->get_hw_stats		= bnxt_re_get_hw_stats;
	ibdev->alloc_hw_stats		= bnxt_re_alloc_hw_port_stats;
	ibdev->dereg_mr			= bnxt_re_dereg_mr;
	ibdev->alloc_mr			= bnxt_re_alloc_mr;
	ibdev->map_mr_sg		= bnxt_re_map_mr_sg;
	ibdev->alloc_mw			= bnxt_re_alloc_mw;
	ibdev->dealloc_mw		= bnxt_re_dealloc_mw;
	ibdev->reg_user_mr		= bnxt_re_reg_user_mr;
	ibdev->rereg_user_mr		= bnxt_re_rereg_user_mr;
	ibdev->disassociate_ucontext	= bnxt_re_disassociate_ucntx;
	ibdev->alloc_ucontext		= bnxt_re_alloc_ucontext;
	ibdev->dealloc_ucontext		= bnxt_re_dealloc_ucontext;
	ibdev->mmap			= bnxt_re_mmap;
	ibdev->process_mad		= bnxt_re_process_mad;

	ret = ib_register_device_compat(rdev);
	return ret;
}

static void bnxt_re_dev_dealloc(struct bnxt_re_dev *rdev)
{
	int i = BNXT_RE_REF_WAIT_COUNT;

	dev_dbg(rdev_to_dev(rdev), "%s:Remove the device %p\n", __func__, rdev);
	/* Wait for rdev refcount to come down */
	while ((atomic_read(&rdev->ref_count) > 1) && i--)
		msleep(100);

	if (atomic_read(&rdev->ref_count) > 1)
		dev_err(rdev_to_dev(rdev),
			"Failed waiting for ref count to deplete %d",
			atomic_read(&rdev->ref_count));

	atomic_set(&rdev->ref_count, 0);
	if_rele(rdev->netdev);
	rdev->netdev = NULL;
	synchronize_rcu();

	kfree(rdev->gid_map);
	kfree(rdev->dbg_stats);
	ib_dealloc_device(&rdev->ibdev);
}

static struct bnxt_re_dev *bnxt_re_dev_alloc(struct ifnet *netdev,
					   struct bnxt_en_dev *en_dev)
{
	struct bnxt_re_dev *rdev;
	u32 count;

	/* Allocate bnxt_re_dev instance here */
	rdev = (struct bnxt_re_dev *)compat_ib_alloc_device(sizeof(*rdev));
	if (!rdev) {
		pr_err("%s: bnxt_re_dev allocation failure!",
			ROCE_DRV_MODULE_NAME);
		return NULL;
	}
	/* Default values */
	atomic_set(&rdev->ref_count, 0);
	rdev->netdev = netdev;
	dev_hold(rdev->netdev);
	rdev->en_dev = en_dev;
	rdev->id = rdev->en_dev->pdev->devfn;
	INIT_LIST_HEAD(&rdev->qp_list);
	mutex_init(&rdev->qp_lock);
	mutex_init(&rdev->cc_lock);
	mutex_init(&rdev->dbq_lock);
	bnxt_re_clear_rsors_stat(&rdev->stats.rsors);
	rdev->cosq[0] = rdev->cosq[1] = 0xFFFF;
	rdev->min_tx_depth = 1;
	rdev->stats.stats_query_sec = 1;
	/* Disable priority vlan as the default mode is DSCP based PFC */
	rdev->cc_param.disable_prio_vlan_tx = 1;

	/* Initialize worker for DBR Pacing */
	INIT_WORK(&rdev->dbq_fifo_check_work, bnxt_re_db_fifo_check);
	INIT_DELAYED_WORK(&rdev->dbq_pacing_work, bnxt_re_pacing_timer_exp);
	rdev->gid_map = kzalloc(sizeof(*(rdev->gid_map)) *
				  BNXT_RE_MAX_SGID_ENTRIES,
				  GFP_KERNEL);
	if (!rdev->gid_map) {
		ib_dealloc_device(&rdev->ibdev);
		return NULL;
	}
	for(count = 0; count < BNXT_RE_MAX_SGID_ENTRIES; count++)
		rdev->gid_map[count] = -1;

	rdev->dbg_stats = kzalloc(sizeof(*rdev->dbg_stats), GFP_KERNEL);
	if (!rdev->dbg_stats) {
		ib_dealloc_device(&rdev->ibdev);
		return NULL;
	}

	return rdev;
}

static int bnxt_re_handle_unaffi_async_event(
		struct creq_func_event *unaffi_async)
{
	switch (unaffi_async->event) {
	case CREQ_FUNC_EVENT_EVENT_TX_WQE_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TX_DATA_ERROR:
	case CREQ_FUNC_EVENT_EVENT_RX_WQE_ERROR:
	case CREQ_FUNC_EVENT_EVENT_RX_DATA_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CQ_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TQM_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCQ_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCS_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCC_ERROR:
	case CREQ_FUNC_EVENT_EVENT_CFCM_ERROR:
	case CREQ_FUNC_EVENT_EVENT_TIM_ERROR:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bnxt_re_handle_qp_async_event(void *qp_event, struct bnxt_re_qp *qp)
{
	struct creq_qp_error_notification *err_event;
	struct ib_event event;
	unsigned int flags;

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_ERR &&
	    !qp->qplib_qp.is_user) {
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_add_flush_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}
	memset(&event, 0, sizeof(event));
	event.device = &qp->rdev->ibdev;
	event.element.qp = &qp->ib_qp;
	event.event = IB_EVENT_QP_FATAL;

	err_event = qp_event;
	switch(err_event->res_err_state_reason) {
	case CFCQ_RES_ERR_STATE_REASON_RES_EXCEED_MAX:
	case CFCQ_RES_ERR_STATE_REASON_RES_PAYLOAD_LENGTH_MISMATCH:
	case CFCQ_RES_ERR_STATE_REASON_RES_OPCODE_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_PSN_SEQ_ERROR_RETRY_LIMIT:
	case CFCQ_RES_ERR_STATE_REASON_RES_RX_INVALID_R_KEY:
	case CFCQ_RES_ERR_STATE_REASON_RES_RX_DOMAIN_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_RX_NO_PERMISSION:
	case CFCQ_RES_ERR_STATE_REASON_RES_RX_RANGE_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_TX_INVALID_R_KEY:
	case CFCQ_RES_ERR_STATE_REASON_RES_TX_DOMAIN_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_TX_NO_PERMISSION:
	case CFCQ_RES_ERR_STATE_REASON_RES_TX_RANGE_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_IVALID_DUP_RKEY:
	case CFCQ_RES_ERR_STATE_REASON_RES_UNALIGN_ATOMIC:
		event.event = IB_EVENT_QP_ACCESS_ERR;
		break;
	case CFCQ_RES_ERR_STATE_REASON_RES_EXCEEDS_WQE:
	case CFCQ_RES_ERR_STATE_REASON_RES_WQE_FORMAT_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_SRQ_LOAD_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_UNSUPPORTED_OPCODE:
	case CFCQ_RES_ERR_STATE_REASON_RES_REM_INVALIDATE:
		event.event = IB_EVENT_QP_REQ_ERR;
		break;
	case CFCQ_RES_ERR_STATE_REASON_RES_IRRQ_OFLOW:
	case CFCQ_RES_ERR_STATE_REASON_RES_CMP_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_CQ_LOAD_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_TX_PCI_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_RX_PCI_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_MEMORY_ERROR:
	case CFCQ_RES_ERR_STATE_REASON_RES_SRQ_ERROR:
		event.event = IB_EVENT_QP_FATAL;
		break;
	default:
		if (qp->qplib_qp.srq)
			event.event = IB_EVENT_QP_LAST_WQE_REACHED;
		break;
	}

	if (err_event->res_err_state_reason)
		dev_err(rdev_to_dev(qp->rdev),
			"%s %s qp_id: %d cons (%d %d) req (%d %d) res (%d %d)\n",
			__func__,  qp->qplib_qp.is_user ? "user" : "kernel",
			qp->qplib_qp.id,
			err_event->sq_cons_idx,
			err_event->rq_cons_idx,
			err_event->req_slow_path_state,
			err_event->req_err_state_reason,
			err_event->res_slow_path_state,
			err_event->res_err_state_reason);

	if (event.device && qp->ib_qp.event_handler)
		qp->ib_qp.event_handler(&event, qp->ib_qp.qp_context);

	return 0;
}

static int bnxt_re_handle_cq_async_error(void *event, struct bnxt_re_cq *cq)
{
	struct creq_cq_error_notification *cqerr;
	bool send = false;

	cqerr = event;
	switch (cqerr->cq_err_reason) {
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_INVALID_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_OVERFLOW_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_REQ_CQ_LOAD_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_INVALID_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_OVERFLOW_ERROR:
	case CREQ_CQ_ERROR_NOTIFICATION_CQ_ERR_REASON_RES_CQ_LOAD_ERROR:
		send = true;
	default:
		break;
	}

	if (send && cq->ibcq.event_handler) {
		struct ib_event ibevent = {};

		ibevent.event = IB_EVENT_CQ_ERR;
		ibevent.element.cq = &cq->ibcq;
		ibevent.device = &cq->rdev->ibdev;

		dev_err(rdev_to_dev(cq->rdev),
			"%s err reason %d\n", __func__, cqerr->cq_err_reason);
		cq->ibcq.event_handler(&ibevent, cq->ibcq.cq_context);
	}

	cq->qplib_cq.is_cq_err_event = true;

	return 0;
}

static int bnxt_re_handle_affi_async_event(struct creq_qp_event *affi_async,
					   void *obj)
{
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_qplib_cq *qplcq;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;
	int rc = 0;
	u8 event;

	if (!obj)
		return rc; /* QP was already dead, still return success */

	event = affi_async->event;
	switch (event) {
	case CREQ_QP_EVENT_EVENT_QP_ERROR_NOTIFICATION:
		qplqp = obj;
		qp = container_of(qplqp, struct bnxt_re_qp, qplib_qp);
		rc = bnxt_re_handle_qp_async_event(affi_async, qp);
		break;
	case CREQ_QP_EVENT_EVENT_CQ_ERROR_NOTIFICATION:
		qplcq = obj;
		cq = container_of(qplcq, struct bnxt_re_cq, qplib_cq);
		rc = bnxt_re_handle_cq_async_error(affi_async, cq);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int bnxt_re_aeq_handler(struct bnxt_qplib_rcfw *rcfw,
			       void *aeqe, void *obj)
{
	struct creq_func_event *unaffi_async;
	struct creq_qp_event *affi_async;
	u8 type;
	int rc;

	type = ((struct creq_base *)aeqe)->type;
	if (type == CREQ_BASE_TYPE_FUNC_EVENT) {
		unaffi_async = aeqe;
		rc = bnxt_re_handle_unaffi_async_event(unaffi_async);
	} else {
		affi_async = aeqe;
		rc = bnxt_re_handle_affi_async_event(affi_async, obj);
	}

	return rc;
}

static int bnxt_re_srqn_handler(struct bnxt_qplib_nq *nq,
				struct bnxt_qplib_srq *handle, u8 event)
{
	struct bnxt_re_srq *srq = to_bnxt_re(handle, struct bnxt_re_srq,
					     qplib_srq);
	struct ib_event ib_event;

	if (srq == NULL) {
		pr_err("%s: SRQ is NULL, SRQN not handled",
			ROCE_DRV_MODULE_NAME);
		return -EINVAL;
	}
	ib_event.device = &srq->rdev->ibdev;
	ib_event.element.srq = &srq->ibsrq;
	if (event == NQ_SRQ_EVENT_EVENT_SRQ_THRESHOLD_EVENT)
		ib_event.event = IB_EVENT_SRQ_LIMIT_REACHED;
	else
		ib_event.event = IB_EVENT_SRQ_ERR;

	if (srq->ibsrq.event_handler) {
		/* Lock event_handler? */
		(*srq->ibsrq.event_handler)(&ib_event,
					     srq->ibsrq.srq_context);
	}
	return 0;
}

static int bnxt_re_cqn_handler(struct bnxt_qplib_nq *nq,
			       struct bnxt_qplib_cq *handle)
{
	struct bnxt_re_cq *cq = to_bnxt_re(handle, struct bnxt_re_cq,
					   qplib_cq);
	u32 *cq_ptr;

	if (cq == NULL) {
		pr_err("%s: CQ is NULL, CQN not handled",
			ROCE_DRV_MODULE_NAME);
		return -EINVAL;
	}
	/* CQ already in destroy path. Do not handle any more events */
	if (handle->destroyed || !atomic_read(&cq->ibcq.usecnt)) {
		if (!handle->destroyed)
			dev_dbg(NULL, "%s: CQ being destroyed, CQN not handled",
				ROCE_DRV_MODULE_NAME);
		return 0;
	}

	if (cq->ibcq.comp_handler) {
		if (cq->uctx_cq_page) {
			cq_ptr = (u32 *)cq->uctx_cq_page;
			*cq_ptr = cq->qplib_cq.toggle;
		}
		/* Lock comp_handler? */
		(*cq->ibcq.comp_handler)(&cq->ibcq, cq->ibcq.cq_context);
	}

	return 0;
}

struct bnxt_qplib_nq *bnxt_re_get_nq(struct bnxt_re_dev *rdev)
{
	int min, indx;

	mutex_lock(&rdev->nqr.load_lock);
	for (indx = 0, min = 0; indx < (rdev->nqr.num_msix - 1); indx++) {
		if (rdev->nqr.nq[min].load > rdev->nqr.nq[indx].load)
			min = indx;
	}
	rdev->nqr.nq[min].load++;
	mutex_unlock(&rdev->nqr.load_lock);

	return &rdev->nqr.nq[min];
}

void bnxt_re_put_nq(struct bnxt_re_dev *rdev, struct bnxt_qplib_nq *nq)
{
	mutex_lock(&rdev->nqr.load_lock);
	nq->load--;
	mutex_unlock(&rdev->nqr.load_lock);
}

static bool bnxt_re_check_min_attr(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_dev_attr *attr;
	bool rc = true;

	attr = rdev->dev_attr;

	if (!attr->max_cq || !attr->max_qp ||
	    !attr->max_sgid || !attr->max_mr) {
		dev_err(rdev_to_dev(rdev),"Insufficient RoCE resources");
		dev_dbg(rdev_to_dev(rdev),
			"max_cq = %d, max_qp = %d, max_dpi = %d, max_sgid = %d, max_mr = %d",
			attr->max_cq, attr->max_qp, attr->max_dpi,
			attr->max_sgid, attr->max_mr);
		rc = false;
	}
	return rc;
}

static void bnxt_re_dispatch_event(struct ib_device *ibdev, struct ib_qp *qp,
				   u8 port_num, enum ib_event_type event)
{
	struct ib_event ib_event;

	ib_event.device = ibdev;
	if (qp) {
		ib_event.element.qp = qp;
		ib_event.event = event;
		if (qp->event_handler)
			qp->event_handler(&ib_event, qp->qp_context);
	} else {
		ib_event.element.port_num = port_num;
		ib_event.event = event;
		ib_dispatch_event(&ib_event);
	}

	dev_dbg(rdev_to_dev(to_bnxt_re_dev(ibdev, ibdev)),
		"ibdev %p Event 0x%x port_num 0x%x", ibdev, event, port_num);
}

static bool bnxt_re_is_qp1_or_shadow_qp(struct bnxt_re_dev *rdev,
					struct bnxt_re_qp *qp)
{
	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL)
		return (qp->ib_qp.qp_type == IB_QPT_GSI) ||
			(qp == rdev->gsi_ctx.gsi_sqp);
	else
		return (qp->ib_qp.qp_type == IB_QPT_GSI);
}

static void bnxt_re_stop_all_nonqp1_nonshadow_qps(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_qp *qpl_qp;
	bool dev_detached = false;
	struct ib_qp_attr qp_attr;
	int num_qps_stopped = 0;
	int mask = IB_QP_STATE;
	struct bnxt_re_qp *qp;
	unsigned long flags;

	if (!rdev)
		return;

restart:
	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		dev_detached = true;

	qp_attr.qp_state = IB_QPS_ERR;
	mutex_lock(&rdev->qp_lock);
	list_for_each_entry(qp, &rdev->qp_list, list) {
		qpl_qp = &qp->qplib_qp;
		if (dev_detached || !bnxt_re_is_qp1_or_shadow_qp(rdev, qp)) {
			if (qpl_qp->state !=
			    CMDQ_MODIFY_QP_NEW_STATE_RESET &&
			    qpl_qp->state !=
			    CMDQ_MODIFY_QP_NEW_STATE_ERR) {
				if (dev_detached) {
					/*
					 * Cant actually send the command down,
					 * marking the state for bookkeeping
					 */
					qpl_qp->state =
						CMDQ_MODIFY_QP_NEW_STATE_ERR;
					qpl_qp->cur_qp_state = qpl_qp->state;
					if (!qpl_qp->is_user) {
						/* Add to flush list */
						flags = bnxt_re_lock_cqs(qp);
						bnxt_qplib_add_flush_qp(qpl_qp);
						bnxt_re_unlock_cqs(qp, flags);
					}
				} else {
					num_qps_stopped++;
					bnxt_re_modify_qp(&qp->ib_qp,
							  &qp_attr, mask,
							  NULL);
				}

				bnxt_re_dispatch_event(&rdev->ibdev, &qp->ib_qp,
						       1, IB_EVENT_QP_FATAL);
				/*
				 * 1. Release qp_lock after a budget to unblock other verb
				 *    requests (like qp_destroy) from stack.
				 * 2. Traverse through the qp_list freshly as addition / deletion
				 *    might have happened since qp_lock is getting released here.
				 */
				if (num_qps_stopped % BNXT_RE_STOP_QPS_BUDGET == 0) {
					mutex_unlock(&rdev->qp_lock);
					goto restart;
				}
			}
		}
	}

	mutex_unlock(&rdev->qp_lock);
}

static int bnxt_re_update_gid(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_qplib_gid gid;
	u16 gid_idx, index;
	int rc = 0;

	if (!test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
		return 0;

	if (sgid_tbl == NULL) {
		dev_err(rdev_to_dev(rdev), "QPLIB: SGID table not allocated");
		return -EINVAL;
	}

	for (index = 0; index < sgid_tbl->active; index++) {
		gid_idx = sgid_tbl->hw_id[index];

		if (!memcmp(&sgid_tbl->tbl[index], &bnxt_qplib_gid_zero,
			    sizeof(bnxt_qplib_gid_zero)))
			continue;
		/* Need to modify the VLAN enable setting of non VLAN GID only
		 * as setting is done for VLAN GID while adding GID
		 *
		 * If disable_prio_vlan_tx is enable, then we'll need to remove the
		 * vlan entry from the sgid_tbl.
		 */
		if (sgid_tbl->vlan[index] == true)
			continue;

		memcpy(&gid, &sgid_tbl->tbl[index], sizeof(gid));

		rc = bnxt_qplib_update_sgid(sgid_tbl, &gid, gid_idx,
					    rdev->dev_addr);
	}

	return rc;
}

static void bnxt_re_clear_cc(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;

	if (_is_chip_p7(rdev->chip_ctx)) {
		cc_param->mask = CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
	} else {
		cc_param->mask = (CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_CC_MODE |
				  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC |
				  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN);

		if (!is_qport_service_type_supported(rdev))
			cc_param->mask |=
			(CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP);
	}

	cc_param->cur_mask  = cc_param->mask;

	if (bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param))
		dev_err(rdev_to_dev(rdev), "Failed to modify cc\n");
}

static int bnxt_re_setup_cc(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_cc_param *cc_param = &rdev->cc_param;
	int rc;

	if (_is_chip_p7(rdev->chip_ctx)) {
		cc_param->enable = 0x0;
		cc_param->mask = CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
	} else {
		cc_param->enable = 0x1;
		cc_param->mask = (CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_CC_MODE |
				  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC |
				  CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN);

		if (!is_qport_service_type_supported(rdev))
			cc_param->mask |=
			(CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP |
			 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP);
	}

	cc_param->cur_mask  = cc_param->mask;

	rc = bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to modify cc\n");
		return rc;
	}
	/* Reset the programming mask */
	cc_param->mask = 0;
	if (cc_param->qp1_tos_dscp != cc_param->tos_dscp) {
		cc_param->qp1_tos_dscp = cc_param->tos_dscp;
		rc = bnxt_re_update_qp1_tos_dscp(rdev);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "%s:Failed to modify QP1:%d",
				__func__, rc);
			goto clear;
		}
	}
	return 0;

clear:
	bnxt_re_clear_cc(rdev);
	return rc;
}

int bnxt_re_query_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
				struct bnxt_re_dscp2pri *d2p, u16 *count,
				u16 target_id)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_dscp2pri_qcfg_input req;
	struct hwrm_queue_dscp2pri_qcfg_output resp;
	struct bnxt_re_dscp2pri *dscp2pri;
	struct bnxt_fw_msg fw_msg;
	u16 in_count = *count;
	dma_addr_t dma_handle;
	int rc = 0, i;
	u16 data_len;
	u8 *kmem;

	data_len = *count * sizeof(*dscp2pri);
	memset(&fw_msg, 0, sizeof(fw_msg));
	memset(&req, 0, sizeof(req));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_QUEUE_DSCP2PRI_QCFG, -1, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	kmem = dma_zalloc_coherent(&en_dev->pdev->dev, data_len, &dma_handle,
				   GFP_KERNEL);
	if (!kmem) {
		dev_err(rdev_to_dev(rdev),
			"dma_zalloc_coherent failure, length = %u\n",
			(unsigned)data_len);
		return -ENOMEM;
	}
	req.dest_data_addr = cpu_to_le64(dma_handle);
	req.dest_data_buffer_size = cpu_to_le16(data_len);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc)
		goto out;

	/* Upload the DSCP-MASK-PRI tuple(s) */
	dscp2pri = (struct bnxt_re_dscp2pri *)kmem;
	for (i = 0; i < le16_to_cpu(resp.entry_cnt) && i < in_count; i++) {
		d2p[i].dscp = dscp2pri->dscp;
		d2p[i].mask = dscp2pri->mask;
		d2p[i].pri = dscp2pri->pri;
		dscp2pri++;
	}
	*count = le16_to_cpu(resp.entry_cnt);
out:
	dma_free_coherent(&en_dev->pdev->dev, data_len, kmem, dma_handle);
	return rc;
}

int bnxt_re_prio_vlan_tx_update(struct bnxt_re_dev *rdev)
{
	/* Remove the VLAN from the GID entry */
	if (rdev->cc_param.disable_prio_vlan_tx)
		rdev->qplib_res.prio = false;
	else
		rdev->qplib_res.prio = true;

	return bnxt_re_update_gid(rdev);
}

int bnxt_re_set_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
			      struct bnxt_re_dscp2pri *d2p, u16 count,
			      u16 target_id)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_dscp2pri_cfg_input req;
	struct hwrm_queue_dscp2pri_cfg_output resp;
	struct bnxt_fw_msg fw_msg;
	struct bnxt_re_dscp2pri *dscp2pri;
	int i, rc, data_len = 3 * 256;
	dma_addr_t dma_handle;
	u8 *kmem;

	memset(&req, 0, sizeof(req));
	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_QUEUE_DSCP2PRI_CFG, -1, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	kmem = dma_alloc_coherent(&en_dev->pdev->dev, data_len, &dma_handle,
				  GFP_KERNEL);
	if (!kmem) {
		dev_err(rdev_to_dev(rdev),
			"dma_alloc_coherent failure, length = %u\n",
			(unsigned)data_len);
		return -ENOMEM;
	}
	req.src_data_addr = cpu_to_le64(dma_handle);

	/* Download the DSCP-MASK-PRI tuple(s) */
	dscp2pri = (struct bnxt_re_dscp2pri *)kmem;
	for (i = 0; i < count; i++) {
		dscp2pri->dscp = d2p[i].dscp;
		dscp2pri->mask = d2p[i].mask;
		dscp2pri->pri = d2p[i].pri;
		dscp2pri++;
	}

	req.entry_cnt = cpu_to_le16(count);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	dma_free_coherent(&en_dev->pdev->dev, data_len, kmem, dma_handle);
	return rc;
}

int bnxt_re_query_hwrm_qportcfg(struct bnxt_re_dev *rdev,
			struct bnxt_re_tc_rec *tc_rec, u16 tid)
{
	u8 max_tc, tc, *qptr, *type_ptr0, *type_ptr1;
	struct hwrm_queue_qportcfg_output resp = {0};
	struct hwrm_queue_qportcfg_input req = {0};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;
	bool def_init = false;
	u8 *tmp_type;
	u8 cos_id;
	int rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req, HWRM_QUEUE_QPORTCFG,
			      -1, tid);
	req.port_id = (tid == 0xFFFF) ? en_dev->pf_port_id : 1;
	if (BNXT_EN_ASYM_Q(en_dev))
		req.flags = htole32(HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX);

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc)
		return rc;

	if (!resp.max_configurable_queues)
		return -EINVAL;

	max_tc = resp.max_configurable_queues;
	tc_rec->max_tc = max_tc;

	if (resp.queue_cfg_info & HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_CFG_INFO_USE_PROFILE_TYPE)
		tc_rec->serv_type_enabled = true;

	qptr = &resp.queue_id0;
	type_ptr0 = &resp.queue_id0_service_profile_type;
	type_ptr1 = &resp.queue_id1_service_profile_type;
	for (tc = 0; tc < max_tc; tc++) {
		tmp_type = tc ? type_ptr1 + (tc - 1) : type_ptr0;

		cos_id = *qptr++;
		/* RoCE CoS queue is the first cos queue.
		 * For MP12 and MP17 order is 405 and 141015.
		 */
		if (is_bnxt_roce_queue(rdev, *qptr, *tmp_type)) {
			tc_rec->cos_id_roce = cos_id;
			tc_rec->tc_roce = tc;
		} else if (is_bnxt_cnp_queue(rdev, *qptr, *tmp_type)) {
			tc_rec->cos_id_cnp = cos_id;
			tc_rec->tc_cnp = tc;
		} else if (!def_init) {
			def_init = true;
			tc_rec->tc_def = tc;
			tc_rec->cos_id_def = cos_id;
		}
		qptr++;
	}

	return rc;
}

int bnxt_re_hwrm_cos2bw_qcfg(struct bnxt_re_dev *rdev, u16 target_id,
			     struct bnxt_re_cos2bw_cfg *cfg)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_cos2bw_qcfg_output resp;
	struct hwrm_queue_cos2bw_qcfg_input req = {0};
	struct bnxt_fw_msg fw_msg;
	int rc, indx;
	void *data;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_QUEUE_COS2BW_QCFG, -1, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	if (rc)
		return rc;
	data = &resp.queue_id0 + offsetof(struct bnxt_re_cos2bw_cfg,
					  queue_id);
	for (indx = 0; indx < 8; indx++, data += (sizeof(cfg->cfg))) {
		memcpy(&cfg->cfg, data, sizeof(cfg->cfg));
		if (indx == 0)
			cfg->queue_id = resp.queue_id0;
		cfg++;
	}

	return rc;
}

int bnxt_re_hwrm_cos2bw_cfg(struct bnxt_re_dev *rdev, u16 target_id,
			    struct bnxt_re_cos2bw_cfg *cfg)
{
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct hwrm_queue_cos2bw_cfg_input req = {0};
	struct hwrm_queue_cos2bw_cfg_output resp = {0};
	struct bnxt_fw_msg fw_msg;
	void *data;
	int indx;
	int rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_QUEUE_COS2BW_CFG, -1, target_id);
	req.port_id = (target_id == 0xFFFF) ? en_dev->pf_port_id : 1;

	/* Chimp wants enable bit to retain previous
	 * config done by L2 driver
	 */
	for (indx = 0; indx < 8; indx++) {
		if (cfg[indx].queue_id < 40) {
			req.enables |= cpu_to_le32(
				HWRM_QUEUE_COS2BW_CFG_INPUT_ENABLES_COS_QUEUE_ID0_VALID <<
				indx);
		}

		data = (char *)&req.unused_0 + indx * (sizeof(*cfg) - 4);
		memcpy(data, &cfg[indx].queue_id, sizeof(*cfg) - 4);
		if (indx == 0) {
			req.queue_id0 = cfg[0].queue_id;
			req.unused_0 = 0;
		}
	}

	memset(&resp, 0, sizeof(resp));
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);
	return rc;
}

int bnxt_re_host_pf_id_query(struct bnxt_re_dev *rdev,
			     struct bnxt_qplib_query_fn_info *fn_info,
			     u32 *pf_mask, u32 *first_pf)
{
	struct hwrm_func_host_pf_ids_query_output resp = {0};
	struct hwrm_func_host_pf_ids_query_input req;
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg;
	int rc;

	memset(&fw_msg, 0, sizeof(fw_msg));
	memset(&req, 0, sizeof(req));
	bnxt_re_init_hwrm_hdr(rdev, (void *)&req,
			      HWRM_FUNC_HOST_PF_IDS_QUERY, -1, -1);
	/* To query the info from the host EPs */
	switch (fn_info->host) {
		case HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_SOC:
		case HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_EP_0:
		case HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_EP_1:
		case HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_EP_2:
		case HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_EP_3:
			req.host = fn_info->host;
		break;
		default:
			req.host = HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_HOST_EP_0;
		break;
	}

	req.filter = fn_info->filter;
	if (req.filter > HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_FILTER_ROCE)
		req.filter = HWRM_FUNC_HOST_PF_IDS_QUERY_INPUT_FILTER_ALL;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = en_dev->en_ops->bnxt_send_fw_msg(en_dev, BNXT_ROCE_ULP, &fw_msg);


	*first_pf = le16_to_cpu(resp.first_pf_id);
	*pf_mask = le16_to_cpu(resp.pf_ordinal_mask);

	return rc;
}

static void bnxt_re_put_stats_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid = 0xffff;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	if (test_and_clear_bit(BNXT_RE_FLAG_STATS_CTX_ALLOC, &rdev->flags)) {
		bnxt_re_net_stats_ctx_free(rdev, hctx->stats.fw_id, tid);
		bnxt_qplib_free_stat_mem(res, &hctx->stats);
	}
}

static void bnxt_re_put_stats2_ctx(struct bnxt_re_dev *rdev)
{
	test_and_clear_bit(BNXT_RE_FLAG_STATS_CTX2_ALLOC, &rdev->flags);
}

static int bnxt_re_get_stats_ctx(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	u16 tid = 0xffff;
	int rc;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	rc = bnxt_qplib_alloc_stat_mem(res->pdev, rdev->chip_ctx, &hctx->stats);
	if (rc)
		return -ENOMEM;
	rc = bnxt_re_net_stats_ctx_alloc(rdev, tid);
	if (rc)
		goto free_stat_mem;
	set_bit(BNXT_RE_FLAG_STATS_CTX_ALLOC, &rdev->flags);

	return 0;

free_stat_mem:
	bnxt_qplib_free_stat_mem(res, &hctx->stats);

	return rc;
}

static int bnxt_re_update_dev_attr(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_qplib_get_dev_attr(&rdev->rcfw);
	if (rc)
		return rc;
	if (!bnxt_re_check_min_attr(rdev))
		return -EINVAL;
	return 0;
}

static void bnxt_re_free_tbls(struct bnxt_re_dev *rdev)
{
	bnxt_qplib_clear_tbls(&rdev->qplib_res);
	bnxt_qplib_free_tbls(&rdev->qplib_res);
}

static int bnxt_re_alloc_init_tbls(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_chip_ctx *chip_ctx = rdev->chip_ctx;
	u8 pppp_factor = 0;
	int rc;

	 /*
	  * TODO: Need a better mechanism for spreading of the
	  * 512 extended PPP pages. For now, spreading it
	  * based on port_count
	  */
	if (_is_chip_p7(chip_ctx) && chip_ctx->modes.db_push)
		pppp_factor = rdev->en_dev->port_count;
	rc = bnxt_qplib_alloc_tbls(&rdev->qplib_res, pppp_factor);
	if (rc)
		return rc;
	bnxt_qplib_init_tbls(&rdev->qplib_res);
	set_bit(BNXT_RE_FLAG_TBLS_ALLOCINIT, &rdev->flags);

	return 0;
}

static void bnxt_re_clean_nqs(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;
	int i;

	if (!rdev->nqr.max_init)
		return;

	for (i = (rdev->nqr.max_init - 1) ; i >= 0; i--) {
		nq = &rdev->nqr.nq[i];
		bnxt_qplib_disable_nq(nq);
		bnxt_re_net_ring_free(rdev, nq->ring_id);
		bnxt_qplib_free_nq_mem(nq);
	}
	rdev->nqr.max_init = 0;
}

static int bnxt_re_setup_nqs(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_ring_attr rattr = {};
	struct bnxt_qplib_nq *nq;
	int rc, i;
	int depth;
	u32 offt;
	u16 vec;

	mutex_init(&rdev->nqr.load_lock);
	/*
	 * TODO: Optimize the depth based on the
	 * number of NQs.
	 */
	depth = BNXT_QPLIB_NQE_MAX_CNT;
	for (i = 0; i < rdev->nqr.num_msix - 1; i++) {
		nq = &rdev->nqr.nq[i];
		vec = rdev->nqr.msix_entries[i + 1].vector;
		offt = rdev->nqr.msix_entries[i + 1].db_offset;
		nq->hwq.max_elements = depth;
		rc = bnxt_qplib_alloc_nq_mem(&rdev->qplib_res, nq);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to get mem for NQ %d, rc = 0x%x",
				i, rc);
			goto fail_mem;
		}

		rattr.dma_arr = nq->hwq.pbl[PBL_LVL_0].pg_map_arr;
		rattr.pages = nq->hwq.pbl[rdev->nqr.nq[i].hwq.level].pg_count;
		rattr.type = bnxt_re_get_rtype(rdev);
		rattr.mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		rattr.depth = nq->hwq.max_elements - 1;
		rattr.lrid = rdev->nqr.msix_entries[i + 1].ring_idx;

		/* Set DBR pacing capability on the first NQ ring only */
		if (!i && bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			rattr.flags = HWRM_RING_ALLOC_INPUT_FLAGS_NQ_DBR_PACING;
		else
			rattr.flags = 0;

		rc = bnxt_re_net_ring_alloc(rdev, &rattr, &nq->ring_id);
		if (rc) {
			nq->ring_id = 0xffff; /* Invalid ring-id */
			dev_err(rdev_to_dev(rdev),
				"Failed to get fw id for NQ %d, rc = 0x%x",
				i, rc);
			goto fail_ring;
		}

		rc = bnxt_qplib_enable_nq(nq, i, vec, offt,
					  &bnxt_re_cqn_handler,
					  &bnxt_re_srqn_handler);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to enable NQ %d, rc = 0x%x", i, rc);
			goto fail_en;
		}
	}

	rdev->nqr.max_init = i;
	return 0;
fail_en:
	/* *nq was i'th nq */
	bnxt_re_net_ring_free(rdev, nq->ring_id);
fail_ring:
	bnxt_qplib_free_nq_mem(nq);
fail_mem:
	rdev->nqr.max_init = i;
	return rc;
}

static void bnxt_re_sysfs_destroy_file(struct bnxt_re_dev *rdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bnxt_re_attributes); i++)
		device_remove_file(&rdev->ibdev.dev, bnxt_re_attributes[i]);
}

static int bnxt_re_sysfs_create_file(struct bnxt_re_dev *rdev)
{
	int i, j, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bnxt_re_attributes); i++) {
		rc = device_create_file(&rdev->ibdev.dev,
					bnxt_re_attributes[i]);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to create IB sysfs with rc = 0x%x", rc);
			/* Must clean up all created device files */
			for (j = 0; j < i; j++)
				device_remove_file(&rdev->ibdev.dev,
						   bnxt_re_attributes[j]);
			clear_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
			ib_unregister_device(&rdev->ibdev);
			return 1;
		}
	}
	return 0;
}

/* worker thread for polling periodic events. Now used for QoS programming*/
static void bnxt_re_worker(struct work_struct *work)
{
	struct bnxt_re_dev *rdev = container_of(work, struct bnxt_re_dev,
						worker.work);
	int rc;

	/* QoS is in 30s cadence for PFs*/
	if (!rdev->is_virtfn && !rdev->worker_30s--)
		rdev->worker_30s = 30;
	/* Use trylock for  bnxt_re_dev_lock as this can be
	 * held for long time by debugfs show path while issuing
	 * HWRMS. If the debugfs name update is not done in this
	 * iteration, the driver will check for the same in the
	 * next schedule of the worker i.e after 1 sec.
	 */
	if (mutex_trylock(&bnxt_re_dev_lock))
		mutex_unlock(&bnxt_re_dev_lock);

	if (!rdev->stats.stats_query_sec)
		goto resched;

	if (test_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags) &&
	    (rdev->is_virtfn ||
	    !_is_ext_stats_supported(rdev->dev_attr->dev_cap_flags))) {
		if (!(rdev->stats.stats_query_counter++ %
		      rdev->stats.stats_query_sec)) {
			rc = bnxt_re_get_qos_stats(rdev);
			if (rc && rc != -ENOMEM)
				clear_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS,
					  &rdev->flags);
			}
	}

resched:
	schedule_delayed_work(&rdev->worker, msecs_to_jiffies(1000));
}

static int bnxt_re_alloc_dbr_sw_stats_mem(struct bnxt_re_dev *rdev)
{
	if (!(rdev->dbr_drop_recov || rdev->dbr_pacing))
		return 0;

	rdev->dbr_sw_stats = kzalloc(sizeof(*rdev->dbr_sw_stats), GFP_KERNEL);
	if (!rdev->dbr_sw_stats)
		return -ENOMEM;

	return 0;
}

static void bnxt_re_free_dbr_sw_stats_mem(struct bnxt_re_dev *rdev)
{
	kfree(rdev->dbr_sw_stats);
	rdev->dbr_sw_stats = NULL;
}

static int bnxt_re_initialize_dbr_drop_recov(struct bnxt_re_dev *rdev)
{
	rdev->dbr_drop_recov_wq =
		create_singlethread_workqueue("bnxt_re_dbr_drop_recov");
	if (!rdev->dbr_drop_recov_wq) {
		dev_err(rdev_to_dev(rdev), "DBR Drop Revov wq alloc failed!");
		return -EINVAL;
	}
	rdev->dbr_drop_recov = true;

	/* Enable configfs setting dbr_drop_recov by default*/
	rdev->user_dbr_drop_recov = true;

	rdev->user_dbr_drop_recov_timeout = BNXT_RE_DBR_RECOV_USERLAND_TIMEOUT;
	return 0;
}

static void bnxt_re_deinitialize_dbr_drop_recov(struct bnxt_re_dev *rdev)
{
	if (rdev->dbr_drop_recov_wq) {
		flush_workqueue(rdev->dbr_drop_recov_wq);
		destroy_workqueue(rdev->dbr_drop_recov_wq);
		rdev->dbr_drop_recov_wq = NULL;
	}
	rdev->dbr_drop_recov = false;
}

static int bnxt_re_initialize_dbr_pacing(struct bnxt_re_dev *rdev)
{
	int rc;

	/* Allocate a page for app use */
	rdev->dbr_page = (void *)__get_free_page(GFP_KERNEL);
	if (!rdev->dbr_page) {
		dev_err(rdev_to_dev(rdev), "DBR page allocation failed!");
		return -ENOMEM;
	}
	memset((u8 *)rdev->dbr_page, 0, PAGE_SIZE);
	rdev->qplib_res.pacing_data = (struct bnxt_qplib_db_pacing_data *)rdev->dbr_page;
	rc = bnxt_re_hwrm_dbr_pacing_qcfg(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query dbr pacing config %d\n", rc);
		goto fail;
	}
	/* Create a work queue for scheduling dbq event */
	rdev->dbq_wq = create_singlethread_workqueue("bnxt_re_dbq");
	if (!rdev->dbq_wq) {
		dev_err(rdev_to_dev(rdev), "DBQ wq alloc failed!");
		rc = -ENOMEM;
		goto fail;
	}
	/* MAP grc window 2 for reading db fifo depth */
	writel_fbsd(rdev->en_dev->softc,  BNXT_GRCPF_REG_WINDOW_BASE_OUT + 4, 0,
			rdev->chip_ctx->dbr_stat_db_fifo & BNXT_GRC_BASE_MASK);
	rdev->dbr_db_fifo_reg_off =
		(rdev->chip_ctx->dbr_stat_db_fifo & BNXT_GRC_OFFSET_MASK) +
		0x2000;
	rdev->qplib_res.pacing_data->grc_reg_offset = rdev->dbr_db_fifo_reg_off;

	rdev->dbr_bar_addr =
		pci_resource_start(rdev->qplib_res.pdev, 0) +
		rdev->dbr_db_fifo_reg_off;

	/* Percentage of DB FIFO */
	rdev->dbq_watermark = BNXT_RE_PACING_DBQ_THRESHOLD;
	rdev->pacing_en_int_th = BNXT_RE_PACING_EN_INT_THRESHOLD;
	rdev->pacing_algo_th = BNXT_RE_PACING_ALGO_THRESHOLD;
	rdev->dbq_pacing_time = BNXT_RE_DBR_INT_TIME;
	rdev->dbr_def_do_pacing = BNXT_RE_DBR_DO_PACING_NO_CONGESTION;
	rdev->do_pacing_save = rdev->dbr_def_do_pacing;
	bnxt_re_set_default_pacing_data(rdev);
	dev_dbg(rdev_to_dev(rdev), "Initialized db pacing\n");

	return 0;
fail:
	free_page((u64)rdev->dbr_page);
	rdev->dbr_page = NULL;
	return rc;
}

static void bnxt_re_deinitialize_dbr_pacing(struct bnxt_re_dev *rdev)
{
	if (rdev->dbq_wq)
		flush_workqueue(rdev->dbq_wq);

	cancel_work_sync(&rdev->dbq_fifo_check_work);
	cancel_delayed_work_sync(&rdev->dbq_pacing_work);

	if (rdev->dbq_wq) {
		destroy_workqueue(rdev->dbq_wq);
		rdev->dbq_wq = NULL;
	}

	if (rdev->dbr_page)
		free_page((u64)rdev->dbr_page);
	rdev->dbr_page = NULL;
	rdev->dbr_pacing = false;
}

/* enable_dbr_pacing needs to be done only for older FWs
 * where host selects primary function. ie. pacing_ext
 * flags is not set.
 */
int bnxt_re_enable_dbr_pacing(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_nq *nq;

	nq = &rdev->nqr.nq[0];
	rdev->dbq_nq_id = nq->ring_id;

	if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) &&
	    bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
		if (bnxt_re_hwrm_dbr_pacing_cfg(rdev, true)) {
			dev_err(rdev_to_dev(rdev),
					"Failed to set dbr pacing config\n");
			return -EIO;
		}
		/* MAP grc window 8 for ARMing the NQ DBQ */
		writel_fbsd(rdev->en_dev->softc, BNXT_GRCPF_REG_WINDOW_BASE_OUT + 28 , 0,
			    rdev->chip_ctx->dbr_aeq_arm_reg & BNXT_GRC_BASE_MASK);
		rdev->dbr_aeq_arm_reg_off =
			(rdev->chip_ctx->dbr_aeq_arm_reg &
			 BNXT_GRC_OFFSET_MASK) + 0x8000;
		writel_fbsd(rdev->en_dev->softc, rdev->dbr_aeq_arm_reg_off , 0, 1);
	}

	return 0;
}

/* disable_dbr_pacing needs to be done only for older FWs
 * where host selects primary function. ie. pacing_ext
 * flags is not set.
 */

int bnxt_re_disable_dbr_pacing(struct bnxt_re_dev *rdev)
{
	int rc = 0;

	if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx) &&
	    bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx))
		rc = bnxt_re_hwrm_dbr_pacing_cfg(rdev, false);

	return rc;
}

static void bnxt_re_ib_uninit(struct bnxt_re_dev *rdev)
{
	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		bnxt_re_sysfs_destroy_file(rdev);
		/* Cleanup ib dev */
		ib_unregister_device(&rdev->ibdev);
		clear_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
		return;
	}
}

static void bnxt_re_dev_uninit(struct bnxt_re_dev *rdev, u8 op_type)
{
	struct bnxt_qplib_dpi *kdpi;
	int rc, wait_count = BNXT_RE_RES_FREE_WAIT_COUNT;

	bnxt_re_net_unregister_async_event(rdev);

	bnxt_re_put_stats2_ctx(rdev);
	if (test_and_clear_bit(BNXT_RE_FLAG_DEV_LIST_INITIALIZED,
			       &rdev->flags)) {
		/* did the caller hold the lock? */
		mutex_lock(&bnxt_re_dev_lock);
		list_del_rcu(&rdev->list);
		mutex_unlock(&bnxt_re_dev_lock);
	}

	bnxt_re_uninit_resolve_wq(rdev);
	bnxt_re_uninit_dcb_wq(rdev);
	bnxt_re_uninit_aer_wq(rdev);

	bnxt_re_deinitialize_dbr_drop_recov(rdev);

	if (bnxt_qplib_dbr_pacing_en(rdev->chip_ctx))
		(void)bnxt_re_disable_dbr_pacing(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_WORKER_REG, &rdev->flags)) {
		cancel_delayed_work_sync(&rdev->worker);
	}

	/* Wait for ULPs to release references */
	while (atomic_read(&rdev->stats.rsors.cq_count) && --wait_count)
		usleep_range(500, 1000);
	if (!wait_count)
		dev_err(rdev_to_dev(rdev),
			"CQ resources not freed by stack, count = 0x%x",
			atomic_read(&rdev->stats.rsors.cq_count));

	kdpi = &rdev->dpi_privileged;
	if (kdpi->umdbr) { /* kernel DPI was allocated with success */
		(void)bnxt_qplib_dealloc_dpi(&rdev->qplib_res, kdpi);
		/*
		 * Driver just need to know no command had failed
		 * during driver load sequence and below command is
		 * required indeed. Piggybacking dpi allocation status.
		 */
	}

	/* Protect the device uninitialization and start_irq/stop_irq L2
	 * callbacks with rtnl lock to avoid race condition between these calls
	 */
	rtnl_lock();
	if (test_and_clear_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags))
		bnxt_re_clean_nqs(rdev);
	rtnl_unlock();

	if (test_and_clear_bit(BNXT_RE_FLAG_TBLS_ALLOCINIT, &rdev->flags))
		bnxt_re_free_tbls(rdev);
	if (test_and_clear_bit(BNXT_RE_FLAG_RCFW_CHANNEL_INIT, &rdev->flags)) {
		rc = bnxt_qplib_deinit_rcfw(&rdev->rcfw);
		if (rc)
			dev_warn(rdev_to_dev(rdev),
				 "Failed to deinitialize fw, rc = 0x%x", rc);
	}

	bnxt_re_put_stats_ctx(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_ALLOC_CTX, &rdev->flags))
		bnxt_qplib_free_hwctx(&rdev->qplib_res);

	rtnl_lock();
	if (test_and_clear_bit(BNXT_RE_FLAG_RCFW_CHANNEL_EN, &rdev->flags))
		bnxt_qplib_disable_rcfw_channel(&rdev->rcfw);

	if (rdev->dbr_pacing)
		bnxt_re_deinitialize_dbr_pacing(rdev);

	bnxt_re_free_dbr_sw_stats_mem(rdev);

	if (test_and_clear_bit(BNXT_RE_FLAG_NET_RING_ALLOC, &rdev->flags))
		bnxt_re_net_ring_free(rdev, rdev->rcfw.creq.ring_id);

	if (test_and_clear_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags))
		bnxt_qplib_free_rcfw_channel(&rdev->qplib_res);

	if (test_and_clear_bit(BNXT_RE_FLAG_GOT_MSIX, &rdev->flags))
		bnxt_re_free_msix(rdev);
	rtnl_unlock();

	bnxt_re_destroy_chip_ctx(rdev);

	if (op_type != BNXT_RE_PRE_RECOVERY_REMOVE) {
		if (test_and_clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED,
				       &rdev->flags))
			bnxt_re_unregister_netdev(rdev);
	}
}

static int bnxt_re_dev_init(struct bnxt_re_dev *rdev, u8 op_type, u8 wqe_mode)
{
	struct bnxt_re_ring_attr rattr = {};
	struct bnxt_qplib_creq_ctx *creq;
	int vec, offset;
	int rc = 0;

	if (op_type != BNXT_RE_POST_RECOVERY_INIT) {
		/* Registered a new RoCE device instance to netdev */
		rc = bnxt_re_register_netdev(rdev);
		if (rc)
			return -EINVAL;
	}
	set_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);

	rc = bnxt_re_setup_chip_ctx(rdev, wqe_mode);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to get chip context rc 0x%x", rc);
		bnxt_re_unregister_netdev(rdev);
		clear_bit(BNXT_RE_FLAG_NETDEV_REGISTERED, &rdev->flags);
		rc = -EINVAL;
		return rc;
	}

	/* Protect the device initialization and start_irq/stop_irq L2 callbacks
	 * with rtnl lock to avoid race condition between these calls
	 */
	rtnl_lock();
	rc = bnxt_re_request_msix(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Requesting MSI-X vectors failed with rc = 0x%x", rc);
		rc = -EINVAL;
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_GOT_MSIX, &rdev->flags);

	/* Establish RCFW Communication Channel to initialize the context
	   memory for the function and all child VFs */
	rc = bnxt_qplib_alloc_rcfw_channel(&rdev->qplib_res);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to alloc mem for rcfw, rc = %#x\n", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_ALLOC_RCFW, &rdev->flags);

	creq = &rdev->rcfw.creq;
	rattr.dma_arr = creq->hwq.pbl[PBL_LVL_0].pg_map_arr;
	rattr.pages = creq->hwq.pbl[creq->hwq.level].pg_count;
	rattr.type = bnxt_re_get_rtype(rdev);
	rattr.mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
	rattr.depth = BNXT_QPLIB_CREQE_MAX_CNT - 1;
	rattr.lrid = rdev->nqr.msix_entries[BNXT_RE_AEQ_IDX].ring_idx;
	rc = bnxt_re_net_ring_alloc(rdev, &rattr, &creq->ring_id);
	if (rc) {
		creq->ring_id = 0xffff;
		dev_err(rdev_to_dev(rdev),
			"Failed to allocate CREQ fw id with rc = 0x%x", rc);
		goto release_rtnl;
	}

	if (!rdev->chip_ctx)
		goto release_rtnl;
	/* Program the NQ ID for DBQ notification */
	if (rdev->chip_ctx->modes.dbr_pacing_v0 ||
	    bnxt_qplib_dbr_pacing_en(rdev->chip_ctx) ||
	    bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		rc = bnxt_re_initialize_dbr_pacing(rdev);
		if (!rc)
			rdev->dbr_pacing = true;
		else
			rdev->dbr_pacing = false;
		dev_dbg(rdev_to_dev(rdev), "%s: initialize db pacing ret %d\n",
			__func__, rc);
	}

	vec = rdev->nqr.msix_entries[BNXT_RE_AEQ_IDX].vector;
	offset = rdev->nqr.msix_entries[BNXT_RE_AEQ_IDX].db_offset;
	rc = bnxt_qplib_enable_rcfw_channel(&rdev->rcfw, vec, offset,
					    &bnxt_re_aeq_handler);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to enable RCFW channel with rc = 0x%x", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_RCFW_CHANNEL_EN, &rdev->flags);

	rc = bnxt_re_update_dev_attr(rdev);
	if (rc)
		goto release_rtnl;
	bnxt_re_set_resource_limits(rdev);
	if (!rdev->is_virtfn && !_is_chip_gen_p5_p7(rdev->chip_ctx)) {
		rc = bnxt_qplib_alloc_hwctx(&rdev->qplib_res);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to alloc hw contexts, rc = 0x%x", rc);
			goto release_rtnl;
		}
		set_bit(BNXT_RE_FLAG_ALLOC_CTX, &rdev->flags);
	}

	rc = bnxt_re_get_stats_ctx(rdev);
	if (rc)
		goto release_rtnl;

	rc = bnxt_qplib_init_rcfw(&rdev->rcfw, rdev->is_virtfn);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to initialize fw with rc = 0x%x", rc);
		goto release_rtnl;
	}
	set_bit(BNXT_RE_FLAG_RCFW_CHANNEL_INIT, &rdev->flags);

	/* Based resource count on the 'new' device caps */
	rc = bnxt_re_update_dev_attr(rdev);
	if (rc)
		goto release_rtnl;
	rc = bnxt_re_alloc_init_tbls(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "tbls alloc-init failed rc = %#x",
			rc);
		goto release_rtnl;
	}
	rc = bnxt_re_setup_nqs(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "NQs alloc-init failed rc = %#x\n",
			rc);
		if (rdev->nqr.max_init == 0)
			goto release_rtnl;

		dev_warn(rdev_to_dev(rdev),
			"expected nqs %d available nqs %d\n",
			rdev->nqr.num_msix, rdev->nqr.max_init);
	}
	set_bit(BNXT_RE_FLAG_SETUP_NQ, &rdev->flags);
	rtnl_unlock();

	rc = bnxt_qplib_alloc_dpi(&rdev->qplib_res, &rdev->dpi_privileged,
				  rdev, BNXT_QPLIB_DPI_TYPE_KERNEL);
	if (rc)
		goto fail;

	if (rdev->dbr_pacing)
		bnxt_re_enable_dbr_pacing(rdev);

	if (rdev->chip_ctx->modes.dbr_drop_recov)
		bnxt_re_initialize_dbr_drop_recov(rdev);

	rc = bnxt_re_alloc_dbr_sw_stats_mem(rdev);
	if (rc)
		goto fail;

	/* This block of code is needed for error recovery support */
	if (!rdev->is_virtfn) {
		struct bnxt_re_tc_rec *tc_rec;

		tc_rec = &rdev->tc_rec[0];
		rc =  bnxt_re_query_hwrm_qportcfg(rdev, tc_rec, 0xFFFF);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to query port config rc:%d", rc);
			return rc;
		}

		/* Query f/w defaults of CC params */
		rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &rdev->cc_param);
		if (rc)
			dev_warn(rdev_to_dev(rdev),
				"Failed to query CC defaults\n");
		if (1) {
			rdev->num_vfs = pci_num_vf(rdev->en_dev->pdev);
			if (rdev->num_vfs) {
				bnxt_re_set_resource_limits(rdev);
				bnxt_qplib_set_func_resources(&rdev->qplib_res);
			}
		}
	}
	INIT_DELAYED_WORK(&rdev->worker, bnxt_re_worker);
	set_bit(BNXT_RE_FLAG_WORKER_REG, &rdev->flags);
	schedule_delayed_work(&rdev->worker, msecs_to_jiffies(1000));

	bnxt_re_init_dcb_wq(rdev);
	bnxt_re_init_aer_wq(rdev);
	bnxt_re_init_resolve_wq(rdev);
	mutex_lock(&bnxt_re_dev_lock);
	list_add_tail_rcu(&rdev->list, &bnxt_re_dev_list);
	/* Added to the list, not in progress anymore */
	gadd_dev_inprogress--;
	set_bit(BNXT_RE_FLAG_DEV_LIST_INITIALIZED, &rdev->flags);
	mutex_unlock(&bnxt_re_dev_lock);


	return rc;
release_rtnl:
	rtnl_unlock();
fail:
	bnxt_re_dev_uninit(rdev, BNXT_RE_COMPLETE_REMOVE);

	return rc;
}

static int bnxt_re_ib_init(struct bnxt_re_dev *rdev)
{
	int rc = 0;

	rc = bnxt_re_register_ib(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Register IB failed with rc = 0x%x", rc);
		goto fail;
	}
	if (bnxt_re_sysfs_create_file(rdev)) {
		bnxt_re_stopqps_and_ib_uninit(rdev);
		goto fail;
	}

	set_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags);
	set_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS, &rdev->flags);
	set_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags);
	bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1, IB_EVENT_PORT_ACTIVE);
	bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1, IB_EVENT_GID_CHANGE);

	return rc;
fail:
	bnxt_re_dev_uninit(rdev, BNXT_RE_COMPLETE_REMOVE);
	return rc;
}

/* wrapper for ib_init funcs */
int _bnxt_re_ib_init(struct bnxt_re_dev *rdev)
{
	return bnxt_re_ib_init(rdev);
}

/* wrapper for aux init funcs */
int _bnxt_re_ib_init2(struct bnxt_re_dev *rdev)
{
	bnxt_re_ib_init_2(rdev);
	return 0; /* add return for future proof */
}

static void bnxt_re_dev_unreg(struct bnxt_re_dev *rdev)
{
	bnxt_re_dev_dealloc(rdev);
}


static int bnxt_re_dev_reg(struct bnxt_re_dev **rdev, struct ifnet *netdev,
			   struct bnxt_en_dev *en_dev)
{
	struct ifnet *realdev = NULL;

	realdev = netdev;
	if (realdev)
		dev_dbg(NULL, "%s: realdev = %p netdev = %p\n", __func__,
			realdev, netdev);
	/*
	 * Note:
	 * The first argument to bnxt_re_dev_alloc() is 'netdev' and
	 * not 'realdev', since in the case of bonding we want to
	 * register the bonded virtual netdev (master) to the ib stack.
	 * And 'en_dev' (for L2/PCI communication) is the first slave
	 * device (PF0 on the card).
	 * In the case of a regular netdev, both netdev and the en_dev
	 * correspond to the same device.
	 */
	*rdev = bnxt_re_dev_alloc(netdev, en_dev);
	if (!*rdev) {
		pr_err("%s: netdev %p not handled",
			ROCE_DRV_MODULE_NAME, netdev);
		return -ENOMEM;
	}
	bnxt_re_hold(*rdev);

	return 0;
}

void bnxt_re_get_link_speed(struct bnxt_re_dev *rdev)
{
	rdev->espeed = rdev->en_dev->espeed;
	return;
}

void bnxt_re_stopqps_and_ib_uninit(struct bnxt_re_dev *rdev)
{
	dev_dbg(rdev_to_dev(rdev), "%s: Stopping QPs, IB uninit on rdev: %p\n",
		__func__, rdev);
	bnxt_re_stop_all_nonqp1_nonshadow_qps(rdev);
	bnxt_re_ib_uninit(rdev);
}

void bnxt_re_remove_device(struct bnxt_re_dev *rdev, u8 op_type,
			   struct auxiliary_device *aux_dev)
{
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_rcfw *rcfw;

	rcfw = &rdev->rcfw;
	cmdq = &rcfw->cmdq;
	if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
		set_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);

	dev_dbg(rdev_to_dev(rdev), "%s: Removing rdev: %p\n", __func__, rdev);
	bnxt_re_dev_uninit(rdev, op_type);
	en_info = auxiliary_get_drvdata(aux_dev);
	if (en_info) {
		rtnl_lock();
		en_info->rdev = NULL;
		rtnl_unlock();
		if (op_type != BNXT_RE_PRE_RECOVERY_REMOVE) {
			clear_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags);
			clear_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags);
			clear_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
		}
	}
	bnxt_re_dev_unreg(rdev);
}

int bnxt_re_add_device(struct bnxt_re_dev **rdev,
		       struct ifnet *netdev,
		       u8 qp_mode, u8 op_type, u8 wqe_mode,
		       u32 num_msix_requested,
		       struct auxiliary_device *aux_dev)
{
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_en_dev *en_dev;
	int rc = 0;

	en_info = auxiliary_get_drvdata(aux_dev);
	en_dev = en_info->en_dev;

	mutex_lock(&bnxt_re_dev_lock);
	/* Check if driver already in mod exit and aux_dev is valid */
	if (gmod_exit || !aux_dev) {
		mutex_unlock(&bnxt_re_dev_lock);
		return -ENODEV;
	}
	/* Add device in progress */
	gadd_dev_inprogress++;
	mutex_unlock(&bnxt_re_dev_lock);

	rc = bnxt_re_dev_reg(rdev, netdev, en_dev);
	if (rc) {
		dev_dbg(NULL, "Failed to create add device for netdev %p\n",
			netdev);
		/*
		 * For BNXT_RE_POST_RECOVERY_INIT special case
		 * called from bnxt_re_start, the work is
		 * complete only after, bnxt_re_start completes
		 * bnxt_unregister_device in case of failure.
		 * So bnxt_re_start will decrement gadd_dev_inprogress
		 * in case of failure.
		 */
		if (op_type != BNXT_RE_POST_RECOVERY_INIT) {
			mutex_lock(&bnxt_re_dev_lock);
			gadd_dev_inprogress--;
			mutex_unlock(&bnxt_re_dev_lock);
		}
		return rc;
	}

	if (rc != 0)
		goto ref_error;

	/*
	 *  num_msix_requested = BNXT_RE_MSIX_FROM_MOD_PARAM indicates fresh driver load.
	 *  Otherwaise, this invocation can be the result of lag create / destroy,
	 *  err revovery, hot fw upgrade, etc..
	 */
	if (num_msix_requested == BNXT_RE_MSIX_FROM_MOD_PARAM) {
		if (bnxt_re_probe_count < BNXT_RE_MAX_DEVICES)
			num_msix_requested = max_msix_vec[bnxt_re_probe_count++];
		else
			/* Consider as default when probe_count exceeds its limit */
			num_msix_requested = 0;

		/* if user specifies only one value, use the same for all PFs */
		if (max_msix_vec_argc == 1)
			num_msix_requested = max_msix_vec[0];
	}

	(*rdev)->num_msix_requested = num_msix_requested;
	(*rdev)->gsi_ctx.gsi_qp_mode = qp_mode;
	(*rdev)->adev = aux_dev;
	(*rdev)->dev_addr = en_dev->softc->func.mac_addr;
	/* Before updating the rdev pointer in bnxt_re_en_dev_info structure,
	 * take the rtnl lock to avoid accessing invalid rdev pointer from
	 * L2 ULP callbacks. This is applicable in all the places where rdev
	 * pointer is updated in bnxt_re_en_dev_info.
	 */
	rtnl_lock();
	en_info->rdev = *rdev;
	rtnl_unlock();
	rc = bnxt_re_dev_init(*rdev, op_type, wqe_mode);
	if (rc) {
ref_error:
		bnxt_re_dev_unreg(*rdev);
		*rdev = NULL;
		/*
		 * For BNXT_RE_POST_RECOVERY_INIT special case
		 * called from bnxt_re_start, the work is
		 * complete only after, bnxt_re_start completes
		 * bnxt_unregister_device in case of failure.
		 * So bnxt_re_start will decrement gadd_dev_inprogress
		 * in case of failure.
		 */
		if (op_type != BNXT_RE_POST_RECOVERY_INIT) {
			mutex_lock(&bnxt_re_dev_lock);
			gadd_dev_inprogress--;
			mutex_unlock(&bnxt_re_dev_lock);
		}
	}
	dev_dbg(rdev_to_dev(*rdev), "%s: Adding rdev: %p\n", __func__, *rdev);
	if (!rc) {
		set_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags);
	}
	return rc;
}

struct bnxt_re_dev *bnxt_re_get_peer_pf(struct bnxt_re_dev *rdev)
{
	struct pci_dev *pdev_in = rdev->en_dev->pdev;
	int tmp_bus_num, bus_num = pdev_in->bus->number;
	int tmp_dev_num, dev_num = PCI_SLOT(pdev_in->devfn);
	int tmp_func_num, func_num = PCI_FUNC(pdev_in->devfn);
	struct bnxt_re_dev *tmp_rdev;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_rdev, &bnxt_re_dev_list, list) {
		tmp_bus_num = tmp_rdev->en_dev->pdev->bus->number;
		tmp_dev_num = PCI_SLOT(tmp_rdev->en_dev->pdev->devfn);
		tmp_func_num = PCI_FUNC(tmp_rdev->en_dev->pdev->devfn);

		if (bus_num == tmp_bus_num && dev_num == tmp_dev_num &&
		    func_num != tmp_func_num) {
			rcu_read_unlock();
			return tmp_rdev;
		}
	}
	rcu_read_unlock();
	return NULL;
}


int bnxt_re_schedule_work(struct bnxt_re_dev *rdev, unsigned long event,
			  struct ifnet *vlan_dev,
			  struct ifnet *netdev,
			  struct auxiliary_device *adev)
{
	struct bnxt_re_work *re_work;

	/* Allocate for the deferred task */
	re_work = kzalloc(sizeof(*re_work), GFP_KERNEL);
	if (!re_work)
		return -ENOMEM;

	re_work->rdev = rdev;
	re_work->event = event;
	re_work->vlan_dev = vlan_dev;
	re_work->adev = adev;
	INIT_WORK(&re_work->work, bnxt_re_task);
	if (rdev)
		atomic_inc(&rdev->sched_count);
	re_work->netdev = netdev;
	queue_work(bnxt_re_wq, &re_work->work);

	return 0;
}


int bnxt_re_get_slot_pf_count(struct bnxt_re_dev *rdev)
{
	struct pci_dev *pdev_in = rdev->en_dev->pdev;
	int tmp_bus_num, bus_num = pdev_in->bus->number;
	int tmp_dev_num, dev_num = PCI_SLOT(pdev_in->devfn);
	struct bnxt_re_dev *tmp_rdev;
	int pf_cnt = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_rdev, &bnxt_re_dev_list, list) {
		tmp_bus_num = tmp_rdev->en_dev->pdev->bus->number;
		tmp_dev_num = PCI_SLOT(tmp_rdev->en_dev->pdev->devfn);

		if (bus_num == tmp_bus_num && dev_num == tmp_dev_num)
			pf_cnt++;
	}
	rcu_read_unlock();
	return pf_cnt;
}

/* Handle all deferred netevents tasks */
static void bnxt_re_task(struct work_struct *work)
{
	struct bnxt_re_en_dev_info *en_info;
	struct auxiliary_device *aux_dev;
	struct bnxt_re_work *re_work;
	struct bnxt_re_dev *rdev;

	re_work = container_of(work, struct bnxt_re_work, work);

	mutex_lock(&bnxt_re_mutex);
	rdev = re_work->rdev;

	/*
	 * If the previous rdev is deleted due to bond creation
	 * do not handle the event
	 */
	if (!bnxt_re_is_rdev_valid(rdev))
		goto exit;

	/* Ignore the event, if the device is not registred with IB stack. This
	 * is to avoid handling any event while the device is added/removed.
	 */
	if (rdev && !test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		dev_dbg(rdev_to_dev(rdev), "%s: Ignoring netdev event 0x%lx",
			__func__, re_work->event);
		goto done;
	}

	/* Extra check to silence coverity. We shouldn't handle any event
	 * when rdev is NULL.
	 */
	if (!rdev)
		goto exit;

	dev_dbg(rdev_to_dev(rdev), "Scheduled work for event 0x%lx",
		re_work->event);

	switch (re_work->event) {
	case NETDEV_UP:
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_PORT_ACTIVE);
		bnxt_re_net_register_async_event(rdev);
		break;

	case NETDEV_DOWN:
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 0);
		bnxt_re_stop_all_nonqp1_nonshadow_qps(rdev);
		bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
				       IB_EVENT_PORT_ERR);
		break;

	case NETDEV_CHANGE:
		if (bnxt_re_get_link_state(rdev) == IB_PORT_DOWN) {
			bnxt_re_stop_all_nonqp1_nonshadow_qps(rdev);
			bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
					       IB_EVENT_PORT_ERR);
			break;
		} else if (bnxt_re_get_link_state(rdev) == IB_PORT_ACTIVE) {
			bnxt_re_dispatch_event(&rdev->ibdev, NULL, 1,
					       IB_EVENT_PORT_ACTIVE);
		}

		/* temporarily disable the check for SR2 */
		if (!bnxt_qplib_query_cc_param(&rdev->qplib_res,
					       &rdev->cc_param) &&
		    !_is_chip_p7(rdev->chip_ctx)) {
			/*
			 *  Disable CC for 10G speed
			 * for non p5 devices
			 */
			if (rdev->sl_espeed == SPEED_10000 &&
			    !_is_chip_gen_p5_p7(rdev->chip_ctx)) {
				if (rdev->cc_param.enable)
					bnxt_re_clear_cc(rdev);
			} else {
				if (!rdev->cc_param.enable &&
				    rdev->cc_param.admin_enable)
					bnxt_re_setup_cc(rdev);
			}
		}
		break;

	case NETDEV_UNREGISTER:
		bnxt_re_stopqps_and_ib_uninit(rdev);
		aux_dev = rdev->adev;
		if (re_work->adev)
			goto done;

		bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, aux_dev);

		break;

	default:
		break;
	}
done:
	if (rdev) {
		/* memory barrier to guarantee task completion
		 * before decrementing sched count
		 */
		mmiowb();
		atomic_dec(&rdev->sched_count);
	}
exit:
	if (re_work->adev && re_work->event == NETDEV_UNREGISTER) {
		en_info = auxiliary_get_drvdata(re_work->adev);
		en_info->ib_uninit_done = true;
		wake_up(&en_info->waitq);
	}
	kfree(re_work);
	mutex_unlock(&bnxt_re_mutex);
}

/*
    "Notifier chain callback can be invoked for the same chain from
    different CPUs at the same time".

    For cases when the netdev is already present, our call to the
    register_netdevice_notifier() will actually get the rtnl_lock()
    before sending NETDEV_REGISTER and (if up) NETDEV_UP
    events.

    But for cases when the netdev is not already present, the notifier
    chain is subjected to be invoked from different CPUs simultaneously.

    This is protected by the netdev_mutex.
*/
static int bnxt_re_netdev_event(struct notifier_block *notifier,
				unsigned long event, void *ptr)
{
	struct ifnet *real_dev, *netdev;
	struct bnxt_re_dev *rdev = NULL;

	netdev = netdev_notifier_info_to_ifp(ptr);
	real_dev = rdma_vlan_dev_real_dev(netdev);
	if (!real_dev)
		real_dev = netdev;
	/* In case of bonding,this will be bond's rdev */
	rdev = bnxt_re_from_netdev(real_dev);

	if (!rdev)
		goto exit;

	dev_info(rdev_to_dev(rdev), "%s: Event = %s (0x%lx), rdev %s (real_dev %s)\n",
		 __func__, bnxt_re_netevent(event), event,
		 rdev ? rdev->netdev ? if_getdname(rdev->netdev) : "->netdev = NULL" : "= NULL",
		 (real_dev == netdev) ? "= netdev" : if_getdname(real_dev));

	if (!test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags))
		goto exit;

	bnxt_re_hold(rdev);

	if (real_dev != netdev) {
		switch (event) {
		case NETDEV_UP:
			bnxt_re_schedule_work(rdev, event, netdev,
					      NULL, NULL);
			break;
		case NETDEV_DOWN:
			break;
		default:
			break;
		}
		goto done;
	}

	switch (event) {
	case NETDEV_CHANGEADDR:
		if (!_is_chip_gen_p5_p7(rdev->chip_ctx))
			bnxt_re_update_shadow_ah(rdev);
		bnxt_qplib_get_guid(rdev->dev_addr,
				    (u8 *)&rdev->ibdev.node_guid);
		break;

	case NETDEV_CHANGE:
		bnxt_re_get_link_speed(rdev);
		bnxt_re_schedule_work(rdev, event, NULL, NULL, NULL);
		break;
	case NETDEV_UNREGISTER:
		/* netdev notifier will call NETDEV_UNREGISTER again later since
		 * we are still holding the reference to the netdev
		 */

		/*
		 *  Workaround to avoid ib_unregister hang. Check for module
		 *  reference and dont free up the device if the reference
		 *  is non zero. Checking only for PF functions.
		 */

		if (rdev) {
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Unreg recvd when module refcnt > 0");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Close all apps using bnxt_re devs");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Remove the configfs entry created for the device");
			dev_info(rdev_to_dev(rdev),
				 "bnxt_re:Refer documentation for details");
			goto done;
		}

		if (atomic_read(&rdev->sched_count) > 0)
			goto done;
		if (!rdev->unreg_sched) {
			bnxt_re_schedule_work(rdev, NETDEV_UNREGISTER,
					      NULL, NULL, NULL);
			rdev->unreg_sched = true;
			goto done;
		}

		break;
	default:
		break;
	}
done:
	if (rdev)
		bnxt_re_put(rdev);
exit:
	return NOTIFY_DONE;
}

static struct notifier_block bnxt_re_netdev_notifier = {
	.notifier_call = bnxt_re_netdev_event
};

static void bnxt_re_remove_base_interface(struct bnxt_re_dev *rdev,
					  struct auxiliary_device *adev)
{
	bnxt_re_stopqps_and_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, adev);
	auxiliary_set_drvdata(adev, NULL);
}

/*
 *  bnxt_re_remove  -	Removes the roce aux device
 *  @adev  -  aux device pointer
 *
 * This function removes the roce device. This gets
 * called in the mod exit path and pci unbind path.
 * If the rdev is bond interace, destroys the lag
 * in module exit path, and in pci unbind case
 * destroys the lag and recreates other base interface.
 * If the device is already removed in error recovery
 * path, it just unregister with the L2.
 */
static void bnxt_re_remove(struct auxiliary_device *adev)
{
	struct bnxt_re_en_dev_info *en_info = auxiliary_get_drvdata(adev);
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	bool primary_dev = false;
	bool secondary_dev = false;

	if (!en_info)
		return;

	mutex_lock(&bnxt_re_mutex);
	en_dev = en_info->en_dev;

	rdev = en_info->rdev;

	if (rdev && bnxt_re_is_rdev_valid(rdev)) {
		if (pci_channel_offline(rdev->rcfw.pdev))
			set_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags);

		if (test_bit(BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV, &en_info->flags))
			primary_dev = true;
		if (test_bit(BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV, &en_info->flags))
			secondary_dev = true;

		/*
		 * en_dev_info of primary device and secondary device have the
		 * same rdev pointer when LAG is configured. This rdev pointer
		 * is rdev of bond interface.
		 */
		if (!primary_dev && !secondary_dev) {
			/* removal of non bond interface */
			bnxt_re_remove_base_interface(rdev, adev);
		} else {
			/*
			 * removal of bond primary/secondary interface. In this
			 * case bond device is already removed, so rdev->binfo
			 * is NULL.
			 */
			auxiliary_set_drvdata(adev, NULL);
		}
	} else {
		/* device is removed from ulp stop, unregister the net dev */
		if (test_bit(BNXT_RE_FLAG_EN_DEV_NETDEV_REG, &en_info->flags)) {
			rtnl_lock();
			en_dev->en_ops->bnxt_unregister_device(en_dev,
							       BNXT_ROCE_ULP);
			rtnl_unlock();
		}
	}
	mutex_unlock(&bnxt_re_mutex);
	return;
}

/* wrapper for all external user context callers */
void _bnxt_re_remove(struct auxiliary_device *adev)
{
	bnxt_re_remove(adev);
}

static void bnxt_re_ib_init_2(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_re_get_device_stats(rdev);
	if (rc)
		dev_err(rdev_to_dev(rdev),
			"Failed initial device stat query");

	bnxt_re_net_register_async_event(rdev);
}

static int bnxt_re_probe(struct auxiliary_device *adev,
			 const struct auxiliary_device_id *id)
{
	struct bnxt_aux_dev *aux_dev =
		container_of(adev, struct bnxt_aux_dev, aux_dev);
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_en_dev *en_dev = NULL;
	struct bnxt_re_dev *rdev;
	int rc = -ENODEV;

	if (aux_dev)
		en_dev = aux_dev->edev;

	if (!en_dev)
		return rc;

	if (en_dev->ulp_version != BNXT_ULP_VERSION) {
		pr_err("%s: probe error: bnxt_en ulp version magic %x is not compatible!\n",
			ROCE_DRV_MODULE_NAME, en_dev->ulp_version);
		return -EINVAL;
	}

	en_info = kzalloc(sizeof(*en_info), GFP_KERNEL);
	if (!en_info)
		return -ENOMEM;
	memset(en_info, 0, sizeof(struct bnxt_re_en_dev_info));
	en_info->en_dev = en_dev;
	auxiliary_set_drvdata(adev, en_info);

	mutex_lock(&bnxt_re_mutex);
	rc = bnxt_re_add_device(&rdev, en_dev->net,
				BNXT_RE_GSI_MODE_ALL,
				BNXT_RE_COMPLETE_INIT,
				BNXT_QPLIB_WQE_MODE_STATIC,
				BNXT_RE_MSIX_FROM_MOD_PARAM, adev);
	if (rc) {
		mutex_unlock(&bnxt_re_mutex);
		return rc;
	}

	rc = bnxt_re_ib_init(rdev);
	if (rc)
		goto err;

	bnxt_re_ib_init_2(rdev);

	dev_dbg(rdev_to_dev(rdev), "%s: adev: %p\n", __func__, adev);
	rdev->adev = adev;

	mutex_unlock(&bnxt_re_mutex);

	return 0;

err:
	mutex_unlock(&bnxt_re_mutex);
	bnxt_re_remove(adev);

	return rc;
}

static const struct auxiliary_device_id bnxt_re_id_table[] = {
	{ .name = BNXT_ADEV_NAME ".rdma", },
	{},
};

MODULE_DEVICE_TABLE(auxiliary, bnxt_re_id_table);

static struct auxiliary_driver bnxt_re_driver = {
	.name = "rdma",
	.probe = bnxt_re_probe,
	.remove = bnxt_re_remove,
	.id_table = bnxt_re_id_table,
};

static int __init bnxt_re_mod_init(void)
{
	int rc = 0;

	pr_info("%s: %s", ROCE_DRV_MODULE_NAME, drv_version);

	bnxt_re_wq = create_singlethread_workqueue("bnxt_re");
	if (!bnxt_re_wq)
		return -ENOMEM;

	rc = bnxt_re_register_netdevice_notifier(&bnxt_re_netdev_notifier);
	if (rc) {
		pr_err("%s: Cannot register to netdevice_notifier",
			ROCE_DRV_MODULE_NAME);
		goto err_netdev;
	}

	INIT_LIST_HEAD(&bnxt_re_dev_list);

	rc = auxiliary_driver_register(&bnxt_re_driver);
	if (rc) {
		pr_err("%s: Failed to register auxiliary driver\n",
		       ROCE_DRV_MODULE_NAME);
		goto err_auxdrv;
	}

	return 0;

err_auxdrv:
	bnxt_re_unregister_netdevice_notifier(&bnxt_re_netdev_notifier);

err_netdev:
	destroy_workqueue(bnxt_re_wq);

	return rc;
}

static void __exit bnxt_re_mod_exit(void)
{
	gmod_exit = 1;
	auxiliary_driver_unregister(&bnxt_re_driver);

	bnxt_re_unregister_netdevice_notifier(&bnxt_re_netdev_notifier);

	if (bnxt_re_wq)
		destroy_workqueue(bnxt_re_wq);
}

module_init(bnxt_re_mod_init);
module_exit(bnxt_re_mod_exit);
