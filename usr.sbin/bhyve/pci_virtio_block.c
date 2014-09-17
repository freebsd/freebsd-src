/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
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
#include "pci_emul.h"
#include "virtio.h"

#define VTBLK_RINGSZ	64

#define VTBLK_MAXSEGS	32

#define VTBLK_S_OK	0
#define VTBLK_S_IOERR	1
#define	VTBLK_S_UNSUPP	2

#define	VTBLK_BLK_ID_BYTES	20

/* Capability bits */
#define	VTBLK_F_SEG_MAX		(1 << 2)	/* Maximum request segments */
#define	VTBLK_F_BLK_SIZE       	(1 << 6)	/* cfg block size valid */

/*
 * Host capabilities
 */
#define VTBLK_S_HOSTCAPS      \
  ( VTBLK_F_SEG_MAX  |						    \
    VTBLK_F_BLK_SIZE |						    \
    VIRTIO_RING_F_INDIRECT_DESC )	/* indirect descriptors */

/*
 * Config space "registers"
 */
struct vtblk_config {
	uint64_t	vbc_capacity;
	uint32_t	vbc_size_max;
	uint32_t	vbc_seg_max;
	uint16_t	vbc_geom_c;
	uint8_t		vbc_geom_h;
	uint8_t		vbc_geom_s;
	uint32_t	vbc_blk_size;
	uint32_t	vbc_sectors_max;
} __packed;

/*
 * Fixed-size block header
 */
struct virtio_blk_hdr {
#define	VBH_OP_READ		0
#define	VBH_OP_WRITE		1
#define	VBH_OP_IDENT		8		
#define	VBH_FLAG_BARRIER	0x80000000	/* OR'ed into vbh_type */
	uint32_t       	vbh_type;
	uint32_t	vbh_ioprio;
	uint64_t	vbh_sector;
} __packed;

/*
 * Debug printf
 */
static int pci_vtblk_debug;
#define DPRINTF(params) if (pci_vtblk_debug) printf params
#define WPRINTF(params) printf params

/*
 * Per-device softc
 */
struct pci_vtblk_softc {
	struct virtio_softc vbsc_vs;
	pthread_mutex_t vsc_mtx;
	struct vqueue_info vbsc_vq;
	int		vbsc_fd;
	struct vtblk_config vbsc_cfg;	
	char vbsc_ident[VTBLK_BLK_ID_BYTES];
};

static void pci_vtblk_reset(void *);
static void pci_vtblk_notify(void *, struct vqueue_info *);
static int pci_vtblk_cfgread(void *, int, int, uint32_t *);
static int pci_vtblk_cfgwrite(void *, int, int, uint32_t);

static struct virtio_consts vtblk_vi_consts = {
	"vtblk",		/* our name */
	1,			/* we support 1 virtqueue */
	sizeof(struct vtblk_config), /* config reg size */
	pci_vtblk_reset,	/* reset */
	pci_vtblk_notify,	/* device-wide qnotify */
	pci_vtblk_cfgread,	/* read PCI config */
	pci_vtblk_cfgwrite,	/* write PCI config */
	NULL,			/* apply negotiated features */
	VTBLK_S_HOSTCAPS,	/* our capabilities */
};

static void
pci_vtblk_reset(void *vsc)
{
	struct pci_vtblk_softc *sc = vsc;

	DPRINTF(("vtblk: device reset requested !\n"));
	vi_reset_dev(&sc->vbsc_vs);
}

static void
pci_vtblk_proc(struct pci_vtblk_softc *sc, struct vqueue_info *vq)
{
	struct virtio_blk_hdr *vbh;
	uint8_t *status;
	int i, n;
	int err;
	int iolen;
	int writeop, type;
	off_t offset;
	struct iovec iov[VTBLK_MAXSEGS + 2];
	uint16_t flags[VTBLK_MAXSEGS + 2];

	n = vq_getchain(vq, iov, VTBLK_MAXSEGS + 2, flags);

	/*
	 * The first descriptor will be the read-only fixed header,
	 * and the last is for status (hence +2 above and below).
	 * The remaining iov's are the actual data I/O vectors.
	 *
	 * XXX - note - this fails on crash dump, which does a
	 * VIRTIO_BLK_T_FLUSH with a zero transfer length
	 */
	assert(n >= 2 && n <= VTBLK_MAXSEGS + 2);

	assert((flags[0] & VRING_DESC_F_WRITE) == 0);
	assert(iov[0].iov_len == sizeof(struct virtio_blk_hdr));
	vbh = iov[0].iov_base;

	status = iov[--n].iov_base;
	assert(iov[n].iov_len == 1);
	assert(flags[n] & VRING_DESC_F_WRITE);

	/*
	 * XXX
	 * The guest should not be setting the BARRIER flag because
	 * we don't advertise the capability.
	 */
	type = vbh->vbh_type & ~VBH_FLAG_BARRIER;
	writeop = (type == VBH_OP_WRITE);

	offset = vbh->vbh_sector * DEV_BSIZE;

	iolen = 0;
	for (i = 1; i < n; i++) {
		/*
		 * - write op implies read-only descriptor,
		 * - read/ident op implies write-only descriptor,
		 * therefore test the inverse of the descriptor bit
		 * to the op.
		 */
		assert(((flags[i] & VRING_DESC_F_WRITE) == 0) == writeop);
		iolen += iov[i].iov_len;
	}

	DPRINTF(("virtio-block: %s op, %d bytes, %d segs, offset %ld\n\r", 
		 writeop ? "write" : "read/ident", iolen, i - 1, offset));

	switch (type) {
	case VBH_OP_WRITE:
		err = pwritev(sc->vbsc_fd, iov + 1, i - 1, offset);
		break;
	case VBH_OP_READ:
		err = preadv(sc->vbsc_fd, iov + 1, i - 1, offset);
		break;
	case VBH_OP_IDENT:
		/* Assume a single buffer */
		strlcpy(iov[1].iov_base, sc->vbsc_ident,
		    MIN(iov[1].iov_len, sizeof(sc->vbsc_ident)));
		err = 0;
		break;
	default:
		err = -ENOSYS;
		break;
	}

	/* convert errno into a virtio block error return */
	if (err < 0) {
		if (err == -ENOSYS)
			*status = VTBLK_S_UNSUPP;
		else
			*status = VTBLK_S_IOERR;
	} else
		*status = VTBLK_S_OK;

	/*
	 * Return the descriptor back to the host.
	 * We wrote 1 byte (our status) to host.
	 */
	vq_relchain(vq, 1);
}

static void
pci_vtblk_notify(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtblk_softc *sc = vsc;

	vq_startchains(vq);
	while (vq_has_descs(vq))
		pci_vtblk_proc(sc, vq);
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static int
pci_vtblk_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	struct stat sbuf;
	MD5_CTX mdctx;
	u_char digest[16];
	struct pci_vtblk_softc *sc;
	off_t size;	
	int fd;
	int sectsz;

	if (opts == NULL) {
		printf("virtio-block: backing device required\n");
		return (1);
	}

	/*
	 * The supplied backing file has to exist
	 */
	fd = open(opts, O_RDWR);
	if (fd < 0) {
		perror("Could not open backing file");
		return (1);
	}

	if (fstat(fd, &sbuf) < 0) {
		perror("Could not stat backing file");
		close(fd);
		return (1);
	}

	/*
	 * Deal with raw devices
	 */
	size = sbuf.st_size;
	sectsz = DEV_BSIZE;
	if (S_ISCHR(sbuf.st_mode)) {
		if (ioctl(fd, DIOCGMEDIASIZE, &size) < 0 ||
		    ioctl(fd, DIOCGSECTORSIZE, &sectsz)) {
			perror("Could not fetch dev blk/sector size");
			close(fd);
			return (1);
		}
		assert(size != 0);
		assert(sectsz != 0);
	}

	sc = calloc(1, sizeof(struct pci_vtblk_softc));

	/* record fd of storage device/file */
	sc->vbsc_fd = fd;

	pthread_mutex_init(&sc->vsc_mtx, NULL);

	/* init virtio softc and virtqueues */
	vi_softc_linkup(&sc->vbsc_vs, &vtblk_vi_consts, sc, pi, &sc->vbsc_vq);
	sc->vbsc_vs.vs_mtx = &sc->vsc_mtx;

	sc->vbsc_vq.vq_qsize = VTBLK_RINGSZ;
	/* sc->vbsc_vq.vq_notify = we have no per-queue notify */

	/*
	 * Create an identifier for the backing file. Use parts of the
	 * md5 sum of the filename
	 */
	MD5Init(&mdctx);
	MD5Update(&mdctx, opts, strlen(opts));
	MD5Final(digest, &mdctx);	
	sprintf(sc->vbsc_ident, "BHYVE-%02X%02X-%02X%02X-%02X%02X",
	    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5]);

	/* setup virtio block config space */
	sc->vbsc_cfg.vbc_capacity = size / DEV_BSIZE; /* 512-byte units */
	sc->vbsc_cfg.vbc_seg_max = VTBLK_MAXSEGS;
	sc->vbsc_cfg.vbc_blk_size = sectsz;
	sc->vbsc_cfg.vbc_size_max = 0;	/* not negotiated */
	sc->vbsc_cfg.vbc_geom_c = 0;	/* no geometry */
	sc->vbsc_cfg.vbc_geom_h = 0;
	sc->vbsc_cfg.vbc_geom_s = 0;
	sc->vbsc_cfg.vbc_sectors_max = 0;

	/*
	 * Should we move some of this into virtio.c?  Could
	 * have the device, class, and subdev_0 as fields in
	 * the virtio constants structure.
	 */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_BLOCK);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_TYPE_BLOCK);

	pci_lintr_request(pi);

	if (vi_intr_init(&sc->vbsc_vs, 1, fbsdrun_virtio_msix()))
		return (1);
	vi_set_io_bar(&sc->vbsc_vs, 0);
	return (0);
}

static int
pci_vtblk_cfgwrite(void *vsc, int offset, int size, uint32_t value)
{

	DPRINTF(("vtblk: write to readonly reg %d\n\r", offset));
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

struct pci_devemu pci_de_vblk = {
	.pe_emu =	"virtio-blk",
	.pe_init =	pci_vtblk_init,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read
};
PCI_EMUL_SET(pci_de_vblk);
