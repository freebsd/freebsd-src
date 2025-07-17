/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 iXsystems Inc.
 * All rights reserved.
 *
 * This software was developed by Jakub Klama <jceel@FreeBSD.org>
 * under sponsorship from iXsystems Inc.
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

#include <sys/param.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/linker_set.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <libgen.h>
#include <sysexits.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#include "virtio.h"
#include "mevent.h"
#include "sockstream.h"

#define	VTCON_RINGSZ	64
#define	VTCON_MAXPORTS	16
#define	VTCON_MAXQ	(VTCON_MAXPORTS * 2 + 2)

#define	VTCON_DEVICE_READY	0
#define	VTCON_DEVICE_ADD	1
#define	VTCON_DEVICE_REMOVE	2
#define	VTCON_PORT_READY	3
#define	VTCON_CONSOLE_PORT	4
#define	VTCON_CONSOLE_RESIZE	5
#define	VTCON_PORT_OPEN		6
#define	VTCON_PORT_NAME		7

#define	VTCON_F_SIZE		0
#define	VTCON_F_MULTIPORT	1
#define	VTCON_F_EMERG_WRITE	2
#define	VTCON_S_HOSTCAPS	\
    (VTCON_F_SIZE | VTCON_F_MULTIPORT | VTCON_F_EMERG_WRITE)

static int pci_vtcon_debug;
#define DPRINTF(params) if (pci_vtcon_debug) PRINTLN params
#define WPRINTF(params) PRINTLN params

struct pci_vtcon_softc;
struct pci_vtcon_port;
struct pci_vtcon_config;
typedef void (pci_vtcon_cb_t)(struct pci_vtcon_port *, void *, struct iovec *,
    int);

struct pci_vtcon_port {
	struct pci_vtcon_softc * vsp_sc;
	int                      vsp_id;
	const char *             vsp_name;
	bool                     vsp_enabled;
	bool                     vsp_console;
	bool                     vsp_rx_ready;
	bool                     vsp_open;
	int                      vsp_rxq;
	int                      vsp_txq;
	void *                   vsp_arg;
	pci_vtcon_cb_t *         vsp_cb;
};

struct pci_vtcon_sock
{
	struct pci_vtcon_port *  vss_port;
	const char *             vss_path;
	struct mevent *          vss_server_evp;
	struct mevent *          vss_conn_evp;
	int                      vss_server_fd;
	int                      vss_conn_fd;
	bool                     vss_open;
};

struct pci_vtcon_softc {
	struct virtio_softc      vsc_vs;
	struct vqueue_info       vsc_queues[VTCON_MAXQ];
	pthread_mutex_t          vsc_mtx;
	uint64_t                 vsc_cfg;
	uint64_t                 vsc_features;
	char *                   vsc_rootdir;
	int                      vsc_kq;
	bool                     vsc_ready;
	struct pci_vtcon_port    vsc_control_port;
 	struct pci_vtcon_port    vsc_ports[VTCON_MAXPORTS];
	struct pci_vtcon_config *vsc_config;
};

struct pci_vtcon_config {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_nr_ports;
	uint32_t emerg_wr;
} __attribute__((packed));

struct pci_vtcon_control {
	uint32_t id;
	uint16_t event;
	uint16_t value;
} __attribute__((packed));

struct pci_vtcon_console_resize {
	uint16_t cols;
	uint16_t rows;
} __attribute__((packed));

static void pci_vtcon_reset(void *);
static void pci_vtcon_notify_rx(void *, struct vqueue_info *);
static void pci_vtcon_notify_tx(void *, struct vqueue_info *);
static int pci_vtcon_cfgread(void *, int, int, uint32_t *);
static int pci_vtcon_cfgwrite(void *, int, int, uint32_t);
static void pci_vtcon_neg_features(void *, uint64_t);
static void pci_vtcon_sock_accept(int, enum ev_type,  void *);
static void pci_vtcon_sock_rx(int, enum ev_type, void *);
static void pci_vtcon_sock_tx(struct pci_vtcon_port *, void *, struct iovec *,
    int);
static void pci_vtcon_control_send(struct pci_vtcon_softc *,
    struct pci_vtcon_control *, const void *, size_t);
static void pci_vtcon_announce_port(struct pci_vtcon_port *);
static void pci_vtcon_open_port(struct pci_vtcon_port *, bool);

static struct virtio_consts vtcon_vi_consts = {
	.vc_name =	"vtcon",
	.vc_nvq =	VTCON_MAXQ,
	.vc_cfgsize =	sizeof(struct pci_vtcon_config),
	.vc_reset =	pci_vtcon_reset,
	.vc_cfgread =	pci_vtcon_cfgread,
	.vc_cfgwrite =	pci_vtcon_cfgwrite,
	.vc_apply_features = pci_vtcon_neg_features,
	.vc_hv_caps =	VTCON_S_HOSTCAPS,
};

static void
pci_vtcon_reset(void *vsc)
{
	struct pci_vtcon_softc *sc;

	sc = vsc;

	DPRINTF(("vtcon: device reset requested!"));
	vi_reset_dev(&sc->vsc_vs);
}

static void
pci_vtcon_neg_features(void *vsc, uint64_t negotiated_features)
{
	struct pci_vtcon_softc *sc = vsc;

	sc->vsc_features = negotiated_features;
}

static int
pci_vtcon_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vtcon_softc *sc = vsc;
	void *ptr;

	ptr = (uint8_t *)sc->vsc_config + offset;
	memcpy(retval, ptr, size);
	return (0);
}

static int
pci_vtcon_cfgwrite(void *vsc __unused, int offset __unused, int size __unused,
    uint32_t val __unused)
{
	return (0);
}

static inline struct pci_vtcon_port *
pci_vtcon_vq_to_port(struct pci_vtcon_softc *sc, struct vqueue_info *vq)
{
	uint16_t num = vq->vq_num;

	if (num == 0 || num == 1)
		return (&sc->vsc_ports[0]);

	if (num == 2 || num == 3)
		return (&sc->vsc_control_port);

	return (&sc->vsc_ports[(num / 2) - 1]);
}

static inline struct vqueue_info *
pci_vtcon_port_to_vq(struct pci_vtcon_port *port, bool tx_queue)
{
	int qnum;

	qnum = tx_queue ? port->vsp_txq : port->vsp_rxq;
	return (&port->vsp_sc->vsc_queues[qnum]);
}

static struct pci_vtcon_port *
pci_vtcon_port_add(struct pci_vtcon_softc *sc, int port_id, const char *name,
    pci_vtcon_cb_t *cb, void *arg)
{
	struct pci_vtcon_port *port;

	port = &sc->vsc_ports[port_id];
	if (port->vsp_enabled) {
		errno = EBUSY;
		return (NULL);
	}
	port->vsp_id = port_id;
	port->vsp_sc = sc;
	port->vsp_name = name;
	port->vsp_cb = cb;
	port->vsp_arg = arg;

	if (port->vsp_id == 0) {
		/* port0 */
		port->vsp_txq = 0;
		port->vsp_rxq = 1;
	} else {
		port->vsp_txq = (port_id + 1) * 2;
		port->vsp_rxq = port->vsp_txq + 1;
	}

	port->vsp_enabled = true;
	return (port);
}

static int
pci_vtcon_sock_add(struct pci_vtcon_softc *sc, const char *port_name,
    const nvlist_t *nvl)
{
	struct pci_vtcon_sock *sock = NULL;
	struct sockaddr_un sun;
	const char *name, *path;
	char *cp, *pathcopy;
	long port;
	int s = -1, fd = -1, error = 0;
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
#endif

	port = strtol(port_name, &cp, 0);
	if (*cp != '\0' || port < 0 || port >= VTCON_MAXPORTS) {
		EPRINTLN("vtcon: Invalid port %s", port_name);
		error = -1;
		goto out;
	}

	path = get_config_value_node(nvl, "path");
	if (path == NULL) {
		EPRINTLN("vtcon: required path missing for port %ld", port);
		error = -1;
		goto out;
	}

	sock = calloc(1, sizeof(struct pci_vtcon_sock));
	if (sock == NULL) {
		error = -1;
		goto out;
	}

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		error = -1;
		goto out;
	}

	pathcopy = strdup(path);
	if (pathcopy == NULL) {
		error = -1;
		goto out;
	}

	fd = open(dirname(pathcopy), O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		free(pathcopy);
		error = -1;
		goto out;
	}

	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(struct sockaddr_un);
	strcpy(pathcopy, path);
	strlcpy(sun.sun_path, basename(pathcopy), sizeof(sun.sun_path));
	free(pathcopy);

	if (bindat(fd, s, (struct sockaddr *)&sun, sun.sun_len) < 0) {
		error = -1;
		goto out;
	}

	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		error = -1;
		goto out;
	}

	if (listen(s, 1) < 0) {
		error = -1;
		goto out;
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_ACCEPT, CAP_EVENT, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(s, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	name = get_config_value_node(nvl, "name");
	if (name == NULL) {
		EPRINTLN("vtcon: required name missing for port %ld", port);
		error = -1;
		goto out;
	}
	sock->vss_port = pci_vtcon_port_add(sc, port, name, pci_vtcon_sock_tx, sock);
	if (sock->vss_port == NULL) {
		error = -1;
		goto out;
	}

	sock->vss_open = false;
	sock->vss_conn_fd = -1;
	sock->vss_server_fd = s;
	sock->vss_server_evp = mevent_add(s, EVF_READ, pci_vtcon_sock_accept,
	    sock);

	if (sock->vss_server_evp == NULL) {
		error = -1;
		goto out;
	}

out:
	if (fd != -1)
		close(fd);

	if (error != 0) {
		if (s != -1)
			close(s);
		free(sock);
	}

	return (error);
}

static void
pci_vtcon_sock_accept(int fd __unused, enum ev_type t __unused, void *arg)
{
	struct pci_vtcon_sock *sock = (struct pci_vtcon_sock *)arg;
	int s;

	s = accept(sock->vss_server_fd, NULL, NULL);
	if (s < 0)
		return;

	if (sock->vss_open) {
		close(s);
		return;
	}

	sock->vss_open = true;
	sock->vss_conn_fd = s;
	sock->vss_conn_evp = mevent_add(s, EVF_READ, pci_vtcon_sock_rx, sock);

	pci_vtcon_open_port(sock->vss_port, true);
}

static void
pci_vtcon_sock_rx(int fd __unused, enum ev_type t __unused, void *arg)
{
	struct pci_vtcon_port *port;
	struct pci_vtcon_sock *sock = (struct pci_vtcon_sock *)arg;
	struct vqueue_info *vq;
	struct vi_req req;
	struct iovec iov;
	static char dummybuf[2048];
	int len, n;

	port = sock->vss_port;
	vq = pci_vtcon_port_to_vq(port, true);

	if (!sock->vss_open || !port->vsp_rx_ready) {
		len = read(sock->vss_conn_fd, dummybuf, sizeof(dummybuf));
		if (len == 0)
			goto close;

		return;
	}

	if (!vq_has_descs(vq)) {
		len = read(sock->vss_conn_fd, dummybuf, sizeof(dummybuf));
		vq_endchains(vq, 1);
		if (len == 0)
			goto close;

		return;
	}

	do {
		n = vq_getchain(vq, &iov, 1, &req);
		assert(n == 1);
		len = readv(sock->vss_conn_fd, &iov, n);

		if (len == 0 || (len < 0 && errno == EWOULDBLOCK)) {
			vq_retchains(vq, 1);
			vq_endchains(vq, 0);
			if (len == 0)
				goto close;

			return;
		}

		vq_relchain(vq, req.idx, len);
	} while (vq_has_descs(vq));

	vq_endchains(vq, 1);

close:
	mevent_delete_close(sock->vss_conn_evp);
	sock->vss_conn_fd = -1;
	sock->vss_open = false;
}

static void
pci_vtcon_sock_tx(struct pci_vtcon_port *port __unused, void *arg __unused,
    struct iovec *iov, int niov)
{
	struct pci_vtcon_sock *sock;
	int i, ret;

	sock = (struct pci_vtcon_sock *)arg;

	if (sock->vss_conn_fd == -1)
		return;

	for (i = 0; i < niov; i++) {
		ret = stream_write(sock->vss_conn_fd, iov[i].iov_base,
		    iov[i].iov_len);
		if (ret <= 0)
			break;
	}

	if (ret <= 0) {
		mevent_delete_close(sock->vss_conn_evp);
		sock->vss_conn_fd = -1;
		sock->vss_open = false;
	}
}

static void
pci_vtcon_control_tx(struct pci_vtcon_port *port, void *arg __unused,
    struct iovec *iov, int niov)
{
	struct pci_vtcon_softc *sc;
	struct pci_vtcon_port *tmp;
	struct pci_vtcon_control resp, *ctrl;
	int i;

	assert(niov == 1);

	sc = port->vsp_sc;
	ctrl = (struct pci_vtcon_control *)iov->iov_base;

	switch (ctrl->event) {
	case VTCON_DEVICE_READY:
		sc->vsc_ready = true;
		/* set port ready events for registered ports */
		for (i = 0; i < VTCON_MAXPORTS; i++) {
			tmp = &sc->vsc_ports[i];
			if (tmp->vsp_enabled)
				pci_vtcon_announce_port(tmp);

			if (tmp->vsp_open)
				pci_vtcon_open_port(tmp, true);
		}
		break;

	case VTCON_PORT_READY:
		tmp = &sc->vsc_ports[ctrl->id];
		if (ctrl->id >= VTCON_MAXPORTS || !tmp->vsp_enabled) {
			WPRINTF(("VTCON_PORT_READY event for unknown port %d",
			    ctrl->id));
			return;
		}

		if (tmp->vsp_console) {
			resp.event = VTCON_CONSOLE_PORT;
			resp.id = ctrl->id;
			resp.value = 1;
			pci_vtcon_control_send(sc, &resp, NULL, 0);
		}
		break;
	}
}

static void
pci_vtcon_announce_port(struct pci_vtcon_port *port)
{
	struct pci_vtcon_control event;

	event.id = port->vsp_id;
	event.event = VTCON_DEVICE_ADD;
	event.value = 1;
	pci_vtcon_control_send(port->vsp_sc, &event, NULL, 0);

	event.event = VTCON_PORT_NAME;
	pci_vtcon_control_send(port->vsp_sc, &event, port->vsp_name,
	    strlen(port->vsp_name));
}

static void
pci_vtcon_open_port(struct pci_vtcon_port *port, bool open)
{
	struct pci_vtcon_control event;

	if (!port->vsp_sc->vsc_ready) {
		port->vsp_open = true;
		return;
	}

	event.id = port->vsp_id;
	event.event = VTCON_PORT_OPEN;
	event.value = (int)open;
	pci_vtcon_control_send(port->vsp_sc, &event, NULL, 0);
}

static void
pci_vtcon_control_send(struct pci_vtcon_softc *sc,
    struct pci_vtcon_control *ctrl, const void *payload, size_t len)
{
	struct vqueue_info *vq;
	struct vi_req req;
	struct iovec iov;
	int n;

	if (len > SIZE_T_MAX - sizeof(struct pci_vtcon_control))
		return;

	vq = pci_vtcon_port_to_vq(&sc->vsc_control_port, true);

	if (!vq_has_descs(vq))
		return;

	n = vq_getchain(vq, &iov, 1, &req);
	assert(n == 1);

	if (iov.iov_len < sizeof(struct pci_vtcon_control) + len)
		goto out;

	memcpy(iov.iov_base, ctrl, sizeof(struct pci_vtcon_control));
	if (len > 0)
		memcpy((uint8_t *)iov.iov_base +
		    sizeof(struct pci_vtcon_control), payload, len);

out:
	vq_relchain(vq, req.idx, sizeof(struct pci_vtcon_control) + len);
	vq_endchains(vq, 1);
}


static void
pci_vtcon_notify_tx(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtcon_softc *sc;
	struct pci_vtcon_port *port;
	struct iovec iov[1];
	struct vi_req req;
	int n;

	sc = vsc;
	port = pci_vtcon_vq_to_port(sc, vq);

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, iov, 1, &req);
		assert(n == 1);
		if (port != NULL)
			port->vsp_cb(port, port->vsp_arg, iov, 1);

		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, req.idx, 0);
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static void
pci_vtcon_notify_rx(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtcon_softc *sc;
	struct pci_vtcon_port *port;

	sc = vsc;
	port = pci_vtcon_vq_to_port(sc, vq);

	if (!port->vsp_rx_ready) {
		port->vsp_rx_ready = 1;
		vq_kick_disable(vq);
	}
}

/*
 * Each console device has a "port" node which contains nodes for
 * each port.  Ports are numbered starting at 0.
 */
static int
pci_vtcon_legacy_config_port(nvlist_t *nvl, int port, char *opt)
{
	char *name, *path;
	char node_name[sizeof("XX")];
	nvlist_t *port_nvl;

	name = strsep(&opt, "=");
	path = opt;
	if (path == NULL) {
		EPRINTLN("vtcon: port %s requires a path", name);
		return (-1);
	}
	if (port >= VTCON_MAXPORTS) {
		EPRINTLN("vtcon: too many ports");
		return (-1);
	}
	snprintf(node_name, sizeof(node_name), "%d", port);
	port_nvl = create_relative_config_node(nvl, node_name);
	set_config_value_node(port_nvl, "name", name);
	set_config_value_node(port_nvl, "path", path);
	return (0);
}

static int
pci_vtcon_legacy_config(nvlist_t *nvl, const char *opts)
{
	char *opt, *str, *tofree;
	nvlist_t *ports_nvl;
	int error, port;

	ports_nvl = create_relative_config_node(nvl, "port");
	tofree = str = strdup(opts);
	error = 0;
	port = 0;
	while ((opt = strsep(&str, ",")) != NULL) {
		error = pci_vtcon_legacy_config_port(ports_nvl, port, opt);
		if (error)
			break;
		port++;
	}
	free(tofree);
	return (error);
}

static int
pci_vtcon_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	struct pci_vtcon_softc *sc;
	nvlist_t *ports_nvl;
	int i;

	sc = calloc(1, sizeof(struct pci_vtcon_softc));
	sc->vsc_config = calloc(1, sizeof(struct pci_vtcon_config));
	sc->vsc_config->max_nr_ports = VTCON_MAXPORTS;
	sc->vsc_config->cols = 80;
	sc->vsc_config->rows = 25;

	pthread_mutex_init(&sc->vsc_mtx, NULL);

	vi_softc_linkup(&sc->vsc_vs, &vtcon_vi_consts, sc, pi, sc->vsc_queues);
	sc->vsc_vs.vs_mtx = &sc->vsc_mtx;

	for (i = 0; i < VTCON_MAXQ; i++) {
		sc->vsc_queues[i].vq_qsize = VTCON_RINGSZ;
		sc->vsc_queues[i].vq_notify = i % 2 == 0
		    ? pci_vtcon_notify_rx
		    : pci_vtcon_notify_tx;
	}

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_CONSOLE);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_SIMPLECOMM);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_ID_CONSOLE);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vsc_vs, 1, fbsdrun_virtio_msix()))
		return (1);
	vi_set_io_bar(&sc->vsc_vs, 0);

	/* create control port */
	sc->vsc_control_port.vsp_sc = sc;
	sc->vsc_control_port.vsp_txq = 2;
	sc->vsc_control_port.vsp_rxq = 3;
	sc->vsc_control_port.vsp_cb = pci_vtcon_control_tx;
	sc->vsc_control_port.vsp_enabled = true;

	ports_nvl = find_relative_config_node(nvl, "port");
	if (ports_nvl != NULL) {
		const char *name;
		void *cookie;
		int type;

		cookie = NULL;
		while ((name = nvlist_next(ports_nvl, &type, &cookie)) !=
		    NULL) {
			if (type != NV_TYPE_NVLIST)
				continue;

			if (pci_vtcon_sock_add(sc, name,
			    nvlist_get_nvlist(ports_nvl, name)) < 0) {
				EPRINTLN("cannot create port %s: %s",
				    name, strerror(errno));
				return (1);
			}
		}
	}

	return (0);
}

static const struct pci_devemu pci_de_vcon = {
	.pe_emu =	"virtio-console",
	.pe_init =	pci_vtcon_init,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read,
	.pe_legacy_config = pci_vtcon_legacy_config,
};
PCI_EMUL_SET(pci_de_vcon);
