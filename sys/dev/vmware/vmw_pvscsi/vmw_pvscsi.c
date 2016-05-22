/*
 * Copyright (c) 2014 EMC Corporation, Inc.  All rights reserved.
 */

/* ************************************************************************
 * Copyright 2008 VMware, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ************************************************************************/

/*
 * pvscsi.c --
 *
 *      This is a driver for the VMware PVSCSI HBA adapter.
 */

#ifndef __FreeBSD__
#include "driver-config.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pci.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>

#include "compat_module.h"
#include "compat_scsi.h"
#include "compat_pci.h"
#include "compat_interrupt.h"
#include "compat_workqueue.h"

#include "pvscsi_defs.h"
#include "pvscsi_version.h"
#include "scsi_defs.h"
#include "vm_device_version.h"
#include "vm_assert.h"

#define PVSCSI_LINUX_DRIVER_DESC "VMware PVSCSI driver"

MODULE_DESCRIPTION(PVSCSI_LINUX_DRIVER_DESC);
MODULE_AUTHOR("VMware, Inc.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(PVSCSI_DRIVER_VERSION_STRING);

/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
#else
#include "vmw_pvscsi.h"
#endif /* __FreeBSD__ */

#define PVSCSI_DEFAULT_NUM_PAGES_PER_RING	8
#define PVSCSI_DEFAULT_NUM_PAGES_MSG_RING	1
#define PVSCSI_DEFAULT_QUEUE_DEPTH		64

#ifndef __FreeBSD__
/* MSI-X has horrible performance in < 2.6.19 due to needless mask frobbing */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#define PVSCSI_DISABLE_MSIX	0
#else
#define PVSCSI_DISABLE_MSIX	1
#endif
#endif /* __FreeBSD__ */


#ifndef __FreeBSD__
#define HOST_ADAPTER(host) ((struct pvscsi_adapter *)(host)->hostdata)

#define LOG(level, fmt, args...)				\
do {								\
	if (pvscsi_debug_level > level)				\
		printk(KERN_DEBUG "pvscsi: " fmt, args);	\
} while (0)
#endif /* __FreeBSD__ */


#ifdef __FreeBSD__
typedef struct pvscsi_adapter pvscsinst_t;

#define MODNM pvscsi
enum {
	PVSCSI_MSIX_VEC0 = 0, /* Only one MSI-X interrupt required */
	PVSCSI_NUM_MSIX
};

MALLOC_DEFINE(M_PVSCSI, "pvscsi", "VMware's para-virtualized scsi driver");
MALLOC_DEFINE(M_PVSCSI_PCI, "pvscsi_pci", "pvscsi's ring queues");
MALLOC_DEFINE(M_PVSCSI_SGL, "pvscsi_sgl", "pvscsi's scatter-gather list");
#endif /* __FreeBSD__ */



/* Command line parameters */
#ifndef __FreeBSD__
static int pvscsi_debug_level;
#endif /* __FreeBSD__ */
static int pvscsi_ring_pages     = PVSCSI_DEFAULT_NUM_PAGES_PER_RING;
static int pvscsi_msg_ring_pages = PVSCSI_DEFAULT_NUM_PAGES_MSG_RING;
#ifndef __FreeBSD__
static int pvscsi_cmd_per_lun    = PVSCSI_DEFAULT_QUEUE_DEPTH;
static compat_mod_param_bool pvscsi_disable_msi;
static compat_mod_param_bool pvscsi_disable_msix   = PVSCSI_DISABLE_MSIX;
static compat_mod_param_bool pvscsi_use_msg        = TRUE;
#else
static bool pvscsi_use_msg       = true;
#endif /* __FreeBSD__ */

#ifndef __FreeBSD__
#define PVSCSI_RW (S_IRUSR | S_IWUSR)

module_param_named(debug_level, pvscsi_debug_level, int, PVSCSI_RW);
MODULE_PARM_DESC(debug_level, "Debug logging level - (default=0)");

module_param_named(ring_pages, pvscsi_ring_pages, int, PVSCSI_RW);
MODULE_PARM_DESC(ring_pages, "Number of pages per req/cmp ring - (default="
		 __stringify(PVSCSI_DEFAULT_NUM_PAGES_PER_RING) ")");

module_param_named(msg_ring_pages, pvscsi_msg_ring_pages, int, PVSCSI_RW);
MODULE_PARM_DESC(msg_ring_pages, "Number of pages for the msg ring - (default="
		 __stringify(PVSCSI_DEFAULT_NUM_PAGES_MSG_RING) ")");

module_param_named(cmd_per_lun, pvscsi_cmd_per_lun, int, PVSCSI_RW);
MODULE_PARM_DESC(cmd_per_lun, "Maximum commands per lun - (default="
		 __stringify(PVSCSI_MAX_REQ_QUEUE_DEPTH) ")");

module_param_named(disable_msi, pvscsi_disable_msi, bool, PVSCSI_RW);
MODULE_PARM_DESC(disable_msi, "Disable MSI use in driver - (default=0)");

module_param_named(disable_msix, pvscsi_disable_msix, bool, PVSCSI_RW);
MODULE_PARM_DESC(disable_msix, "Disable MSI-X use in driver - (default="
		 __stringify(PVSCSI_DISABLE_MSIX) ")");

module_param_named(use_msg, pvscsi_use_msg, bool, PVSCSI_RW);
MODULE_PARM_DESC(use_msg, "Use msg ring when available - (default=1)");

static const struct pci_device_id pvscsi_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_PVSCSI) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pvscsi_pci_tbl);
#else
#define pvscsi_dev(adapter) ((adapter)->pvs_dev)
#define pvscsi_simdev(sim) (pvscsi_dev((pvscsinst_t *)cam_sim_softc(sim)))

/*
 * To share code between the Linux and Isilon ports we key off the variable
 * "irq" to figure out if we need to actually lock or not
 */
#define spin_lock_irqsave(l,f) if (irq) PVSCSILCK
#define spin_unlock_irqrestore(l,f) if (irq) PVSCSIULCK

#define SCSI_SENSE_BUFFERSIZE sizeof(csio->sense_data)
#define SIMPLE_QUEUE_TAG MSG_SIMPLE_Q_TAG
#define HEAD_OF_QUEUE_TAG MSG_HEAD_OF_Q_TAG
#define ORDERED_QUEUE_TAG MSG_ORDERED_Q_TAG

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

struct scsi_cmnd {
	struct ccb_scsiio *qsc_csio;
	void *sense_buffer;
	unsigned short cmd_len;
	unsigned char *cmn;
	struct scsi_device *device;
	enum dma_data_direction sc_data_direction;
	unsigned char tag;
	unsigned char *cmnd;
	pvscsinst_t *adapter;
};

struct scsi_device {
	unsigned int id, lun, channel;
	bool tagged_supported;
};
#endif /* __FreeBSD__ */

static struct pvscsi_ctx *
#ifndef __FreeBSD__
pvscsi_find_context(const struct pvscsi_adapter *adapter, struct scsi_cmnd *cmd)
#else /* __FreeBSD__ */
pvscsi_find_context(struct pvscsi_adapter *adapter, struct ccb_scsiio *cmd)
#endif /* __FreeBSD__ */
{
	struct pvscsi_ctx *ctx, *end;

	end = &adapter->cmd_map[adapter->req_depth];
	for (ctx = adapter->cmd_map; ctx < end; ctx++)
		if (ctx->cmd == cmd)
			return ctx;

	return NULL;
}

static struct pvscsi_ctx *
pvscsi_acquire_context(struct pvscsi_adapter *adapter, struct scsi_cmnd *cmd)
{
	struct pvscsi_ctx *ctx;

	if (list_empty(&adapter->cmd_pool))
		return NULL;

	ctx = list_entry(adapter->cmd_pool.next, struct pvscsi_ctx, list);
#ifndef __FreeBSD__
	ctx->cmd = cmd;
#else
	ctx->cmd = cmd->qsc_csio;
	ctx->toed = false;
#endif /* __FreeBSD__ */
	list_del(&ctx->list);

	return ctx;
}

static void pvscsi_release_context(struct pvscsi_adapter *adapter,
				   struct pvscsi_ctx *ctx)
{
	ctx->cmd = NULL;
	list_add(&ctx->list, &adapter->cmd_pool);
}

/*
 * Map a pvscsi_ctx struct to a context ID field value; we map to a simple
 * non-zero integer.
 */
static u64 pvscsi_map_context(const struct pvscsi_adapter *adapter,
			      const struct pvscsi_ctx *ctx)
{
	return ctx - adapter->cmd_map + 1;
}

static struct pvscsi_ctx *
pvscsi_get_context(const struct pvscsi_adapter *adapter, u64 context)
{
	return &adapter->cmd_map[context - 1];
}

/**************************************************************
 *
 *   VMWARE PVSCSI Hypervisor Communication Implementation
 *
 *   This code is largely independent of any Linux internals.
 *
 **************************************************************/

static void pvscsi_reg_write(const struct pvscsi_adapter *adapter,
			     u32 offset, u32 val)
{
#ifndef __FreeBSD__
	writel(val, adapter->mmioBase + offset);
#else /* __FreeBSD__ */
	bus_space_write_4(adapter->pvs_mmtag, adapter->pvs_mmhndl, offset, val);
#endif /* __FreeBSD__ */
}

static u32 pvscsi_reg_read(const struct pvscsi_adapter *adapter, u32 offset)
{
#ifndef __FreeBSD__
	return readl(adapter->mmioBase + offset);
#else /* __FreeBSD__ */
	return bus_space_read_4(adapter->pvs_mmtag, adapter->pvs_mmhndl, offset);
#endif /* __FreeBSD__ */
}

static u32 pvscsi_read_intr_status(const struct pvscsi_adapter *adapter)
{
	return pvscsi_reg_read(adapter, PVSCSI_REG_OFFSET_INTR_STATUS);
}

static void pvscsi_write_intr_status(const struct pvscsi_adapter *adapter,
				     u32 val)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_INTR_STATUS, val);
}

static void pvscsi_unmask_intr(const struct pvscsi_adapter *adapter)
{
	u32 intr_bits;

	intr_bits = PVSCSI_INTR_CMPL_MASK;
	if (adapter->use_msg) {
		intr_bits |= PVSCSI_INTR_MSG_MASK;
	}

	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_INTR_MASK, intr_bits);
}

static void pvscsi_mask_intr(const struct pvscsi_adapter *adapter)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_INTR_MASK, 0);
}

static void pvscsi_write_cmd_desc(const struct pvscsi_adapter *adapter,
				  u32 cmd, const void *desc, size_t len)
{
#ifndef __FreeBSD__
	u32 *ptr = (u32 *)desc;
#else /* __FreeBSD__ */
	const u32 *ptr = (const u32 *)desc;
#endif /* __FreeBSD__ */
	unsigned i;

	len /= sizeof(u32);
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_COMMAND, cmd);
	for (i = 0; i < len; i++)
		pvscsi_reg_write(adapter,
				 PVSCSI_REG_OFFSET_COMMAND_DATA, ptr[i]);
}

#ifndef __FreeBSD__
static void pvscsi_abort_cmd(const struct pvscsi_adapter *adapter,
			     const struct pvscsi_ctx *ctx)
{
	struct PVSCSICmdDescAbortCmd cmd = { 0 };

	cmd.target = ctx->cmd->device->id;
#else /* __FreeBSD__ */
static void pvscsi_abort_cmd(struct pvscsi_adapter *adapter,
			     struct pvscsi_ctx *ctx,
			     target_id_t trg)
{
	struct PVSCSICmdDescAbortCmd cmd = { 0 };
	cmd.target = trg;
#endif /* __FreeBSD__ */
	cmd.context = pvscsi_map_context(adapter, ctx);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_ABORT_CMD, &cmd, sizeof cmd);
}

static void pvscsi_kick_rw_io(const struct pvscsi_adapter *adapter)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_KICK_RW_IO, 0);
}

static void pvscsi_process_request_ring(const struct pvscsi_adapter *adapter)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_KICK_NON_RW_IO, 0);
}

static int scsi_is_rw(unsigned char op)
{
	return op == READ_6  || op == WRITE_6 ||
	       op == READ_10 || op == WRITE_10 ||
	       op == READ_12 || op == WRITE_12 ||
	       op == READ_16 || op == WRITE_16;
}

static void pvscsi_kick_io(const struct pvscsi_adapter *adapter,
			   unsigned char op)
{
	if (scsi_is_rw(op))
		pvscsi_kick_rw_io(adapter);
	else
		pvscsi_process_request_ring(adapter);
}

static void ll_adapter_reset(const struct pvscsi_adapter *adapter)
{
	LOG(0, "Adapter Reset on %p\n", adapter);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
}

static void ll_bus_reset(const struct pvscsi_adapter *adapter)
{
	LOG(0, "Reseting bus on %p\n", adapter);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_RESET_BUS, NULL, 0);
}

static void ll_device_reset(const struct pvscsi_adapter *adapter, u32 target)
{
	struct PVSCSICmdDescResetDevice cmd = { 0 };

	LOG(0, "Reseting device: target=%u\n", target);

#ifdef __FreeBSD__
	device_t device = pvscsi_dev(adapter);
	device_printf(device, "Resetting target %u\n", target); 
#endif /* _FreeBSD__ */
	cmd.target = target;

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_RESET_DEVICE,
			      &cmd, sizeof cmd);
#ifdef __FreeBSD__
	device_printf(device, "Done resetting target %u\n", target); 
#endif /* _FreeBSD__ */
}


#ifndef __FreeBSD__
/**************************************************************
 *
 *   VMWARE Hypervisor ring / SCSI mid-layer interactions
 *
 *   Functions which have to deal with both ring semantics
 *   and Linux SCSI internals are placed here.
 *
 **************************************************************/

static void pvscsi_create_sg(struct pvscsi_ctx *ctx,
			     struct scatterlist *sg, unsigned count)
{
	unsigned i;
	struct PVSCSISGElement *sge;

	BUG_ON(count > PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT);

	sge = &ctx->sgl->sge[0];
	for (i = 0; i < count; i++, sg++) {
		sge[i].addr   = sg_dma_address(sg);
		sge[i].length = sg_dma_len(sg);
		sge[i].flags  = 0;
	}
}
#endif /* __FreeBSD__ */

#ifdef  __FreeBSD__
/*
 * Takes a list of physical segments and translates them into the VMware
 * device emulation's scatter/gather format.  It does not initiate the I/O.
 * It reports any errors in the translation through the ctx structure.
 *
 * The bus_dma_segment_t pointed to by dm_segs is allocated on the stack.
 */
static void
pvscsi_queue_io(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct pvscsi_ctx *ctx = (struct pvscsi_ctx *)arg;

	if (error || ctx->dmamapping_errno) {
		ctx->dmamapping_errno = error;
		return;
	}

	if (nseg > PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT) {
		ctx->dmamapping_errno = EFBIG;
		return;
	}

	unsigned i;
	struct PVSCSISGElement *sge = &ctx->sgl->sge[0];

	struct PVSCSIRingReqDesc *e = ctx->e;
	e->flags |= PVSCSI_FLAG_CMD_WITH_SG_LIST;

	for (i = 0; i < nseg; i++) {
		sge[i].addr   = dm_segs[i].ds_addr;
		sge[i].length = dm_segs[i].ds_len;
		sge[i].flags  = 0;
	}

	ctx->dmamapping_errno = 0;
	e->flags |= PVSCSI_FLAG_CMD_WITH_SG_LIST;
	ctx->sglPA = pci_map_single(adapter->dev, ctx->sgl,
				    PAGE_SIZE, PCI_DMA_TODEVICE);
	e->dataAddr = ctx->sglPA;
}
#define scsi_bufflen(cmd) (cmd)->qsc_csio->dxfer_len
#define pvscsi_create_sg(a,b,c)
#define scsi_sg_count(a) 2
#define scsi_dma_map(a) 2

static inline dma_addr_t
sg_dma_address_fn(void)
{
	panic("This code-path shouldn't have been taken");
	return 0;
}
#define sg_dma_address(sg) sg_dma_address_fn()
#define IRQ_RETVAL(a) 0

#define SAM_STAT_GOOD SCSI_STATUS_OK
#define SAM_STAT_CHECK_CONDITION SCSI_STATUS_CHECK_COND
#define SAM_STAT_COMMAND_TERMINATED SCSI_STATUS_CMD_TERMINATED
#endif /* __FreeBSD__ */
/*
 * Map all data buffers for a command into PCI space and
 * setup the scatter/gather list if needed.
 */
static void pvscsi_map_buffers(struct pvscsi_adapter *adapter,
			       struct pvscsi_ctx *ctx, struct scsi_cmnd *cmd,
			       struct PVSCSIRingReqDesc *e)
{
	unsigned count;
	unsigned bufflen = scsi_bufflen(cmd);

	e->dataLen = bufflen;
	e->dataAddr = 0;
	if (bufflen == 0)
		return;

#ifdef __FreeBSD__
	struct ccb_scsiio *csio = cmd->qsc_csio;
	ctx->e = e;
	ctx->dmamapping_errno = 0;

	int error;

	error = bus_dmamap_load_ccb(adapter->pvs_dmat, ctx->dmap,
	    (union ccb *)csio, pvscsi_queue_io, ctx, BUS_DMA_NOWAIT);
	if (error)
		ctx->dmamapping_errno = error;

	if (ctx->dmamapping_errno) {
		if (ctx->dmamapping_errno == EFBIG)
			csio->ccb_h.flags = CAM_REQ_TOO_BIG;
		else
			csio->ccb_h.flags = CAM_REQ_CMP_ERR;
	}

	/*
	 * Setup 'count' and 'segs' so that we choose the path that sets
	 * PVSCSI_FLAG_CMD_WITH_SG_LIST and uses a scatter/gather list
	 */
#endif /* __FreeBSD__ */
	count = scsi_sg_count(cmd);
	if (count != 0) {
#ifndef __FreeBSD__
		struct scatterlist *sg = scsi_sglist(cmd);
		int segs = pci_map_sg(adapter->dev, sg, count,
				      cmd->sc_data_direction);
#else
		int segs = 2; /* Force the more generic path below */
#endif /* __FreeBSD__ */
		if (segs > 1) {
			pvscsi_create_sg(ctx, sg, segs);

			e->flags |= PVSCSI_FLAG_CMD_WITH_SG_LIST;
			ctx->sglPA = pci_map_single(adapter->dev, ctx->sgl,
						    PAGE_SIZE, PCI_DMA_TODEVICE);
			e->dataAddr = ctx->sglPA;
		} else
			e->dataAddr = sg_dma_address(sg);
	} else {
#ifndef __FreeBSD__
		ctx->dataPA = pci_map_single(adapter->dev,
					     scsi_request_buffer(cmd), bufflen,
					     cmd->sc_data_direction);
#endif /* __FreeBSD__ */
		e->dataAddr = ctx->dataPA;
	}
}

static void pvscsi_unmap_buffers(const struct pvscsi_adapter *adapter,
				 struct pvscsi_ctx *ctx)
{
#ifndef __FreeBSD__
	struct scsi_cmnd *cmd;
	unsigned bufflen;

	cmd = ctx->cmd;
	bufflen = scsi_bufflen(cmd);

	if (bufflen != 0) {
		unsigned count = scsi_sg_count(cmd);

		if (count != 0) {
			pci_unmap_sg(adapter->dev, scsi_sglist(cmd), count,
				     cmd->sc_data_direction);
			if (ctx->sglPA) {
				pci_unmap_single(adapter->dev, ctx->sglPA,
						 PAGE_SIZE, PCI_DMA_TODEVICE);
				ctx->sglPA = 0;
			}
		} else
			pci_unmap_single(adapter->dev, ctx->dataPA, bufflen,
					 cmd->sc_data_direction);
	}
	if (cmd->sense_buffer)
		pci_unmap_single(adapter->dev, ctx->sensePA,
				 SCSI_SENSE_BUFFERSIZE, PCI_DMA_FROMDEVICE);
#else /* __FreeBSD__ */
	struct ccb_scsiio *csio = ctx->cmd;

	if (csio->dxfer_len)
		bus_dmamap_unload(adapter->pvs_dmat, ctx->dmap);
#endif /* __FreeBSD__ */
}

static int pvscsi_allocate_rings(struct pvscsi_adapter *adapter)
{
	adapter->rings_state = pci_alloc_consistent(adapter->dev, PAGE_SIZE,
						    &adapter->ringStatePA);
	if (!adapter->rings_state)
		return -ENOMEM;

	adapter->req_pages = min(PVSCSI_MAX_NUM_PAGES_REQ_RING,
				 pvscsi_ring_pages);
	adapter->req_depth = adapter->req_pages
					* PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE;
	adapter->req_ring = pci_alloc_consistent(adapter->dev,
						 adapter->req_pages * PAGE_SIZE,
						 &adapter->reqRingPA);
	if (!adapter->req_ring)
		return -ENOMEM;

	adapter->cmp_pages = min(PVSCSI_MAX_NUM_PAGES_CMP_RING,
				 pvscsi_ring_pages);
	adapter->cmp_ring = pci_alloc_consistent(adapter->dev,
						 adapter->cmp_pages * PAGE_SIZE,
						 &adapter->cmpRingPA);
	if (!adapter->cmp_ring)
		return -ENOMEM;

#ifndef __FreeBSD__
	BUG_ON(adapter->ringStatePA & ~PAGE_MASK);
	BUG_ON(adapter->reqRingPA   & ~PAGE_MASK);
	BUG_ON(adapter->cmpRingPA   & ~PAGE_MASK);
#else
	BUG_ON(adapter->ringStatePA & PAGE_MASK);
	BUG_ON(adapter->reqRingPA   & PAGE_MASK);
	BUG_ON(adapter->cmpRingPA   & PAGE_MASK);
#endif /* __FreeBSD__ */

	if (!adapter->use_msg)
		return 0;

	adapter->msg_pages = min(PVSCSI_MAX_NUM_PAGES_MSG_RING,
				 pvscsi_msg_ring_pages);
	adapter->msg_ring = pci_alloc_consistent(adapter->dev,
						 adapter->msg_pages * PAGE_SIZE,
						 &adapter->msgRingPA);
	if (!adapter->msg_ring)
		return -ENOMEM;
#ifndef __FreeBSD__
	BUG_ON(adapter->msgRingPA & ~PAGE_MASK);
#else
	BUG_ON(adapter->msgRingPA & PAGE_MASK);
#endif /* __FreeBSD__ */

	return 0;
}

static void pvscsi_setup_all_rings(const struct pvscsi_adapter *adapter)
{
	struct PVSCSICmdDescSetupRings cmd = { 0 };
	dma_addr_t base;
	unsigned i;

	cmd.ringsStatePPN   = adapter->ringStatePA >> PAGE_SHIFT;
	cmd.reqRingNumPages = adapter->req_pages;
	cmd.cmpRingNumPages = adapter->cmp_pages;

	base = adapter->reqRingPA;
	for (i = 0; i < adapter->req_pages; i++) {
		cmd.reqRingPPNs[i] = base >> PAGE_SHIFT;
		base += PAGE_SIZE;
	}

	base = adapter->cmpRingPA;
	for (i = 0; i < adapter->cmp_pages; i++) {
		cmd.cmpRingPPNs[i] = base >> PAGE_SHIFT;
		base += PAGE_SIZE;
	}

	memset(adapter->rings_state, 0, PAGE_SIZE);
	memset(adapter->req_ring, 0, adapter->req_pages * PAGE_SIZE);
	memset(adapter->cmp_ring, 0, adapter->cmp_pages * PAGE_SIZE);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_SETUP_RINGS,
			      &cmd, sizeof cmd);

	if (adapter->use_msg) {
		struct PVSCSICmdDescSetupMsgRing cmd_msg = { 0 };

		cmd_msg.numPages = adapter->msg_pages;

		base = adapter->msgRingPA;
		for (i = 0; i < adapter->msg_pages; i++) {
			cmd_msg.ringPPNs[i] = base >> PAGE_SHIFT;
			base += PAGE_SIZE;
		}
		memset(adapter->msg_ring, 0, adapter->msg_pages * PAGE_SIZE);

		pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_SETUP_MSG_RING,
				      &cmd_msg, sizeof cmd_msg);
	}
}

/*
 * Pull a completion descriptor off and pass the completion back
 * to the SCSI mid layer.
 */
void pvscsi_complete_request(struct pvscsi_adapter *adapter,
				    const struct PVSCSIRingCmpDesc *e)
{
	struct pvscsi_ctx *ctx;
#ifndef __FreeBSD__
	struct scsi_cmnd *cmd;
#else /* __FreeBSD__ */
	bool toed = false;
	struct ccb_scsiio *cmd;
	device_t device = pvscsi_dev(adapter);
#endif /* __FreeBSD__ */
	u32 btstat = e->hostStatus;
	u32 sdstat = e->scsiStatus;
	u64 edataLen = e->dataLen;

	mtx_assert(&adapter->pvs_camlock, MA_OWNED);
	ctx = pvscsi_get_context(adapter, e->context);
	cmd = ctx->cmd;
#ifdef __FreeBSD__
	callout_stop(&ctx->calloutx); /* disables ABORT or SCSI IO callout */
	toed = ctx->toed;
	if (toed) {
		device_printf(device, "ccb:%p marked for timeout returned with,"
			"ctx:%p, h:%u s:%u\n", cmd, ctx, btstat, sdstat);
	}
#endif /* __FreeBSD__ */
	pvscsi_unmap_buffers(adapter, ctx);
	pvscsi_release_context(adapter, ctx);

#ifndef __FreeBSD__
	cmd->result = 0;
#endif /* __FreeBSD__ */

	if (sdstat != SAM_STAT_GOOD &&
	    (btstat == BTSTAT_SUCCESS ||
	     btstat == BTSTAT_LINKED_COMMAND_COMPLETED ||
	     btstat == BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG)) {
#ifndef __FreeBSD__
		if (sdstat == SAM_STAT_COMMAND_TERMINATED)
			cmd->result = (DID_RESET << 16);
		else {
			cmd->result = (DID_OK << 16) | sdstat;
			if (sdstat == SAM_STAT_CHECK_CONDITION &&
			    cmd->sense_buffer)
				cmd->result |= (DRIVER_SENSE << 24);
		}
#else /* __FreeBSD__ */
		cmd->scsi_status = sdstat;
		if (sdstat == SAM_STAT_COMMAND_TERMINATED)
			cmd->ccb_h.status = CAM_SCSI_BUS_RESET;
		else if (sdstat == SAM_STAT_CHECK_CONDITION)
			cmd->ccb_h.status = CAM_SCSI_STATUS_ERROR |
			    CAM_AUTOSNS_VALID;
		else
			cmd->ccb_h.status = CAM_SCSI_STATUS_ERROR;
#endif /* __FreeBSD__ */
	} else
		switch (btstat) {
		case BTSTAT_SUCCESS:
		case BTSTAT_LINKED_COMMAND_COMPLETED:
		case BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG:
			/* If everything went fine, let's move on..  */
#ifndef __FreeBSD__
			cmd->result = (DID_OK << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_REQ_CMP;
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_DATARUN:
		case BTSTAT_DATA_UNDERRUN:
#ifndef __FreeBSD__
			/* Report residual data in underruns */
			scsi_set_resid(cmd, scsi_bufflen(cmd) - e->dataLen);
			cmd->result = (DID_ERROR << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_DATA_RUN_ERR;
			cmd->resid = cmd->dxfer_len - edataLen;
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_SELTIMEO:
			/* Our emulation returns this for non-connected devs */
#ifndef __FreeBSD__
			cmd->result = (DID_BAD_TARGET << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_SEL_TIMEOUT;
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_LUNMISMATCH:
		case BTSTAT_TAGREJECT:
		case BTSTAT_BADMSG:
#ifndef __FreeBSD__
			cmd->result = (DRIVER_INVALID << 24);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_LUN_INVALID;
			break;
#endif /* __FreeBSD__ */
			/* fall through */

		case BTSTAT_HAHARDWARE:
		case BTSTAT_INVPHASE:
		case BTSTAT_HATIMEOUT:
		case BTSTAT_NORESPONSE:
		case BTSTAT_DISCONNECT:
		case BTSTAT_HASOFTWARE:
		case BTSTAT_BUSFREE:
		case BTSTAT_SENSFAILED:
#ifndef __FreeBSD__
			cmd->result |= (DID_ERROR << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_REQ_CMP_ERR;
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_SENTRST:
		case BTSTAT_RECVRST:
		case BTSTAT_BUSRESET:
#ifndef __FreeBSD__
			cmd->result = (DID_RESET << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_SCSI_BUS_RESET;
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_ABORTQUEUE:
#ifndef __FreeBSD__
			/*
			 * Linux seems to do better with DID_BUS_BUSY instead of
			 * DID_ABORT.
			 */
			cmd->result = (DID_BUS_BUSY << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			device_printf(device, "Command %s\n", toed ?
							"timedout" : "aborted");
			if(toed) {
				cmd->ccb_h.status = CAM_CMD_TIMEOUT;
			} else {
				cmd->ccb_h.status = CAM_REQ_ABORTED;
			}
#endif /* __FreeBSD__ */
			break;

		case BTSTAT_SCSIPARITY:
#ifndef __FreeBSD__
			cmd->result = (DID_PARITY << 16);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_UNCOR_PARITY;
#endif /* __FreeBSD__ */
			break;

		default:
#ifndef __FreeBSD__
			cmd->result = (DID_ERROR << 16);
			LOG(0, "Unknown completion status: 0x%x\n", btstat);
#else /* __FreeBSD__ */
			cmd->scsi_status = sdstat;
			cmd->ccb_h.status = CAM_REQ_CMP_ERR;
			device_printf(device, "Unknown completion status: "
							"0x%x\n", btstat);
#endif /* __FreeBSD__ */
	}

#ifndef __FreeBSD__
	LOG(3, "cmd=%p %x ctx=%p result=0x%x status=0x%x,%x\n",
		cmd, cmd->cmnd[0], ctx, cmd->result, btstat, sdstat);

	cmd->scsi_done(cmd);
#else /* __FreeBSD__ */
	xpt_done((union ccb *)cmd);
#endif /* __FreeBSD__ */
}

/*
 * barrier usage : Since the PVSCSI device is emulated, there could be cases
 * where we may want to serialize some accesses between the driver and the
 * emulation layer. We use compiler barriers instead of the more expensive
 * memory barriers because PVSCSI is only supported on X86 which has strong
 * memory access ordering.
 */
static void pvscsi_process_completion_ring(struct pvscsi_adapter *adapter)
{
	struct PVSCSIRingsState *s = adapter->rings_state;
	struct PVSCSIRingCmpDesc *ring = adapter->cmp_ring;
	u32 cmp_entries = s->cmpNumEntriesLog2;

	while (s->cmpConsIdx != s->cmpProdIdx) {
		struct PVSCSIRingCmpDesc *e = ring + (s->cmpConsIdx &
						      MASK(cmp_entries));
		/*
		 * This barrier() ensures that *e is not dereferenced while
		 * the device emulation still writes data into the slot.
		 * Since the device emulation advances s->cmpProdIdx only after
		 * updating the slot we want to check it first.
		 */
		barrier();
		pvscsi_complete_request(adapter, e);
		/*
		 * This barrier() ensures that compiler doesn't reorder write
		 * to s->cmpConsIdx before the read of (*e) inside
		 * pvscsi_complete_request. Otherwise, device emulation may
		 * overwrite *e before we had a chance to read it.
		 */
		barrier();
		s->cmpConsIdx++;
	}
}

/*
 * Translate a Linux SCSI request into a request ring entry.
 */
static int pvscsi_queue_ring(struct pvscsi_adapter *adapter,
			     struct pvscsi_ctx *ctx, struct scsi_cmnd *cmd)
{
	struct PVSCSIRingsState *s;
	struct PVSCSIRingReqDesc *e;
	struct scsi_device *sdev;
	u32 req_entries;
#ifdef __FreeBSD__
	struct ccb_scsiio *csio = cmd->qsc_csio;
#endif /* __FreeBSD__ */

	s = adapter->rings_state;
	sdev = cmd->device;
	req_entries = s->reqNumEntriesLog2;

	/*
	 * If this condition holds, we might have room on the request ring, but
	 * we might not have room on the completion ring for the response.
	 * However, we have already ruled out this possibility - we would not
	 * have successfully allocated a context if it were true, since we only
	 * have one context per request entry.  Check for it anyway, since it
	 * would be a serious bug.
	 */
	if (s->reqProdIdx - s->cmpConsIdx >= 1 << req_entries) {
#ifndef __FreeBSD__
		printk(KERN_ERR "pvscsi: ring full: reqProdIdx=%d cmpConsIdx=%d\n",
#else
		device_printf(pvscsi_dev(adapter), "Error, ring full: reqProdIdx=%d cmpConsIdx=%d\n",
#endif /* __FreeBSD__ */
			s->reqProdIdx, s->cmpConsIdx);
#ifndef __FreeBSD__
		return -1;
#else
		return CAM_RESRC_UNAVAIL;
#endif /* __FreeBSD__ */
	}

	e = adapter->req_ring + (s->reqProdIdx & MASK(req_entries));

	e->bus    = sdev->channel;
	e->target = sdev->id;
	memset(e->lun, 0, sizeof e->lun);
#ifndef  __FreeBSD__
	e->lun[1] = sdev->lun;
#endif /* __FreeBSD__ */

	if (cmd->sense_buffer) {
		ctx->sensePA = pci_map_single(adapter->dev, cmd->sense_buffer,
					      SCSI_SENSE_BUFFERSIZE,
					      PCI_DMA_FROMDEVICE);
		e->senseAddr = ctx->sensePA;
		e->senseLen = SCSI_SENSE_BUFFERSIZE;
	} else {
		e->senseLen  = 0;
		e->senseAddr = 0;
	}
	e->cdbLen   = cmd->cmd_len;
	e->vcpuHint = smp_processor_id();
	memcpy(e->cdb, cmd->cmnd, e->cdbLen);

	e->tag = SIMPLE_QUEUE_TAG;
	if (sdev->tagged_supported &&
	    (cmd->tag == HEAD_OF_QUEUE_TAG ||
	     cmd->tag == ORDERED_QUEUE_TAG))
		e->tag = cmd->tag;

	if (cmd->sc_data_direction == DMA_FROM_DEVICE)
		e->flags = PVSCSI_FLAG_CMD_DIR_TOHOST;
	else if (cmd->sc_data_direction == DMA_TO_DEVICE)
		e->flags = PVSCSI_FLAG_CMD_DIR_TODEVICE;
	else if (cmd->sc_data_direction == DMA_NONE)
		e->flags = PVSCSI_FLAG_CMD_DIR_NONE;
	else
		e->flags = 0;

	pvscsi_map_buffers(adapter, ctx, cmd, e);

	e->context = pvscsi_map_context(adapter, ctx);

	barrier();

	s->reqProdIdx++;

	return 0;
}

static inline void
PRINT_RINGSTATE(pvscsinst_t *adapter)
{
	struct PVSCSIRingsState *s __unused;

	s = adapter->rings_state;
	LOG(0,
		      "req> %d %d %d cmp> %d %d %d msg> %d %d %d\n",
		      s->reqProdIdx,
		      s->reqConsIdx,
		      s->reqNumEntriesLog2,
		      s->cmpProdIdx,
		      s->cmpConsIdx,
		      s->cmpNumEntriesLog2,
		      s->msgProdIdx,
		      s->msgConsIdx,
		      s->msgNumEntriesLog2);
}


#ifdef __FreeBSD__
static void
pvscsi_abort_timeout(void *data)
{
	struct pvscsi_ctx *ctx;
	pvscsinst_t *adapter;
	target_id_t tid;
	struct ccb_scsiio *cmd;
	pvscsitarg_t *targ;

	ctx = (struct pvscsi_ctx *)data;
	adapter = ctx->adapter;
	cmd = ctx->cmd;

	if (!cmd) {
		device_printf(pvscsi_dev(adapter),
			"abort TIMEOUT ctx>%p with NULL cmd\n", ctx);
		return;
	}

	tid = cmd->ccb_h.target_id;

	targ = adapter->pvs_tarrg + tid;
	targ->pvt_ntrs++;

	pvscsi_process_request_ring(adapter);
	ll_device_reset(adapter, tid);
	pvscsi_process_completion_ring(adapter);
}


static void
pvscsi_scsiio_timeout(void *data)
{
	struct pvscsi_ctx *ctx;
	pvscsinst_t *adapter;
	pvscsitarg_t *targ;
	struct timeval tv;
	struct ccb_scsiio *cmd;

	ctx = (struct pvscsi_ctx *)data;
	adapter = ctx->adapter;
	cmd = ctx->cmd;

	mtx_assert(&adapter->pvs_camlock, MA_OWNED);

	if (!cmd) {
		device_printf(pvscsi_dev(adapter),
			"SCSI IO TIMEOUT ctx>%p with NULL cmd\n", ctx);
		return;
	}

	pvscsi_process_completion_ring(adapter);
	if (ctx->cmd != cmd) {
		device_printf(pvscsi_dev(adapter), "ctx>%p almost timed-out\n",
									ctx);
		return;
	}

	getmicrotime(&tv);
	device_printf(pvscsi_dev(adapter), "SCSI IO TIMEOUT ctx>%p ccb>%p at "
		"%ld.%06ld\n", ctx, ctx->cmd, tv.tv_sec, tv.tv_usec);

	/* Update the targ_t from the targ array with the TO info */
	targ = adapter->pvs_tarrg + ctx->cmd->ccb_h.target_id;
	targ->pvt_ntos++;
	ctx->toed = true;

	/* Kick off a task abort and process completion once more */
	pvscsi_abort_cmd(adapter, ctx, cmd->ccb_h.target_id);
	pvscsi_process_completion_ring(adapter);

	/* If the cmd has not been aborted start a timer for the abort */
	if (ctx->cmd == cmd) {
		callout_reset(&ctx->calloutx, PVSCSI_ABORT_TIMEOUT * hz,
						pvscsi_abort_timeout, ctx);
	}
}
#endif /* __FreeBSD__ */

static int pvscsi_queue_locked(struct scsi_cmnd *cmd,
			       void (*done)(struct scsi_cmnd *))
{
#ifndef __FreeBSD__
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
#else
	struct pvscsi_adapter *adapter = cmd->adapter;
	int timeout = (cmd->qsc_csio->ccb_h.timeout * hz)/1000;
#endif /* __FreeBSD__ */
	struct pvscsi_ctx *ctx;
#ifndef __FreeBSD__
	unsigned long flags;

	spin_lock_irqsave(&adapter->hw_lock, flags);
#endif /* __FreeBSD__ */

	ctx = pvscsi_acquire_context(adapter, cmd);
	if (!ctx || pvscsi_queue_ring(adapter, ctx, cmd) != 0) {
		if (ctx)
			pvscsi_release_context(adapter, ctx);
#ifndef __FreeBSD__
		spin_unlock_irqrestore(&adapter->hw_lock, flags);
#endif /* __FreeBSD__ */
		return SCSI_MLQUEUE_HOST_BUSY;
	}

#ifndef __FreeBSD__
	cmd->scsi_done = done;

	LOG(3, "queued cmd %p, ctx %p, op=%x\n", cmd, ctx, cmd->cmnd[0]);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);
#else /* __FreeBSD__ */
	ctx->toed = false;
	if (adapter->pvs_timeout_one_comm_targ ==
					cmd->qsc_csio->ccb_h.target_id) {
		timeout = 1;
		adapter->pvs_timeout_one_comm_targ = -1;
	}
	callout_reset(&ctx->calloutx, timeout,
		adapter->pvs_reset_target_on_timeout ?
			pvscsi_abort_timeout: pvscsi_scsiio_timeout,
		ctx);
	cmd->qsc_csio->ccb_h.status |= CAM_SIM_QUEUED;

#endif /* __FreeBSD__ */

	pvscsi_kick_io(adapter, cmd->cmnd[0]);

	return 0;
}

#ifndef __FreeBSD__
#if defined(DEF_SCSI_QCMD) || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
static int pvscsi_queue(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	unsigned long flags;
	int retval;

	spin_lock_irqsave(host->host_lock, flags);

	scsi_cmd_get_serial(host, cmd);
	retval = pvscsi_queue_locked(cmd, cmd->scsi_done);

	spin_unlock_irqrestore(host->host_lock, flags);

	return retval;
}
#else
#define pvscsi_queue	pvscsi_queue_locked
#endif

static int pvscsi_abort(struct scsi_cmnd *cmd)
{
	struct pvscsi_adapter *adapter = HOST_ADAPTER(cmd->device->host);
	struct pvscsi_ctx *ctx;
	unsigned long flags;

	printk(KERN_DEBUG "pvscsi: task abort on host %u, %p\n",
	       adapter->host->host_no, cmd);
#else /* __FreeBSD__ */
static int pvscsi_abort(pvscsinst_t *adapter, struct ccb_scsiio *cmd)
{
	struct pvscsi_ctx *ctx;
	int irq = 0;
#endif /* __FreeBSD__ */

	spin_lock_irqsave(&adapter->hw_lock, flags);

	/*
	 * Poll the completion ring first - we might be trying to abort
	 * a command that is waiting to be dispatched in the completion ring.
	 */
	pvscsi_process_completion_ring(adapter);

	/*
	 * If there is no context for the command, it either already succeeded
	 * or else was never properly issued.  Not our problem.
	 */
	ctx = pvscsi_find_context(adapter, cmd);
	if (!ctx) {
#ifndef __FreeBSD__
		LOG(1, "Failed to abort cmd %p\n", cmd);
#else /* __FreeBSD__ */
		device_t device  = pvscsi_dev(adapter);
		device_printf(device, "Failed to abort cmd %p\n", cmd);
#endif /* __FreeBSD__ */
		goto out;
	}

#ifndef __FreeBSD__
	pvscsi_abort_cmd(adapter, ctx);
#else /* __FreeBSD__ */
	pvscsi_abort_cmd(adapter, ctx, cmd->ccb_h.target_id);
#endif /* __FreeBSD__ */

	pvscsi_process_completion_ring(adapter);

out:
	spin_unlock_irqrestore(&adapter->hw_lock, flags);
	return SUCCESS;
}

#ifndef __FreeBSD__
/*
 * Abort all outstanding requests.  This is only safe to use if the completion
 * ring will never be walked again or the device has been reset, because it
 * destroys the 1-1 mapping between context field passed to emulation and our
 * request structure.
 */
static void pvscsi_reset_all(struct pvscsi_adapter *adapter)
{
	unsigned i;

	for (i = 0; i < adapter->req_depth; i++) {
		struct pvscsi_ctx *ctx = &adapter->cmd_map[i];
		struct scsi_cmnd *cmd = ctx->cmd;
		if (cmd) {
			printk(KERN_ERR "pvscsi: forced reset on cmd %p\n", cmd);
			pvscsi_unmap_buffers(adapter, ctx);
			pvscsi_release_context(adapter, ctx);
			cmd->result = (DID_RESET << 16);
			cmd->scsi_done(cmd);
		}
	}
}

static int pvscsi_host_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;
	char use_msg;

	printk(KERN_INFO "pvscsi: host reset on host %u\n", host->host_no);

	spin_lock_irqsave(&adapter->hw_lock, flags);

	use_msg = adapter->use_msg;

	if (use_msg) {
		adapter->use_msg = 0;
		spin_unlock_irqrestore(&adapter->hw_lock, flags);

		/*
		 * Now that we know that the ISR won't add more work on the
		 * workqueue we can safely flush any outstanding work.
		 */
		flush_workqueue(adapter->workqueue);
		spin_lock_irqsave(&adapter->hw_lock, flags);
	}

	/*
	 * We're going to tear down the entire ring structure and set it back
	 * up, so stalling new requests until all completions are flushed and
	 * the rings are back in place.
	 */

	pvscsi_process_request_ring(adapter);

	ll_adapter_reset(adapter);

	/*
	 * Now process any completions.  Note we do this AFTER adapter reset,
	 * which is strange, but stops races where completions get posted
	 * between processing the ring and issuing the reset.  The backend will
	 * not touch the ring memory after reset, so the immediately pre-reset
	 * completion ring state is still valid.
	 */
	pvscsi_process_completion_ring(adapter);

	pvscsi_reset_all(adapter);
	adapter->use_msg = use_msg;
	pvscsi_setup_all_rings(adapter);
	pvscsi_unmask_intr(adapter);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}
#endif /* __FreeBSD__ */

#ifndef __FreeBSD__
static int pvscsi_bus_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;

	printk(KERN_INFO "pvscsi: bus reset on host %u\n", host->host_no);
#else /* __FreeBSD__ */
static int pvscsi_bus_reset(pvscsinst_t *adapter)
{
	int irq = 0; /* So that we do not lock */
#endif /* __FreeBSD__ */

	/*
	 * We don't want to queue new requests for this bus after
	 * flushing all pending requests to emulation, since new
	 * requests could then sneak in during this bus reset phase,
	 * so take the lock now.
	 */
	spin_lock_irqsave(&adapter->hw_lock, flags);

	pvscsi_process_request_ring(adapter);
	ll_bus_reset(adapter);
	pvscsi_process_completion_ring(adapter);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}

#ifndef __FreeBSD__
static int pvscsi_device_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;

	printk(KERN_INFO "pvscsi: device reset on scsi%u:%u\n",
	       host->host_no, cmd->device->id);
#else /* __FreeBSD__ */
static int pvscsi_device_reset(pvscsinst_t *adapter, target_id_t trg)
{
	int irq = 0;
#endif /* __FreeBSD__ */

	/*
	 * We don't want to queue new requests for this device after flushing
	 * all pending requests to emulation, since new requests could then
	 * sneak in during this device reset phase, so take the lock now.
	 */
	spin_lock_irqsave(&adapter->hw_lock, flags);

	pvscsi_process_request_ring(adapter);
#ifndef __FreeBSD__
	ll_device_reset(adapter, cmd->device->id);
#else /* __FreeBSD__ */
	ll_device_reset(adapter, trg);
#endif /* __FreeBSD__ */
	pvscsi_process_completion_ring(adapter);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}

#ifndef __FreeBSD__
static struct scsi_host_template pvscsi_template;

static const char *pvscsi_info(struct Scsi_Host *host)
{
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	static char buf[512];

	sprintf(buf, "VMware PVSCSI storage adapter rev %d, req/cmp/msg rings: "
		"%u/%u/%u pages, cmd_per_lun=%u", adapter->rev,
		adapter->req_pages, adapter->cmp_pages, adapter->msg_pages,
		pvscsi_template.cmd_per_lun);

	return buf;
}

static struct scsi_host_template pvscsi_template = {
	.module				= THIS_MODULE,
	.name				= "VMware PVSCSI Host Adapter",
	.proc_name			= "pvscsi",
	.info				= pvscsi_info,
	.queuecommand			= pvscsi_queue,
	.this_id			= -1,
	.sg_tablesize			= PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT,
	.dma_boundary			= UINT_MAX,
	.max_sectors			= 0xffff,
	.use_clustering			= ENABLE_CLUSTERING,
	.eh_abort_handler		= pvscsi_abort,
	.eh_device_reset_handler	= pvscsi_device_reset,
	.eh_bus_reset_handler		= pvscsi_bus_reset,
	.eh_host_reset_handler		= pvscsi_host_reset,
};
#else /* __FreeBSD__ */
static void
pvscsi_device_lost_or_found(pvscsinst_t *adapter, u32 bus, u32 trg, u8 lun,
    bool lost)
{
	struct cam_path *path;
	struct ccb_getdev ccb = { };
	cam_status err;

	if (lun) {
		device_printf(pvscsi_dev(adapter), "hotplug/removal event for "
			"a non-zero LUN '%d:%d'.  Ignoring event\n", trg, lun);
		return;
	}

	PVSCSILCK;
	err = xpt_create_path(&path, NULL, cam_sim_path(adapter->pvs_camsim),
					trg, lun);
	if (err != CAM_REQ_CMP) {
		device_printf(pvscsi_dev(adapter), "hotplug/removal event "
			"failed to allocate path. '%d:%d'  Ignoring event\n",
			trg, lun);
		PVSCSIULCK;
		return;
	}

	xpt_setup_ccb(&(ccb.ccb_h), path, /*priority*/5);
	if (lost) {
		device_printf(pvscsi_dev(adapter), "removing targ:lun %d:%d\n",
								trg, lun);
		xpt_async(AC_LOST_DEVICE, path, NULL);
	} else {
		device_printf(pvscsi_dev(adapter), "adding targ:lun %d:%d\n",
								trg, lun);
		xpt_async(AC_INQ_CHANGED, path, &ccb);
		xpt_async(AC_FOUND_DEVICE, path, &ccb);
	}
	xpt_free_path(path);

	PVSCSIULCK;
}
#endif /* __FreeBSD__ */

#ifndef __FreeBSD__
static void pvscsi_process_msg(const struct pvscsi_adapter *adapter,
			       const struct PVSCSIRingMsgDesc *e)
#else /* __FreeBSD__ */
static void pvscsi_process_msg(struct pvscsi_adapter *adapter,
			       struct PVSCSIRingMsgDesc *e)
#endif /* __FreeBSD__ */
{
#ifndef __FreeBSD__
	struct PVSCSIRingsState *s = adapter->rings_state;
	struct Scsi_Host *host = adapter->host;
	struct scsi_device *sdev;

	printk(KERN_INFO "pvscsi: msg type: 0x%x - MSG RING: %u/%u (%u) \n",
	       e->type, s->msgProdIdx, s->msgConsIdx, s->msgNumEntriesLog2);

#endif /* __FreeBSD__ */
	ASSERT_ON_COMPILE(PVSCSI_MSG_LAST == 2);

	if (e->type == PVSCSI_MSG_DEV_ADDED) {
		struct PVSCSIMsgDescDevStatusChanged *desc;
		desc = (struct PVSCSIMsgDescDevStatusChanged *)e;

#ifndef __FreeBSD__
		printk(KERN_INFO "pvscsi: msg: device added at scsi%u:%u:%u\n",
		       desc->bus, desc->target, desc->lun[1]);

		if (!scsi_host_get(host))
			return;

		sdev = scsi_device_lookup(host, desc->bus, desc->target,
					  desc->lun[1]);
		if (sdev) {
			printk(KERN_INFO "pvscsi: device already exists\n");
			scsi_device_put(sdev);
		} else
			scsi_add_device(adapter->host, desc->bus,
					desc->target, desc->lun[1]);

		scsi_host_put(host);
#else /* __FreeBSD__ */
	pvscsi_device_lost_or_found(adapter, desc->bus, desc->target,
	    desc->lun[1], false);
#endif /* __FreeBSD__ */
	} else if (e->type == PVSCSI_MSG_DEV_REMOVED) {
		struct PVSCSIMsgDescDevStatusChanged *desc;
		desc = (struct PVSCSIMsgDescDevStatusChanged *)e;

#ifndef __FreeBSD__
		printk(KERN_INFO "pvscsi: msg: device removed at scsi%u:%u:%u\n",
		       desc->bus, desc->target, desc->lun[1]);

		if (!scsi_host_get(host))
			return;

		sdev = scsi_device_lookup(host, desc->bus, desc->target,
					  desc->lun[1]);
		if (sdev) {
			scsi_remove_device(sdev);
			scsi_device_put(sdev);
		} else
			printk(KERN_INFO "pvscsi: failed to lookup scsi%u:%u:%u\n",
			       desc->bus, desc->target, desc->lun[1]);

		scsi_host_put(host);
#else /* __FreeBSD__ */
	pvscsi_device_lost_or_found(adapter, desc->bus, desc->target,
							desc->lun[1], true);
#endif /* __FreeBSD__ */
	}
}

static int pvscsi_msg_pending(const struct pvscsi_adapter *adapter)
{
	struct PVSCSIRingsState *s = adapter->rings_state;

	return s->msgProdIdx != s->msgConsIdx;
}

#ifndef __FreeBSD__
static void pvscsi_process_msg_ring(const struct pvscsi_adapter *adapter)
#else
static void pvscsi_process_msg_ring(struct pvscsi_adapter *adapter)
#endif /* __FreeBSD__ */
{
	struct PVSCSIRingsState *s = adapter->rings_state;
	struct PVSCSIRingMsgDesc *ring = adapter->msg_ring;
	u32 msg_entries = s->msgNumEntriesLog2;

	while (pvscsi_msg_pending(adapter)) {
		struct PVSCSIRingMsgDesc *e = ring + (s->msgConsIdx &
						      MASK(msg_entries));

		barrier();
		pvscsi_process_msg(adapter, e);
		barrier();
		s->msgConsIdx++;
	}
}

#ifndef __FreeBSD__
static void pvscsi_msg_workqueue_handler(compat_work_arg data)
#else
static void pvscsi_msg_workqueue_handler(struct work_struct *data)
#endif /* __FreeBSD__ */
{
	struct pvscsi_adapter *adapter;

	adapter = COMPAT_WORK_GET_DATA(data, struct pvscsi_adapter, work);

	pvscsi_process_msg_ring(adapter);
}

static int pvscsi_setup_msg_workqueue(struct pvscsi_adapter *adapter)
{
	char name[32]; /* XXX: magic number */

	if (!pvscsi_use_msg)
		return 0;

	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_COMMAND,
			 PVSCSI_CMD_SETUP_MSG_RING);

	if (pvscsi_reg_read(adapter, PVSCSI_REG_OFFSET_COMMAND_STATUS) == -1)
		return 0;

#ifndef __FreeBSD__
	snprintf(name, sizeof name, "pvscsi_wq_%u", adapter->host->host_no);
#else
	snprintf(name, sizeof name, "pvscsi_wq_%u",
					device_get_unit(adapter->pvs_dev));
#endif /* __FreeBSD__ */

	adapter->workqueue = create_singlethread_workqueue(name);
	if (!adapter->workqueue) {
		printk(KERN_ERR "pvscsi: failed to create work queue\n");
		return 0;
	}
	COMPAT_INIT_WORK(&adapter->work, pvscsi_msg_workqueue_handler, adapter);
	return 1;
}

#ifndef __FreeBSD__
static irqreturn_t pvscsi_isr COMPAT_IRQ_HANDLER_ARGS(irq, devp)
#else
static irqreturn_t pvscsi_isr(int irq, void *devp)
#endif /* __FreeBSD__ */
{
	struct pvscsi_adapter *adapter = devp;
	int handled;

	if (adapter->use_msi || adapter->use_msix)
		handled = TRUE;
	else {
		/*
		 * N.B INTx interrupts will not work with xpt_polled_action()
		 * The symptom will be a swatchdog involving dashutdown()
		 */
		u32 val = pvscsi_read_intr_status(adapter);
		handled = (val & PVSCSI_INTR_ALL_SUPPORTED) != 0;
		if (handled)
			pvscsi_write_intr_status(devp, val);
	}

	if (handled) {
#ifndef __FreeBSD__
		unsigned long flags;
#endif /* __FreeBSD__ */

		spin_lock_irqsave(&adapter->hw_lock, flags);

		pvscsi_process_completion_ring(adapter);
		if (adapter->use_msg && pvscsi_msg_pending(adapter))
			queue_work(adapter->workqueue, &adapter->work);

		spin_unlock_irqrestore(&adapter->hw_lock, flags);
	}

	return IRQ_RETVAL(handled);
}

#ifdef __FreeBSD__
static void
pvscsi_isr_freebsd(void *adapter)
{
	(void)pvscsi_isr(1, adapter);
}

static void
pvscsi_poll(struct cam_sim *sim)
{
	/* N.B. This mechanism will not work with INTx interrupts */
	pvscsinst_t *adapter = (pvscsinst_t *)cam_sim_softc(sim);
	(void)pvscsi_isr(0, adapter);
}
#endif /* __FreeBSD__ */

static void pvscsi_free_sgls(const struct pvscsi_adapter *adapter)
{
	struct pvscsi_ctx *ctx = adapter->cmd_map;
	unsigned i;

	for (i = 0; i < adapter->req_depth; ++i, ++ctx)
		free_page((unsigned long)ctx->sgl);
}

#ifndef __FreeBSD__
static int pvscsi_setup_msix(const struct pvscsi_adapter *adapter,
			     unsigned int *irq)
{
#ifdef CONFIG_PCI_MSI
	struct msix_entry entry = { 0, PVSCSI_VECTOR_COMPLETION };
	int ret;

	ret = pci_enable_msix(adapter->dev, &entry, 1);
	if (ret)
		return ret;

	*irq = entry.vector;

	return 0;
#else
	return -1;
#endif
}
#else /* __FreeBSD__ */
static bool
pvscsi_setup_intr(pvscsinst_t *adapter, void *isr, bool msix)
{
	int rid = 0;
	struct resource *res;
	int err;
	device_t device = adapter->pvs_dev;

	if (msix) {
		int msix_vecs_needed = PVSCSI_NUM_MSIX;

		rid++; /* RID 1 in the interrupt space is for MSIX interrupts */

		if (pci_msix_count(adapter->pvs_dev) < PVSCSI_NUM_MSIX) {
			device_printf(device,
			    "pci_msix_count():%d < PVSCSI_NUM_MSIX\n",
			    pci_msix_count(adapter->pvs_dev));
			return false;
		}

		err = pci_alloc_msix(adapter->pvs_dev, &msix_vecs_needed);
		if (err != 0 || msix_vecs_needed < PVSCSI_NUM_MSIX) {
			device_printf(device,
			    "retval>%d, msix_vecs_needed>%d\n",
			    err, msix_vecs_needed);
			return false;
		}
	}

	res = bus_alloc_resource_any(adapter->pvs_dev, SYS_RES_IRQ, &rid,
		RF_SHAREABLE|RF_ACTIVE);
	if (res == NULL) {
		device_printf(device, "Couldn't allocate interrupt resource\n");
		if (msix)
			pci_release_msi(adapter->pvs_dev);
		return false;
	}

	err = bus_setup_intr(adapter->pvs_dev, res, INTR_MPSAFE|INTR_TYPE_CAM,
		NULL, isr, adapter, &adapter->pvs_intcookie);
	if (err != 0) {
		device_printf(device, "bus_setup_intr failed: %d\n", err);
		bus_release_resource(adapter->pvs_dev, SYS_RES_IRQ, rid, res);
		if (msix)
			pci_release_msi(adapter->pvs_dev);
		return false;
	}

	adapter->pvs_intmsix = msix;
	adapter->pvs_intres = res;
	adapter->pvs_intrid = rid;

	return true;
}
#endif /* __FreeBSD__ */

static void pvscsi_shutdown_intr(struct pvscsi_adapter *adapter)
{
#ifndef __FreeBSD__
	if (adapter->irq) {
		free_irq(adapter->irq, adapter);
		adapter->irq = 0;
	}
#ifdef CONFIG_PCI_MSI
	if (adapter->use_msi) {
		pci_disable_msi(adapter->dev);
		adapter->use_msi = 0;
	}

	if (adapter->use_msix) {
		pci_disable_msix(adapter->dev);
		adapter->use_msix = 0;
	}
#endif
#else /* __FreeBSD__ */
	/* Undo the interrupt isr registration */
	bus_teardown_intr(adapter->pvs_dev, adapter->pvs_intres,
							adapter->pvs_intcookie);
	bus_release_resource(adapter->pvs_dev, SYS_RES_IRQ,
				     adapter->pvs_intrid, adapter->pvs_intres);
	if (adapter->pvs_intmsix) pci_release_msi(adapter->pvs_dev);
#endif /* __FreeBSD__ */
}

static void pvscsi_release_resources(struct pvscsi_adapter *adapter)
{
	pvscsi_shutdown_intr(adapter);

	if (adapter->workqueue)
		destroy_workqueue(adapter->workqueue);

#ifndef __FreeBSD__
	if (adapter->mmioBase)
		pci_iounmap(adapter->dev, adapter->mmioBase);

	pci_release_regions(adapter->dev);
#endif /* __FreeBSD__ */

	if (adapter->cmd_map) {
#ifndef __FreeBSD__
		pvscsi_free_sgls(adapter);
		kfree(adapter->cmd_map);
#else
		KASSERT((adapter->cmd_map_size > 0), "adapter->");
		pvscsi_free_sgls(adapter);
		kfree(adapter->cmd_map, adapter->cmd_map_size);
#endif
	}

	if (adapter->rings_state)
		pci_free_consistent(adapter->dev, PAGE_SIZE,
				    adapter->rings_state, adapter->ringStatePA);

	if (adapter->req_ring)
		pci_free_consistent(adapter->dev,
				    adapter->req_pages * PAGE_SIZE,
				    adapter->req_ring, adapter->reqRingPA);

	if (adapter->cmp_ring)
		pci_free_consistent(adapter->dev,
				    adapter->cmp_pages * PAGE_SIZE,
				    adapter->cmp_ring, adapter->cmpRingPA);

	if (adapter->msg_ring)
		pci_free_consistent(adapter->dev,
				    adapter->msg_pages * PAGE_SIZE,
				    adapter->msg_ring, adapter->msgRingPA);

#ifdef __FreeBSD__
	/* Undo the memory-mapped register mapping */
	bus_release_resource(adapter->pvs_dev, SYS_RES_MEMORY,
			     adapter->pvs_mmrid, adapter->pvs_mmres);
#endif /* __FreeBSD__ */
}

/*
 * Allocate scatter gather lists.
 *
 * These are statically allocated.  Trying to be clever was not worth it.
 *
 * Dynamic allocation can fail, and we can't go deeep into the memory
 * allocator, since we're a SCSI driver, and trying too hard to allocate
 * memory might generate disk I/O.  We also don't want to fail disk I/O
 * in that case because we can't get an allocation - the I/O could be
 * trying to swap out data to free memory.  Since that is pathological,
 * just use a statically allocated scatter list.
 *
 */
static int pvscsi_allocate_sg(struct pvscsi_adapter *adapter)
{
	struct pvscsi_ctx *ctx;
	int i;

	ctx = adapter->cmd_map;
	ASSERT_ON_COMPILE(sizeof(struct pvscsi_sg_list) <= PAGE_SIZE);

	for (i = 0; i < adapter->req_depth; ++i, ++ctx) {
		ctx->sgl = (void *)__get_free_page(GFP_KERNEL);
		ctx->sglPA = 0;
#ifndef __FreeBSD__
		BUG_ON(ctx->sglPA & ~PAGE_MASK);
#else
		BUG_ON(ctx->sglPA & PAGE_MASK);
#endif /* __FreeBSD__ */
		if (!ctx->sgl) {
			for (; i >= 0; --i, --ctx) {
				free_page((unsigned long)ctx->sgl);
				ctx->sgl = NULL;
			}
			return -ENOMEM;
		}
	}

	return 0;
}

/*
 * Query the device, fetch the config info and return the
 * maximum number of targets on the adapter. In case of
 * failure due to any reason return default i.e. 16.
 */
static uint32 pvscsi_get_max_targets(struct pvscsi_adapter *adapter)
{
	PVSCSICmdDescConfigCmd cmd;
	PVSCSIConfigPageHeader *header;
	dma_addr_t	       configPagePA;
	void		       *config_page;
	uint32		       numPhys;

	ASSERT_ON_COMPILE(sizeof(PVSCSIConfigPageController) <= PAGE_SIZE);

	numPhys = 16;
	config_page = pci_alloc_consistent(adapter->dev, PAGE_SIZE,
				           &configPagePA);
	if (!config_page) {
		printk(KERN_INFO "pvscsi: failed to allocate memory for"
		       " config page\n");
		goto exit;
	}

#ifndef __FreeBSD__
	BUG_ON(configPagePA & ~PAGE_MASK);
#else
	BUG_ON(configPagePA & PAGE_MASK);
#endif /* __FreeBSD__ */

	/* Fetch config info from the device. */
	cmd.configPageAddress = QWORD(PVSCSI_CONFIG_CONTROLLER_ADDRESS, 0);
	cmd.configPageNum = PVSCSI_CONFIG_PAGE_CONTROLLER;
	cmd.cmpAddr = configPagePA;
	cmd._pad = 0;

	/*
	 * Mark the completion page header with error values. If the device
	 * completes the command successfully, it sets the status values to
	 * indicate success.
	 */
	header = config_page;
	memset(header, 0, sizeof *header);
	header->hostStatus = BTSTAT_INVPARAM;
	header->scsiStatus = SDSTAT_CHECK;

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_CONFIG, &cmd, sizeof cmd);

	if (header->hostStatus == BTSTAT_SUCCESS &&
	    header->scsiStatus == SDSTAT_GOOD) {
		PVSCSIConfigPageController *config;

		config = config_page;
		numPhys = config->numPhys;
	} else
		printk(KERN_INFO "pvscsi: PVSCSI_CMD_CONFIG failed."
		       " hostStatus = 0x%x, scsiStatus = 0x%x\n",
		       header->hostStatus, header->scsiStatus);

	pci_free_consistent(adapter->dev, PAGE_SIZE, config_page, configPagePA);
exit:
	return numPhys;
}

#ifndef __FreeBSD__
static int pvscsi_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	struct pvscsi_adapter *adapter;
	struct Scsi_Host *host;
	unsigned int i;
	int error;

	error = -ENODEV;

	if (pci_enable_device(pdev))
		return error;

	if (pdev->vendor != PCI_VENDOR_ID_VMWARE)
		goto out_disable_device;

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) == 0 &&
	    pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)) == 0) {
		printk(KERN_INFO "pvscsi: using 64bit dma\n");
	} else if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) == 0 &&
		   pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)) == 0) {
		printk(KERN_INFO "pvscsi: using 32bit dma\n");
	} else {
		printk(KERN_ERR "pvscsi: failed to set DMA mask\n");
		goto out_disable_device;
	}

	pvscsi_template.can_queue =
		min(PVSCSI_MAX_NUM_PAGES_REQ_RING, pvscsi_ring_pages) *
		PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE;
	pvscsi_template.cmd_per_lun =
		min(pvscsi_template.can_queue, pvscsi_cmd_per_lun);
	host = scsi_host_alloc(&pvscsi_template, sizeof(struct pvscsi_adapter));
	if (!host) {
		printk(KERN_ERR "pvscsi: failed to allocate host\n");
		goto out_disable_device;
	}

	adapter = HOST_ADAPTER(host);
	memset(adapter, 0, sizeof *adapter);
	adapter->dev  = pdev;
	adapter->host = host;

	spin_lock_init(&adapter->hw_lock);

	host->max_channel = 0;
	host->max_id      = 16;
	host->max_lun     = 1;
	host->max_cmd_len = 16;

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &adapter->rev);

	if (pci_request_regions(pdev, "pvscsi")) {
		printk(KERN_ERR "pvscsi: pci memory selection failed\n");
		goto out_free_host;
	}

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if ((pci_resource_flags(pdev, i) & PCI_BASE_ADDRESS_SPACE_IO))
			continue;

		if (pci_resource_len(pdev, i) <	PVSCSI_MEM_SPACE_SIZE)
			continue;

		break;
	}

	if (i == DEVICE_COUNT_RESOURCE) {
		printk(KERN_ERR "pvscsi: adapter has no suitable MMIO region\n");
		goto out_release_resources;
	}

	adapter->mmioBase = pci_iomap(pdev, i, PVSCSI_MEM_SPACE_SIZE);
	if (!adapter->mmioBase) {
		printk(KERN_ERR "pvscsi: can't iomap for BAR %d memsize %lu\n",
		       i, PVSCSI_MEM_SPACE_SIZE);
		goto out_release_resources;
	}

	pci_set_master(pdev);
	pci_set_drvdata(pdev, host);

	ll_adapter_reset(adapter);

	adapter->use_msg = pvscsi_setup_msg_workqueue(adapter);

	error = pvscsi_allocate_rings(adapter);
	if (error) {
		printk(KERN_ERR "pvscsi: unable to allocate ring memory\n");
		goto out_release_resources;
	}

	/*
	 * Ask the device for max number of targets.
	 */
	host->max_id = pvscsi_get_max_targets(adapter);
	printk(KERN_INFO "pvscsi: host->max_id: %u\n", host->max_id);

	/*
	 * From this point on we should reset the adapter if anything goes
	 * wrong.
	 */
	pvscsi_setup_all_rings(adapter);

	adapter->cmd_map = kmalloc(adapter->req_depth *
				   sizeof(struct pvscsi_ctx), GFP_KERNEL);
	if (!adapter->cmd_map) {
		printk(KERN_ERR "pvscsi: failed to allocate memory.\n");
		error = -ENOMEM;
		goto out_reset_adapter;
	}
	memset(adapter->cmd_map, 0,
	       adapter->req_depth * sizeof(struct pvscsi_ctx));

	INIT_LIST_HEAD(&adapter->cmd_pool);
	for (i = 0; i < adapter->req_depth; i++) {
		struct pvscsi_ctx *ctx = adapter->cmd_map + i;
		list_add(&ctx->list, &adapter->cmd_pool);
	}

	error = pvscsi_allocate_sg(adapter);
	if (error) {
		printk(KERN_ERR "pvscsi: unable to allocate s/g table\n");
		goto out_reset_adapter;
	}

	if (!pvscsi_disable_msix &&
	    pvscsi_setup_msix(adapter, &adapter->irq) == 0) {
		printk(KERN_INFO "pvscsi: using MSI-X\n");
		adapter->use_msix = 1;
	} else if (!pvscsi_disable_msi && pci_enable_msi(pdev) == 0) {
		printk(KERN_INFO "pvscsi: using MSI\n");
		adapter->use_msi = 1;
		adapter->irq = pdev->irq;
	} else {
		printk(KERN_INFO "pvscsi: using INTx\n");
		adapter->irq = pdev->irq;
	}

	error = request_irq(adapter->irq, pvscsi_isr, COMPAT_IRQF_SHARED,
			    "pvscsi", adapter);
	if (error) {
		printk(KERN_ERR "pvscsi: unable to request IRQ: %d\n", error);
		adapter->irq = 0;
		goto out_reset_adapter;
	}

	error = scsi_add_host(host, &pdev->dev);
	if (error) {
		printk(KERN_ERR "pvscsi: scsi_add_host failed: %d\n", error);
		goto out_reset_adapter;
	}

	printk(KERN_INFO "VMware PVSCSI rev %d on bus:%u slot:%u func:%u host #%u\n",
	       adapter->rev, pdev->bus->number, PCI_SLOT(pdev->devfn),
	       PCI_FUNC(pdev->devfn), host->host_no);

	pvscsi_unmask_intr(adapter);

	scsi_scan_host(host);

	return 0;

out_reset_adapter:
	ll_adapter_reset(adapter);
out_release_resources:
	pvscsi_release_resources(adapter);
out_free_host:
	scsi_host_put(host);
out_disable_device:
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);

	return error;
}

static void __pvscsi_shutdown(struct pvscsi_adapter *adapter)
{
	pvscsi_mask_intr(adapter);

	if (adapter->workqueue)
		flush_workqueue(adapter->workqueue);

	pvscsi_shutdown_intr(adapter);

	pvscsi_process_request_ring(adapter);
	pvscsi_process_completion_ring(adapter);
	ll_adapter_reset(adapter);
}

static void COMPAT_PCI_DECLARE_SHUTDOWN(pvscsi_shutdown, dev)
{
	struct Scsi_Host *host = pci_get_drvdata(COMPAT_PCI_TO_DEV(dev));
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);

	__pvscsi_shutdown(adapter);
}

static void pvscsi_remove(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);

	scsi_remove_host(host);

	__pvscsi_shutdown(adapter);
	pvscsi_release_resources(adapter);

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

static struct pci_driver pvscsi_pci_driver = {
	.name		= "pvscsi",
	.id_table	= pvscsi_pci_tbl,
	.probe		= pvscsi_probe,
	.remove		= pvscsi_remove,
	COMPAT_PCI_SHUTDOWN(pvscsi_shutdown)
};

static int __init pvscsi_init(void)
{
	printk(KERN_DEBUG "%s - version %s\n",
	       PVSCSI_LINUX_DRIVER_DESC, PVSCSI_DRIVER_VERSION_STRING);
	return pci_register_driver(&pvscsi_pci_driver);
}

static void __exit pvscsi_exit(void)
{
	pci_unregister_driver(&pvscsi_pci_driver);
}

module_init(pvscsi_init);
module_exit(pvscsi_exit);
#else /* __FreeBSD__ */

static void
pvscsi_action(struct cam_sim *psim, union ccb *pccb)
{
	pvscsinst_t * adapter = cam_sim_softc(psim);

	device_t device = pvscsi_dev(adapter);

	switch (pccb->ccb_h.func_code) {
		default: {
			/*
			device_printf(device, "%s(%d) invoked for '%u'\n",
					__FUNCTION__, pccb->ccb_h.func_code,
							pccb->ccb_h.target_id);
			 */
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		}

		case XPT_SCSI_IO: {
			struct ccb_scsiio *csio = &pccb->csio;

			LOG(0, "Command(0x%x) for %d:%d\n",
						csio->cdb_io.cdb_bytes[0],
						pccb->ccb_h.target_id,
						pccb->ccb_h.target_lun);

			struct scsi_cmnd command = { 0 }, *cmd = &command;
			struct scsi_device quasi_sdev = { 0 };

			cmd->qsc_csio = csio;
			cmd->device = &quasi_sdev;

			if (csio->ccb_h.target_lun) {
				pccb->ccb_h.status = CAM_LUN_INVALID;
				xpt_done(pccb);
				break;
			}

			cmd->cmd_len = csio->cdb_len;
			if (csio->ccb_h.flags & CAM_CDB_POINTER)
				cmd->cmnd = (void *)csio->cdb_io.cdb_ptr;
			else
				cmd->cmnd = (void *)&csio->cdb_io.cdb_bytes;

			KASSERT(!(csio->ccb_h.flags &
				(CAM_SENSE_PHYS|CAM_SENSE_PTR)), ("%x",
				csio->ccb_h.flags)); /* We expect a struct */
			cmd->sense_buffer = &csio->sense_data;

			#define CSIODIR (csio->ccb_h.flags & CAM_DIR_MASK)
			if (CSIODIR == CAM_DIR_IN)
				cmd->sc_data_direction = DMA_FROM_DEVICE;
			else if (CSIODIR == CAM_DIR_OUT)
				cmd->sc_data_direction = DMA_TO_DEVICE;
			else if (CSIODIR == CAM_DIR_NONE)
				cmd->sc_data_direction = DMA_NONE;
			else
				cmd->sc_data_direction = 0;

			quasi_sdev.channel = 0;
			quasi_sdev.id = csio->ccb_h.target_id;
			quasi_sdev.lun = csio->ccb_h.target_lun;
			if (csio->ccb_h.flags & CAM_TAG_ACTION_VALID) {
				quasi_sdev.tagged_supported = true;
				cmd->tag = csio->tag_action;
			}
			cmd->adapter = adapter;

			pvscsi_queue_locked(cmd, NULL);
			break;
		}

		case XPT_PATH_INQ: {
			struct ccb_pathinq *cpi = &pccb->cpi;

			cpi->version_num = 1;
			cpi->hba_inquiry =  PI_TAG_ABLE;
			cpi->target_sprt = 0;
			cpi->hba_misc = 0;
			cpi->hba_eng_cnt = 0;
			cpi->max_target = adapter->pvs_max_targets - 1;
			cpi->max_lun = 0;	/* 7 or 0 */
			cpi->initiator_id = 7;
			cpi->bus_id = cam_sim_bus(psim);
			strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
			strncpy(cpi->hba_vid, "VMware", HBA_IDLEN);
			strncpy(cpi->dev_name, cam_sim_name(psim), DEV_IDLEN);
			cpi->unit_number = cam_sim_unit(psim);
			cpi->transport = XPORT_SPI;
			cpi->transport_version = 2;
			cpi->protocol = PROTO_SCSI;
			cpi->protocol_version = SCSI_REV_SPC2;

			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}

		case XPT_RESET_BUS: {
			device_printf(device, "Bus reset initiated\n");
			pvscsi_bus_reset(adapter);
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			device_printf(device, "Bus reset completed\n");
			break;
		}

		case XPT_RESET_DEV: {
			target_id_t trg = pccb->ccb_h.target_id;

			if (pccb->ccb_h.target_lun) {
				device_printf(device, "Non-zero LU number %lu\n",
						pccb->ccb_h.target_lun);
				pccb->ccb_h.status = CAM_LUN_INVALID;
				xpt_done(pccb);
				break;
			}
			device_printf(device, "target %d reset initiated\n",
									trg);
			pvscsi_device_reset(adapter, trg);
			device_printf(device, "target %d reset completed\n",
									trg);
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}

		case XPT_ABORT: {
			pvscsi_abort(adapter,
				(struct ccb_scsiio *)(pccb->cab.abort_ccb));
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}

		case XPT_GET_TRAN_SETTINGS: {
			struct ccb_trans_settings *cts = &pccb->cts;

			cts->transport = XPORT_SPI;
			cts->transport_version = 2;
			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_SPC2;

			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	}
}


static int
pvscsi_pci_detach(device_t device)
{
	pvscsinst_t *adapter = device_get_softc(device);

	PVSCSILCK;
	xpt_async(AC_LOST_DEVICE, adapter->pvs_campath, NULL);
	xpt_free_path(adapter->pvs_campath);
	xpt_bus_deregister(cam_sim_path(adapter->pvs_camsim));

	cam_sim_free(adapter->pvs_camsim, true);
	PVSCSIULCK;
	mtx_destroy(&adapter->pvs_camlock);
	free(adapter->pvs_tarrg, M_PVSCSI);
	adapter->pvs_tarrg = NULL;

	pvscsi_mask_intr(adapter);

	pci_disable_io(device, SYS_RES_MEMORY);
	pci_disable_busmaster(device);

	pvscsi_release_resources(adapter);

	return 0;
}

static int
pvscsi_pci_attach(device_t device)
{
	int retval;
	struct resource *res = NULL;
	pvscsinst_t *adapter = device_get_softc(device);
	int rid = -1, i, error;

	memset(adapter, 0, sizeof(*adapter));
	adapter->pvs_timeout_one_comm_targ = -1;
	adapter->pvs_reset_target_on_timeout = 0;

	retval = pci_enable_busmaster(device);
	if(retval) {
		device_printf(device, "Could not enable bus-mastering, %d",
									retval);
		return retval;
	}

	retval = pci_enable_io(device, SYS_RES_MEMORY);
	if(retval) {
		device_printf(device, "Could not enable memory range, %d",
									retval);
		pci_disable_busmaster(device);
		return retval;
	}

	for (i = 0; i < PCIR_MAX_BAR_0; i++) {
		rid = PCIR_BAR(i);

		res = bus_alloc_resource_any(device, SYS_RES_MEMORY, &rid,
								RF_ACTIVE);
		if (res)
			break;
	}

	if (!res) {
		device_printf(device, "Could not find/activate memory range\n");
		goto out_disable_device;
	}

	LOG(0, "Acquired device registers at rid %d\n", rid);

	adapter->pvs_mmres = res;
	adapter->pvs_mmrid = rid;
	adapter->pvs_mmtag = rman_get_bustag(adapter->pvs_mmres);
	adapter->pvs_mmhndl = rman_get_bushandle(adapter->pvs_mmres);
	adapter->pvs_dev = device;

	ll_adapter_reset(adapter);

	adapter->use_msg = pvscsi_setup_msg_workqueue(adapter);

	error = pvscsi_allocate_rings(adapter);
	if (error) {
		printk(KERN_ERR "vmw_pvscsi: unable to allocate ring memory\n");
		goto out_release_resources;
	}

	/*
	 * Ask the device for max number of targets.
	 */
	adapter->pvs_max_targets = pvscsi_get_max_targets(adapter);
	device_printf(device, "Maximum number of targets is %u\n",
						adapter->pvs_max_targets);

	/*
	 * From this point on we should reset the adapter if anything goes
	 * wrong.
	 */
	pvscsi_setup_all_rings(adapter);

	adapter->cmd_map = kcalloc(adapter->req_depth,
				   sizeof(struct pvscsi_ctx), GFP_KERNEL);
	if (!adapter->cmd_map) {
		device_printf(device, "failed to allocate memory.\n");
		error = -ENOMEM;
		goto out_reset_adapter;
	}
	adapter->cmd_map_size = adapter->req_depth * sizeof(struct pvscsi_ctx);

	if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR,
		    BUS_SPACE_MAXADDR, NULL, NULL, BUS_SPACE_MAXADDR,
		    PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT, BUS_SPACE_MAXADDR, 0,
		    busdma_lock_mutex, &adapter->pvs_camlock,
		    &adapter->pvs_dmat) != 0) {
		device_printf(device, "DMA tag\n");
		goto out_reset_adapter;
	}

	adapter->pvs_tarrg = malloc((sizeof(pvscsitarg_t)) *
			adapter->pvs_max_targets, M_PVSCSI,M_WAITOK|M_ZERO);
	if (!adapter->pvs_tarrg) {
		goto out_reset_adapter;
	}
	mtx_init(&adapter->pvs_camlock, "pvscsi camlock", NULL, MTX_DEF);

	INIT_LIST_HEAD(&adapter->cmd_pool);
	for (i = 0; i < adapter->req_depth; i++) {
		struct pvscsi_ctx *ctx = adapter->cmd_map + i;
		if (bus_dmamap_create(adapter->pvs_dmat, 0, &ctx->dmap) != 0) {
			device_printf(device, "dmap alloc failed, %d\n", i);
			goto out_delete_dmat;
		}
		ctx->adapter = adapter;
		callout_init_mtx(&ctx->calloutx, &adapter->pvs_camlock, 0);
		list_add(&ctx->list, &adapter->cmd_pool);
	}

	error = pvscsi_allocate_sg(adapter);
	if (error) {
		device_printf(device, "Unable to allocate s/g table\n");
		goto out_delete_dmat;
	}

	if (pvscsi_setup_intr(adapter, pvscsi_isr_freebsd, true)) {
		device_printf(device, "Using MSI-X interrupts\n");
		adapter->use_msix = 1;
	} else if (pvscsi_setup_intr(adapter, pvscsi_isr_freebsd, false)) {
		device_printf(device, "Using INTx interrupts\n");
		adapter->use_msix = adapter->use_msi = 0;
	} else {
		goto out_delete_dmat;
	}

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(device),
		SYSCTL_CHILDREN(device_get_sysctl_tree(device)), OID_AUTO,
		"drop_next_command_to_target", CTLFLAG_RW,
		&adapter->pvs_timeout_one_comm_targ, 0U,
		"Drop the next I/O to this target(for test purposes)");

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(device),
		SYSCTL_CHILDREN(device_get_sysctl_tree(device)), OID_AUTO,
		"reset_target_on_command_timeout", CTLFLAG_RW,
		&adapter->pvs_reset_target_on_timeout, 0U,
		"Reset the target on I/O timing out(for test purposes)");

	/* Register with CAM as a SIM */
	adapter->pvs_camdevq = cam_simq_alloc(adapter->req_depth);
	if (!adapter->pvs_camdevq) {
		device_printf(device, "cam_simq_alloc(%d) failed\n",
							adapter->req_depth);
		goto out_delete_dmat;
	}
	adapter->pvs_camsim = cam_sim_alloc(pvscsi_action, pvscsi_poll,
				"pvscsi", adapter,
				device_get_unit(adapter->pvs_dev),
				&adapter->pvs_camlock, adapter->req_depth,
				adapter->req_depth, adapter->pvs_camdevq);

	if (!adapter->pvs_camsim) {
		device_printf(device, "cam_sim_alloc() failed\n");
		goto out_cam_simq;
	}

	PVSCSILCK;
	if (xpt_bus_register(adapter->pvs_camsim, NULL, 0) != CAM_SUCCESS) {
		PVSCSIULCK;
		device_printf(device, "xpt_bus_register() failed\n");
		goto out_cam_sim;
	}

	if (xpt_create_path(&adapter->pvs_campath, NULL,
			    cam_sim_path(adapter->pvs_camsim),
			    CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		PVSCSIULCK;
		device_printf(device, "xpt_create_path() failed\n");
		goto out_cam_busreg;
	}
	PVSCSIULCK;

	PRINT_RINGSTATE(adapter);
	device_printf(device, "INTR %d\n", pvscsi_read_intr_status(adapter));

	pvscsi_unmask_intr(adapter);

	/* pvscsi_dma(adapter); */
	return 0;

out_cam_busreg:
	xpt_bus_deregister(cam_sim_path(adapter->pvs_camsim));
out_cam_sim:
	cam_sim_free(adapter->pvs_camsim, false);
out_cam_simq:
	cam_simq_free(adapter->pvs_camdevq);
out_delete_dmat:
	bus_dma_tag_destroy(adapter->pvs_dmat);
	mtx_destroy(&adapter->pvs_camlock);
	free(adapter->pvs_tarrg, M_PVSCSI);
	adapter->pvs_tarrg = NULL;
out_reset_adapter:
	ll_adapter_reset(adapter);
out_release_resources:
	pvscsi_release_resources(adapter);
out_disable_device:
	pci_disable_io(device, SYS_RES_MEMORY);
	pci_disable_busmaster(device);
	return ENXIO;
}

static int
pvscsi_pci_probe(device_t device)
{
	if ((pci_get_vendor(device) != PCI_VENDOR_ID_VMWARE) ||
		(pci_get_device(device) != PCI_DEVICE_ID_VMWARE_PVSCSI)) {
		return ENXIO;
	}

	device_set_desc(device, "VMware para-virtual SCSI driver v1.1.2.0");

	return 0;
}

static device_method_t
pvscsi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pvscsi_pci_probe),
	DEVMETHOD(device_attach,	pvscsi_pci_attach),
	DEVMETHOD(device_detach,	pvscsi_pci_detach),
	DEVMETHOD_END,
};

static driver_t pvscsi_pci_driver = {
	"pvscsi",
	pvscsi_pci_methods,
	sizeof(pvscsinst_t),
};

static int
pvscsi_mod(module_t modp, int modev, void *arg)
{
	switch(modev) {
		case MOD_LOAD: {
			/* One time module initialization here */
			break;
		}
		case MOD_UNLOAD: {
			/* One time module uninitialization here */
			break;
		}
		default:
			break;
	}

	return 0;
}

static devclass_t pvscsi_devclass;
DRIVER_MODULE(MODNM, pci, pvscsi_pci_driver, pvscsi_devclass, pvscsi_mod, NULL);
MODULE_VERSION(MODNM, 1);
#endif /* __FreeBSD__ */
