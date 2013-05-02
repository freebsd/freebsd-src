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
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <md5.h>
#include <pthread.h>
#include <pthread_np.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "mevent.h"
#include "virtio.h"

#define VTNET_RINGSZ	1024

#define VTNET_MAXSEGS	32

/*
 * PCI config-space register offsets
 */
#define VTNET_R_CFG0	24
#define VTNET_R_CFG1	25
#define VTNET_R_CFG2	26
#define VTNET_R_CFG3	27
#define VTNET_R_CFG4	28
#define VTNET_R_CFG5	29
#define VTNET_R_CFG6	30
#define VTNET_R_CFG7	31
#define VTNET_R_MAX	31

#define VTNET_REGSZ	VTNET_R_MAX+1

/*
 * Host capabilities
 */
#define VTNET_S_HOSTCAPS      \
  ( 0x00000020 |	/* host supplies MAC */ \
    0x00008000 |	/* host can merge Rx buffers */ \
    0x00010000 )	/* config status available */

/*
 * Queue definitions.
 */
#define VTNET_RXQ	0
#define VTNET_TXQ	1
#define VTNET_CTLQ	2

#define VTNET_MAXQ	3

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
 * Fixed network header size
 */
struct virtio_net_rxhdr {
	uint8_t		vrh_flags;
	uint8_t		vrh_gso_type;
	uint16_t	vrh_hdr_len;
	uint16_t	vrh_gso_size;
	uint16_t	vrh_csum_start;
	uint16_t	vrh_csum_offset;
	uint16_t	vrh_bufs;
} __packed;

/*
 * Debug printf
 */
static int pci_vtnet_debug;
#define DPRINTF(params) if (pci_vtnet_debug) printf params
#define WPRINTF(params) printf params

/*
 * Per-device softc
 */
struct pci_vtnet_softc {
	struct pci_devinst *vsc_pi;
	pthread_mutex_t vsc_mtx;
	struct mevent	*vsc_mevp;

	int		vsc_curq;
	int		vsc_status;
	int		vsc_isr;
	int		vsc_tapfd;
	int		vsc_rx_ready;
	int		resetting;

	uint32_t	vsc_features;
	uint8_t		vsc_macaddr[6];

	uint64_t	vsc_pfn[VTNET_MAXQ];
	struct	vring_hqueue vsc_hq[VTNET_MAXQ];
	uint16_t	vsc_msix_table_idx[VTNET_MAXQ];

	pthread_mutex_t	rx_mtx;
	int		rx_in_progress;

	pthread_t 	tx_tid;
	pthread_mutex_t	tx_mtx;
	pthread_cond_t	tx_cond;
	int		tx_in_progress;
};
#define	vtnet_ctx(sc)	((sc)->vsc_pi->pi_vmctx)

/*
 * Return the size of IO BAR that maps virtio header and device specific
 * region. The size would vary depending on whether MSI-X is enabled or
 * not.
 */
static uint64_t
pci_vtnet_iosize(struct pci_devinst *pi)
{
	if (pci_msix_enabled(pi))
		return (VTNET_REGSZ);
	else
		return (VTNET_REGSZ - (VTCFG_R_CFG1 - VTCFG_R_MSIX));
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
	 * We're just computing (a-b) mod 2^16
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

static uint16_t
pci_vtnet_qsize(int qnum)
{
	/* XXX no ctl queue currently */
	if (qnum == VTNET_CTLQ) {
		return (0);
	}

	/* XXX fixed currently. Maybe different for tx/rx/ctl */
	return (VTNET_RINGSZ);
}

static void
pci_vtnet_ring_reset(struct pci_vtnet_softc *sc, int ring)
{
	struct vring_hqueue *hq;

	assert(ring < VTNET_MAXQ);

	hq = &sc->vsc_hq[ring];

	/*
	 * Reset all soft state
	 */
	hq->hq_cur_aidx = 0;
}

/*
 * If the transmit thread is active then stall until it is done.
 */
static void
pci_vtnet_txwait(struct pci_vtnet_softc *sc)
{

	pthread_mutex_lock(&sc->tx_mtx);
	while (sc->tx_in_progress) {
		pthread_mutex_unlock(&sc->tx_mtx);
		usleep(10000);
		pthread_mutex_lock(&sc->tx_mtx);
	}
	pthread_mutex_unlock(&sc->tx_mtx);
}

/*
 * If the receive thread is active then stall until it is done.
 */
static void
pci_vtnet_rxwait(struct pci_vtnet_softc *sc)
{

	pthread_mutex_lock(&sc->rx_mtx);
	while (sc->rx_in_progress) {
		pthread_mutex_unlock(&sc->rx_mtx);
		usleep(10000);
		pthread_mutex_lock(&sc->rx_mtx);
	}
	pthread_mutex_unlock(&sc->rx_mtx);
}

static void
pci_vtnet_update_status(struct pci_vtnet_softc *sc, uint32_t value)
{
	int i;

	if (value == 0) {
		DPRINTF(("vtnet: device reset requested !\n"));
		
		sc->resetting = 1;

		/*
		 * Wait for the transmit and receive threads to finish their
		 * processing.
		 */
		pci_vtnet_txwait(sc);
		pci_vtnet_rxwait(sc);

		sc->vsc_rx_ready = 0;
		pci_vtnet_ring_reset(sc, VTNET_RXQ);
		pci_vtnet_ring_reset(sc, VTNET_TXQ);

		for (i = 0; i < VTNET_MAXQ; i++)
			sc->vsc_msix_table_idx[i] = VIRTIO_MSI_NO_VECTOR;

		sc->vsc_isr = 0;
		sc->vsc_features = 0;

		sc->resetting = 0;
	}

	sc->vsc_status = value;
}

/*
 * Called to send a buffer chain out to the tap device
 */
static void
pci_vtnet_tap_tx(struct pci_vtnet_softc *sc, struct iovec *iov, int iovcnt,
		 int len)
{
	char pad[60];

	if (sc->vsc_tapfd == -1)
		return;

	/*
	 * If the length is < 60, pad out to that and add the
	 * extra zero'd segment to the iov. It is guaranteed that
	 * there is always an extra iov available by the caller.
	 */
	if (len < 60) {
		memset(pad, 0, 60 - len);
		iov[iovcnt].iov_base = pad;
		iov[iovcnt].iov_len = 60 - len;
		iovcnt++;
	}
	(void) writev(sc->vsc_tapfd, iov, iovcnt);
}

/*
 *  Called when there is read activity on the tap file descriptor.
 * Each buffer posted by the guest is assumed to be able to contain
 * an entire ethernet frame + rx header.
 *  MP note: the dummybuf is only used for discarding frames, so there
 * is no need for it to be per-vtnet or locked.
 */
static uint8_t dummybuf[2048];

static void
pci_vtnet_tap_rx(struct pci_vtnet_softc *sc)
{
	struct virtio_desc *vd;
	struct virtio_used *vu;
	struct vring_hqueue *hq;
	struct virtio_net_rxhdr *vrx;
	uint8_t *buf;
	int i;
	int len;
	int ndescs;
	int didx, uidx, aidx;	/* descriptor, avail and used index */

	/*
	 * Should never be called without a valid tap fd
	 */
	assert(sc->vsc_tapfd != -1);

	/*
	 * But, will be called when the rx ring hasn't yet
	 * been set up or the guest is resetting the device.
	 */
	if (!sc->vsc_rx_ready || sc->resetting) {
		/*
		 * Drop the packet and try later.
		 */
		(void) read(sc->vsc_tapfd, dummybuf, sizeof(dummybuf));
		return;
	}

	/*
	 * Calculate the number of available rx buffers
	 */
	hq = &sc->vsc_hq[VTNET_RXQ];

	ndescs = hq_num_avail(hq);

	if (ndescs == 0) {
		/*
		 * Drop the packet and try later
		 */
		(void) read(sc->vsc_tapfd, dummybuf, sizeof(dummybuf));
		return;
	}

	aidx = hq->hq_cur_aidx;
	uidx = *hq->hq_used_idx;
	for (i = 0; i < ndescs; i++) {
		/*
		 * 'aidx' indexes into the an array of descriptor indexes
		 */
		didx = hq->hq_avail_ring[aidx % hq->hq_size];
		assert(didx >= 0 && didx < hq->hq_size);

		vd = &hq->hq_dtable[didx];

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vrx = paddr_guest2host(vtnet_ctx(sc), vd->vd_addr, vd->vd_len);
		buf = (uint8_t *)(vrx + 1);

		len = read(sc->vsc_tapfd, buf,
			   vd->vd_len - sizeof(struct virtio_net_rxhdr));

		if (len < 0 && errno == EWOULDBLOCK) {
			break;
		}

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers, which is always 1 without TSO
		 * support.
		 */
		memset(vrx, 0, sizeof(struct virtio_net_rxhdr));
		vrx->vrh_bufs = 1;

		/*
		 * Write this descriptor into the used ring
		 */
		vu = &hq->hq_used_ring[uidx % hq->hq_size];
		vu->vu_idx = didx;
		vu->vu_tlen = len + sizeof(struct virtio_net_rxhdr);
		uidx++;
		aidx++;
	}

	/*
	 * Update the used pointer, and signal an interrupt if allowed
	 */
	*hq->hq_used_idx = uidx;
	hq->hq_cur_aidx = aidx;

	if ((*hq->hq_avail_flags & VRING_AVAIL_F_NO_INTERRUPT) == 0) {
		if (use_msix) {
			pci_generate_msix(sc->vsc_pi,
					  sc->vsc_msix_table_idx[VTNET_RXQ]);
		} else {
			sc->vsc_isr |= 1;
			pci_generate_msi(sc->vsc_pi, 0);
		}
	}
}

static void
pci_vtnet_tap_callback(int fd, enum ev_type type, void *param)
{
	struct pci_vtnet_softc *sc = param;

	pthread_mutex_lock(&sc->rx_mtx);
	sc->rx_in_progress = 1;
	pci_vtnet_tap_rx(sc);
	sc->rx_in_progress = 0;
	pthread_mutex_unlock(&sc->rx_mtx);

}

static void
pci_vtnet_ping_rxq(struct pci_vtnet_softc *sc)
{
	/*
	 * A qnotify means that the rx process can now begin
	 */
	if (sc->vsc_rx_ready == 0) {
		sc->vsc_rx_ready = 1;
	}
}

static void
pci_vtnet_proctx(struct pci_vtnet_softc *sc, struct vring_hqueue *hq)
{
	struct iovec iov[VTNET_MAXSEGS + 1];
	struct virtio_desc *vd;
	struct virtio_used *vu;
	int i;
	int plen;
	int tlen;
	int uidx, aidx, didx;

	uidx = *hq->hq_used_idx;
	aidx = hq->hq_cur_aidx;
	didx = hq->hq_avail_ring[aidx % hq->hq_size];
	assert(didx >= 0 && didx < hq->hq_size);

	vd = &hq->hq_dtable[didx];

	/*
	 * Run through the chain of descriptors, ignoring the
	 * first header descriptor. However, include the header
	 * length in the total length that will be put into the
	 * used queue.
	 */
	tlen = vd->vd_len;
	vd = &hq->hq_dtable[vd->vd_next];

	for (i = 0, plen = 0;
	     i < VTNET_MAXSEGS;
	     i++, vd = &hq->hq_dtable[vd->vd_next]) {
		iov[i].iov_base = paddr_guest2host(vtnet_ctx(sc),
						   vd->vd_addr, vd->vd_len);
		iov[i].iov_len = vd->vd_len;
		plen += vd->vd_len;
		tlen += vd->vd_len;

		if ((vd->vd_flags & VRING_DESC_F_NEXT) == 0)
			break;
	}
	assert(i < VTNET_MAXSEGS);

	DPRINTF(("virtio: packet send, %d bytes, %d segs\n\r", plen, i + 1));
	pci_vtnet_tap_tx(sc, iov, i + 1, plen);

	/*
	 * Return this chain back to the host
	 */
	vu = &hq->hq_used_ring[uidx % hq->hq_size];
	vu->vu_idx = didx;
	vu->vu_tlen = tlen;
	hq->hq_cur_aidx = aidx + 1;
	*hq->hq_used_idx = uidx + 1;
}

static void
pci_vtnet_ping_txq(struct pci_vtnet_softc *sc)
{
	struct vring_hqueue *hq = &sc->vsc_hq[VTNET_TXQ];
	int ndescs;

	/*
	 * Calculate number of ring entries to process
	 */
	ndescs = hq_num_avail(hq);

	if (ndescs == 0)
		return;

	/* Signal the tx thread for processing */
	pthread_mutex_lock(&sc->tx_mtx);
	if (sc->tx_in_progress == 0)
		pthread_cond_signal(&sc->tx_cond);
	pthread_mutex_unlock(&sc->tx_mtx);
}

/*
 * Thread which will handle processing of TX desc
 */
static void *
pci_vtnet_tx_thread(void *param)
{
	struct pci_vtnet_softc *sc = (struct pci_vtnet_softc *) param;
	struct vring_hqueue *hq; 
	int i, ndescs, needintr,error;
	
	needintr = 0;
	hq = &sc->vsc_hq[VTNET_TXQ];
	
	/* 
	 * Let us wait till the tx queue pointers get initialised & 
	 * first tx signaled 
	 */
	pthread_mutex_lock(&sc->tx_mtx);
	error = pthread_cond_wait(&sc->tx_cond, &sc->tx_mtx);
	assert(error == 0);
	
	for (;;) {
		pthread_mutex_lock(&sc->tx_mtx);
		for (;;) {
			if (sc->resetting) {
				ndescs = 0;
				needintr = 0;
			} else
				ndescs = hq_num_avail(hq);
			
			if (ndescs != 0) 
				break;
			
			if (needintr) {
				/*
				 * Generate an interrupt if able
				 */
				if ((*hq->hq_avail_flags &
				     VRING_AVAIL_F_NO_INTERRUPT) == 0) {
					if (use_msix) {
						pci_generate_msix(sc->vsc_pi,
						     sc->vsc_msix_table_idx[VTNET_TXQ]);
					} else {
						sc->vsc_isr |= 1;
						pci_generate_msi(sc->vsc_pi, 0);
					}
				}
			}
			needintr = 0;
			sc->tx_in_progress = 0;
			error = pthread_cond_wait(&sc->tx_cond, &sc->tx_mtx);
			assert(error == 0);
		}
		sc->tx_in_progress = 1;
		pthread_mutex_unlock(&sc->tx_mtx);

		while (ndescs > 0) {
			/*
			 * Run through all the entries, placing them into
			 * iovecs and sending when an end-of-packet is found
			 */
			for (i = 0; i < ndescs; i++)
				pci_vtnet_proctx(sc, hq);
			needintr = 1;
			ndescs = hq_num_avail(hq);
		}
	}
}	

static void
pci_vtnet_ping_ctlq(struct pci_vtnet_softc *sc)
{

	DPRINTF(("vtnet: control qnotify!\n\r"));	
}

static void
pci_vtnet_ring_init(struct pci_vtnet_softc *sc, uint64_t pfn)
{
	struct vring_hqueue *hq;
	int qnum = sc->vsc_curq;

	assert(qnum < VTNET_MAXQ);

	sc->vsc_pfn[qnum] = pfn << VRING_PFN;
	
	/*
	 * Set up host pointers to the various parts of the
	 * queue
	 */
	hq = &sc->vsc_hq[qnum];
	hq->hq_size = pci_vtnet_qsize(qnum);

	hq->hq_dtable = paddr_guest2host(vtnet_ctx(sc), pfn << VRING_PFN,
					 vring_size(hq->hq_size));
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
pci_vtnet_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	MD5_CTX mdctx;
	unsigned char digest[16];
	char nstr[80];
	char tname[MAXCOMLEN + 1];
	struct pci_vtnet_softc *sc;
	const char *env_msi;

	sc = malloc(sizeof(struct pci_vtnet_softc));
	memset(sc, 0, sizeof(struct pci_vtnet_softc));

	pi->pi_arg = sc;
	sc->vsc_pi = pi;

	pthread_mutex_init(&sc->vsc_mtx, NULL);
 
	/*
	 * Use MSI if set by user
	 */
	if ((env_msi = getenv("BHYVE_USE_MSI")) != NULL) {
		if (strcasecmp(env_msi, "yes") == 0)
			use_msix = 0;
	}

	/*
	 * Attempt to open the tap device
	 */
	sc->vsc_tapfd = -1;
	if (opts != NULL) {
		char tbuf[80];

		strcpy(tbuf, "/dev/");
		strlcat(tbuf, opts, sizeof(tbuf));

		sc->vsc_tapfd = open(tbuf, O_RDWR);
		if (sc->vsc_tapfd == -1) {
			WPRINTF(("open of tap device %s failed\n", tbuf));
		} else {
			/*
			 * Set non-blocking and register for read
			 * notifications with the event loop
			 */
			int opt = 1;
			if (ioctl(sc->vsc_tapfd, FIONBIO, &opt) < 0) {
				WPRINTF(("tap device O_NONBLOCK failed\n"));
				close(sc->vsc_tapfd);
				sc->vsc_tapfd = -1;
			}

			sc->vsc_mevp = mevent_add(sc->vsc_tapfd,
						  EVF_READ,
						  pci_vtnet_tap_callback,
						  sc);
			if (sc->vsc_mevp == NULL) {
				WPRINTF(("Could not register event\n"));
				close(sc->vsc_tapfd);
				sc->vsc_tapfd = -1;
			}
		}		
	}

	/*
	 * The MAC address is the standard NetApp OUI of 00-a0-98,
	 * followed by an MD5 of the vm name. The slot/func number is
	 * prepended to this for slots other than 1:0, so that 
	 * a bootloader can netboot from the equivalent of slot 1.
	 */
	if (pi->pi_slot == 1 && pi->pi_func == 0) {
		strncpy(nstr, vmname, sizeof(nstr));
	} else {
		snprintf(nstr, sizeof(nstr), "%d-%d-%s", pi->pi_slot,
		    pi->pi_func, vmname);
	}

	MD5Init(&mdctx);
	MD5Update(&mdctx, nstr, strlen(nstr));
	MD5Final(digest, &mdctx);

	sc->vsc_macaddr[0] = 0x00;
	sc->vsc_macaddr[1] = 0xa0;
	sc->vsc_macaddr[2] = 0x98;
	sc->vsc_macaddr[3] = digest[0];
	sc->vsc_macaddr[4] = digest[1];
	sc->vsc_macaddr[5] = digest[2];

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_NET);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_NETWORK);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_TYPE_NET);
	
	if (use_msix) {
		/* MSI-X support */
		int i;

		for (i = 0; i < VTNET_MAXQ; i++)
			sc->vsc_msix_table_idx[i] = VIRTIO_MSI_NO_VECTOR;

		/*
		 * BAR 1 used to map MSI-X table and PBA
		 */
		if (pci_emul_add_msixcap(pi, VTNET_MAXQ, 1))
			return (1);
	} else {
		/* MSI support */
		pci_emul_add_msicap(pi, 1);
	}
	
	pci_emul_alloc_bar(pi, 0, PCIBAR_IO, VTNET_REGSZ);

	sc->resetting = 0;

	sc->rx_in_progress = 0;
	pthread_mutex_init(&sc->rx_mtx, NULL); 

	/* 
	 * Initialize tx semaphore & spawn TX processing thread
	 * As of now, only one thread for TX desc processing is
	 * spawned. 
	 */
	sc->tx_in_progress = 0;
	pthread_mutex_init(&sc->tx_mtx, NULL);
	pthread_cond_init(&sc->tx_cond, NULL);
	pthread_create(&sc->tx_tid, NULL, pci_vtnet_tx_thread, (void *)sc);
        snprintf(tname, sizeof(tname), "%s vtnet%d tx", vmname, pi->pi_slot);
        pthread_set_name_np(sc->tx_tid, tname);

	return (0);
}

/*
 * Function pointer array to handle queue notifications
 */
static void (*pci_vtnet_qnotify[VTNET_MAXQ])(struct pci_vtnet_softc *) = {
	pci_vtnet_ping_rxq,
	pci_vtnet_ping_txq,
	pci_vtnet_ping_ctlq
};

static uint64_t
vtnet_adjust_offset(struct pci_devinst *pi, uint64_t offset)
{
	/*
	 * Device specific offsets used by guest would change based on
	 * whether MSI-X capability is enabled or not
	 */
	if (!pci_msix_enabled(pi)) {
		if (offset >= VTCFG_R_MSIX)
			return (offset + (VTCFG_R_CFG1 - VTCFG_R_MSIX));
	}

	return (offset);
}

static void
pci_vtnet_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
		int baridx, uint64_t offset, int size, uint64_t value)
{
	struct pci_vtnet_softc *sc = pi->pi_arg;
	void *ptr;

	if (use_msix) {
		if (baridx == pci_msix_table_bar(pi) ||
		    baridx == pci_msix_pba_bar(pi)) {
			pci_emul_msix_twrite(pi, offset, size, value);
			return;
		}
	}

	assert(baridx == 0);

	if (offset + size > pci_vtnet_iosize(pi)) {
		DPRINTF(("vtnet_write: 2big, offset %ld size %d\n",
			 offset, size));
		return;
	}

	pthread_mutex_lock(&sc->vsc_mtx);

	offset = vtnet_adjust_offset(pi, offset);

	switch (offset) {
	case VTCFG_R_GUESTCAP:
		assert(size == 4);
		sc->vsc_features = value & VTNET_S_HOSTCAPS;
		break;
	case VTCFG_R_PFN:
		assert(size == 4);
		pci_vtnet_ring_init(sc, value);
		break;
	case VTCFG_R_QSEL:
		assert(size == 2);
		assert(value < VTNET_MAXQ);
		sc->vsc_curq = value;
		break;
	case VTCFG_R_QNOTIFY:
		assert(size == 2);
		assert(value < VTNET_MAXQ);
		(*pci_vtnet_qnotify[value])(sc);
		break;
	case VTCFG_R_STATUS:
		assert(size == 1);
		pci_vtnet_update_status(sc, value);
		break;
	case VTCFG_R_CFGVEC:
		assert(size == 2);
		sc->vsc_msix_table_idx[VTNET_CTLQ] = value;
		break;
	case VTCFG_R_QVEC:
		assert(size == 2);
		assert(sc->vsc_curq != VTNET_CTLQ);
		sc->vsc_msix_table_idx[sc->vsc_curq] = value;
		break;
	case VTNET_R_CFG0:
	case VTNET_R_CFG1:
	case VTNET_R_CFG2:
	case VTNET_R_CFG3:
	case VTNET_R_CFG4:
	case VTNET_R_CFG5:
		assert((size + offset) <= (VTNET_R_CFG5 + 1));
		ptr = &sc->vsc_macaddr[offset - VTNET_R_CFG0];
		/*
		 * The driver is allowed to change the MAC address
		 */
		sc->vsc_macaddr[offset - VTNET_R_CFG0] = value;
		if (size == 1) {
			*(uint8_t *) ptr = value;
		} else if (size == 2) {
			*(uint16_t *) ptr = value;
		} else {
			*(uint32_t *) ptr = value;
		}
		break;
	case VTCFG_R_HOSTCAP:
	case VTCFG_R_QNUM:
	case VTCFG_R_ISR:
	case VTNET_R_CFG6:
	case VTNET_R_CFG7:
		DPRINTF(("vtnet: write to readonly reg %ld\n\r", offset));
		break;
	default:
		DPRINTF(("vtnet: unknown i/o write offset %ld\n\r", offset));
		value = 0;
		break;
	}

	pthread_mutex_unlock(&sc->vsc_mtx);
}

uint64_t
pci_vtnet_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	       int baridx, uint64_t offset, int size)
{
	struct pci_vtnet_softc *sc = pi->pi_arg;
	void *ptr;
	uint64_t value;

	if (use_msix) {
		if (baridx == pci_msix_table_bar(pi) ||
		    baridx == pci_msix_pba_bar(pi)) {
			return (pci_emul_msix_tread(pi, offset, size));
		}
	}

	assert(baridx == 0);

	if (offset + size > pci_vtnet_iosize(pi)) {
		DPRINTF(("vtnet_read: 2big, offset %ld size %d\n",
			 offset, size));
		return (0);
	}

	pthread_mutex_lock(&sc->vsc_mtx);

	offset = vtnet_adjust_offset(pi, offset);

	switch (offset) {
	case VTCFG_R_HOSTCAP:
		assert(size == 4);
		value = VTNET_S_HOSTCAPS;
		break;
	case VTCFG_R_GUESTCAP:
		assert(size == 4);
		value = sc->vsc_features; /* XXX never read ? */
		break;
	case VTCFG_R_PFN:
		assert(size == 4);
		value = sc->vsc_pfn[sc->vsc_curq] >> VRING_PFN;
		break;
	case VTCFG_R_QNUM:
		assert(size == 2);
		value = pci_vtnet_qsize(sc->vsc_curq);
		break;
	case VTCFG_R_QSEL:
		assert(size == 2);
		value = sc->vsc_curq;  /* XXX never read ? */
		break;
	case VTCFG_R_QNOTIFY:
		assert(size == 2);
		value = sc->vsc_curq;  /* XXX never read ? */
		break;
	case VTCFG_R_STATUS:
		assert(size == 1);
		value = sc->vsc_status;
		break;
	case VTCFG_R_ISR:
		assert(size == 1);
		value = sc->vsc_isr;
		sc->vsc_isr = 0;     /* a read clears this flag */
		break;
	case VTCFG_R_CFGVEC:
		assert(size == 2);
		value = sc->vsc_msix_table_idx[VTNET_CTLQ];
		break;
	case VTCFG_R_QVEC:
		assert(size == 2);
		assert(sc->vsc_curq != VTNET_CTLQ);
		value = sc->vsc_msix_table_idx[sc->vsc_curq];
		break;
	case VTNET_R_CFG0:
	case VTNET_R_CFG1:
	case VTNET_R_CFG2:
	case VTNET_R_CFG3:
	case VTNET_R_CFG4:
	case VTNET_R_CFG5:
		assert((size + offset) <= (VTNET_R_CFG5 + 1));
		ptr = &sc->vsc_macaddr[offset - VTNET_R_CFG0];
		if (size == 1) {
			value = *(uint8_t *) ptr;
		} else if (size == 2) {
			value = *(uint16_t *) ptr;
		} else {
			value = *(uint32_t *) ptr;
		}
		break;
	case VTNET_R_CFG6:
		assert(size != 4);
		value = 0x01; /* XXX link always up */
		break;
	case VTNET_R_CFG7:
		assert(size == 1);
		value = 0; /* XXX link status in LSB */
		break;
	default:
		DPRINTF(("vtnet: unknown i/o read offset %ld\n\r", offset));
		value = 0;
		break;
	}

	pthread_mutex_unlock(&sc->vsc_mtx);

	return (value);
}

struct pci_devemu pci_de_vnet = {
	.pe_emu = 	"virtio-net",
	.pe_init =	pci_vtnet_init,
	.pe_barwrite =	pci_vtnet_write,
	.pe_barread =	pci_vtnet_read
};
PCI_EMUL_SET(pci_de_vnet);
