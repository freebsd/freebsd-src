/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*$FreeBSD$*/


#include <sys/mman.h>
#include <stdbool.h>
#include <stdlib.h>
#include "irdma_umain.h"
#include "irdma-abi.h"
#include "irdma_uquery.h"

#include "ice_devids.h"
#include "i40e_devids.h"

#include "abi.h"

/**
 *  Driver version
 */
char libirdma_version[] = "1.2.17-k";

unsigned int irdma_dbg;

#define INTEL_HCA(d) \
	{ .vendor = PCI_VENDOR_ID_INTEL,  \
	  .device = d }

struct hca_info {
	unsigned vendor;
	unsigned device;
};

static const struct hca_info hca_table[] = {
	INTEL_HCA(ICE_DEV_ID_E823L_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_E823L_SFP),
	INTEL_HCA(ICE_DEV_ID_E823L_10G_BASE_T),
	INTEL_HCA(ICE_DEV_ID_E823L_1GBE),
	INTEL_HCA(ICE_DEV_ID_E823L_QSFP),
	INTEL_HCA(ICE_DEV_ID_E810C_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_E810C_QSFP),
	INTEL_HCA(ICE_DEV_ID_E810C_SFP),
	INTEL_HCA(ICE_DEV_ID_E810_XXV_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_E810_XXV_QSFP),
	INTEL_HCA(ICE_DEV_ID_E810_XXV_SFP),
	INTEL_HCA(ICE_DEV_ID_E823C_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_E823C_QSFP),
	INTEL_HCA(ICE_DEV_ID_E823C_SFP),
	INTEL_HCA(ICE_DEV_ID_E823C_10G_BASE_T),
	INTEL_HCA(ICE_DEV_ID_E823C_SGMII),
	INTEL_HCA(ICE_DEV_ID_C822N_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_C822N_QSFP),
	INTEL_HCA(ICE_DEV_ID_C822N_SFP),
	INTEL_HCA(ICE_DEV_ID_E822C_10G_BASE_T),
	INTEL_HCA(ICE_DEV_ID_E822C_SGMII),
	INTEL_HCA(ICE_DEV_ID_E822L_BACKPLANE),
	INTEL_HCA(ICE_DEV_ID_E822L_SFP),
	INTEL_HCA(ICE_DEV_ID_E822L_10G_BASE_T),
	INTEL_HCA(ICE_DEV_ID_E822L_SGMII),
};

static struct ibv_context_ops irdma_ctx_ops = {
	.query_device = irdma_uquery_device,
	.query_port = irdma_uquery_port,
	.alloc_pd = irdma_ualloc_pd,
	.dealloc_pd = irdma_ufree_pd,
	.reg_mr = irdma_ureg_mr,
	.rereg_mr = NULL,
	.dereg_mr = irdma_udereg_mr,
	.alloc_mw = irdma_ualloc_mw,
	.dealloc_mw = irdma_udealloc_mw,
	.bind_mw = irdma_ubind_mw,
	.create_cq = irdma_ucreate_cq,
	.poll_cq = irdma_upoll_cq,
	.req_notify_cq = irdma_uarm_cq,
	.cq_event = irdma_cq_event,
	.resize_cq = irdma_uresize_cq,
	.destroy_cq = irdma_udestroy_cq,
	.create_qp = irdma_ucreate_qp,
	.query_qp = irdma_uquery_qp,
	.modify_qp = irdma_umodify_qp,
	.destroy_qp = irdma_udestroy_qp,
	.post_send = irdma_upost_send,
	.post_recv = irdma_upost_recv,
	.create_ah = irdma_ucreate_ah,
	.destroy_ah = irdma_udestroy_ah,
	.attach_mcast = irdma_uattach_mcast,
	.detach_mcast = irdma_udetach_mcast,
};

/**
 * libirdma_query_device - fill libirdma_device structure
 * @ctx_in - ibv_context identifying device
 * @out - libirdma_device structure to fill quered info
 *
 * ctx_in is not used at the moment
 */
int
libirdma_query_device(struct ibv_context *ctx_in, struct libirdma_device *out)
{
	if (!out)
		return EIO;
	if (sizeof(out->lib_ver) < sizeof(libirdma_version))
		return ERANGE;

	out->query_ver = 1;
	snprintf(out->lib_ver, min(sizeof(libirdma_version), sizeof(out->lib_ver)),
		 "%s", libirdma_version);

	return 0;
}

static int
irdma_init_context(struct verbs_device *vdev,
		   struct ibv_context *ctx, int cmd_fd)
{
	struct irdma_uvcontext *iwvctx;
	struct irdma_get_context cmd = {};
	struct irdma_get_context_resp resp = {};
	struct ibv_pd *ibv_pd;
	u64 mmap_key;

	iwvctx = container_of(ctx, struct irdma_uvcontext, ibv_ctx);
	iwvctx->ibv_ctx.cmd_fd = cmd_fd;
	cmd.userspace_ver = IRDMA_ABI_VER;
	if (ibv_cmd_get_context(&iwvctx->ibv_ctx, &cmd.ibv_cmd, sizeof(cmd),
				&resp.ibv_resp, sizeof(resp))) {
		/* failed first attempt */
		printf("%s %s get context failure\n", __FILE__, __func__);
		return -1;
	}
	iwvctx->uk_attrs.feature_flags = resp.feature_flags;
	iwvctx->uk_attrs.hw_rev = resp.hw_rev;
	iwvctx->uk_attrs.max_hw_wq_frags = resp.max_hw_wq_frags;
	iwvctx->uk_attrs.max_hw_read_sges = resp.max_hw_read_sges;
	iwvctx->uk_attrs.max_hw_inline = resp.max_hw_inline;
	iwvctx->uk_attrs.max_hw_rq_quanta = resp.max_hw_rq_quanta;
	iwvctx->uk_attrs.max_hw_wq_quanta = resp.max_hw_wq_quanta;
	iwvctx->uk_attrs.max_hw_sq_chunk = resp.max_hw_sq_chunk;
	iwvctx->uk_attrs.max_hw_cq_size = resp.max_hw_cq_size;
	iwvctx->uk_attrs.min_hw_cq_size = resp.min_hw_cq_size;
	iwvctx->uk_attrs.min_hw_wq_size = IRDMA_QP_SW_MIN_WQSIZE;
	iwvctx->abi_ver = IRDMA_ABI_VER;
	mmap_key = resp.db_mmap_key;

	iwvctx->db = mmap(NULL, IRDMA_HW_PAGE_SIZE, PROT_WRITE | PROT_READ,
			  MAP_SHARED, cmd_fd, mmap_key);
	if (iwvctx->db == MAP_FAILED)
		goto err_free;

	iwvctx->ibv_ctx.ops = irdma_ctx_ops;

	ibv_pd = irdma_ualloc_pd(&iwvctx->ibv_ctx);
	if (!ibv_pd) {
		munmap(iwvctx->db, IRDMA_HW_PAGE_SIZE);
		goto err_free;
	}

	ibv_pd->context = &iwvctx->ibv_ctx;
	iwvctx->iwupd = container_of(ibv_pd, struct irdma_upd, ibv_pd);

	return 0;

err_free:

	printf("%s %s failure\n", __FILE__, __func__);
	return -1;
}

static void
irdma_cleanup_context(struct verbs_device *device,
		      struct ibv_context *ibctx)
{
	struct irdma_uvcontext *iwvctx;

	iwvctx = container_of(ibctx, struct irdma_uvcontext, ibv_ctx);
	irdma_ufree_pd(&iwvctx->iwupd->ibv_pd);
	munmap(iwvctx->db, IRDMA_HW_PAGE_SIZE);

}

static struct verbs_device_ops irdma_dev_ops = {
	.init_context = irdma_init_context,
	.uninit_context = irdma_cleanup_context,
};

static struct verbs_device *
irdma_driver_init(const char *uverbs_sys_path,
		  int abi_version)
{
	struct irdma_udevice *dev;
	int i = 0;
	unsigned int device_found = 0;
	unsigned vendor_id, device_id;
	unsigned hca_size;
	char buf[8];

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/vendor",
				buf, sizeof(buf)) < 0)
		return NULL;
	sscanf(buf, "%i", &vendor_id);
	if (vendor_id != PCI_VENDOR_ID_INTEL)
		return NULL;

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/device",
				buf, sizeof(buf)) < 0)
		return NULL;
	sscanf(buf, "%i", &device_id);

	hca_size = sizeof(hca_table) / sizeof(struct hca_info);
	while (i < hca_size && !device_found) {
		if (device_id != hca_table[i].device)
			device_found = 1;
		++i;
	}

	if (!device_found)
		return NULL;

	if (abi_version < IRDMA_MIN_ABI_VERSION ||
	    abi_version > IRDMA_MAX_ABI_VERSION) {
		printf("Invalid ABI version: %d of %s\n",
		       abi_version, uverbs_sys_path);
		return NULL;
	}

	dev = calloc(1, sizeof(struct irdma_udevice));
	if (!dev) {
		printf("Device creation for %s failed\n", uverbs_sys_path);
		return NULL;
	}

	dev->ibv_dev.ops = &irdma_dev_ops;
	dev->ibv_dev.sz = sizeof(*dev);
	dev->ibv_dev.size_of_context = sizeof(struct irdma_uvcontext) -
	    sizeof(struct ibv_context);

	return &dev->ibv_dev;
}

static __attribute__((constructor))
void
irdma_register_driver(void)
{
	verbs_register_driver("irdma", irdma_driver_init);
}
