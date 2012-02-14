/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2006 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>

#ifndef HAVE_IBV_REGISTER_DRIVER
#include <sysfs/libsysfs.h>
#endif

#ifndef HAVE_IBV_READ_SYSFS_FILE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "mthca.h"
#include "mthca-abi.h"

#ifndef PCI_VENDOR_ID_MELLANOX
#define PCI_VENDOR_ID_MELLANOX			0x15b3
#endif

#ifndef PCI_DEVICE_ID_MELLANOX_TAVOR
#define PCI_DEVICE_ID_MELLANOX_TAVOR		0x5a44
#endif

#ifndef PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT
#define PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT	0x6278
#endif

#ifndef PCI_DEVICE_ID_MELLANOX_ARBEL
#define PCI_DEVICE_ID_MELLANOX_ARBEL		0x6282
#endif

#ifndef PCI_DEVICE_ID_MELLANOX_SINAI_OLD
#define PCI_DEVICE_ID_MELLANOX_SINAI_OLD	0x5e8c
#endif

#ifndef PCI_DEVICE_ID_MELLANOX_SINAI
#define PCI_DEVICE_ID_MELLANOX_SINAI		0x6274
#endif

#ifndef PCI_VENDOR_ID_TOPSPIN
#define PCI_VENDOR_ID_TOPSPIN			0x1867
#endif

#define HCA(v, d, t) \
	{ .vendor = PCI_VENDOR_ID_##v,			\
	  .device = PCI_DEVICE_ID_MELLANOX_##d,		\
	  .type = MTHCA_##t }

struct {
	unsigned		vendor;
	unsigned		device;
	enum mthca_hca_type	type;
} hca_table[] = {
	HCA(MELLANOX, TAVOR,	    TAVOR),
	HCA(MELLANOX, ARBEL_COMPAT, TAVOR),
	HCA(MELLANOX, ARBEL,	    ARBEL),
	HCA(MELLANOX, SINAI_OLD,    ARBEL),
	HCA(MELLANOX, SINAI,	    ARBEL),
	HCA(TOPSPIN,  TAVOR,	    TAVOR),
	HCA(TOPSPIN,  ARBEL_COMPAT, TAVOR),
	HCA(TOPSPIN,  ARBEL,	    ARBEL),
	HCA(TOPSPIN,  SINAI_OLD,    ARBEL),
	HCA(TOPSPIN,  SINAI,	    ARBEL),
};

static struct ibv_context_ops mthca_ctx_ops = {
	.query_device  = mthca_query_device,
	.query_port    = mthca_query_port,
	.alloc_pd      = mthca_alloc_pd,
	.dealloc_pd    = mthca_free_pd,
	.reg_mr        = mthca_reg_mr,
	.dereg_mr      = mthca_dereg_mr,
	.create_cq     = mthca_create_cq,
	.poll_cq       = mthca_poll_cq,
	.resize_cq     = mthca_resize_cq,
	.destroy_cq    = mthca_destroy_cq,
	.create_srq    = mthca_create_srq,
	.modify_srq    = mthca_modify_srq,
	.query_srq     = mthca_query_srq,
	.destroy_srq   = mthca_destroy_srq,
	.create_qp     = mthca_create_qp,
	.query_qp      = mthca_query_qp,
	.modify_qp     = mthca_modify_qp,
	.destroy_qp    = mthca_destroy_qp,
	.create_ah     = mthca_create_ah,
	.destroy_ah    = mthca_destroy_ah,
	.attach_mcast  = mthca_attach_mcast,
	.detach_mcast  = mthca_detach_mcast
};

static struct ibv_context *mthca_alloc_context(struct ibv_device *ibdev, int cmd_fd)
{
	struct mthca_context            *context;
	struct ibv_get_context           cmd;
	struct mthca_alloc_ucontext_resp resp;
	int                              i;

	context = calloc(1, sizeof *context);
	if (!context)
		return NULL;

	context->ibv_ctx.cmd_fd = cmd_fd;

	if (ibv_cmd_get_context(&context->ibv_ctx, &cmd, sizeof cmd,
				&resp.ibv_resp, sizeof resp))
		goto err_free;

	context->num_qps        = resp.qp_tab_size;
	context->qp_table_shift = ffs(context->num_qps) - 1 - MTHCA_QP_TABLE_BITS;
	context->qp_table_mask  = (1 << context->qp_table_shift) - 1;

	/*
	 * Need to set ibv_ctx.device because mthca_is_memfree() will
	 * look at it to figure out the HCA type.
	 */
	context->ibv_ctx.device = ibdev;

	if (mthca_is_memfree(&context->ibv_ctx)) {
		context->db_tab = mthca_alloc_db_tab(resp.uarc_size);
		if (!context->db_tab)
			goto err_free;
	} else
		context->db_tab = NULL;

	pthread_mutex_init(&context->qp_table_mutex, NULL);
	for (i = 0; i < MTHCA_QP_TABLE_SIZE; ++i)
		context->qp_table[i].refcnt = 0;

	context->uar = mmap(NULL, to_mdev(ibdev)->page_size, PROT_WRITE,
			    MAP_SHARED, cmd_fd, 0);
	if (context->uar == MAP_FAILED)
		goto err_db_tab;

	pthread_spin_init(&context->uar_lock, PTHREAD_PROCESS_PRIVATE);

	context->pd = mthca_alloc_pd(&context->ibv_ctx);
	if (!context->pd)
		goto err_unmap;

	context->pd->context = &context->ibv_ctx;

	context->ibv_ctx.ops = mthca_ctx_ops;

	if (mthca_is_memfree(&context->ibv_ctx)) {
		context->ibv_ctx.ops.req_notify_cq = mthca_arbel_arm_cq;
		context->ibv_ctx.ops.cq_event      = mthca_arbel_cq_event;
		context->ibv_ctx.ops.post_send     = mthca_arbel_post_send;
		context->ibv_ctx.ops.post_recv     = mthca_arbel_post_recv;
		context->ibv_ctx.ops.post_srq_recv = mthca_arbel_post_srq_recv;
	} else {
		context->ibv_ctx.ops.req_notify_cq = mthca_tavor_arm_cq;
		context->ibv_ctx.ops.cq_event      = NULL;
		context->ibv_ctx.ops.post_send     = mthca_tavor_post_send;
		context->ibv_ctx.ops.post_recv     = mthca_tavor_post_recv;
		context->ibv_ctx.ops.post_srq_recv = mthca_tavor_post_srq_recv;
	}

	return &context->ibv_ctx;

err_unmap:
	munmap(context->uar, to_mdev(ibdev)->page_size);

err_db_tab:
	mthca_free_db_tab(context->db_tab);

err_free:
	free(context);
	return NULL;
}

static void mthca_free_context(struct ibv_context *ibctx)
{
	struct mthca_context *context = to_mctx(ibctx);

	mthca_free_pd(context->pd);
	munmap(context->uar, to_mdev(ibctx->device)->page_size);
	mthca_free_db_tab(context->db_tab);
	free(context);
}

static struct ibv_device_ops mthca_dev_ops = {
	.alloc_context = mthca_alloc_context,
	.free_context  = mthca_free_context
};

/*
 * Keep a private implementation of HAVE_IBV_READ_SYSFS_FILE to handle
 * old versions of libibverbs that didn't implement it.  This can be
 * removed when libibverbs 1.0.3 or newer is available "everywhere."
 */
#ifndef HAVE_IBV_READ_SYSFS_FILE
static int ibv_read_sysfs_file(const char *dir, const char *file,
			       char *buf, size_t size)
{
	char path[256];
	int fd;
	int len;

	snprintf(path, sizeof path, "%s/%s", dir, file);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	len = read(fd, buf, size);

	close(fd);

	if (len > 0 && buf[len - 1] == '\n')
		buf[--len] = '\0';

	return len;
}
#endif /* HAVE_IBV_READ_SYSFS_FILE */

static struct ibv_device *mthca_driver_init(const char *uverbs_sys_path,
					    int abi_version)
{
	char			value[8];
	struct mthca_device    *dev;
	unsigned                vendor, device;
	int                     i;

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/vendor",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &vendor);

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/device",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &device);

	for (i = 0; i < sizeof hca_table / sizeof hca_table[0]; ++i)
		if (vendor == hca_table[i].vendor &&
		    device == hca_table[i].device)
			goto found;

	return NULL;

found:
	if (abi_version > MTHCA_UVERBS_ABI_VERSION) {
		fprintf(stderr, PFX "Fatal: ABI version %d of %s is too new (expected %d)\n",
			abi_version, uverbs_sys_path, MTHCA_UVERBS_ABI_VERSION);
		return NULL;
	}

	dev = malloc(sizeof *dev);
	if (!dev) {
		fprintf(stderr, PFX "Fatal: couldn't allocate device for %s\n",
			uverbs_sys_path);
		return NULL;
	}

	dev->ibv_dev.ops = mthca_dev_ops;
	dev->hca_type    = hca_table[i].type;
	dev->page_size   = sysconf(_SC_PAGESIZE);

	return &dev->ibv_dev;
}

#ifdef HAVE_IBV_REGISTER_DRIVER
static __attribute__((constructor)) void mthca_register_driver(void)
{
	ibv_register_driver("mthca", mthca_driver_init);
}
#else
/*
 * Export the old libsysfs sysfs_class_device-based driver entry point
 * if libibverbs does not export an ibv_register_driver() function.
 */
struct ibv_device *openib_driver_init(struct sysfs_class_device *sysdev)
{
	int abi_ver = 0;
	char value[8];

	if (ibv_read_sysfs_file(sysdev->path, "abi_version",
				value, sizeof value) > 0)
		abi_ver = strtol(value, NULL, 10);

	return mthca_driver_init(sysdev->path, abi_ver);
}
#endif /* HAVE_IBV_REGISTER_DRIVER */
