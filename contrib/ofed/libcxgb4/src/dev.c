/*
 * Copyright (c) 2006-2014 Chelsio, Inc. All rights reserved.
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
#endif				/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include "libcxgb4.h"
#include "cxgb4-abi.h"

#define PCI_VENDOR_ID_CHELSIO		0x1425

/*
 * Macros needed to support the PCI Device ID Table ...
 */
#define CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN \
	struct { \
		unsigned vendor; \
		unsigned device; \
	} hca_table[] = {

#define CH_PCI_DEVICE_ID_FUNCTION \
		0x4

#define CH_PCI_ID_TABLE_ENTRY(__DeviceID) \
		{ \
			.vendor = PCI_VENDOR_ID_CHELSIO, \
			.device = (__DeviceID), \
		}

#define CH_PCI_DEVICE_ID_TABLE_DEFINE_END \
	}

#include "t4_chip_type.h"
#include "t4_pci_id_tbl.h"

unsigned long c4iw_page_size;
unsigned long c4iw_page_shift;
unsigned long c4iw_page_mask;
int ma_wr;
int t5_en_wc = 1;

SLIST_HEAD(devices_struct, c4iw_dev) devices;

static struct ibv_context_ops c4iw_ctx_ops = {
	.query_device = c4iw_query_device,
	.query_port = c4iw_query_port,
	.alloc_pd = c4iw_alloc_pd,
	.dealloc_pd = c4iw_free_pd,
	.reg_mr = c4iw_reg_mr,
	.dereg_mr = c4iw_dereg_mr,
	.create_cq = c4iw_create_cq,
	.resize_cq = c4iw_resize_cq,
	.destroy_cq = c4iw_destroy_cq,
	.create_srq = c4iw_create_srq,
	.modify_srq = c4iw_modify_srq,
	.destroy_srq = c4iw_destroy_srq,
	.create_qp = c4iw_create_qp,
	.modify_qp = c4iw_modify_qp,
	.destroy_qp = c4iw_destroy_qp,
	.query_qp = c4iw_query_qp,
	.create_ah = c4iw_create_ah,
	.destroy_ah = c4iw_destroy_ah,
	.attach_mcast = c4iw_attach_mcast,
	.detach_mcast = c4iw_detach_mcast,
	.post_srq_recv = c4iw_post_srq_recv,
	.req_notify_cq = c4iw_arm_cq,
};

static struct ibv_context *c4iw_alloc_context(struct ibv_device *ibdev,
					      int cmd_fd)
{
	struct c4iw_context *context;
	struct ibv_get_context cmd;
	struct c4iw_alloc_ucontext_resp resp;
	struct c4iw_dev *rhp = to_c4iw_dev(ibdev);
	struct ibv_query_device qcmd;
	uint64_t raw_fw_ver;
	struct ibv_device_attr attr;

	context = malloc(sizeof *context);
	if (!context)
		return NULL;

	memset(context, 0, sizeof *context);
	context->ibv_ctx.cmd_fd = cmd_fd;

	resp.status_page_size = 0;
	resp.reserved = 0;
	if (ibv_cmd_get_context(&context->ibv_ctx, &cmd, sizeof cmd,
				&resp.ibv_resp, sizeof resp))
		goto err_free;

	if (resp.reserved)
		PDBG("%s c4iw_alloc_ucontext_resp reserved field modified by kernel\n",
		     __FUNCTION__);

	context->status_page_size = resp.status_page_size;
	if (resp.status_page_size) {
		context->status_page = mmap(NULL, resp.status_page_size,
					    PROT_READ, MAP_SHARED, cmd_fd,
					    resp.status_page_key);
		if (context->status_page == MAP_FAILED)
			goto err_free;
	} 

	context->ibv_ctx.device = ibdev;
	context->ibv_ctx.ops = c4iw_ctx_ops;

	switch (rhp->chip_version) {
	case CHELSIO_T5:
		PDBG("%s T5/T4 device\n", __FUNCTION__);
	case CHELSIO_T4:
		PDBG("%s T4 device\n", __FUNCTION__);
		context->ibv_ctx.ops.async_event = c4iw_async_event;
		context->ibv_ctx.ops.post_send = c4iw_post_send;
		context->ibv_ctx.ops.post_recv = c4iw_post_receive;
		context->ibv_ctx.ops.poll_cq = c4iw_poll_cq;
		context->ibv_ctx.ops.req_notify_cq = c4iw_arm_cq;
		break;
	default:
		PDBG("%s unknown hca type %d\n", __FUNCTION__,
		     rhp->chip_version);
		goto err_unmap;
		break;
	}

	if (!rhp->mmid2ptr) {
		int ret;

		ret = ibv_cmd_query_device(&context->ibv_ctx, &attr, &raw_fw_ver, &qcmd,
					   sizeof qcmd);
		if (ret)
			goto err_unmap;
		rhp->max_mr = attr.max_mr;
		rhp->mmid2ptr = calloc(attr.max_mr, sizeof(void *));
		if (!rhp->mmid2ptr) {
			goto err_unmap;
		}
		rhp->max_qp = T4_QID_BASE + attr.max_cq;
		rhp->qpid2ptr = calloc(T4_QID_BASE + attr.max_cq, sizeof(void *));
		if (!rhp->qpid2ptr) {
			goto err_unmap;
		}
		rhp->max_cq = T4_QID_BASE + attr.max_cq;
		rhp->cqid2ptr = calloc(T4_QID_BASE + attr.max_cq, sizeof(void *));
		if (!rhp->cqid2ptr)
			goto err_unmap;
	}

	return &context->ibv_ctx;

err_unmap:
	munmap(context->status_page, context->status_page_size);
err_free:
	if (rhp->cqid2ptr)
		free(rhp->cqid2ptr);
	if (rhp->qpid2ptr)
		free(rhp->cqid2ptr);
	if (rhp->mmid2ptr)
		free(rhp->cqid2ptr);
	free(context);
	return NULL;
}

static void c4iw_free_context(struct ibv_context *ibctx)
{
	struct c4iw_context *context = to_c4iw_context(ibctx);

	if (context->status_page_size)
		munmap(context->status_page, context->status_page_size);
	free(context);
}

static struct ibv_device_ops c4iw_dev_ops = {
	.alloc_context = c4iw_alloc_context,
	.free_context = c4iw_free_context
};

#ifdef STALL_DETECTION

int stall_to;

static void dump_cq(struct c4iw_cq *chp)
{
	int i;

	fprintf(stderr,
 		"CQ: %p id %u queue %p cidx 0x%08x sw_queue %p sw_cidx %d sw_pidx %d sw_in_use %d depth %u error %u gen %d "
		"cidx_inc %d bits_type_ts %016" PRIx64 " notempty %d\n", chp,
                chp->cq.cqid, chp->cq.queue, chp->cq.cidx,
	 	chp->cq.sw_queue, chp->cq.sw_cidx, chp->cq.sw_pidx, chp->cq.sw_in_use,
                chp->cq.size, chp->cq.error, chp->cq.gen, chp->cq.cidx_inc, be64_to_cpu(chp->cq.bits_type_ts),
		t4_cq_notempty(&chp->cq) || (chp->iq ? t4_iq_notempty(chp->iq) : 0));

	for (i=0; i < chp->cq.size; i++) {
		u64 *p = (u64 *)(chp->cq.queue + i);

		fprintf(stderr, "%02x: %016" PRIx64 " %016" PRIx64, i, be64_to_cpu(p[0]), be64_to_cpu(p[1]));
		if (i == chp->cq.cidx)
			fprintf(stderr, " <-- cidx\n");
		else
			fprintf(stderr, "\n");
		p+= 2;
		fprintf(stderr, "%02x: %016" PRIx64 " %016" PRIx64 "\n", i, be64_to_cpu(p[0]), be64_to_cpu(p[1]));
		p+= 2;
		fprintf(stderr, "%02x: %016" PRIx64 " %016" PRIx64 "\n", i, be64_to_cpu(p[0]), be64_to_cpu(p[1]));
		p+= 2;
		fprintf(stderr, "%02x: %016" PRIx64 " %016" PRIx64 "\n", i, be64_to_cpu(p[0]), be64_to_cpu(p[1]));
		p+= 2;
	}
}

static void dump_qp(struct c4iw_qp *qhp)
{
	int i;
	int j;
	struct t4_swsqe *swsqe;
	struct t4_swrqe *swrqe;
	u16 cidx, pidx;
	u64 *p;

	fprintf(stderr,
		"QP: %p id %u error %d flushed %d qid_mask 0x%x\n"
		"    SQ: id %u queue %p sw_queue %p cidx %u pidx %u in_use %u wq_pidx %u depth %u flags 0x%x flush_cidx %d\n"
		"    RQ: id %u queue %p sw_queue %p cidx %u pidx %u in_use %u depth %u\n",
		qhp,
		qhp->wq.sq.qid,
		qhp->wq.error,
		qhp->wq.flushed,
		qhp->wq.qid_mask,
		qhp->wq.sq.qid,
		qhp->wq.sq.queue,
		qhp->wq.sq.sw_sq,
		qhp->wq.sq.cidx,
		qhp->wq.sq.pidx,
		qhp->wq.sq.in_use,
		qhp->wq.sq.wq_pidx,
		qhp->wq.sq.size,
		qhp->wq.sq.flags,
		qhp->wq.sq.flush_cidx,
		qhp->wq.rq.qid,
		qhp->wq.rq.queue,
		qhp->wq.rq.sw_rq,
		qhp->wq.rq.cidx,
		qhp->wq.rq.pidx,
		qhp->wq.rq.in_use,
		qhp->wq.rq.size);
	cidx = qhp->wq.sq.cidx;
	pidx = qhp->wq.sq.pidx;
	if (cidx != pidx)
		fprintf(stderr, "SQ: \n");
	while (cidx != pidx) {
		swsqe = &qhp->wq.sq.sw_sq[cidx];
		fprintf(stderr, "%04u: wr_id %016" PRIx64
			" sq_wptr %08x read_len %u opcode 0x%x "
			"complete %u signaled %u cqe %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
			cidx,
			swsqe->wr_id,
			swsqe->idx,
			swsqe->read_len,
			swsqe->opcode,
			swsqe->complete,
			swsqe->signaled,
			cpu_to_be64(swsqe->cqe.u.flits[0]),
			cpu_to_be64(swsqe->cqe.u.flits[1]),
			cpu_to_be64((u64)swsqe->cqe.reserved),
			cpu_to_be64(swsqe->cqe.bits_type_ts));
		if (++cidx == qhp->wq.sq.size)
			cidx = 0;
	}

	fprintf(stderr, "SQ WQ: \n");
	p = (u64 *)qhp->wq.sq.queue;
	for (i=0; i < qhp->wq.sq.size * T4_SQ_NUM_SLOTS; i++) {
		for (j=0; j < T4_EQ_ENTRY_SIZE / 16; j++) {
			fprintf(stderr, "%04u %016" PRIx64 " %016" PRIx64 " ",
				i, ntohll(p[0]), ntohll(p[1]));
			if (j == 0 && i == qhp->wq.sq.wq_pidx)
				fprintf(stderr, " <-- pidx");
			fprintf(stderr, "\n");
			p += 2;
		}
	}
	cidx = qhp->wq.rq.cidx;
	pidx = qhp->wq.rq.pidx;
	if (cidx != pidx)
		fprintf(stderr, "RQ: \n");
	while (cidx != pidx) {
		swrqe = &qhp->wq.rq.sw_rq[cidx];
		fprintf(stderr, "%04u: wr_id %016" PRIx64 "\n",
			cidx,
			swrqe->wr_id );
		if (++cidx == qhp->wq.rq.size)
			cidx = 0;
	}

	fprintf(stderr, "RQ WQ: \n");
	p = (u64 *)qhp->wq.rq.queue;
	for (i=0; i < qhp->wq.rq.size * T4_RQ_NUM_SLOTS; i++) {
		for (j=0; j < T4_EQ_ENTRY_SIZE / 16; j++) {
			fprintf(stderr, "%04u %016" PRIx64 " %016" PRIx64 " ",
				i, ntohll(p[0]), ntohll(p[1]));
			if (j == 0 && i == qhp->wq.rq.pidx)
				fprintf(stderr, " <-- pidx");
			if (j == 0 && i == qhp->wq.rq.cidx)
				fprintf(stderr, " <-- cidx");
			fprintf(stderr, "\n");
			p+=2;
		}
	}
}

void dump_state()
{
	struct c4iw_dev *dev;
	int i;

	fprintf(stderr, "STALL DETECTED:\n");
	SLIST_FOREACH(dev, &devices, list) {
		//pthread_spin_lock(&dev->lock);
		fprintf(stderr, "Device %s\n", dev->ibv_dev.name);
		for (i=0; i < dev->max_cq; i++) {
			if (dev->cqid2ptr[i]) {
				struct c4iw_cq *chp = dev->cqid2ptr[i];
				//pthread_spin_lock(&chp->lock);
				dump_cq(chp);
				//pthread_spin_unlock(&chp->lock);
			}
		}
		for (i=0; i < dev->max_qp; i++) {
			if (dev->qpid2ptr[i]) {
				struct c4iw_qp *qhp = dev->qpid2ptr[i];
				//pthread_spin_lock(&qhp->lock);
				dump_qp(qhp);
				//pthread_spin_unlock(&qhp->lock);
			}
		}
		//pthread_spin_unlock(&dev->lock);
	}
	fprintf(stderr, "DUMP COMPLETE:\n");
	fflush(stderr);
}
#endif /* end of STALL_DETECTION */

/*
 * c4iw_abi_version is used to store ABI for iw_cxgb4 so the user mode library
 * can know if the driver supports the kernel mode db ringing. 
 */
int c4iw_abi_version = 1;

static struct ibv_device *cxgb4_driver_init(const char *uverbs_sys_path,
					    int abi_version)
{
	char devstr[IBV_SYSFS_PATH_MAX], ibdev[16], value[128], *cp;
	char t5nexstr[IBV_SYSFS_PATH_MAX];
	struct c4iw_dev *dev;
	unsigned vendor, device, fw_maj, fw_min;
	int i;
	char devnum=0;
        char ib_param[16];

#ifndef __linux__
	if (ibv_read_sysfs_file(uverbs_sys_path, "ibdev",
				ibdev, sizeof ibdev) < 0)
		return NULL;
	/* 
	 * Extract the non-numeric part of ibdev
	 * say "t5nex0" -> devname=="t5nex", devnum=0
	 */
	if (strstr(ibdev,"t5nex")) {
		devnum = atoi(ibdev+strlen("t5nex"));
		sprintf(t5nexstr, "/dev/t5nex/%d", devnum);
	} else
		return NULL;

	if (ibv_read_sysfs_file(t5nexstr, "\%pnpinfo",
				value, sizeof value) < 0)
		return NULL;
	else {
		if (strstr(value,"vendor=")) {
			strncpy(ib_param, strstr(value,"vendor=")+strlen("vendor="),6);
			sscanf(ib_param,"%i",&vendor);
		}

		if (strstr(value,"device=")) {
			strncpy(ib_param, strstr(value,"device=")+strlen("device="),6);
			sscanf(ib_param,"%i",&device);
		}
	}
#else
	if (ibv_read_sysfs_file(uverbs_sys_path, "device/vendor",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &vendor);

	if (ibv_read_sysfs_file(uverbs_sys_path, "device/device",
				value, sizeof value) < 0)
		return NULL;
	sscanf(value, "%i", &device);
#endif

	for (i = 0; i < sizeof hca_table / sizeof hca_table[0]; ++i)
		if (vendor == hca_table[i].vendor &&
		    device == hca_table[i].device)
			goto found;

	return NULL;

found:
	c4iw_abi_version = abi_version;	


#ifndef __linux__
	if (ibv_read_sysfs_file(t5nexstr, "firmware_version",
				value, sizeof value) < 0)
		return NULL;
#else
	/*
	 * Verify that the firmware major number matches.  Major number
	 * mismatches are fatal.  Minor number mismatches are tolerated.
	 */
	if (ibv_read_sysfs_file(uverbs_sys_path, "ibdev",
				ibdev, sizeof ibdev) < 0)
		return NULL;

	memset(devstr, 0, sizeof devstr);
	snprintf(devstr, sizeof devstr, "%s/class/infiniband/%s",
		 ibv_get_sysfs_path(), ibdev);
	if (ibv_read_sysfs_file(devstr, "fw_ver", value, sizeof value) < 0)
		return NULL;
#endif

	cp = strtok(value+1, ".");
	sscanf(cp, "%i", &fw_maj);
	cp = strtok(NULL, ".");
	sscanf(cp, "%i", &fw_min);

	if (fw_maj < FW_MAJ) {
		fprintf(stderr, "libcxgb4: Fatal firmware version mismatch.  "
			"Firmware major number is %u and libcxgb4 needs %u.\n",
			fw_maj, FW_MAJ);
		fflush(stderr);
		return NULL;
	}

	DBGLOG("libcxgb4");

	if (fw_min < FW_MIN) {
		PDBG("libcxgb4: non-fatal firmware version mismatch.  "
			"Firmware minor number is %u and libcxgb4 needs %u.\n",
			fw_maj, FW_MAJ);
		fflush(stderr);
	}

	PDBG("%s found vendor %d device %d type %d\n",
		__FUNCTION__, vendor, device,
		CHELSIO_PCI_ID_CHIP_VERSION(hca_table[i].device));

	dev = calloc(1, sizeof *dev);
	if (!dev) {
		return NULL;
	}

	pthread_spin_init(&dev->lock, PTHREAD_PROCESS_PRIVATE);
	dev->ibv_dev.ops = c4iw_dev_ops;
	dev->chip_version = CHELSIO_PCI_ID_CHIP_VERSION(hca_table[i].device);
	dev->abi_version = abi_version;

	PDBG("%s device claimed\n", __FUNCTION__);
	SLIST_INSERT_HEAD(&devices, dev, list);
#ifdef STALL_DETECTION
{
	char *c = getenv("CXGB4_STALL_TIMEOUT");
	if (c) {
		stall_to = strtol(c, NULL, 0);
		if (errno || stall_to < 0)
			stall_to = 0;
	}
}
#endif
{
	char *c = getenv("CXGB4_MA_WR");
	if (c) {
		ma_wr = strtol(c, NULL, 0);
		if (ma_wr != 1)
			ma_wr = 0;
	}
}
{
	char *c = getenv("T5_ENABLE_WC");
	if (c) {
		t5_en_wc = strtol(c, NULL, 0);
		if (t5_en_wc != 1)
			t5_en_wc = 0;
	}
}

	return &dev->ibv_dev;
}

static __attribute__((constructor)) void cxgb4_register_driver(void)
{
	c4iw_page_size = sysconf(_SC_PAGESIZE);
	c4iw_page_shift = long_log2(c4iw_page_size);
	c4iw_page_mask = ~(c4iw_page_size - 1);
	ibv_register_driver("cxgb4", cxgb4_driver_init);
}

#ifdef STATS
void __attribute__ ((destructor)) cs_fini(void);
void  __attribute__ ((destructor)) cs_fini(void)
{
	syslog(LOG_NOTICE, "cxgb4 stats - sends %lu recv %lu read %lu "
	       "write %lu arm %lu cqe %lu mr %lu qp %lu cq %lu\n",
	       c4iw_stats.send, c4iw_stats.recv, c4iw_stats.read,
	       c4iw_stats.write, c4iw_stats.arm, c4iw_stats.cqe,
	       c4iw_stats.mr, c4iw_stats.qp, c4iw_stats.cq);
}
#endif
