/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
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
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abi.h"
#include "main.h"
#include "verbs.h"

#define PCI_VENDOR_ID_BROADCOM		0x14E4

BNXT_RE_DEFINE_CNA_TABLE(cna_table) = {
	CNA(BROADCOM, 0x1605),  /* BCM57454 Stratus NPAR */
	CNA(BROADCOM, 0x1606),	/* BCM57454 Stratus VF */
	CNA(BROADCOM, 0x1614),	/* BCM57454 Stratus */
	CNA(BROADCOM, 0x16C0),	/* BCM57417 NPAR */
	CNA(BROADCOM, 0x16C1),  /* BMC57414 VF */
	CNA(BROADCOM, 0x16CE),	/* BMC57311 */
	CNA(BROADCOM, 0x16CF),	/* BMC57312 */
	CNA(BROADCOM, 0x16D6),	/* BMC57412*/
	CNA(BROADCOM, 0x16D7),	/* BMC57414 */
	CNA(BROADCOM, 0x16D8),	/* BMC57416 Cu */
	CNA(BROADCOM, 0x16D9),	/* BMC57417 Cu */
	CNA(BROADCOM, 0x16DF),	/* BMC57314 */
	CNA(BROADCOM, 0x16E2),	/* BMC57417 */
	CNA(BROADCOM, 0x16E3),	/* BMC57416 */
	CNA(BROADCOM, 0x16E5),	/* BMC57314 VF */
	CNA(BROADCOM, 0x16EB),	/* BCM57412 NPAR */
	CNA(BROADCOM, 0x16ED),	/* BCM57414 NPAR */
	CNA(BROADCOM, 0x16EF),	/* BCM57416 NPAR */
	CNA(BROADCOM, 0x16F0),  /* BCM58730 */
	CNA(BROADCOM, 0x16F1),	/* BCM57452 Stratus Mezz */
	CNA(BROADCOM, 0x1750),	/* Chip num 57500 */
	CNA(BROADCOM, 0x1751),  /* BCM57504 Gen P5 */
	CNA(BROADCOM, 0x1752),  /* BCM57502 Gen P5 */
	CNA(BROADCOM, 0x1760),  /* BCM57608 Thor 2*/
	CNA(BROADCOM, 0xD82E),  /* BCM5760x TH2 VF */
	CNA(BROADCOM, 0x1803),  /* BCM57508 Gen P5 NPAR */
	CNA(BROADCOM, 0x1804),  /* BCM57504 Gen P5 NPAR */
	CNA(BROADCOM, 0x1805),  /* BCM57502 Gen P5 NPAR */
	CNA(BROADCOM, 0x1807),  /* BCM5750x Gen P5 VF */
	CNA(BROADCOM, 0x1809),  /* BCM5750x Gen P5 VF HV */
	CNA(BROADCOM, 0xD800),  /* BCM880xx SR VF */
	CNA(BROADCOM, 0xD802),  /* BCM58802 SR */
	CNA(BROADCOM, 0xD804),  /* BCM58804 SR */
	CNA(BROADCOM, 0xD818)   /* BCM58818 Gen P5 SR2 */
};

static struct ibv_context_ops bnxt_re_cntx_ops = {
	.query_device  = bnxt_re_query_device,
	.query_port    = bnxt_re_query_port,
	.alloc_pd      = bnxt_re_alloc_pd,
	.dealloc_pd    = bnxt_re_free_pd,
	.reg_mr        = bnxt_re_reg_mr,
	.dereg_mr      = bnxt_re_dereg_mr,
	.create_cq     = bnxt_re_create_cq,
	.poll_cq       = bnxt_re_poll_cq,
	.req_notify_cq = bnxt_re_arm_cq,
	.cq_event      = bnxt_re_cq_event,
	.resize_cq     = bnxt_re_resize_cq,
	.destroy_cq    = bnxt_re_destroy_cq,
	.create_srq    = bnxt_re_create_srq,
	.modify_srq    = bnxt_re_modify_srq,
	.query_srq     = bnxt_re_query_srq,
	.destroy_srq   = bnxt_re_destroy_srq,
	.post_srq_recv = bnxt_re_post_srq_recv,
	.create_qp     = bnxt_re_create_qp,
	.query_qp      = bnxt_re_query_qp,
	.modify_qp     = bnxt_re_modify_qp,
	.destroy_qp    = bnxt_re_destroy_qp,
	.post_send     = bnxt_re_post_send,
	.post_recv     = bnxt_re_post_recv,
	.async_event   = bnxt_re_async_event,
	.create_ah     = bnxt_re_create_ah,
	.destroy_ah    = bnxt_re_destroy_ah
};

bool _is_chip_gen_p5(struct bnxt_re_chip_ctx *cctx)
{
	return (cctx->chip_num == CHIP_NUM_57508 ||
		cctx->chip_num == CHIP_NUM_57504 ||
		cctx->chip_num == CHIP_NUM_57502);
}

bool _is_chip_a0(struct bnxt_re_chip_ctx *cctx)
{
	return !cctx->chip_rev;
}

bool _is_chip_thor2(struct bnxt_re_chip_ctx *cctx)
{
	return (cctx->chip_num == CHIP_NUM_58818 ||
		cctx->chip_num == CHIP_NUM_57608);
}

bool _is_chip_gen_p5_thor2(struct bnxt_re_chip_ctx *cctx)
{
	return(_is_chip_gen_p5(cctx) || _is_chip_thor2(cctx));
}

bool _is_db_drop_recovery_enable(struct bnxt_re_context *cntx)
{
	return cntx->comp_mask & BNXT_RE_COMP_MASK_UCNTX_DBR_RECOVERY_ENABLED;
}

/* Determine the env variable */
static int single_threaded_app(void)
{
	char *env;

	env = getenv("BNXT_SINGLE_THREADED");
	if (env)
		return strcmp(env, "1") ? 0 : 1;

	return 0;
}

static int enable_dynamic_debug(void)
{
	char *env;

	env = getenv("BNXT_DYN_DBG");
	if (env)
		return strcmp(env, "1") ? 0 : 1;

	return 0;
}

/* Static Context Init functions */
static int _bnxt_re_init_context(struct bnxt_re_dev *dev,
				 struct bnxt_re_context *cntx,
				 struct bnxt_re_cntx_resp *resp, int cmd_fd)
{
	bnxt_single_threaded = 0;
	cntx->cctx = malloc(sizeof(struct bnxt_re_chip_ctx));
	if (!cntx->cctx)
		goto failed;

	if (BNXT_RE_ABI_VERSION >= 4) {
		cntx->cctx->chip_num = resp->chip_id0 & 0xFFFF;
		cntx->cctx->chip_rev = (resp->chip_id0 >>
					BNXT_RE_CHIP_ID0_CHIP_REV_SFT) & 0xFF;
		cntx->cctx->chip_metal = (resp->chip_id0 >>
					  BNXT_RE_CHIP_ID0_CHIP_MET_SFT) &
					  0xFF;
		cntx->cctx->chip_is_gen_p5_thor2 = _is_chip_gen_p5_thor2(cntx->cctx);
	}
	if (BNXT_RE_ABI_VERSION != 4) {
		cntx->dev_id = resp->dev_id;
		cntx->max_qp = resp->max_qp;
	}

	if (BNXT_RE_ABI_VERSION > 5)
		cntx->modes = resp->modes;
	cntx->comp_mask = resp->comp_mask;
	dev->pg_size = resp->pg_size;
	dev->cqe_size = resp->cqe_size;
	dev->max_cq_depth = resp->max_cqd;

	/* mmap shared page. */
	cntx->shpg = mmap(NULL, dev->pg_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED, cmd_fd, 0);
	if (cntx->shpg == MAP_FAILED) {
		cntx->shpg = NULL;
		goto free;
	}

	if (cntx->comp_mask & BNXT_RE_COMP_MASK_UCNTX_DBR_PACING_ENABLED) {
		cntx->dbr_page = mmap(NULL, dev->pg_size, PROT_READ,
				      MAP_SHARED, cmd_fd, BNXT_RE_DBR_PAGE);
		if (cntx->dbr_page == MAP_FAILED) {
			munmap(cntx->shpg, dev->pg_size);
			cntx->shpg = NULL;
			cntx->dbr_page = NULL;
			goto free;
		}
	}

	/* check for ENV for single thread */
	bnxt_single_threaded = single_threaded_app();
	if (bnxt_single_threaded)
		fprintf(stderr, DEV " Running in Single threaded mode\n");
	bnxt_dyn_debug = enable_dynamic_debug();
	pthread_mutex_init(&cntx->shlock, NULL);

	return 0;

free:
	free(cntx->cctx);
failed:
	fprintf(stderr, DEV "Failed to initialize context for device\n");
	return errno;
}

static void _bnxt_re_uninit_context(struct bnxt_re_dev *dev,
				    struct bnxt_re_context *cntx)
{
	int ret;

	if (cntx->comp_mask & BNXT_RE_COMP_MASK_UCNTX_DBR_PACING_ENABLED)
		munmap(cntx->dbr_page, dev->pg_size);
	/* Unmap if anything device specific was
	 * mapped in init_context.
	 */
	pthread_mutex_destroy(&cntx->shlock);
	if (cntx->shpg)
		munmap(cntx->shpg, dev->pg_size);

	/* Un-map DPI only for the first PD that was
	 * allocated in this context.
	 */
	if (cntx->udpi.wcdbpg && cntx->udpi.wcdbpg != MAP_FAILED) {
		munmap(cntx->udpi.wcdbpg, dev->pg_size);
		cntx->udpi.wcdbpg = NULL;
		bnxt_re_destroy_pbuf_list(cntx);
	}

	if (cntx->udpi.dbpage && cntx->udpi.dbpage != MAP_FAILED) {
		munmap(cntx->udpi.dbpage, dev->pg_size);
		cntx->udpi.dbpage = NULL;
	}
	if (_is_db_drop_recovery_enable(cntx)) {
		if (cntx->dbr_cq) {
			ret = pthread_cancel(cntx->dbr_thread);
			if (ret)
				fprintf(stderr, DEV "pthread_cancel error %d\n", ret);

			if (cntx->db_recovery_page)
				munmap(cntx->db_recovery_page, dev->pg_size);
			ret = ibv_destroy_cq(cntx->dbr_cq);
			if (ret)
				fprintf(stderr, DEV "ibv_destroy_cq error %d\n", ret);
		}

		if (cntx->dbr_ev_chan) {
			ret = ibv_destroy_comp_channel(cntx->dbr_ev_chan);
			if (ret)
				fprintf(stderr,
					DEV "ibv_destroy_comp_channel error\n");
		}
		pthread_spin_destroy(&cntx->qp_dbr_res.lock);
		pthread_spin_destroy(&cntx->cq_dbr_res.lock);
		pthread_spin_destroy(&cntx->srq_dbr_res.lock);
	}
	free(cntx->cctx);
}

/* Context Init functions */
int bnxt_re_init_context(struct verbs_device *vdev, struct ibv_context *ibvctx,
			 int cmd_fd)
{
	struct bnxt_re_cntx_resp resp = {};
	struct bnxt_re_cntx_req req = {};
	struct bnxt_re_context *cntx;
	struct bnxt_re_dev *rdev;
	int ret = 0;

	rdev = to_bnxt_re_dev(&vdev->device);
	cntx = to_bnxt_re_context(ibvctx);
	ibvctx->cmd_fd = cmd_fd;

	req.comp_mask |= BNXT_RE_COMP_MASK_REQ_UCNTX_POW2_SUPPORT;
	req.comp_mask |= BNXT_RE_COMP_MASK_REQ_UCNTX_RSVD_WQE;
	ret = ibv_cmd_get_context(ibvctx, &req.cmd, sizeof(req),
				  &resp.resp, sizeof(resp));

	if (ret) {
		fprintf(stderr, DEV "Failed to get context for device, ret = 0x%x, errno %d\n", ret, errno);
		return errno;
	}

	ret = _bnxt_re_init_context(rdev, cntx, &resp, cmd_fd);
	if (!ret)
		ibvctx->ops = bnxt_re_cntx_ops;

	cntx->rdev = rdev;
	ret = bnxt_re_query_device_compat(&cntx->ibvctx, &rdev->devattr);

	return ret;
}

void bnxt_re_uninit_context(struct verbs_device *vdev,
			    struct ibv_context *ibvctx)
{
	struct bnxt_re_context *cntx;
	struct bnxt_re_dev *rdev;

	cntx = to_bnxt_re_context(ibvctx);
	rdev = cntx->rdev;
	_bnxt_re_uninit_context(rdev, cntx);
}

static struct verbs_device_ops bnxt_re_dev_ops = {
	.init_context = bnxt_re_init_context,
	.uninit_context = bnxt_re_uninit_context,
};

static struct verbs_device *bnxt_re_driver_init(const char *uverbs_sys_path,
						int abi_version)
{
	char value[10];
	struct bnxt_re_dev *dev;
	unsigned vendor, device;
	int i;

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/vendor",
				value, sizeof(value)) < 0)
		return NULL;
	vendor = strtol(value, NULL, 16);

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/device",
				value, sizeof(value)) < 0)
		return NULL;
	device = strtol(value, NULL, 16);

	for (i = 0; i < sizeof(cna_table) / sizeof(cna_table[0]); ++i)
		if (vendor == cna_table[i].vendor &&
		    device == cna_table[i].device)
			goto found;
	return NULL;
found:
	if (abi_version != BNXT_RE_ABI_VERSION) {
		fprintf(stderr, DEV "FATAL: Max supported ABI of %s is %d "
			"check for the latest version of kernel driver and"
			"user library\n", uverbs_sys_path, abi_version);
		return NULL;
	}

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		fprintf(stderr, DEV "Failed to allocate device for %s\n",
			uverbs_sys_path);
		return NULL;
	}

	dev->vdev.sz = sizeof(*dev);
	dev->vdev.size_of_context =
		sizeof(struct bnxt_re_context) - sizeof(struct ibv_context);
	dev->vdev.ops = &bnxt_re_dev_ops;

	return &dev->vdev;
}

static __attribute__((constructor)) void bnxt_re_register_driver(void)
{
	verbs_register_driver("bnxtre", bnxt_re_driver_init);
}
