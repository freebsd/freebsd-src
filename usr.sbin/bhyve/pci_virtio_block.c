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

#include "bhyverun.h"
#include "pci_emul.h"
#include "virtio.h"

#define VTBLK_RINGSZ	64

#define VTBLK_CFGSZ	28

#define VTBLK_R_CFG		VTCFG_R_CFG1 
#define VTBLK_R_CFG_END		VTBLK_R_CFG + VTBLK_CFGSZ -1
#define VTBLK_R_MAX		VTBLK_R_CFG_END

#define VTBLK_REGSZ		VTBLK_R_MAX+1

#define VTBLK_MAXSEGS	32

#define VTBLK_S_OK	0
#define VTBLK_S_IOERR	1

/*
 * Host capabilities
 */
#define VTBLK_S_HOSTCAPS      \
  ( 0x00000004 |	/* host maximum request segments */ \
    0x10000000 )	/* supports indirect descriptors */

static int use_msix = 1;

struct vring_hqueue {
	/* Internal state */
	uint16_t	hq_size;
	uint16_t	hq_cur_aidx;		/* trails behind 'avail_idx' */

	 /* Host-context pointers to the queue */
	struct virtio_desc *hq_dtable;
	uint16_t	*hq_avail_flags;
	uint16_t	*hq_avail_idx;		/* monotonically increasing */
	uint16_t	*hq_avail_ring;

	uint16_t	*hq_used_flags;
	uint16_t	*hq_used_idx;		/* monotonically increasing */
	struct virtio_used *hq_used_ring;
};

/*
 * Config space
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
CTASSERT(sizeof(struct vtblk_config) == VTBLK_CFGSZ);

/*
 * Fixed-size block header
 */
struct virtio_blk_hdr {
#define	VBH_OP_READ		0
#define	VBH_OP_WRITE		1
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
	struct pci_devinst *vbsc_pi;
	int		vbsc_fd;
	int		vbsc_status;
	int		vbsc_isr;
	int		vbsc_lastq;
	uint32_t	vbsc_features;
	uint64_t	vbsc_pfn;
	struct vring_hqueue vbsc_q;
	struct vtblk_config vbsc_cfg;	
	uint16_t	msix_table_idx_req;
	uint16_t	msix_table_idx_cfg;
};
#define	vtblk_ctx(sc)	((sc)->vbsc_pi->pi_vmctx)

/* 
 * Return the size of IO BAR that maps virtio header and device specific
 * region. The size would vary depending on whether MSI-X is enabled or 
 * not
 */ 
static uint64_t
pci_vtblk_iosize(struct pci_devinst *pi)
{

	if (pci_msix_enabled(pi)) 
		return (VTBLK_REGSZ);
	else
		return (VTBLK_REGSZ - (VTCFG_R_CFG1 - VTCFG_R_MSIX));
}

/*
 * Return the number of available descriptors in the vring taking care
 * of the 16-bit index wraparound.
 */
static int
hq_num_avail(struct vring_hqueue *hq)
{
	uint16_t ndesc;

	/*
	 * We're just computing (a-b) in GF(216).
	 *
	 * The only glitch here is that in standard C,
	 * uint16_t promotes to (signed) int when int has
	 * more than 16 bits (pretty much always now), so
	 * we have to force it back to unsigned.
	 */
	ndesc = (unsigned)*hq->hq_avail_idx - (unsigned)hq->hq_cur_aidx;

	assert(ndesc <= hq->hq_size);

	return (ndesc);
}

static void
pci_vtblk_update_status(struct pci_vtblk_softc *sc, uint32_t value)
{
	if (value == 0) {
		DPRINTF(("vtblk: device reset requested !\n"));
		sc->vbsc_isr = 0;
		sc->msix_table_idx_req = VIRTIO_MSI_NO_VECTOR;
		sc->msix_table_idx_cfg = VIRTIO_MSI_NO_VECTOR;
		sc->vbsc_features = 0;
		sc->vbsc_pfn = 0;
		sc->vbsc_lastq = 0;
		memset(&sc->vbsc_q, 0, sizeof(struct vring_hqueue));
	}

	sc->vbsc_status = value;
}

static void
pci_vtblk_proc(struct pci_vtblk_softc *sc, struct vring_hqueue *hq)
{
	struct iovec iov[VTBLK_MAXSEGS];
	struct virtio_blk_hdr *vbh;
	struct virtio_desc *vd, *vid;
	struct virtio_used *vu;
	uint8_t *status;
	int i;
	int err;
	int iolen;
	int uidx, aidx, didx;
	int indirect, writeop, type;
	off_t offset;

	uidx = *hq->hq_used_idx;
	aidx = hq->hq_cur_aidx;
	didx = hq->hq_avail_ring[aidx % hq->hq_size];
	assert(didx >= 0 && didx < hq->hq_size);

	vd = &hq->hq_dtable[didx];

	indirect = ((vd->vd_flags & VRING_DESC_F_INDIRECT) != 0);

	if (indirect) {
		vid = paddr_guest2host(vtblk_ctx(sc), vd->vd_addr, vd->vd_len);
		vd = &vid[0];
	}

	/*
	 * The first descriptor will be the read-only fixed header
	 */
	vbh = paddr_guest2host(vtblk_ctx(sc), vd->vd_addr,
			    sizeof(struct virtio_blk_hdr));
	assert(vd->vd_len == sizeof(struct virtio_blk_hdr));
	assert(vd->vd_flags & VRING_DESC_F_NEXT);
	assert((vd->vd_flags & VRING_DESC_F_WRITE) == 0);

	/*
	 * XXX
	 * The guest should not be setting the BARRIER flag because
	 * we don't advertise the capability.
	 */
	type = vbh->vbh_type & ~VBH_FLAG_BARRIER;
	writeop = (type == VBH_OP_WRITE);

	offset = vbh->vbh_sector * DEV_BSIZE;

	/*
	 * Build up the iovec based on the guest's data descriptors
	 */
	i = iolen = 0;
	while (1) {
		if (indirect)
			vd = &vid[i + 1];	/* skip first indirect desc */
		else
			vd = &hq->hq_dtable[vd->vd_next];

		if ((vd->vd_flags & VRING_DESC_F_NEXT) == 0)
			break;

		if (i == VTBLK_MAXSEGS)
			break;

		/*
		 * - write op implies read-only descriptor,
		 * - read op implies write-only descriptor,
		 * therefore test the inverse of the descriptor bit
		 * to the op.
		 */
		assert(((vd->vd_flags & VRING_DESC_F_WRITE) == 0) ==
		       writeop);

		iov[i].iov_base = paddr_guest2host(vtblk_ctx(sc),
						   vd->vd_addr,
						   vd->vd_len);
		iov[i].iov_len = vd->vd_len;
		iolen += vd->vd_len;
		i++;
	}

	/* Lastly, get the address of the status byte */
	status = paddr_guest2host(vtblk_ctx(sc), vd->vd_addr, 1);
	assert(vd->vd_len == 1);
	assert((vd->vd_flags & VRING_DESC_F_NEXT) == 0);
	assert(vd->vd_flags & VRING_DESC_F_WRITE);

	DPRINTF(("virtio-block: %s op, %d bytes, %d segs, offset %ld\n\r", 
		 writeop ? "write" : "read", iolen, i, offset));

	if (writeop)
		err = pwritev(sc->vbsc_fd, iov, i, offset);
	else
		err = preadv(sc->vbsc_fd, iov, i, offset);

	*status = err < 0 ? VTBLK_S_IOERR : VTBLK_S_OK;

	/*
	 * Return the single descriptor back to the host
	 */
	vu = &hq->hq_used_ring[uidx % hq->hq_size];
	vu->vu_idx = didx;
	vu->vu_tlen = 1;
	hq->hq_cur_aidx++;
	*hq->hq_used_idx += 1;

	/*
	 * Generate an interrupt if able
	 */
	if ((*hq->hq_avail_flags & VRING_AVAIL_F_NO_INTERRUPT) == 0) { 
		if (use_msix) {
			pci_generate_msix(sc->vbsc_pi, sc->msix_table_idx_req);	
		} else if (sc->vbsc_isr == 0) {
			sc->vbsc_isr = 1;
			pci_generate_msi(sc->vbsc_pi, 0);
		}
	}
}

static void
pci_vtblk_qnotify(struct pci_vtblk_softc *sc)
{
	struct vring_hqueue *hq = &sc->vbsc_q;
	int ndescs;

	while ((ndescs = hq_num_avail(hq)) != 0) {
		/*
		 * Run through all the entries, placing them into iovecs and
		 * sending when an end-of-packet is found
		 */
 		pci_vtblk_proc(sc, hq);
 	}
}

static void
pci_vtblk_ring_init(struct pci_vtblk_softc *sc, uint64_t pfn)
{
	struct vring_hqueue *hq;

	sc->vbsc_pfn = pfn << VRING_PFN;
	
	/*
	 * Set up host pointers to the various parts of the
	 * queue
	 */
	hq = &sc->vbsc_q;
	hq->hq_size = VTBLK_RINGSZ;

	hq->hq_dtable = paddr_guest2host(vtblk_ctx(sc), pfn << VRING_PFN,
					 vring_size(VTBLK_RINGSZ));
	hq->hq_avail_flags =  (uint16_t *)(hq->hq_dtable + hq->hq_size);
	hq->hq_avail_idx = hq->hq_avail_flags + 1;
	hq->hq_avail_ring = hq->hq_avail_flags + 2;
	hq->hq_used_flags = (uint16_t *)roundup2((uintptr_t)hq->hq_avail_ring,
						 VRING_ALIGN);
	hq->hq_used_idx = hq->hq_used_flags + 1;
	hq->hq_used_ring = (struct virtio_used *)(hq->hq_used_flags + 2);

	/*
	 * Initialize queue indexes
	 */
	hq->hq_cur_aidx = 0;
}

static int
pci_vtblk_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	struct stat sbuf;
	struct pci_vtblk_softc *sc;
	off_t size;	
	int fd;
	int sectsz;
	const char *env_msi;

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

	sc = malloc(sizeof(struct pci_vtblk_softc));
	memset(sc, 0, sizeof(struct pci_vtblk_softc));

	pi->pi_arg = sc;
	sc->vbsc_pi = pi;
	sc->vbsc_fd = fd;

	/* setup virtio block config space */
	sc->vbsc_cfg.vbc_capacity = size / sectsz;
	sc->vbsc_cfg.vbc_seg_max = VTBLK_MAXSEGS;
	sc->vbsc_cfg.vbc_blk_size = sectsz;
	sc->vbsc_cfg.vbc_size_max = 0;	/* not negotiated */
	sc->vbsc_cfg.vbc_geom_c = 0;	/* no geometry */
	sc->vbsc_cfg.vbc_geom_h = 0;
	sc->vbsc_cfg.vbc_geom_s = 0;
	sc->vbsc_cfg.vbc_sectors_max = 0;

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_BLOCK);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_TYPE_BLOCK);

	if ((env_msi = getenv("BHYVE_USE_MSI"))) {
		if (strcasecmp(env_msi, "yes") == 0)
			use_msix = 0;
	} 

	if (use_msix) {
		/* MSI-X Support */
		sc->msix_table_idx_req = VIRTIO_MSI_NO_VECTOR;	
		sc->msix_table_idx_cfg = VIRTIO_MSI_NO_VECTOR;	
		
		if (pci_emul_add_msixcap(pi, 2, 1))
			return (1);
	} else {
		/* MSI Support */	
		pci_emul_add_msicap(pi, 1);
	}	
	
	pci_emul_alloc_bar(pi, 0, PCIBAR_IO, VTBLK_REGSZ);

	return (0);
}

static uint64_t
vtblk_adjust_offset(struct pci_devinst *pi, uint64_t offset)
{
	/*
	 * Device specific offsets used by guest would change 
	 * based on whether MSI-X capability is enabled or not
	 */ 
	if (!pci_msix_enabled(pi)) {
		if (offset >= VTCFG_R_MSIX) 
			return (offset + (VTCFG_R_CFG1 - VTCFG_R_MSIX));
	}

	return (offset);
}

static void
pci_vtblk_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
		int baridx, uint64_t offset, int size, uint64_t value)
{
	struct pci_vtblk_softc *sc = pi->pi_arg;

	if (use_msix) {
		if (baridx == pci_msix_table_bar(pi) ||
		    baridx == pci_msix_pba_bar(pi)) {
			pci_emul_msix_twrite(pi, offset, size, value);
			return;
		}
	}
	
	assert(baridx == 0);

	if (offset + size > pci_vtblk_iosize(pi)) {
		DPRINTF(("vtblk_write: 2big, offset %ld size %d\n",
			 offset, size));
		return;
	}

	offset = vtblk_adjust_offset(pi, offset);
	
	switch (offset) {
	case VTCFG_R_GUESTCAP:
		assert(size == 4);
		sc->vbsc_features = value & VTBLK_S_HOSTCAPS;
		break;
	case VTCFG_R_PFN:
		assert(size == 4);
		pci_vtblk_ring_init(sc, value);
		break;
	case VTCFG_R_QSEL:
		assert(size == 2);
		sc->vbsc_lastq = value;
		break;
	case VTCFG_R_QNOTIFY:
		assert(size == 2);
		assert(value == 0);
		pci_vtblk_qnotify(sc);
		break;
	case VTCFG_R_STATUS:
		assert(size == 1);
		pci_vtblk_update_status(sc, value);
		break;
	case VTCFG_R_CFGVEC:
		assert(size == 2);
		sc->msix_table_idx_cfg = value;	
		break;	
	case VTCFG_R_QVEC:
		assert(size == 2);
		sc->msix_table_idx_req = value;
		break;	
	case VTCFG_R_HOSTCAP:
	case VTCFG_R_QNUM:
	case VTCFG_R_ISR:
	case VTBLK_R_CFG ... VTBLK_R_CFG_END:
		DPRINTF(("vtblk: write to readonly reg %ld\n\r", offset));
		break;
	default:
		DPRINTF(("vtblk: unknown i/o write offset %ld\n\r", offset));
		value = 0;
		break;
	}
}

uint64_t
pci_vtblk_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	       int baridx, uint64_t offset, int size)
{
	struct pci_vtblk_softc *sc = pi->pi_arg;
	void *ptr;
	uint32_t value;

	if (use_msix) {
		if (baridx == pci_msix_table_bar(pi) ||
		    baridx == pci_msix_pba_bar(pi)) {
			return (pci_emul_msix_tread(pi, offset, size));
		}
	}

	assert(baridx == 0);

	if (offset + size > pci_vtblk_iosize(pi)) {
		DPRINTF(("vtblk_read: 2big, offset %ld size %d\n",
			 offset, size));
		return (0);
	}

	offset = vtblk_adjust_offset(pi, offset);

	switch (offset) {
	case VTCFG_R_HOSTCAP:
		assert(size == 4);
		value = VTBLK_S_HOSTCAPS;
		break;
	case VTCFG_R_GUESTCAP:
		assert(size == 4);
		value = sc->vbsc_features; /* XXX never read ? */
		break;
	case VTCFG_R_PFN:
		assert(size == 4);
		value = sc->vbsc_pfn >> VRING_PFN;
		break;
	case VTCFG_R_QNUM:
		value = (sc->vbsc_lastq == 0) ? VTBLK_RINGSZ: 0;
		break;
	case VTCFG_R_QSEL:
		assert(size == 2);
		value = sc->vbsc_lastq; /* XXX never read ? */
		break;
	case VTCFG_R_QNOTIFY:
		assert(size == 2);
		value = 0; /* XXX never read ? */
		break;
	case VTCFG_R_STATUS:
		assert(size == 1);
		value = sc->vbsc_status;
		break;
	case VTCFG_R_ISR:
		assert(size == 1);
		value = sc->vbsc_isr;
		sc->vbsc_isr = 0;     /* a read clears this flag */
		break;
	case VTCFG_R_CFGVEC:
		assert(size == 2);
		value = sc->msix_table_idx_cfg;
		break;
	case VTCFG_R_QVEC:
		assert(size == 2);
		value = sc->msix_table_idx_req;
		break;	
	case VTBLK_R_CFG ... VTBLK_R_CFG_END:
		assert(size + offset <= (VTBLK_R_CFG_END + 1));
		ptr = (uint8_t *)&sc->vbsc_cfg + offset - VTBLK_R_CFG;
		if (size == 1) {
			value = *(uint8_t *) ptr;
		} else if (size == 2) {
			value = *(uint16_t *) ptr;
		} else {
			value = *(uint32_t *) ptr;
		}
		break;
	default:
		DPRINTF(("vtblk: unknown i/o read offset %ld\n\r", offset));
		value = 0;
		break;
	}

	return (value);
}

struct pci_devemu pci_de_vblk = {
	.pe_emu =	"virtio-blk",
	.pe_init =	pci_vtblk_init,
	.pe_barwrite =	pci_vtblk_write,
	.pe_barread =	pci_vtblk_read
};
PCI_EMUL_SET(pci_de_vblk);
