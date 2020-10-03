/*-
 * Copyright (c) 2015 iXsystems Inc.
 * Copyright (c) 2017-2018 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * VirtIO filesystem passthrough using 9p protocol.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/uio.h>
#include <sys/capsicum.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include <lib9p.h>
#include <backend/fs.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "virtio.h"

#define	VT9P_MAX_IOV	128
#define VT9P_RINGSZ	256
#define	VT9P_MAXTAGSZ	256
#define	VT9P_CONFIGSPACESZ	(VT9P_MAXTAGSZ + sizeof(uint16_t))

static int pci_vt9p_debug;
#define DPRINTF(params) if (pci_vt9p_debug) printf params
#define WPRINTF(params) printf params

/*
 * Per-device softc
 */
struct pci_vt9p_softc {
	struct virtio_softc      vsc_vs;
	struct vqueue_info       vsc_vq;
	pthread_mutex_t          vsc_mtx;
	uint64_t                 vsc_cfg;
	uint64_t                 vsc_features;
	char *                   vsc_rootpath;
	struct pci_vt9p_config * vsc_config;
	struct l9p_backend *     vsc_fs_backend;
	struct l9p_server *      vsc_server;
        struct l9p_connection *  vsc_conn;
};

struct pci_vt9p_request {
	struct pci_vt9p_softc *	vsr_sc;
	struct iovec *		vsr_iov;
	size_t			vsr_niov;
	size_t			vsr_respidx;
	size_t			vsr_iolen;
	uint16_t		vsr_idx;
};

struct pci_vt9p_config {
	uint16_t tag_len;
	char tag[0];
} __attribute__((packed));

static int pci_vt9p_send(struct l9p_request *, const struct iovec *,
    const size_t, const size_t, void *);
static void pci_vt9p_drop(struct l9p_request *, const struct iovec *, size_t,
    void *);
static void pci_vt9p_reset(void *);
static void pci_vt9p_notify(void *, struct vqueue_info *);
static int pci_vt9p_cfgread(void *, int, int, uint32_t *);
static void pci_vt9p_neg_features(void *, uint64_t);

static struct virtio_consts vt9p_vi_consts = {
	"vt9p",			/* our name */
	1,			/* we support 1 virtqueue */
	VT9P_CONFIGSPACESZ,	/* config reg size */
	pci_vt9p_reset,		/* reset */
	pci_vt9p_notify,	/* device-wide qnotify */
	pci_vt9p_cfgread,	/* read virtio config */
	NULL,			/* write virtio config */
	pci_vt9p_neg_features,	/* apply negotiated features */
	(1 << 0),		/* our capabilities */
};


static void
pci_vt9p_reset(void *vsc)
{
	struct pci_vt9p_softc *sc;

	sc = vsc;

	DPRINTF(("vt9p: device reset requested !\n"));
	vi_reset_dev(&sc->vsc_vs);
}

static void
pci_vt9p_neg_features(void *vsc, uint64_t negotiated_features)
{
	struct pci_vt9p_softc *sc = vsc;

	sc->vsc_features = negotiated_features;
}

static int
pci_vt9p_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vt9p_softc *sc = vsc;
	void *ptr;

	ptr = (uint8_t *)sc->vsc_config + offset;
	memcpy(retval, ptr, size);
	return (0);
}

static int
pci_vt9p_get_buffer(struct l9p_request *req, struct iovec *iov, size_t *niov,
    void *arg)
{
	struct pci_vt9p_request *preq = req->lr_aux;
	size_t n = preq->vsr_niov - preq->vsr_respidx;
	
	memcpy(iov, preq->vsr_iov + preq->vsr_respidx,
	    n * sizeof(struct iovec));
	*niov = n;
	return (0);
}

static int
pci_vt9p_send(struct l9p_request *req, const struct iovec *iov,
    const size_t niov, const size_t iolen, void *arg)
{
	struct pci_vt9p_request *preq = req->lr_aux;
	struct pci_vt9p_softc *sc = preq->vsr_sc;

	preq->vsr_iolen = iolen;

	pthread_mutex_lock(&sc->vsc_mtx);
	vq_relchain(&sc->vsc_vq, preq->vsr_idx, preq->vsr_iolen);
	vq_endchains(&sc->vsc_vq, 1);
	pthread_mutex_unlock(&sc->vsc_mtx);
	free(preq);
	return (0);
}

static void
pci_vt9p_drop(struct l9p_request *req, const struct iovec *iov, size_t niov,
    void *arg)
{
	struct pci_vt9p_request *preq = req->lr_aux;
	struct pci_vt9p_softc *sc = preq->vsr_sc;

	pthread_mutex_lock(&sc->vsc_mtx);
	vq_relchain(&sc->vsc_vq, preq->vsr_idx, 0);
	vq_endchains(&sc->vsc_vq, 1);
	pthread_mutex_unlock(&sc->vsc_mtx);
	free(preq);
}

static void
pci_vt9p_notify(void *vsc, struct vqueue_info *vq)
{
	struct iovec iov[VT9P_MAX_IOV];
	struct pci_vt9p_softc *sc;
	struct pci_vt9p_request *preq;
	uint16_t idx, n, i;
	uint16_t flags[VT9P_MAX_IOV];

	sc = vsc;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, VT9P_MAX_IOV, flags);
		preq = calloc(1, sizeof(struct pci_vt9p_request));
		preq->vsr_sc = sc;
		preq->vsr_idx = idx;
		preq->vsr_iov = iov;
		preq->vsr_niov = n;
		preq->vsr_respidx = 0;

		/* Count readable descriptors */
		for (i = 0; i < n; i++) {
			if (flags[i] & VRING_DESC_F_WRITE)
				break;

			preq->vsr_respidx++;
		}

		for (int i = 0; i < n; i++) {
			DPRINTF(("vt9p: vt9p_notify(): desc%d base=%p, "
			    "len=%zu, flags=0x%04x\r\n", i, iov[i].iov_base,
			    iov[i].iov_len, flags[i]));
		}

		l9p_connection_recv(sc->vsc_conn, iov, preq->vsr_respidx, preq);
	}
}


static int
pci_vt9p_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	struct pci_vt9p_softc *sc;
	char *opt;
	char *sharename = NULL;
	char *rootpath = NULL;
	int rootfd;
	bool ro = false;
	cap_rights_t rootcap;

	if (opts == NULL) {
		printf("virtio-9p: share name and path required\n");
		return (1);
	}

	while ((opt = strsep(&opts, ",")) != NULL) {
		if (strchr(opt, '=') != NULL) {
			if (sharename != NULL) {
				printf("virtio-9p: more than one share name given\n");
				return (1);
			}

			sharename = strsep(&opt, "=");
			rootpath = opt;
			continue;
		}

		if (strcmp(opt, "ro") == 0) {
			DPRINTF(("read-only mount requested\r\n"));
			ro = true;
			continue;
		}

		printf("virtio-9p: invalid option '%s'\n", opt);
		return (1);
	}

	if (strlen(sharename) > VT9P_MAXTAGSZ) {
		printf("virtio-9p: share name too long\n");
		return (1);
	}

	rootfd = open(rootpath, O_DIRECTORY);
	if (rootfd < 0)
		return (-1);

	sc = calloc(1, sizeof(struct pci_vt9p_softc));
	sc->vsc_config = calloc(1, sizeof(struct pci_vt9p_config) +
	    VT9P_MAXTAGSZ);

	pthread_mutex_init(&sc->vsc_mtx, NULL);

	cap_rights_init(&rootcap,
	    CAP_LOOKUP, CAP_ACL_CHECK, CAP_ACL_DELETE, CAP_ACL_GET,
	    CAP_ACL_SET, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FSTAT,
	    CAP_CREATE, CAP_FCHMODAT, CAP_FCHOWNAT, CAP_FTRUNCATE,
	    CAP_LINKAT_SOURCE, CAP_LINKAT_TARGET, CAP_MKDIRAT, CAP_MKNODAT,
	    CAP_PREAD, CAP_PWRITE, CAP_RENAMEAT_SOURCE, CAP_RENAMEAT_TARGET,
	    CAP_SEEK, CAP_SYMLINKAT, CAP_UNLINKAT, CAP_EXTATTR_DELETE,
	    CAP_EXTATTR_GET, CAP_EXTATTR_LIST, CAP_EXTATTR_SET,
	    CAP_FUTIMES, CAP_FSTATFS, CAP_FSYNC, CAP_FPATHCONF);

	if (cap_rights_limit(rootfd, &rootcap) != 0)
		return (1);

	sc->vsc_config->tag_len = (uint16_t)strlen(sharename);
	memcpy(sc->vsc_config->tag, sharename, sc->vsc_config->tag_len);
	
	if (l9p_backend_fs_init(&sc->vsc_fs_backend, rootfd, ro) != 0) {
		errno = ENXIO;
		return (1);
	}

	if (l9p_server_init(&sc->vsc_server, sc->vsc_fs_backend) != 0) {
		errno = ENXIO;
		return (1);
	}

	if (l9p_connection_init(sc->vsc_server, &sc->vsc_conn) != 0) {
		errno = EIO;
		return (1);
	}

	sc->vsc_conn->lc_msize = L9P_MAX_IOV * PAGE_SIZE;
	sc->vsc_conn->lc_lt.lt_get_response_buffer = pci_vt9p_get_buffer;
	sc->vsc_conn->lc_lt.lt_send_response = pci_vt9p_send;
	sc->vsc_conn->lc_lt.lt_drop_response = pci_vt9p_drop;

	vi_softc_linkup(&sc->vsc_vs, &vt9p_vi_consts, sc, pi, &sc->vsc_vq);
	sc->vsc_vs.vs_mtx = &sc->vsc_mtx;
	sc->vsc_vq.vq_qsize = VT9P_RINGSZ;

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_9P);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_TYPE_9P);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vsc_vs, 1, fbsdrun_virtio_msix()))
		return (1);
	vi_set_io_bar(&sc->vsc_vs, 0);

	return (0);
}

struct pci_devemu pci_de_v9p = {
	.pe_emu =	"virtio-9p",
	.pe_init =	pci_vt9p_init,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read
};
PCI_EMUL_SET(pci_de_v9p);
