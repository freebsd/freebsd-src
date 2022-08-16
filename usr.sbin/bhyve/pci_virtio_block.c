/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 * Copyright 2020-2021 Joyent, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <machine/vmm_snapshot.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <md5.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#include "virtio.h"
#include "block_if.h"

#define	VTBLK_BSIZE	512
#define	VTBLK_RINGSZ	128

_Static_assert(VTBLK_RINGSZ <= BLOCKIF_RING_MAX, "Each ring entry must be able to queue a request");

#define	VTBLK_S_OK	0
#define	VTBLK_S_IOERR	1
#define	VTBLK_S_UNSUPP	2

#define	VTBLK_BLK_ID_BYTES	20 + 1

/* Capability bits */
#define	VTBLK_F_BARRIER		(1 << 0)	/* Does host support barriers? */
#define	VTBLK_F_SIZE_MAX	(1 << 1)	/* Indicates maximum segment size */
#define	VTBLK_F_SEG_MAX		(1 << 2)	/* Indicates maximum # of segments */
#define	VTBLK_F_GEOMETRY	(1 << 4)	/* Legacy geometry available  */
#define	VTBLK_F_RO		(1 << 5)	/* Disk is read-only */
#define	VTBLK_F_BLK_SIZE	(1 << 6)	/* Block size of disk is available*/
#define	VTBLK_F_SCSI		(1 << 7)	/* Supports scsi command passthru */
#define	VTBLK_F_FLUSH		(1 << 9)	/* Writeback mode enabled after reset */
#define	VTBLK_F_WCE		(1 << 9)	/* Legacy alias for FLUSH */
#define	VTBLK_F_TOPOLOGY	(1 << 10)	/* Topology information is available */
#define	VTBLK_F_CONFIG_WCE	(1 << 11)	/* Writeback mode available in config */
#define	VTBLK_F_MQ		(1 << 12)	/* Multi-Queue */
#define	VTBLK_F_DISCARD		(1 << 13)	/* Trim blocks */
#define	VTBLK_F_WRITE_ZEROES	(1 << 14)	/* Write zeros */

/*
 * Host capabilities
 */
#define	VTBLK_S_HOSTCAPS      \
  ( VTBLK_F_SEG_MAX  |						    \
    VTBLK_F_BLK_SIZE |						    \
    VTBLK_F_FLUSH    |						    \
    VTBLK_F_TOPOLOGY |						    \
    VIRTIO_RING_F_INDIRECT_DESC )	/* indirect descriptors */

/*
 * The current blockif_delete() interface only allows a single delete
 * request at a time.
 */
#define	VTBLK_MAX_DISCARD_SEG	1

/*
 * An arbitrary limit to prevent excessive latency due to large
 * delete requests.
 */
#define	VTBLK_MAX_DISCARD_SECT	((16 << 20) / VTBLK_BSIZE)	/* 16 MiB */

/*
 * Config space "registers"
 */
struct vtblk_config {
	uint64_t	vbc_capacity;
	uint32_t	vbc_size_max;
	uint32_t	vbc_seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} vbc_geometry;
	uint32_t	vbc_blk_size;
	struct {
		uint8_t physical_block_exp;
		uint8_t alignment_offset;
		uint16_t min_io_size;
		uint32_t opt_io_size;
	} vbc_topology;
	uint8_t		vbc_writeback;
	uint8_t		unused0[1];
	uint16_t	num_queues;
	uint32_t	max_discard_sectors;
	uint32_t	max_discard_seg;
	uint32_t	discard_sector_alignment;
	uint32_t	max_write_zeroes_sectors;
	uint32_t	max_write_zeroes_seg;
	uint8_t		write_zeroes_may_unmap;
	uint8_t		unused1[3];
} __packed;

/*
 * Fixed-size block header
 */
struct virtio_blk_hdr {
#define	VBH_OP_READ		0
#define	VBH_OP_WRITE		1
#define	VBH_OP_SCSI_CMD		2
#define	VBH_OP_SCSI_CMD_OUT	3
#define	VBH_OP_FLUSH		4
#define	VBH_OP_FLUSH_OUT	5
#define	VBH_OP_IDENT		8
#define	VBH_OP_DISCARD		11
#define	VBH_OP_WRITE_ZEROES	13

#define	VBH_FLAG_BARRIER	0x80000000	/* OR'ed into vbh_type */
	uint32_t	vbh_type;
	uint32_t	vbh_ioprio;
	uint64_t	vbh_sector;
} __packed;

/*
 * Debug printf
 */
static int pci_vtblk_debug;
#define	DPRINTF(params) if (pci_vtblk_debug) PRINTLN params
#define	WPRINTF(params) PRINTLN params

struct pci_vtblk_ioreq {
	struct blockif_req		io_req;
	struct pci_vtblk_softc		*io_sc;
	uint8_t				*io_status;
	uint16_t			io_idx;
};

struct virtio_blk_discard_write_zeroes {
	uint64_t	sector;
	uint32_t	num_sectors;
	struct {
		uint32_t unmap:1;
		uint32_t reserved:31;
	} flags;
};

/*
 * Per-device softc
 */
struct pci_vtblk_softc {
	struct virtio_softc vbsc_vs;
	pthread_mutex_t vsc_mtx;
	struct vqueue_info vbsc_vq;
	struct vtblk_config vbsc_cfg;
	struct virtio_consts vbsc_consts;
	struct blockif_ctxt *bc;
	char vbsc_ident[VTBLK_BLK_ID_BYTES];
	struct pci_vtblk_ioreq vbsc_ios[VTBLK_RINGSZ];
};

static void pci_vtblk_reset(void *);
static void pci_vtblk_notify(void *, struct vqueue_info *);
static int pci_vtblk_cfgread(void *, int, int, uint32_t *);
static int pci_vtblk_cfgwrite(void *, int, int, uint32_t);
#ifdef BHYVE_SNAPSHOT
static void pci_vtblk_pause(void *);
static void pci_vtblk_resume(void *);
static int pci_vtblk_snapshot(void *, struct vm_snapshot_meta *);
#endif

static struct virtio_consts vtblk_vi_consts = {
	"vtblk",		/* our name */
	1,			/* we support 1 virtqueue */
	sizeof(struct vtblk_config),	/* config reg size */
	pci_vtblk_reset,	/* reset */
	pci_vtblk_notify,	/* device-wide qnotify */
	pci_vtblk_cfgread,	/* read PCI config */
	pci_vtblk_cfgwrite,	/* write PCI config */
	NULL,			/* apply negotiated features */
	VTBLK_S_HOSTCAPS,	/* our capabilities */
#ifdef BHYVE_SNAPSHOT
	pci_vtblk_pause,	/* pause blockif threads */
	pci_vtblk_resume,	/* resume blockif threads */
	pci_vtblk_snapshot,	/* save / restore device state */
#endif
};

static void
pci_vtblk_reset(void *vsc)
{
	struct pci_vtblk_softc *sc = vsc;

	DPRINTF(("vtblk: device reset requested !"));
	vi_reset_dev(&sc->vbsc_vs);
}

static void
pci_vtblk_done_locked(struct pci_vtblk_ioreq *io, int err)
{
	struct pci_vtblk_softc *sc = io->io_sc;

	/* convert errno into a virtio block error return */
	if (err == EOPNOTSUPP || err == ENOSYS)
		*io->io_status = VTBLK_S_UNSUPP;
	else if (err != 0)
		*io->io_status = VTBLK_S_IOERR;
	else
		*io->io_status = VTBLK_S_OK;

	/*
	 * Return the descriptor back to the host.
	 * We wrote 1 byte (our status) to host.
	 */
	vq_relchain(&sc->vbsc_vq, io->io_idx, 1);
	vq_endchains(&sc->vbsc_vq, 0);
}

#ifdef BHYVE_SNAPSHOT
static void
pci_vtblk_pause(void *vsc)
{
	struct pci_vtblk_softc *sc = vsc;

	DPRINTF(("vtblk: device pause requested !\n"));
	blockif_pause(sc->bc);
}

static void
pci_vtblk_resume(void *vsc)
{
	struct pci_vtblk_softc *sc = vsc;

	DPRINTF(("vtblk: device resume requested !\n"));
	blockif_resume(sc->bc);
}

static int
pci_vtblk_snapshot(void *vsc, struct vm_snapshot_meta *meta)
{
	int ret;
	struct pci_vtblk_softc *sc = vsc;

	SNAPSHOT_VAR_OR_LEAVE(sc->vbsc_cfg, meta, ret, done);
	SNAPSHOT_BUF_OR_LEAVE(sc->vbsc_ident, sizeof(sc->vbsc_ident),
			      meta, ret, done);

done:
	return (ret);
}
#endif

static void
pci_vtblk_done(struct blockif_req *br, int err)
{
	struct pci_vtblk_ioreq *io = br->br_param;
	struct pci_vtblk_softc *sc = io->io_sc;

	pthread_mutex_lock(&sc->vsc_mtx);
	pci_vtblk_done_locked(io, err);
	pthread_mutex_unlock(&sc->vsc_mtx);
}

static void
pci_vtblk_proc(struct pci_vtblk_softc *sc, struct vqueue_info *vq)
{
	struct virtio_blk_hdr *vbh;
	struct pci_vtblk_ioreq *io;
	int i, n;
	int err;
	ssize_t iolen;
	int writeop, type;
	struct vi_req req;
	struct iovec iov[BLOCKIF_IOV_MAX + 2];
	struct virtio_blk_discard_write_zeroes *discard;

	n = vq_getchain(vq, iov, BLOCKIF_IOV_MAX + 2, &req);

	/*
	 * The first descriptor will be the read-only fixed header,
	 * and the last is for status (hence +2 above and below).
	 * The remaining iov's are the actual data I/O vectors.
	 *
	 * XXX - note - this fails on crash dump, which does a
	 * VIRTIO_BLK_T_FLUSH with a zero transfer length
	 */
	assert(n >= 2 && n <= BLOCKIF_IOV_MAX + 2);

	io = &sc->vbsc_ios[req.idx];
	assert(req.readable != 0);
	assert(iov[0].iov_len == sizeof(struct virtio_blk_hdr));
	vbh = (struct virtio_blk_hdr *)iov[0].iov_base;
	memcpy(&io->io_req.br_iov, &iov[1], sizeof(struct iovec) * (n - 2));
	io->io_req.br_iovcnt = n - 2;
	io->io_req.br_offset = vbh->vbh_sector * VTBLK_BSIZE;
	io->io_status = (uint8_t *)iov[--n].iov_base;
	assert(req.writable != 0);
	assert(iov[n].iov_len == 1);

	/*
	 * XXX
	 * The guest should not be setting the BARRIER flag because
	 * we don't advertise the capability.
	 */
	type = vbh->vbh_type & ~VBH_FLAG_BARRIER;
	writeop = (type == VBH_OP_WRITE || type == VBH_OP_DISCARD);
	/*
	 * - Write op implies read-only descriptor
	 * - Read/ident op implies write-only descriptor
	 *
	 * By taking away either the read-only fixed header or the write-only
	 * status iovec, the following condition should hold true.
	 */
	assert(n == (writeop ? req.readable : req.writable));

	iolen = 0;
	for (i = 1; i < n; i++) {
		iolen += iov[i].iov_len;
	}
	io->io_req.br_resid = iolen;

	DPRINTF(("virtio-block: %s op, %zd bytes, %d segs, offset %ld",
		 writeop ? "write/discard" : "read/ident", iolen, i - 1,
		 io->io_req.br_offset));

	switch (type) {
	case VBH_OP_READ:
		err = blockif_read(sc->bc, &io->io_req);
		break;
	case VBH_OP_WRITE:
		err = blockif_write(sc->bc, &io->io_req);
		break;
	case VBH_OP_DISCARD:
		/*
		 * We currently only support a single request, if the guest
		 * has submitted a request that doesn't conform to the
		 * requirements, we return a error.
		 */
		if (iov[1].iov_len != sizeof (*discard)) {
			pci_vtblk_done_locked(io, EINVAL);
			return;
		}

		/* The segments to discard are provided rather than data */
		discard = (struct virtio_blk_discard_write_zeroes *)
		    iov[1].iov_base;

		/*
		 * virtio v1.1 5.2.6.2:
		 * The device MUST set the status byte to VIRTIO_BLK_S_UNSUPP
		 * for discard and write zeroes commands if any unknown flag is
		 * set. Furthermore, the device MUST set the status byte to
		 * VIRTIO_BLK_S_UNSUPP for discard commands if the unmap flag
		 * is set.
		 *
		 * Currently there are no known flags for a DISCARD request.
		 */
		if (discard->flags.unmap != 0 || discard->flags.reserved != 0) {
			pci_vtblk_done_locked(io, ENOTSUP);
			return;
		}

		/* Make sure the request doesn't exceed our size limit */
		if (discard->num_sectors > VTBLK_MAX_DISCARD_SECT) {
			pci_vtblk_done_locked(io, EINVAL);
			return;
		}

		io->io_req.br_offset = discard->sector * VTBLK_BSIZE;
		io->io_req.br_resid = discard->num_sectors * VTBLK_BSIZE;
		err = blockif_delete(sc->bc, &io->io_req);
		break;
	case VBH_OP_FLUSH:
	case VBH_OP_FLUSH_OUT:
		err = blockif_flush(sc->bc, &io->io_req);
		break;
	case VBH_OP_IDENT:
		/* Assume a single buffer */
		/* S/n equal to buffer is not zero-terminated. */
		memset(iov[1].iov_base, 0, iov[1].iov_len);
		strncpy(iov[1].iov_base, sc->vbsc_ident,
		    MIN(iov[1].iov_len, sizeof(sc->vbsc_ident)));
		pci_vtblk_done_locked(io, 0);
		return;
	default:
		pci_vtblk_done_locked(io, EOPNOTSUPP);
		return;
	}
	assert(err == 0);
}

static void
pci_vtblk_notify(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtblk_softc *sc = vsc;

	while (vq_has_descs(vq))
		pci_vtblk_proc(sc, vq);
}

static void
pci_vtblk_resized(struct blockif_ctxt *bctxt, void *arg, size_t new_size)
{
	struct pci_vtblk_softc *sc;

	sc = arg;

	sc->vbsc_cfg.vbc_capacity = new_size / VTBLK_BSIZE; /* 512-byte units */
	vi_interrupt(&sc->vbsc_vs, VIRTIO_PCI_ISR_CONFIG,
	    sc->vbsc_vs.vs_msix_cfg_idx);
}

static int
pci_vtblk_init(struct vmctx *ctx, struct pci_devinst *pi, nvlist_t *nvl)
{
	char bident[sizeof("XX:X:X")];
	struct blockif_ctxt *bctxt;
	const char *path, *serial;
	MD5_CTX mdctx;
	u_char digest[16];
	struct pci_vtblk_softc *sc;
	off_t size;
	int i, sectsz, sts, sto;

	/*
	 * The supplied backing file has to exist
	 */
	snprintf(bident, sizeof(bident), "%d:%d", pi->pi_slot, pi->pi_func);
	bctxt = blockif_open(nvl, bident);
	if (bctxt == NULL) {
		perror("Could not open backing file");
		return (1);
	}

	size = blockif_size(bctxt);
	sectsz = blockif_sectsz(bctxt);
	blockif_psectsz(bctxt, &sts, &sto);

	sc = calloc(1, sizeof(struct pci_vtblk_softc));
	sc->bc = bctxt;
	for (i = 0; i < VTBLK_RINGSZ; i++) {
		struct pci_vtblk_ioreq *io = &sc->vbsc_ios[i];
		io->io_req.br_callback = pci_vtblk_done;
		io->io_req.br_param = io;
		io->io_sc = sc;
		io->io_idx = i;
	}

	bcopy(&vtblk_vi_consts, &sc->vbsc_consts, sizeof (vtblk_vi_consts));
	if (blockif_candelete(sc->bc))
		sc->vbsc_consts.vc_hv_caps |= VTBLK_F_DISCARD;

	pthread_mutex_init(&sc->vsc_mtx, NULL);

	/* init virtio softc and virtqueues */
	vi_softc_linkup(&sc->vbsc_vs, &sc->vbsc_consts, sc, pi, &sc->vbsc_vq);
	sc->vbsc_vs.vs_mtx = &sc->vsc_mtx;

	sc->vbsc_vq.vq_qsize = VTBLK_RINGSZ;
	/* sc->vbsc_vq.vq_notify = we have no per-queue notify */

	/*
	 * If an explicit identifier is not given, create an
	 * identifier using parts of the md5 sum of the filename.
	 */
	bzero(sc->vbsc_ident, VTBLK_BLK_ID_BYTES);
	if ((serial = get_config_value_node(nvl, "serial")) != NULL ||
	    (serial = get_config_value_node(nvl, "ser")) != NULL) {
		strlcpy(sc->vbsc_ident, serial, VTBLK_BLK_ID_BYTES);
	} else {
		path = get_config_value_node(nvl, "path");
		MD5Init(&mdctx);
		MD5Update(&mdctx, path, strlen(path));
		MD5Final(digest, &mdctx);
		snprintf(sc->vbsc_ident, VTBLK_BLK_ID_BYTES,
		    "BHYVE-%02X%02X-%02X%02X-%02X%02X",
		    digest[0], digest[1], digest[2], digest[3], digest[4],
		    digest[5]);
	}

	/* setup virtio block config space */
	sc->vbsc_cfg.vbc_capacity = size / VTBLK_BSIZE; /* 512-byte units */
	sc->vbsc_cfg.vbc_size_max = 0;	/* not negotiated */

	/*
	 * If Linux is presented with a seg_max greater than the virtio queue
	 * size, it can stumble into situations where it violates its own
	 * invariants and panics.  For safety, we keep seg_max clamped, paying
	 * heed to the two extra descriptors needed for the header and status
	 * of a request.
	 */
	sc->vbsc_cfg.vbc_seg_max = MIN(VTBLK_RINGSZ - 2, BLOCKIF_IOV_MAX);
	sc->vbsc_cfg.vbc_geometry.cylinders = 0;	/* no geometry */
	sc->vbsc_cfg.vbc_geometry.heads = 0;
	sc->vbsc_cfg.vbc_geometry.sectors = 0;
	sc->vbsc_cfg.vbc_blk_size = sectsz;
	sc->vbsc_cfg.vbc_topology.physical_block_exp =
	    (sts > sectsz) ? (ffsll(sts / sectsz) - 1) : 0;
	sc->vbsc_cfg.vbc_topology.alignment_offset =
	    (sto != 0) ? ((sts - sto) / sectsz) : 0;
	sc->vbsc_cfg.vbc_topology.min_io_size = 0;
	sc->vbsc_cfg.vbc_topology.opt_io_size = 0;
	sc->vbsc_cfg.vbc_writeback = 0;
	sc->vbsc_cfg.max_discard_sectors = VTBLK_MAX_DISCARD_SECT;
	sc->vbsc_cfg.max_discard_seg = VTBLK_MAX_DISCARD_SEG;
	sc->vbsc_cfg.discard_sector_alignment = MAX(sectsz, sts) / VTBLK_BSIZE;

	/*
	 * Should we move some of this into virtio.c?  Could
	 * have the device, class, and subdev_0 as fields in
	 * the virtio constants structure.
	 */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_BLOCK);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_ID_BLOCK);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vbsc_vs, 1, fbsdrun_virtio_msix())) {
		blockif_close(sc->bc);
		free(sc);
		return (1);
	}
	vi_set_io_bar(&sc->vbsc_vs, 0);
	blockif_register_resize_callback(sc->bc, pci_vtblk_resized, sc);
	return (0);
}

static int
pci_vtblk_cfgwrite(void *vsc, int offset, int size, uint32_t value)
{

	DPRINTF(("vtblk: write to readonly reg %d", offset));
	return (1);
}

static int
pci_vtblk_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vtblk_softc *sc = vsc;
	void *ptr;

	/* our caller has already verified offset and size */
	ptr = (uint8_t *)&sc->vbsc_cfg + offset;
	memcpy(retval, ptr, size);
	return (0);
}

static const struct pci_devemu pci_de_vblk = {
	.pe_emu =	"virtio-blk",
	.pe_init =	pci_vtblk_init,
	.pe_legacy_config = blockif_legacy_config,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read,
#ifdef BHYVE_SNAPSHOT
	.pe_snapshot =	vi_pci_snapshot,
	.pe_pause =     vi_pci_pause,
	.pe_resume =    vi_pci_resume,
#endif
};
PCI_EMUL_SET(pci_de_vblk);
