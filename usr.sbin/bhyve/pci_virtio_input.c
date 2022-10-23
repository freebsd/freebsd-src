/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Beckhoff Automation GmbH & Co. KG
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
 * virtio input device emulation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>

#include <capsicum_helpers.h>
#endif
#include <sys/ioctl.h>
#include <sys/linker_set.h>
#include <sys/uio.h>

#include <dev/evdev/input.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "mevent.h"
#include "pci_emul.h"
#include "virtio.h"

#define VTINPUT_RINGSZ 64

#define VTINPUT_MAX_PKT_LEN 10

/*
 * Queue definitions.
 */
#define VTINPUT_EVENTQ 0
#define VTINPUT_STATUSQ 1

#define VTINPUT_MAXQ 2

static int pci_vtinput_debug;
#define DPRINTF(params)        \
	if (pci_vtinput_debug) \
	PRINTLN params
#define WPRINTF(params) PRINTLN params

enum vtinput_config_select {
	VTINPUT_CFG_UNSET = 0x00,
	VTINPUT_CFG_ID_NAME = 0x01,
	VTINPUT_CFG_ID_SERIAL = 0x02,
	VTINPUT_CFG_ID_DEVIDS = 0x03,
	VTINPUT_CFG_PROP_BITS = 0x10,
	VTINPUT_CFG_EV_BITS = 0x11,
	VTINPUT_CFG_ABS_INFO = 0x12
};

struct vtinput_absinfo {
	uint32_t min;
	uint32_t max;
	uint32_t fuzz;
	uint32_t flat;
	uint32_t res;
} __packed;

struct vtinput_devids {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
} __packed;

struct vtinput_config {
	uint8_t select;
	uint8_t subsel;
	uint8_t size;
	uint8_t reserved[5];
	union {
		char string[128];
		uint8_t bitmap[128];
		struct vtinput_absinfo abs;
		struct vtinput_devids ids;
	} u;
} __packed;

struct vtinput_event {
	uint16_t type;
	uint16_t code;
	uint32_t value;
} __packed;

struct vtinput_event_elem {
	struct vtinput_event event;
	struct iovec iov;
	uint16_t idx;
};

struct vtinput_eventqueue {
	struct vtinput_event_elem *events;
	uint32_t size;
	uint32_t idx;
};

/*
 * Per-device softc
 */
struct pci_vtinput_softc {
	struct virtio_softc vsc_vs;
	struct vqueue_info vsc_queues[VTINPUT_MAXQ];
	pthread_mutex_t vsc_mtx;
	const char *vsc_evdev;
	int vsc_fd;
	struct vtinput_config vsc_config;
	int vsc_config_valid;
	struct mevent *vsc_evp;
	struct vtinput_eventqueue vsc_eventqueue;
};

static void pci_vtinput_reset(void *);
static int pci_vtinput_cfgread(void *, int, int, uint32_t *);
static int pci_vtinput_cfgwrite(void *, int, int, uint32_t);

static struct virtio_consts vtinput_vi_consts = {
	.vc_name =	"vtinput",
	.vc_nvq =	VTINPUT_MAXQ,
	.vc_cfgsize =	sizeof(struct vtinput_config),
	.vc_reset =	pci_vtinput_reset,
	.vc_cfgread =	pci_vtinput_cfgread,
	.vc_cfgwrite =	pci_vtinput_cfgwrite,
	.vc_hv_caps =	0,
};

static void
pci_vtinput_reset(void *vsc)
{
	struct pci_vtinput_softc *sc = vsc;

	DPRINTF(("%s: device reset requested", __func__));
	vi_reset_dev(&sc->vsc_vs);
}

static void
pci_vtinput_notify_eventq(void *vsc __unused, struct vqueue_info *vq __unused)
{
	DPRINTF(("%s", __func__));
}

static void
pci_vtinput_notify_statusq(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtinput_softc *sc = vsc;

	while (vq_has_descs(vq)) {
		/* get descriptor chain */
		struct iovec iov;
		struct vi_req req;
		const int n = vq_getchain(vq, &iov, 1, &req);
		if (n <= 0) {
			WPRINTF(("%s: invalid descriptor: %d", __func__, n));
			return;
		}

		/* get event */
		struct vtinput_event event;
		memcpy(&event, iov.iov_base, sizeof(event));

		/*
		 * on multi touch devices:
		 * - host send EV_MSC to guest
		 * - guest sends EV_MSC back to host
		 * - host writes EV_MSC to evdev
		 * - evdev saves EV_MSC in it's event buffer
		 * - host receives an extra EV_MSC by reading the evdev event
		 *   buffer
		 * - frames become larger and larger
		 * avoid endless loops by ignoring EV_MSC
		 */
		if (event.type == EV_MSC) {
			vq_relchain(vq, req.idx, sizeof(event));
			continue;
		}

		/* send event to evdev */
		struct input_event host_event;
		host_event.type = event.type;
		host_event.code = event.code;
		host_event.value = event.value;
		if (gettimeofday(&host_event.time, NULL) != 0) {
			WPRINTF(("%s: failed gettimeofday", __func__));
		}
		if (write(sc->vsc_fd, &host_event, sizeof(host_event)) == -1) {
			WPRINTF(("%s: failed to write host_event", __func__));
		}

		vq_relchain(vq, req.idx, sizeof(event));
	}
	vq_endchains(vq, 1);
}

static int
pci_vtinput_get_bitmap(struct pci_vtinput_softc *sc, int cmd, int count)
{
	if (count <= 0 || !sc) {
		return (-1);
	}

	/* query bitmap */
	memset(sc->vsc_config.u.bitmap, 0, sizeof(sc->vsc_config.u.bitmap));
	if (ioctl(sc->vsc_fd, cmd, sc->vsc_config.u.bitmap) < 0) {
		return (-1);
	}

	/* get number of set bytes in bitmap */
	for (int i = count - 1; i >= 0; i--) {
		if (sc->vsc_config.u.bitmap[i]) {
			return i + 1;
		}
	}

	return (-1);
}

static int
pci_vtinput_read_config_id_name(struct pci_vtinput_softc *sc)
{
	char name[128];
	if (ioctl(sc->vsc_fd, EVIOCGNAME(sizeof(name) - 1), name) < 0) {
		return (1);
	}

	memcpy(sc->vsc_config.u.string, name, sizeof(name));
	sc->vsc_config.size = strnlen(name, sizeof(name));

	return (0);
}

static int
pci_vtinput_read_config_id_serial(struct pci_vtinput_softc *sc)
{
	/* serial isn't supported */
	sc->vsc_config.size = 0;

	return (0);
}

static int
pci_vtinput_read_config_id_devids(struct pci_vtinput_softc *sc)
{
	struct input_id devids;
	if (ioctl(sc->vsc_fd, EVIOCGID, &devids)) {
		return (1);
	}

	sc->vsc_config.u.ids.bustype = devids.bustype;
	sc->vsc_config.u.ids.vendor = devids.vendor;
	sc->vsc_config.u.ids.product = devids.product;
	sc->vsc_config.u.ids.version = devids.version;
	sc->vsc_config.size = sizeof(struct vtinput_devids);

	return (0);
}

static int
pci_vtinput_read_config_prop_bits(struct pci_vtinput_softc *sc)
{
	/*
	 * Evdev bitmap countains 1 bit per count. Additionally evdev bitmaps
	 * are arrays of longs instead of chars. Calculate how many longs are
	 * required for evdev bitmap. Multiply that with sizeof(long) to get the
	 * number of elements.
	 */
	const int count = howmany(INPUT_PROP_CNT, sizeof(long) * 8) *
	    sizeof(long);
	const unsigned int cmd = EVIOCGPROP(count);
	const int size = pci_vtinput_get_bitmap(sc, cmd, count);
	if (size <= 0) {
		return (1);
	}

	sc->vsc_config.size = size;

	return (0);
}

static int
pci_vtinput_read_config_ev_bits(struct pci_vtinput_softc *sc, uint8_t type)
{
	int count;

	switch (type) {
	case EV_KEY:
		count = KEY_CNT;
		break;
	case EV_REL:
		count = REL_CNT;
		break;
	case EV_ABS:
		count = ABS_CNT;
		break;
	case EV_MSC:
		count = MSC_CNT;
		break;
	case EV_SW:
		count = SW_CNT;
		break;
	case EV_LED:
		count = LED_CNT;
		break;
	default:
		return (1);
	}

	/*
	 * Evdev bitmap countains 1 bit per count. Additionally evdev bitmaps
	 * are arrays of longs instead of chars. Calculate how many longs are
	 * required for evdev bitmap. Multiply that with sizeof(long) to get the
	 * number of elements.
	 */
	count = howmany(count, sizeof(long) * 8) * sizeof(long);
	const unsigned int cmd = EVIOCGBIT(sc->vsc_config.subsel, count);
	const int size = pci_vtinput_get_bitmap(sc, cmd, count);
	if (size <= 0) {
		return (1);
	}

	sc->vsc_config.size = size;

	return (0);
}

static int
pci_vtinput_read_config_abs_info(struct pci_vtinput_softc *sc)
{
	/* check if evdev has EV_ABS */
	if (!pci_vtinput_read_config_ev_bits(sc, EV_ABS)) {
		return (1);
	}

	/* get abs information */
	struct input_absinfo abs;
	if (ioctl(sc->vsc_fd, EVIOCGABS(sc->vsc_config.subsel), &abs) < 0) {
		return (1);
	}

	/* save abs information */
	sc->vsc_config.u.abs.min = abs.minimum;
	sc->vsc_config.u.abs.max = abs.maximum;
	sc->vsc_config.u.abs.fuzz = abs.fuzz;
	sc->vsc_config.u.abs.flat = abs.flat;
	sc->vsc_config.u.abs.res = abs.resolution;
	sc->vsc_config.size = sizeof(struct vtinput_absinfo);

	return (0);
}

static int
pci_vtinput_read_config(struct pci_vtinput_softc *sc)
{
	switch (sc->vsc_config.select) {
	case VTINPUT_CFG_UNSET:
		return (0);
	case VTINPUT_CFG_ID_NAME:
		return pci_vtinput_read_config_id_name(sc);
	case VTINPUT_CFG_ID_SERIAL:
		return pci_vtinput_read_config_id_serial(sc);
	case VTINPUT_CFG_ID_DEVIDS:
		return pci_vtinput_read_config_id_devids(sc);
	case VTINPUT_CFG_PROP_BITS:
		return pci_vtinput_read_config_prop_bits(sc);
	case VTINPUT_CFG_EV_BITS:
		return pci_vtinput_read_config_ev_bits(
		    sc, sc->vsc_config.subsel);
	case VTINPUT_CFG_ABS_INFO:
		return pci_vtinput_read_config_abs_info(sc);
	default:
		return (1);
	}
}

static int
pci_vtinput_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vtinput_softc *sc = vsc;

	/* check for valid offset and size */
	if (offset + size > (int)sizeof(struct vtinput_config)) {
		WPRINTF(("%s: read to invalid offset/size %d/%d", __func__,
		    offset, size));
		memset(retval, 0, size);
		return (0);
	}

	/* read new config values, if select and subsel changed. */
	if (!sc->vsc_config_valid) {
		if (pci_vtinput_read_config(sc) != 0) {
			DPRINTF(("%s: could not read config %d/%d", __func__,
			    sc->vsc_config.select, sc->vsc_config.subsel));
			memset(retval, 0, size);
			return (0);
		}
		sc->vsc_config_valid = 1;
	}

	uint8_t *ptr = (uint8_t *)&sc->vsc_config;
	memcpy(retval, ptr + offset, size);

	return (0);
}

static int
pci_vtinput_cfgwrite(void *vsc, int offset, int size, uint32_t value)
{
	struct pci_vtinput_softc *sc = vsc;

	/* guest can only write to select and subsel fields */
	if (offset + size > 2) {
		WPRINTF(("%s: write to readonly reg %d", __func__, offset));
		return (1);
	}

	/* copy value into config */
	uint8_t *ptr = (uint8_t *)&sc->vsc_config;
	memcpy(ptr + offset, &value, size);

	/* select/subsel changed, query new config on next cfgread */
	sc->vsc_config_valid = 0;

	return (0);
}

static int
vtinput_eventqueue_add_event(
    struct vtinput_eventqueue *queue, struct input_event *e)
{
	/* check if queue is full */
	if (queue->idx >= queue->size) {
		/* alloc new elements for queue */
		const uint32_t newSize = queue->idx;
		void *newPtr = realloc(queue->events,
		    queue->size * sizeof(struct vtinput_event_elem));
		if (newPtr == NULL) {
			WPRINTF(("%s: realloc memory for eventqueue failed!",
			    __func__));
			return (1);
		}
		queue->events = newPtr;
		queue->size = newSize;
	}

	/* save event */
	struct vtinput_event *event = &queue->events[queue->idx].event;
	event->type = e->type;
	event->code = e->code;
	event->value = e->value;
	queue->idx++;

	return (0);
}

static void
vtinput_eventqueue_clear(struct vtinput_eventqueue *queue)
{
	/* just reset index to clear queue */
	queue->idx = 0;
}

static void
vtinput_eventqueue_send_events(
    struct vtinput_eventqueue *queue, struct vqueue_info *vq)
{
	/*
	 * First iteration through eventqueue:
	 *   Get descriptor chains.
	 */
	for (uint32_t i = 0; i < queue->idx; ++i) {
		/* get descriptor */
		if (!vq_has_descs(vq)) {
			/*
			 * We don't have enough descriptors for all events.
			 * Return chains back to guest.
			 */
			vq_retchains(vq, i);
			WPRINTF((
			    "%s: not enough available descriptors, dropping %d events",
			    __func__, queue->idx));
			goto done;
		}

		/* get descriptor chain */
		struct iovec iov;
		struct vi_req req;
		const int n = vq_getchain(vq, &iov, 1, &req);
		if (n <= 0) {
			WPRINTF(("%s: invalid descriptor: %d", __func__, n));
			return;
		}
		if (n != 1) {
			WPRINTF(
			    ("%s: invalid number of descriptors in chain: %d",
				__func__, n));
			/* release invalid chain */
			vq_relchain(vq, req.idx, 0);
			return;
		}
		if (iov.iov_len < sizeof(struct vtinput_event)) {
			WPRINTF(("%s: invalid descriptor length: %lu", __func__,
			    iov.iov_len));
			/* release invalid chain */
			vq_relchain(vq, req.idx, 0);
			return;
		}

		/* save descriptor */
		queue->events[i].iov = iov;
		queue->events[i].idx = req.idx;
	}

	/*
	 * Second iteration through eventqueue:
	 *   Send events to guest by releasing chains
	 */
	for (uint32_t i = 0; i < queue->idx; ++i) {
		struct vtinput_event_elem event = queue->events[i];
		memcpy(event.iov.iov_base, &event.event,
		    sizeof(struct vtinput_event));
		vq_relchain(vq, event.idx, sizeof(struct vtinput_event));
	}
done:
	/* clear queue and send interrupt to guest */
	vtinput_eventqueue_clear(queue);
	vq_endchains(vq, 1);

	return;
}

static int
vtinput_read_event_from_host(int fd, struct input_event *event)
{
	const int len = read(fd, event, sizeof(struct input_event));
	if (len != sizeof(struct input_event)) {
		if (len == -1 && errno != EAGAIN) {
			WPRINTF(("%s: event read failed! len = %d, errno = %d",
			    __func__, len, errno));
		}

		/* host doesn't have more events for us */
		return (1);
	}

	return (0);
}

static void
vtinput_read_event(int fd __attribute((unused)),
    enum ev_type t __attribute__((unused)), void *arg __attribute__((unused)))
{
	struct pci_vtinput_softc *sc = arg;

	/* skip if driver isn't ready */
	if (!(sc->vsc_vs.vs_status & VIRTIO_CONFIG_STATUS_DRIVER_OK))
		return;

	/* read all events from host */
	struct input_event event;
	while (vtinput_read_event_from_host(sc->vsc_fd, &event) == 0) {
		/* add events to our queue */
		vtinput_eventqueue_add_event(&sc->vsc_eventqueue, &event);

		/* only send events to guest on EV_SYN or SYN_REPORT */
		if (event.type != EV_SYN || event.type != SYN_REPORT) {
			continue;
		}

		/* send host events to guest */
		vtinput_eventqueue_send_events(
		    &sc->vsc_eventqueue, &sc->vsc_queues[VTINPUT_EVENTQ]);
	}
}

static int
pci_vtinput_legacy_config(nvlist_t *nvl, const char *opts)
{
	if (opts == NULL)
		return (-1);

	/*
	 * parse opts:
	 *   virtio-input,/dev/input/eventX
	 */
	char *cp = strchr(opts, ',');
	if (cp == NULL) {
		set_config_value_node(nvl, "path", opts);
		return (0);
	}
	char *path = strndup(opts, cp - opts);
	set_config_value_node(nvl, "path", path);
	free(path);

	return (pci_parse_legacy_config(nvl, cp + 1));
}

static int
pci_vtinput_init(struct vmctx *ctx __unused, struct pci_devinst *pi,
    nvlist_t *nvl)
{
	struct pci_vtinput_softc *sc;

	/*
	 * Keep it here.
	 * Else it's possible to access it uninitialized by jumping to failed.
	 */
	pthread_mutexattr_t mtx_attr = NULL;

	sc = calloc(1, sizeof(struct pci_vtinput_softc));

	sc->vsc_evdev = get_config_value_node(nvl, "path");
	if (sc->vsc_evdev == NULL) {
		WPRINTF(("%s: missing required path config value", __func__));
		goto failed;
	}

	/*
	 * open evdev by using non blocking I/O:
	 *   read from /dev/input/eventX would block our thread otherwise
	 */
	sc->vsc_fd = open(sc->vsc_evdev, O_RDWR | O_NONBLOCK);
	if (sc->vsc_fd < 0) {
		WPRINTF(("%s: failed to open %s", __func__, sc->vsc_evdev));
		goto failed;
	}

	/* check if evdev is really a evdev */
	int evversion;
	int error = ioctl(sc->vsc_fd, EVIOCGVERSION, &evversion);
	if (error < 0) {
		WPRINTF(("%s: %s is no evdev", __func__, sc->vsc_evdev));
		goto failed;
	}

	/* gain exclusive access to evdev */
	error = ioctl(sc->vsc_fd, EVIOCGRAB, 1);
	if (error < 0) {
		WPRINTF(("%s: failed to grab %s", __func__, sc->vsc_evdev));
		goto failed;
	}

	if (pthread_mutexattr_init(&mtx_attr)) {
		WPRINTF(("%s: init mutexattr failed", __func__));
		goto failed;
	}
	if (pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE)) {
		WPRINTF(("%s: settype mutexattr failed", __func__));
		goto failed;
	}
	if (pthread_mutex_init(&sc->vsc_mtx, &mtx_attr)) {
		WPRINTF(("%s: init mutex failed", __func__));
		goto failed;
	}

	/* init softc */
	sc->vsc_eventqueue.idx = 0;
	sc->vsc_eventqueue.size = VTINPUT_MAX_PKT_LEN;
	sc->vsc_eventqueue.events = calloc(
	    sc->vsc_eventqueue.size, sizeof(struct vtinput_event_elem));
	sc->vsc_config_valid = 0;
	if (sc->vsc_eventqueue.events == NULL) {
		WPRINTF(("%s: failed to alloc eventqueue", __func__));
		goto failed;
	}

	/* register event handler */
	sc->vsc_evp = mevent_add(sc->vsc_fd, EVF_READ, vtinput_read_event, sc);
	if (sc->vsc_evp == NULL) {
		WPRINTF(("%s: could not register mevent", __func__));
		goto failed;
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_rights_init(&rights, CAP_EVENT, CAP_IOCTL, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(sc->vsc_fd, &rights) == -1) {
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	}
#endif

	/* link virtio to softc */
	vi_softc_linkup(
	    &sc->vsc_vs, &vtinput_vi_consts, sc, pi, sc->vsc_queues);
	sc->vsc_vs.vs_mtx = &sc->vsc_mtx;

	/* init virtio queues */
	sc->vsc_queues[VTINPUT_EVENTQ].vq_qsize = VTINPUT_RINGSZ;
	sc->vsc_queues[VTINPUT_EVENTQ].vq_notify = pci_vtinput_notify_eventq;
	sc->vsc_queues[VTINPUT_STATUSQ].vq_qsize = VTINPUT_RINGSZ;
	sc->vsc_queues[VTINPUT_STATUSQ].vq_notify = pci_vtinput_notify_statusq;

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_INPUT);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_INPUTDEV);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_INPUTDEV_OTHER);
	pci_set_cfgdata8(pi, PCIR_REVID, VIRTIO_REV_INPUT);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_SUBDEV_INPUT);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_SUBVEN_INPUT);

	/* add MSI-X table BAR */
	if (vi_intr_init(&sc->vsc_vs, 1, fbsdrun_virtio_msix()))
		goto failed;
	/* add virtio register */
	vi_set_io_bar(&sc->vsc_vs, 0);

	return (0);

failed:
	if (sc == NULL) {
		return (-1);
	}

	if (sc->vsc_evp)
		mevent_delete(sc->vsc_evp);
	if (sc->vsc_eventqueue.events)
		free(sc->vsc_eventqueue.events);
	if (sc->vsc_mtx)
		pthread_mutex_destroy(&sc->vsc_mtx);
	if (mtx_attr)
		pthread_mutexattr_destroy(&mtx_attr);
	if (sc->vsc_fd)
		close(sc->vsc_fd);

	free(sc);

	return (-1);
}

static const struct pci_devemu pci_de_vinput = {
	.pe_emu = "virtio-input",
	.pe_init = pci_vtinput_init,
	.pe_legacy_config = pci_vtinput_legacy_config,
	.pe_barwrite = vi_pci_write,
	.pe_barread = vi_pci_read,
};
PCI_EMUL_SET(pci_de_vinput);
