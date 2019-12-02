/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
#include <net/ethernet.h>
#include <net/if.h> /* IFNAMSIZ */

#include <err.h>
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
#include <pthread_np.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "mevent.h"
#include "virtio.h"
#include "net_utils.h"
#include "net_backends.h"
#include "iov.h"

#define VTNET_RINGSZ	1024

#define VTNET_MAXSEGS	256

#define VTNET_MAX_PKT_LEN	(65536 + 64)

#define VTNET_S_HOSTCAPS      \
  ( VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS | \
    VIRTIO_F_NOTIFY_ON_EMPTY | VIRTIO_RING_F_INDIRECT_DESC)

/*
 * PCI config-space "registers"
 */
struct virtio_net_config {
	uint8_t  mac[6];
	uint16_t status;
} __packed;

/*
 * Queue definitions.
 */
#define VTNET_RXQ	0
#define VTNET_TXQ	1
#define VTNET_CTLQ	2	/* NB: not yet supported */

#define VTNET_MAXQ	3

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
	struct virtio_softc vsc_vs;
	struct vqueue_info vsc_queues[VTNET_MAXQ - 1];
	pthread_mutex_t vsc_mtx;

	net_backend_t	*vsc_be;

	int		resetting;	/* protected by tx_mtx */

	uint64_t	vsc_features;	/* negotiated features */
	
	pthread_mutex_t	rx_mtx;
	int		rx_merge;	/* merged rx bufs in use */

	pthread_t 	tx_tid;
	pthread_mutex_t	tx_mtx;
	pthread_cond_t	tx_cond;
	int		tx_in_progress;

	struct virtio_net_config vsc_config;
	struct virtio_consts vsc_consts;
};

static void pci_vtnet_reset(void *);
/* static void pci_vtnet_notify(void *, struct vqueue_info *); */
static int pci_vtnet_cfgread(void *, int, int, uint32_t *);
static int pci_vtnet_cfgwrite(void *, int, int, uint32_t);
static void pci_vtnet_neg_features(void *, uint64_t);

static struct virtio_consts vtnet_vi_consts = {
	"vtnet",		/* our name */
	VTNET_MAXQ - 1,		/* we currently support 2 virtqueues */
	sizeof(struct virtio_net_config), /* config reg size */
	pci_vtnet_reset,	/* reset */
	NULL,			/* device-wide qnotify -- not used */
	pci_vtnet_cfgread,	/* read PCI config */
	pci_vtnet_cfgwrite,	/* write PCI config */
	pci_vtnet_neg_features,	/* apply negotiated features */
	VTNET_S_HOSTCAPS,	/* our capabilities */
};

static void
pci_vtnet_reset(void *vsc)
{
	struct pci_vtnet_softc *sc = vsc;

	DPRINTF(("vtnet: device reset requested !\n\r"));

	/* Acquire the RX lock to block RX processing. */
	pthread_mutex_lock(&sc->rx_mtx);

	/*
	 * Make sure receive operation is disabled at least until we
	 * re-negotiate the features, since receive operation depends
	 * on the value of sc->rx_merge and the header length, which
	 * are both set in pci_vtnet_neg_features().
	 * Receive operation will be enabled again once the guest adds
	 * the first receive buffers and kicks us.
	 */
	netbe_rx_disable(sc->vsc_be);

	/* Set sc->resetting and give a chance to the TX thread to stop. */
	pthread_mutex_lock(&sc->tx_mtx);
	sc->resetting = 1;
	while (sc->tx_in_progress) {
		pthread_mutex_unlock(&sc->tx_mtx);
		usleep(10000);
		pthread_mutex_lock(&sc->tx_mtx);
	}

	/*
	 * Now reset rings, MSI-X vectors, and negotiated capabilities.
	 * Do that with the TX lock held, since we need to reset
	 * sc->resetting.
	 */
	vi_reset_dev(&sc->vsc_vs);

	sc->resetting = 0;
	pthread_mutex_unlock(&sc->tx_mtx);
	pthread_mutex_unlock(&sc->rx_mtx);
}

struct virtio_mrg_rxbuf_info {
	uint16_t idx;
	uint16_t pad;
	uint32_t len;
};

static void
pci_vtnet_rx(struct pci_vtnet_softc *sc)
{
	struct virtio_mrg_rxbuf_info info[VTNET_MAXSEGS];
	struct iovec iov[VTNET_MAXSEGS + 1];
	struct vqueue_info *vq;
	uint32_t cur_iov_bytes;
	struct iovec *cur_iov;
	uint16_t cur_iov_len;
	uint32_t ulen;
	int n_chains;
	int len;

	vq = &sc->vsc_queues[VTNET_RXQ];
	for (;;) {
		/*
		 * Get a descriptor chain to store the next ingress
		 * packet. In case of mergeable rx buffers, get as
		 * many chains as necessary in order to make room
		 * for a maximum sized LRO packet.
		 */
		cur_iov_bytes = 0;
		cur_iov_len = 0;
		cur_iov = iov;
		n_chains = 0;
		do {
			int n = vq_getchain(vq, &info[n_chains].idx, cur_iov,
			    VTNET_MAXSEGS - cur_iov_len, NULL);

			if (n == 0) {
				/*
				 * No rx buffers. Enable RX kicks and double
				 * check.
				 */
				vq_kick_enable(vq);
				if (!vq_has_descs(vq)) {
					/*
					 * Still no buffers. Return the unused
					 * chains (if any), interrupt if needed
					 * (including for NOTIFY_ON_EMPTY), and
					 * disable the backend until the next
					 * kick.
					 */
					vq_retchains(vq, n_chains);
					vq_endchains(vq, /*used_all_avail=*/1);
					netbe_rx_disable(sc->vsc_be);
					return;
				}

				/* More rx buffers found, so keep going. */
				vq_kick_disable(vq);
				continue;
			}
			assert(n >= 1 && cur_iov_len + n <= VTNET_MAXSEGS);
			cur_iov_len += n;
			if (!sc->rx_merge) {
				n_chains = 1;
				break;
			}
			info[n_chains].len = (uint32_t)count_iov(cur_iov, n);
			cur_iov_bytes += info[n_chains].len;
			cur_iov += n;
			n_chains++;
		} while (cur_iov_bytes < VTNET_MAX_PKT_LEN &&
			    cur_iov_len < VTNET_MAXSEGS);

		len = netbe_recv(sc->vsc_be, iov, cur_iov_len);

		if (len <= 0) {
			/*
			 * No more packets (len == 0), or backend errored
			 * (err < 0). Return unused available buffers
			 * and stop.
			 */
			vq_retchains(vq, n_chains);
			/* Interrupt if needed/appropriate and stop. */
			vq_endchains(vq, /*used_all_avail=*/0);
			return;
		}

		ulen = (uint32_t)len; /* avoid too many casts below */

		/* Publish the used buffers to the guest. */
		if (!sc->rx_merge) {
			vq_relchain(vq, info[0].idx, ulen);
		} else {
			struct virtio_net_rxhdr *hdr = iov[0].iov_base;
			uint32_t iolen;
			int i = 0;

			assert(iov[0].iov_len >= sizeof(*hdr));

			do {
				iolen = info[i].len;
				if (iolen > ulen) {
					iolen = ulen;
				}
				vq_relchain_prepare(vq, info[i].idx, iolen);
				ulen -= iolen;
				i++;
				assert(i <= n_chains);
			} while (ulen > 0);

			hdr->vrh_bufs = i;
			vq_relchain_publish(vq);
			vq_retchains(vq, n_chains - i);
		}
	}

}

/*
 * Called when there is read activity on the backend file descriptor.
 * Each buffer posted by the guest is assumed to be able to contain
 * an entire ethernet frame + rx header.
 */
static void
pci_vtnet_rx_callback(int fd, enum ev_type type, void *param)
{
	struct pci_vtnet_softc *sc = param;

	pthread_mutex_lock(&sc->rx_mtx);
	pci_vtnet_rx(sc);
	pthread_mutex_unlock(&sc->rx_mtx);

}

/* Called on RX kick. */
static void
pci_vtnet_ping_rxq(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtnet_softc *sc = vsc;

	/*
	 * A qnotify means that the rx process can now begin.
	 */
	pthread_mutex_lock(&sc->rx_mtx);
	vq_kick_disable(vq);
	netbe_rx_enable(sc->vsc_be);
	pthread_mutex_unlock(&sc->rx_mtx);
}

/* TX virtqueue processing, called by the TX thread. */
static void
pci_vtnet_proctx(struct pci_vtnet_softc *sc, struct vqueue_info *vq)
{
	struct iovec iov[VTNET_MAXSEGS + 1];
	uint16_t idx;
	ssize_t len;
	int n;

	/*
	 * Obtain chain of descriptors. The first descriptor also
	 * contains the virtio-net header.
	 */
	n = vq_getchain(vq, &idx, iov, VTNET_MAXSEGS, NULL);
	assert(n >= 1 && n <= VTNET_MAXSEGS);

	len = netbe_send(sc->vsc_be, iov, n);

	/* chain is processed, release it and set len */
	vq_relchain(vq, idx, len > 0 ? len : 0);
}

/* Called on TX kick. */
static void
pci_vtnet_ping_txq(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtnet_softc *sc = vsc;

	/*
	 * Any ring entries to process?
	 */
	if (!vq_has_descs(vq))
		return;

	/* Signal the tx thread for processing */
	pthread_mutex_lock(&sc->tx_mtx);
	vq_kick_disable(vq);
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
	struct pci_vtnet_softc *sc = param;
	struct vqueue_info *vq;
	int error;

	vq = &sc->vsc_queues[VTNET_TXQ];

	/*
	 * Let us wait till the tx queue pointers get initialised &
	 * first tx signaled
	 */
	pthread_mutex_lock(&sc->tx_mtx);
	error = pthread_cond_wait(&sc->tx_cond, &sc->tx_mtx);
	assert(error == 0);

	for (;;) {
		/* note - tx mutex is locked here */
		while (sc->resetting || !vq_has_descs(vq)) {
			vq_kick_enable(vq);
			if (!sc->resetting && vq_has_descs(vq))
				break;

			sc->tx_in_progress = 0;
			error = pthread_cond_wait(&sc->tx_cond, &sc->tx_mtx);
			assert(error == 0);
		}
		vq_kick_disable(vq);
		sc->tx_in_progress = 1;
		pthread_mutex_unlock(&sc->tx_mtx);

		do {
			/*
			 * Run through entries, placing them into
			 * iovecs and sending when an end-of-packet
			 * is found
			 */
			pci_vtnet_proctx(sc, vq);
		} while (vq_has_descs(vq));

		/*
		 * Generate an interrupt if needed.
		 */
		vq_endchains(vq, /*used_all_avail=*/1);

		pthread_mutex_lock(&sc->tx_mtx);
	}
}

#ifdef notyet
static void
pci_vtnet_ping_ctlq(void *vsc, struct vqueue_info *vq)
{

	DPRINTF(("vtnet: control qnotify!\n\r"));
}
#endif

static int
pci_vtnet_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	struct pci_vtnet_softc *sc;
	char tname[MAXCOMLEN + 1];
	int mac_provided;

	/*
	 * Allocate data structures for further virtio initializations.
	 * sc also contains a copy of vtnet_vi_consts, since capabilities
	 * change depending on the backend.
	 */
	sc = calloc(1, sizeof(struct pci_vtnet_softc));

	sc->vsc_consts = vtnet_vi_consts;
	pthread_mutex_init(&sc->vsc_mtx, NULL);

	sc->vsc_queues[VTNET_RXQ].vq_qsize = VTNET_RINGSZ;
	sc->vsc_queues[VTNET_RXQ].vq_notify = pci_vtnet_ping_rxq;
	sc->vsc_queues[VTNET_TXQ].vq_qsize = VTNET_RINGSZ;
	sc->vsc_queues[VTNET_TXQ].vq_notify = pci_vtnet_ping_txq;
#ifdef notyet
	sc->vsc_queues[VTNET_CTLQ].vq_qsize = VTNET_RINGSZ;
        sc->vsc_queues[VTNET_CTLQ].vq_notify = pci_vtnet_ping_ctlq;
#endif
 
	/*
	 * Attempt to open the backend device and read the MAC address
	 * if specified.
	 */
	mac_provided = 0;
	if (opts != NULL) {
		char *devname;
		char *vtopts;
		int err;

		devname = vtopts = strdup(opts);
		(void) strsep(&vtopts, ",");

		if (vtopts != NULL) {
			err = net_parsemac(vtopts, sc->vsc_config.mac);
			if (err != 0) {
				free(devname);
				free(sc);
				return (err);
			}
			mac_provided = 1;
		}

		err = netbe_init(&sc->vsc_be, devname, pci_vtnet_rx_callback,
		          sc);
		free(devname);
		if (err) {
			free(sc);
			return (err);
		}
		sc->vsc_consts.vc_hv_caps |= netbe_get_cap(sc->vsc_be);
	}

	if (!mac_provided) {
		net_genmac(pi, sc->vsc_config.mac);
	}

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_NET);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_NETWORK);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_TYPE_NET);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	/* Link is up if we managed to open backend device. */
	sc->vsc_config.status = (opts == NULL || sc->vsc_be);
	
	vi_softc_linkup(&sc->vsc_vs, &sc->vsc_consts, sc, pi, sc->vsc_queues);
	sc->vsc_vs.vs_mtx = &sc->vsc_mtx;

	/* use BAR 1 to map MSI-X table and PBA, if we're using MSI-X */
	if (vi_intr_init(&sc->vsc_vs, 1, fbsdrun_virtio_msix())) {
		free(sc);
		return (1);
	}

	/* use BAR 0 to map config regs in IO space */
	vi_set_io_bar(&sc->vsc_vs, 0);

	sc->resetting = 0;

	sc->rx_merge = 0;
	pthread_mutex_init(&sc->rx_mtx, NULL); 

	/* 
	 * Initialize tx semaphore & spawn TX processing thread.
	 * As of now, only one thread for TX desc processing is
	 * spawned. 
	 */
	sc->tx_in_progress = 0;
	pthread_mutex_init(&sc->tx_mtx, NULL);
	pthread_cond_init(&sc->tx_cond, NULL);
	pthread_create(&sc->tx_tid, NULL, pci_vtnet_tx_thread, (void *)sc);
	snprintf(tname, sizeof(tname), "vtnet-%d:%d tx", pi->pi_slot,
	    pi->pi_func);
	pthread_set_name_np(sc->tx_tid, tname);

	return (0);
}

static int
pci_vtnet_cfgwrite(void *vsc, int offset, int size, uint32_t value)
{
	struct pci_vtnet_softc *sc = vsc;
	void *ptr;

	if (offset < (int)sizeof(sc->vsc_config.mac)) {
		assert(offset + size <= (int)sizeof(sc->vsc_config.mac));
		/*
		 * The driver is allowed to change the MAC address
		 */
		ptr = &sc->vsc_config.mac[offset];
		memcpy(ptr, &value, size);
	} else {
		/* silently ignore other writes */
		DPRINTF(("vtnet: write to readonly reg %d\n\r", offset));
	}

	return (0);
}

static int
pci_vtnet_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vtnet_softc *sc = vsc;
	void *ptr;

	ptr = (uint8_t *)&sc->vsc_config + offset;
	memcpy(retval, ptr, size);
	return (0);
}

static void
pci_vtnet_neg_features(void *vsc, uint64_t negotiated_features)
{
	struct pci_vtnet_softc *sc = vsc;
	unsigned int rx_vhdrlen;

	sc->vsc_features = negotiated_features;

	if (negotiated_features & VIRTIO_NET_F_MRG_RXBUF) {
		rx_vhdrlen = sizeof(struct virtio_net_rxhdr);
		sc->rx_merge = 1;
	} else {
		/*
		 * Without mergeable rx buffers, virtio-net header is 2
		 * bytes shorter than sizeof(struct virtio_net_rxhdr).
		 */
		rx_vhdrlen = sizeof(struct virtio_net_rxhdr) - 2;
		sc->rx_merge = 0;
	}

	/* Tell the backend to enable some capabilities it has advertised. */
	netbe_set_cap(sc->vsc_be, negotiated_features, rx_vhdrlen);
}

static struct pci_devemu pci_de_vnet = {
	.pe_emu = 	"virtio-net",
	.pe_init =	pci_vtnet_init,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read
};
PCI_EMUL_SET(pci_de_vnet);
