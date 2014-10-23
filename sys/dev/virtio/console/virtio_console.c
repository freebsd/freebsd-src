/*-
 * Copyright (c) 2014, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for VirtIO console devices. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sglist.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/queue.h>

#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/console/virtio_console.h>

#include "virtio_if.h"

#define VTCON_MAX_PORTS	1
#define VTCON_TTY_PREFIX "V"
#define VTCON_BULK_BUFSZ 128

struct vtcon_softc;

struct vtcon_port {
	struct vtcon_softc	*vtcport_sc;
	TAILQ_ENTRY(vtcon_port)  vtcport_next;
	struct mtx		 vtcport_mtx;
	int			 vtcport_id;
	struct tty		*vtcport_tty;
	struct virtqueue	*vtcport_invq;
	struct virtqueue	*vtcport_outvq;
	char			 vtcport_name[16];
};

#define VTCON_PORT_MTX(_port)		&(_port)->vtcport_mtx
#define VTCON_PORT_LOCK_INIT(_port) \
    mtx_init(VTCON_PORT_MTX((_port)), (_port)->vtcport_name, NULL, MTX_DEF)
#define VTCON_PORT_LOCK(_port)		mtx_lock(VTCON_PORT_MTX((_port)))
#define VTCON_PORT_UNLOCK(_port)	mtx_unlock(VTCON_PORT_MTX((_port)))
#define VTCON_PORT_LOCK_DESTROY(_port)	mtx_destroy(VTCON_PORT_MTX((_port)))
#define VTCON_PORT_LOCK_ASSERT(_port) \
    mtx_assert(VTCON_PORT_MTX((_port)), MA_OWNED)
#define VTCON_PORT_LOCK_ASSERT_NOTOWNED(_port) \
    mtx_assert(VTCON_PORT_MTX((_port)), MA_NOTOWNED)

struct vtcon_softc {
	device_t		 vtcon_dev;
	struct mtx		 vtcon_mtx;
	uint64_t		 vtcon_features;
	uint32_t		 vtcon_flags;
#define VTCON_FLAG_DETACHED	0x0001
#define VTCON_FLAG_SIZE		0x0010
#define VTCON_FLAG_MULTIPORT	0x0020

	struct task		 vtcon_ctrl_task;
	struct virtqueue	*vtcon_ctrl_rxvq;
	struct virtqueue	*vtcon_ctrl_txvq;

	uint32_t		 vtcon_max_ports;
	TAILQ_HEAD(, vtcon_port)
				 vtcon_ports;

	/*
	 * Ports can be added and removed during runtime, but we have
	 * to allocate all the virtqueues during attach. This array is
	 * indexed by the port ID.
	 */
	struct vtcon_port_extra {
		struct vtcon_port	*port;
		struct virtqueue	*invq;
		struct virtqueue	*outvq;
	}			*vtcon_portsx;
};

#define VTCON_MTX(_sc)		&(_sc)->vtcon_mtx
#define VTCON_LOCK_INIT(_sc, _name) \
    mtx_init(VTCON_MTX((_sc)), (_name), NULL, MTX_DEF)
#define VTCON_LOCK(_sc)		mtx_lock(VTCON_MTX((_sc)))
#define VTCON_UNLOCK(_sc)	mtx_unlock(VTCON_MTX((_sc)))
#define VTCON_LOCK_DESTROY(_sc)	mtx_destroy(VTCON_MTX((_sc)))
#define VTCON_LOCK_ASSERT(_sc)	mtx_assert(VTCON_MTX((_sc)), MA_OWNED)
#define VTCON_LOCK_ASSERT_NOTOWNED(_sc) \
    mtx_assert(VTCON_MTX((_sc)), MA_NOTOWNED)

#define VTCON_ASSERT_VALID_PORTID(_sc, _id)			\
    KASSERT((_id) >= 0 && (_id) < (_sc)->vtcon_max_ports,	\
        ("%s: port ID %d out of range", __func__, _id))

#define VTCON_FEATURES  0

static struct virtio_feature_desc vtcon_feature_desc[] = {
	{ VIRTIO_CONSOLE_F_SIZE,	"ConsoleSize"	},
	{ VIRTIO_CONSOLE_F_MULTIPORT,	"MultiplePorts"	},

	{ 0, NULL }
};

static int	 vtcon_modevent(module_t, int, void *);

static int	 vtcon_probe(device_t);
static int	 vtcon_attach(device_t);
static int	 vtcon_detach(device_t);
static int	 vtcon_config_change(device_t);

static void	 vtcon_negotiate_features(struct vtcon_softc *);
static int	 vtcon_alloc_virtqueues(struct vtcon_softc *);
static void	 vtcon_read_config(struct vtcon_softc *,
		     struct virtio_console_config *);

static void	 vtcon_determine_max_ports(struct vtcon_softc *,
		     struct virtio_console_config *);
static void	 vtcon_deinit_ports(struct vtcon_softc *);
static void	 vtcon_stop(struct vtcon_softc *);

static void	 vtcon_ctrl_rx_vq_intr(void *);
static int	 vtcon_ctrl_enqueue_msg(struct vtcon_softc *,
		     struct virtio_console_control *);
static int	 vtcon_ctrl_add_msg(struct vtcon_softc *);
static void	 vtcon_ctrl_readd_msg(struct vtcon_softc *,
		     struct virtio_console_control *);
static int	 vtcon_ctrl_populate(struct vtcon_softc *);
static void	 vtcon_ctrl_send_msg(struct vtcon_softc *,
		     struct virtio_console_control *control);
static void	 vtcon_ctrl_send_event(struct vtcon_softc *, uint32_t,
		     uint16_t, uint16_t);
static int	 vtcon_ctrl_init(struct vtcon_softc *);
static void	 vtcon_ctrl_drain(struct vtcon_softc *);
static void	 vtcon_ctrl_deinit(struct vtcon_softc *);
static void	 vtcon_ctrl_port_add_event(struct vtcon_softc *, int);
static void	 vtcon_ctrl_port_remove_event(struct vtcon_softc *, int);
static void	 vtcon_ctrl_port_console_event(struct vtcon_softc *, int);
static void	 vtcon_ctrl_port_open_event(struct vtcon_softc *, int);
static void	 vtcon_ctrl_process_msg(struct vtcon_softc *,
		     struct virtio_console_control *);
static void	 vtcon_ctrl_task_cb(void *, int);

static int	 vtcon_port_add_inbuf(struct vtcon_port *);
static void	 vtcon_port_readd_inbuf(struct vtcon_port *, void *);
static int	 vtcon_port_populate(struct vtcon_port *);
static void	 vtcon_port_destroy(struct vtcon_port *);
static int	 vtcon_port_create(struct vtcon_softc *, int,
		     struct vtcon_port **);
static void	 vtcon_port_drain_inbufs(struct vtcon_port *);
static void	 vtcon_port_teardown(struct vtcon_port *, int);
static void	 vtcon_port_change_size(struct vtcon_port *, uint16_t,
		     uint16_t);
static void	 vtcon_port_enable_intr(struct vtcon_port *);
static void	 vtcon_port_disable_intr(struct vtcon_port *);
static void	 vtcon_port_intr(struct vtcon_port *);
static void	 vtcon_port_in_vq_intr(void *);
static void	 vtcon_port_put(struct vtcon_port *, void *, int);
static void	 vtcon_port_send_ctrl_msg(struct vtcon_port *, uint16_t,
		     uint16_t);
static struct vtcon_port *vtcon_port_lookup_by_id(struct vtcon_softc *, int);

static int	 vtcon_tty_open(struct tty *);
static void	 vtcon_tty_close(struct tty *);
static void	 vtcon_tty_outwakeup(struct tty *);
static void	 vtcon_tty_free(void *);

static void	 vtcon_get_console_size(struct vtcon_softc *, uint16_t *,
		     uint16_t *);

static void	 vtcon_enable_interrupts(struct vtcon_softc *);
static void	 vtcon_disable_interrupts(struct vtcon_softc *);

static int	 vtcon_pending_free;

static struct ttydevsw vtcon_tty_class = {
	.tsw_flags	= 0,
	.tsw_open	= vtcon_tty_open,
	.tsw_close	= vtcon_tty_close,
	.tsw_outwakeup	= vtcon_tty_outwakeup,
	.tsw_free	= vtcon_tty_free,
};

static device_method_t vtcon_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtcon_probe),
	DEVMETHOD(device_attach,	vtcon_attach),
	DEVMETHOD(device_detach,	vtcon_detach),

	/* VirtIO methods. */
	DEVMETHOD(virtio_config_change,	vtcon_config_change),

	DEVMETHOD_END
};

static driver_t vtcon_driver = {
	"vtcon",
	vtcon_methods,
	sizeof(struct vtcon_softc)
};
static devclass_t vtcon_devclass;

DRIVER_MODULE(virtio_console, virtio_pci, vtcon_driver, vtcon_devclass,
    vtcon_modevent, 0);
MODULE_VERSION(virtio_console, 1);
MODULE_DEPEND(virtio_console, virtio, 1, 1, 1);

static int
vtcon_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = 0;
		break;
	case MOD_QUIESCE:
	case MOD_UNLOAD:
		error = vtcon_pending_free != 0 ? EBUSY : 0;
		/* error = EOPNOTSUPP; */
		break;
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtcon_probe(device_t dev)
{

	if (virtio_get_device_type(dev) != VIRTIO_ID_CONSOLE)
		return (ENXIO);

	device_set_desc(dev, "VirtIO Console Adapter");

	return (BUS_PROBE_DEFAULT);
}

static int
vtcon_attach(device_t dev)
{
	struct vtcon_softc *sc;
	struct virtio_console_config concfg;
	int error;

	sc = device_get_softc(dev);
	sc->vtcon_dev = dev;

	VTCON_LOCK_INIT(sc, device_get_nameunit(dev));
	TASK_INIT(&sc->vtcon_ctrl_task, 0, vtcon_ctrl_task_cb, sc);
	TAILQ_INIT(&sc->vtcon_ports);

	virtio_set_feature_desc(dev, vtcon_feature_desc);
	vtcon_negotiate_features(sc);

	if (virtio_with_feature(dev, VIRTIO_CONSOLE_F_SIZE))
		sc->vtcon_flags |= VTCON_FLAG_SIZE;
	if (virtio_with_feature(dev, VIRTIO_CONSOLE_F_MULTIPORT))
		sc->vtcon_flags |= VTCON_FLAG_MULTIPORT;

	vtcon_read_config(sc, &concfg);
	vtcon_determine_max_ports(sc, &concfg);

	error = vtcon_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		error = vtcon_ctrl_init(sc);
	else
		error = vtcon_port_create(sc, 0, NULL);
	if (error)
		goto fail;

	error = virtio_setup_intr(dev, INTR_TYPE_TTY);
	if (error) {
		device_printf(dev, "cannot setup virtqueue interrupts\n");
		goto fail;
	}

	vtcon_enable_interrupts(sc);

	vtcon_ctrl_send_event(sc, VIRTIO_CONSOLE_BAD_ID,
	    VIRTIO_CONSOLE_DEVICE_READY, 1);

fail:
	if (error)
		vtcon_detach(dev);

	return (error);
}

static int
vtcon_detach(device_t dev)
{
	struct vtcon_softc *sc;

	sc = device_get_softc(dev);

	VTCON_LOCK(sc);
	sc->vtcon_flags |= VTCON_FLAG_DETACHED;
	if (device_is_attached(dev))
		vtcon_stop(sc);
	VTCON_UNLOCK(sc);

	taskqueue_drain(taskqueue_thread, &sc->vtcon_ctrl_task);

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		vtcon_ctrl_deinit(sc);

	vtcon_deinit_ports(sc);

	VTCON_LOCK_DESTROY(sc);

	return (0);
}

static int
vtcon_config_change(device_t dev)
{
	struct vtcon_softc *sc;
	struct vtcon_port *port;
	uint16_t cols, rows;

	sc = device_get_softc(dev);

	/*
	 * With the multiport feature, all configuration changes are
	 * done through control virtqueue events. This is a spurious
	 * interrupt.
	 */
	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		return (0);

	if (sc->vtcon_flags & VTCON_FLAG_SIZE) {
		/*
		 * For now, assume the first (only) port is the 'console'.
		 * Note QEMU does not implement this feature yet.
		 */
		VTCON_LOCK(sc);
		if ((port = vtcon_port_lookup_by_id(sc, 0)) != NULL) {
			vtcon_get_console_size(sc, &cols, &rows);
			vtcon_port_change_size(port, cols, rows);
		}
		VTCON_UNLOCK(sc);
	}

	return (0);
}

static void
vtcon_negotiate_features(struct vtcon_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtcon_dev;
	features = VTCON_FEATURES;

	sc->vtcon_features = virtio_negotiate_features(dev, features);
}

#define VTCON_GET_CONFIG(_dev, _feature, _field, _cfg)			\
	if (virtio_with_feature(_dev, _feature)) {			\
		virtio_read_device_config(_dev,				\
		    offsetof(struct virtio_console_config, _field),	\
		    &(_cfg)->_field, sizeof((_cfg)->_field));		\
	}

static void
vtcon_read_config(struct vtcon_softc *sc, struct virtio_console_config *concfg)
{
	device_t dev;

	dev = sc->vtcon_dev;

	bzero(concfg, sizeof(struct virtio_console_config));

	/* Read the configuration if the feature was negotiated. */
	VTCON_GET_CONFIG(dev, VIRTIO_CONSOLE_F_SIZE, cols, concfg);
	VTCON_GET_CONFIG(dev, VIRTIO_CONSOLE_F_SIZE, rows, concfg);
	VTCON_GET_CONFIG(dev, VIRTIO_CONSOLE_F_MULTIPORT, max_nr_ports, concfg);
}

#undef VTCON_GET_CONFIG

static int
vtcon_alloc_virtqueues(struct vtcon_softc *sc)
{
	device_t dev;
	struct vq_alloc_info *info;
	struct vtcon_port_extra *portx;
	int i, idx, portidx, nvqs, error;

	dev = sc->vtcon_dev;

	sc->vtcon_portsx = malloc(sizeof(struct vtcon_port_extra) *
	    sc->vtcon_max_ports, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->vtcon_portsx == NULL)
		return (ENOMEM);

	nvqs = sc->vtcon_max_ports * 2;
	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		nvqs += 2;

	info = malloc(sizeof(struct vq_alloc_info) * nvqs, M_TEMP, M_NOWAIT);
	if (info == NULL)
		return (ENOMEM);

	for (i = 0, idx = 0, portidx = 0; i < nvqs / 2; i++, idx+=2) {

		if (i == 1) {
			/* The control virtqueues are after the first port. */
			VQ_ALLOC_INFO_INIT(&info[idx], 0,
			    vtcon_ctrl_rx_vq_intr, sc, &sc->vtcon_ctrl_rxvq,
			    "%s-control rx", device_get_nameunit(dev));
			VQ_ALLOC_INFO_INIT(&info[idx+1], 0,
			    NULL, sc, &sc->vtcon_ctrl_txvq,
			    "%s-control tx", device_get_nameunit(dev));
			continue;
		}

		portx = &sc->vtcon_portsx[portidx];

		VQ_ALLOC_INFO_INIT(&info[idx], 0, vtcon_port_in_vq_intr,
		    portx, &portx->invq, "%s-port%d in",
		    device_get_nameunit(dev), portidx);
		VQ_ALLOC_INFO_INIT(&info[idx+1], 0, NULL,
		    NULL, &portx->outvq, "%s-port%d out",
		    device_get_nameunit(dev), portidx);

		portidx++;
	}

	error = virtio_alloc_virtqueues(dev, 0, nvqs, info);
	free(info, M_TEMP);

	return (error);
}

static void
vtcon_determine_max_ports(struct vtcon_softc *sc,
    struct virtio_console_config *concfg)
{

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT) {
		sc->vtcon_max_ports =
		    min(concfg->max_nr_ports, VTCON_MAX_PORTS);
		if (sc->vtcon_max_ports == 0)
			sc->vtcon_max_ports = 1;
	} else
		sc->vtcon_max_ports = 1;
}

static void
vtcon_deinit_ports(struct vtcon_softc *sc)
{
	struct vtcon_port *port, *tmp;

	TAILQ_FOREACH_SAFE(port, &sc->vtcon_ports, vtcport_next, tmp) {
		vtcon_port_teardown(port, 1);
	}

	if (sc->vtcon_portsx != NULL) {
		free(sc->vtcon_portsx, M_DEVBUF);
		sc->vtcon_portsx = NULL;
	}
}

static void
vtcon_stop(struct vtcon_softc *sc)
{

	vtcon_disable_interrupts(sc);
	virtio_stop(sc->vtcon_dev);
}

static void
vtcon_ctrl_rx_vq_intr(void *xsc)
{
	struct vtcon_softc *sc;

	sc = xsc;

	/*
	 * Some events require us to potentially block, but it easier
	 * to just defer all event handling to a seperate thread.
	 */
	taskqueue_enqueue(taskqueue_thread, &sc->vtcon_ctrl_task);
}

static int
vtcon_ctrl_enqueue_msg(struct vtcon_softc *sc,
    struct virtio_console_control *control)
{
	struct sglist_seg segs[1];
	struct sglist sg;
	struct virtqueue *vq;
	int error __unused;

	vq = sc->vtcon_ctrl_rxvq;

	sglist_init(&sg, 1, segs);
	error = sglist_append(&sg, control, sizeof(*control));
	KASSERT(error == 0 && sg.sg_nseg == 1,
	    ("%s: error %d adding control msg to sglist", __func__, error));

	return (virtqueue_enqueue(vq, control, &sg, 0, 1));
}

static int
vtcon_ctrl_add_msg(struct vtcon_softc *sc)
{
	struct virtio_console_control *control;
	int error;

	control = malloc(sizeof(*control), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (control == NULL)
		return (ENOMEM);

	error = vtcon_ctrl_enqueue_msg(sc, control);
	if (error)
		free(control, M_DEVBUF);

	return (error);
}

static void
vtcon_ctrl_readd_msg(struct vtcon_softc *sc,
    struct virtio_console_control *control)
{
	int error;

	bzero(control, sizeof(*control));

	error = vtcon_ctrl_enqueue_msg(sc, control);
	KASSERT(error == 0,
	    ("%s: cannot requeue control buffer %d", __func__, error));
}

static int
vtcon_ctrl_populate(struct vtcon_softc *sc)
{
	struct virtqueue *vq;
	int nbufs, error;

	vq = sc->vtcon_ctrl_rxvq;
	error = ENOSPC;

	for (nbufs = 0; !virtqueue_full(vq); nbufs++) {
		error = vtcon_ctrl_add_msg(sc);
		if (error)
			break;
	}

	if (nbufs > 0) {
		virtqueue_notify(vq);
		/*
		 * EMSGSIZE signifies the virtqueue did not have enough
		 * entries available to hold the last buf. This is not
		 * an error.
		 */
		if (error == EMSGSIZE)
			error = 0;
	}

	return (error);
}

static void
vtcon_ctrl_send_msg(struct vtcon_softc *sc,
    struct virtio_console_control *control)
{
	struct sglist_seg segs[1];
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = sc->vtcon_ctrl_txvq;
	KASSERT(virtqueue_empty(vq),
	    ("%s: virtqueue is not emtpy", __func__));

	sglist_init(&sg, 1, segs);
	error = sglist_append(&sg, control, sizeof(*control));
	KASSERT(error == 0 && sg.sg_nseg == 1,
	    ("%s: error %d adding control msg to sglist", __func__, error));

	error = virtqueue_enqueue(vq, control, &sg, 1, 0);
	if (error == 0) {
		virtqueue_notify(vq);
		virtqueue_poll(vq, NULL);
	}
}

static void
vtcon_ctrl_send_event(struct vtcon_softc *sc, uint32_t portid, uint16_t event,
    uint16_t value)
{
	struct virtio_console_control control;

	if ((sc->vtcon_flags & VTCON_FLAG_MULTIPORT) == 0)
		return;

	control.id = portid;
	control.event = event;
	control.value = value;

	vtcon_ctrl_send_msg(sc, &control);
}

static int
vtcon_ctrl_init(struct vtcon_softc *sc)
{
	int error;

	error = vtcon_ctrl_populate(sc);

	return (error);
}

static void
vtcon_ctrl_drain(struct vtcon_softc *sc)
{
	struct virtio_console_control *control;
	struct virtqueue *vq;
	int last;

	vq = sc->vtcon_ctrl_rxvq;
	last = 0;

	if (vq == NULL)
		return;

	while ((control = virtqueue_drain(vq, &last)) != NULL)
		free(control, M_DEVBUF);
}

static void
vtcon_ctrl_deinit(struct vtcon_softc *sc)
{

	vtcon_ctrl_drain(sc);
}

static void
vtcon_ctrl_port_add_event(struct vtcon_softc *sc, int id)
{
	device_t dev;
	struct vtcon_port *port;
	int error;

	dev = sc->vtcon_dev;

	if (vtcon_port_lookup_by_id(sc, id) != NULL) {
		device_printf(dev, "%s: adding port %d, but already exists\n",
		    __func__, id);
		return;
	}

	error = vtcon_port_create(sc, id, &port);
	if (error) {
		device_printf(dev, "%s: cannot create port %d: %d\n",
		    __func__, id, error);
		return;
	}

	vtcon_port_send_ctrl_msg(port, VIRTIO_CONSOLE_PORT_READY, 1);
}

static void
vtcon_ctrl_port_remove_event(struct vtcon_softc *sc, int id)
{
	device_t dev;
	struct vtcon_port *port;

	dev = sc->vtcon_dev;

	port = vtcon_port_lookup_by_id(sc, id);
	if (port == NULL) {
		device_printf(dev, "%s: remove port %d, but does not exist\n",
		    __func__, id);
		return;
	}

	vtcon_port_teardown(port, 1);
}

static void
vtcon_ctrl_port_console_event(struct vtcon_softc *sc, int id)
{
	device_t dev;

	dev = sc->vtcon_dev;

	/*
	 * BMV: I don't think we need to do anything.
	 */
	device_printf(dev, "%s: port %d console event\n", __func__, id);
}

static void
vtcon_ctrl_port_open_event(struct vtcon_softc *sc, int id)
{
	device_t dev;
	struct vtcon_port *port;

	dev = sc->vtcon_dev;

	port = vtcon_port_lookup_by_id(sc, id);
	if (port == NULL) {
		device_printf(dev, "%s: open port %d, but does not exist\n",
		    __func__, id);
		return;
	}

	vtcon_port_enable_intr(port);
}

static void
vtcon_ctrl_process_msg(struct vtcon_softc *sc,
    struct virtio_console_control *control)
{
	device_t dev;
	int id;

	dev = sc->vtcon_dev;
	id = control->id;

	if (id < 0 || id >= sc->vtcon_max_ports) {
		device_printf(dev, "%s: invalid port ID %d\n", __func__, id);
		return;
	}

	switch (control->event) {
	case VIRTIO_CONSOLE_PORT_ADD:
		vtcon_ctrl_port_add_event(sc, id);
		break;

	case VIRTIO_CONSOLE_PORT_REMOVE:
		vtcon_ctrl_port_remove_event(sc, id);
		break;

	case VIRTIO_CONSOLE_CONSOLE_PORT:
		vtcon_ctrl_port_console_event(sc, id);
		break;

	case VIRTIO_CONSOLE_RESIZE:
		break;

	case VIRTIO_CONSOLE_PORT_OPEN:
		vtcon_ctrl_port_open_event(sc, id);
		break;

	case VIRTIO_CONSOLE_PORT_NAME:
		break;
	}
}

static void
vtcon_ctrl_task_cb(void *xsc, int pending)
{
	struct vtcon_softc *sc;
	struct virtqueue *vq;
	struct virtio_console_control *control;

	sc = xsc;
	vq = sc->vtcon_ctrl_rxvq;

	VTCON_LOCK(sc);
	while ((sc->vtcon_flags & VTCON_FLAG_DETACHED) == 0) {
		control = virtqueue_dequeue(vq, NULL);
		if (control == NULL)
			break;

		VTCON_UNLOCK(sc);
		vtcon_ctrl_process_msg(sc, control);
		VTCON_LOCK(sc);
		vtcon_ctrl_readd_msg(sc, control);
	}
	VTCON_UNLOCK(sc);

	if (virtqueue_enable_intr(vq) != 0)
		taskqueue_enqueue(taskqueue_thread, &sc->vtcon_ctrl_task);
}

static int
vtcon_port_enqueue_inbuf(struct vtcon_port *port, void *buf, size_t len)
{
	struct sglist_seg segs[1];
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = port->vtcport_invq;

	sglist_init(&sg, 1, segs);
	error = sglist_append(&sg, buf, len);
	KASSERT(error == 0 && sg.sg_nseg == 1,
	    ("%s: error %d adding buffer to sglist", __func__, error));

	return (virtqueue_enqueue(vq, buf, &sg, 0, 1));
}

static int
vtcon_port_add_inbuf(struct vtcon_port *port)
{
	void *buf;
	int error;

	buf = malloc(VTCON_BULK_BUFSZ, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (buf == NULL)
		return (ENOMEM);

	error = vtcon_port_enqueue_inbuf(port, buf, VTCON_BULK_BUFSZ);
	if (error)
		free(buf, M_DEVBUF);

	return (error);
}

static void
vtcon_port_readd_inbuf(struct vtcon_port *port, void *buf)
{
	int error __unused;

	error = vtcon_port_enqueue_inbuf(port, buf, VTCON_BULK_BUFSZ);
	KASSERT(error == 0,
	    ("%s: cannot requeue input buffer %d", __func__, error));
}

static int
vtcon_port_populate(struct vtcon_port *port)
{
	struct virtqueue *vq;
	int nbufs, error;

	vq = port->vtcport_invq;
	error = ENOSPC;

	for (nbufs = 0; !virtqueue_full(vq); nbufs++) {
		error = vtcon_port_add_inbuf(port);
		if (error)
			break;
	}

	if (nbufs > 0) {
		virtqueue_notify(vq);
		/*
		 * EMSGSIZE signifies the virtqueue did not have enough
		 * entries available to hold the last buf. This is not
		 * an error.
		 */
		if (error == EMSGSIZE)
			error = 0;
	}

	return (error);
}

static void
vtcon_port_destroy(struct vtcon_port *port)
{

	port->vtcport_sc = NULL;
	port->vtcport_id = -1;
	VTCON_PORT_LOCK_DESTROY(port);
	free(port, M_DEVBUF);
}

static int
vtcon_port_create(struct vtcon_softc *sc, int id, struct vtcon_port **portp)
{
	device_t dev;
	struct vtcon_port_extra *portx;
	struct vtcon_port *port;
	int error;

	MPASS(id < sc->vtcon_max_ports);
	dev = sc->vtcon_dev;
	portx = &sc->vtcon_portsx[id];

	if (portx->port != NULL)
		return (EEXIST);

	port = malloc(sizeof(struct vtcon_port), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (port == NULL)
		return (ENOMEM);

	port->vtcport_sc = sc;
	port->vtcport_id = id;
	snprintf(port->vtcport_name, sizeof(port->vtcport_name), "%s-port%d",
	    device_get_nameunit(dev), id);
	VTCON_PORT_LOCK_INIT(port);
	port->vtcport_tty = tty_alloc_mutex(&vtcon_tty_class, port,
	    &port->vtcport_mtx);

	/*
	 * Assign virtqueues saved from attach. To be safe, clear the
	 * virtqueue too.
	 */
	port->vtcport_invq = portx->invq;
	port->vtcport_outvq = portx->outvq;
	vtcon_port_drain_inbufs(port);

	error = vtcon_port_populate(port);
	if (error) {
		vtcon_port_teardown(port, 0);
		return (error);
	}

	tty_makedev(port->vtcport_tty, NULL, "%s%r.%r", VTCON_TTY_PREFIX,
	    device_get_unit(dev), id);

	VTCON_LOCK(sc);
	portx->port = port;
	TAILQ_INSERT_TAIL(&sc->vtcon_ports, port, vtcport_next);
	VTCON_UNLOCK(sc);

	if (portp != NULL)
		*portp = port;

	return (0);
}

static void
vtcon_port_drain_inbufs(struct vtcon_port *port)
{
	struct virtqueue *vq;
	void *buf;
	int last;

	vq = port->vtcport_invq;
	last = 0;

	if (vq == NULL)
		return;

	while ((buf = virtqueue_drain(vq, &last)) != NULL)
		free(buf, M_DEVBUF);
}

static void
vtcon_port_teardown(struct vtcon_port *port, int ontailq)
{
	struct vtcon_softc *sc;
	struct vtcon_port_extra *portx;
	struct tty *tp;
	int id;

	sc = port->vtcport_sc;
	id = port->vtcport_id;
	tp = port->vtcport_tty;

	VTCON_ASSERT_VALID_PORTID(sc, id);
	portx = &sc->vtcon_portsx[id];

	VTCON_PORT_LOCK(port);
	vtcon_port_drain_inbufs(port);
	VTCON_PORT_UNLOCK(port);

	VTCON_LOCK(sc);
	KASSERT(portx->port == NULL || portx->port == port,
	    ("%s: port %d mismatch %p/%p", __func__, id, portx->port, port));
	portx->port = NULL;
	if (ontailq != 0)
		TAILQ_REMOVE(&sc->vtcon_ports, port, vtcport_next);
	VTCON_UNLOCK(sc);

	if (tp != NULL) {
		port->vtcport_tty = NULL;
		atomic_add_int(&vtcon_pending_free, 1);

		VTCON_PORT_LOCK(port);
		tty_rel_gone(tp);
	} else
		vtcon_port_destroy(port);
}

static void
vtcon_port_change_size(struct vtcon_port *port, uint16_t cols, uint16_t rows)
{
	struct tty *tp;
	struct winsize sz;

	tp = port->vtcport_tty;

	if (tp == NULL)
		return;

	bzero(&sz, sizeof(struct winsize));
	sz.ws_col = cols;
	sz.ws_row = rows;

	VTCON_PORT_LOCK(port);
	tty_set_winsize(tp, &sz);
	VTCON_PORT_UNLOCK(port);
}

static void
vtcon_port_enable_intr(struct vtcon_port *port)
{

	/*
	 * NOTE: The out virtqueue is always polled, so we keep its
	 * interupt disabled.
	 */

	virtqueue_enable_intr(port->vtcport_invq);
}

static void
vtcon_port_disable_intr(struct vtcon_port *port)
{

	if (port->vtcport_invq != NULL)
		virtqueue_disable_intr(port->vtcport_invq);
	if (port->vtcport_outvq != NULL)
		virtqueue_disable_intr(port->vtcport_outvq);
}

static void
vtcon_port_intr(struct vtcon_port *port)
{
	struct tty *tp;
	struct virtqueue *vq;
	char *buf;
	uint32_t len;
	int i, deq;

	tp = port->vtcport_tty;
	vq = port->vtcport_invq;

again:
	deq = 0;

	VTCON_PORT_LOCK(port);
	while ((buf = virtqueue_dequeue(vq, &len)) != NULL) {
		deq++;
		for (i = 0; i < len; i++)
			ttydisc_rint(tp, buf[i], 0);
		vtcon_port_readd_inbuf(port, buf);
	}
	ttydisc_rint_done(tp);
	VTCON_PORT_UNLOCK(port);

	if (deq > 0)
		virtqueue_notify(vq);

	if (virtqueue_enable_intr(vq) != 0)
		goto again;
}

static void
vtcon_port_in_vq_intr(void *xportx)
{
	struct vtcon_port_extra *portx;
	struct vtcon_port *port;

	portx = xportx;
	port = portx->port;

	if (port != NULL)
		vtcon_port_intr(port);
}

static void
vtcon_port_put(struct vtcon_port *port, void *buf, int bufsize)
{
	struct sglist_seg segs[1];
	struct sglist sg;
	struct virtqueue *vq;
	int error;

	vq = port->vtcport_outvq;

	sglist_init(&sg, 1, segs);
	error = sglist_append(&sg, buf, bufsize);
	KASSERT(error == 0 && sg.sg_nseg == 1,
	    ("%s: error %d adding buffer to sglist", __func__, error));

	KASSERT(virtqueue_empty(vq), ("%s: port %p virtqueue not emtpy",
	     __func__, port));

	if (virtqueue_enqueue(vq, buf, &sg, 1, 0) == 0) {
		virtqueue_notify(vq);
		virtqueue_poll(vq, NULL);
	}
}

static void
vtcon_port_send_ctrl_msg(struct vtcon_port *port, uint16_t event,
    uint16_t value)
{
	struct vtcon_softc *sc;

	sc = port->vtcport_sc;

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		vtcon_ctrl_send_event(sc, port->vtcport_id, event, value);
}

static struct vtcon_port *
vtcon_port_lookup_by_id(struct vtcon_softc *sc, int id)
{
	struct vtcon_port *port;

	TAILQ_FOREACH(port, &sc->vtcon_ports, vtcport_next) {
		if (port->vtcport_id == id)
			break;
	}

	return (port);
}

static int
vtcon_tty_open(struct tty *tp)
{
	struct vtcon_port *port;

	port = tty_softc(tp);

	vtcon_port_send_ctrl_msg(port, VIRTIO_CONSOLE_PORT_OPEN, 1);

	return (0);
}

static void
vtcon_tty_close(struct tty *tp)
{
	struct vtcon_port *port;

	port = tty_softc(tp);

	vtcon_port_send_ctrl_msg(port, VIRTIO_CONSOLE_PORT_OPEN, 0);
}

static void
vtcon_tty_outwakeup(struct tty *tp)
{
	struct vtcon_port *port;
	char buf[VTCON_BULK_BUFSZ];
	int len;

	port = tty_softc(tp);

	while ((len = ttydisc_getc(tp, buf, sizeof(buf))) != 0)
		vtcon_port_put(port, buf, len);
}

static void
vtcon_tty_free(void *xport)
{
	struct vtcon_port *port;

	port = xport;

	vtcon_port_destroy(port);
	atomic_subtract_int(&vtcon_pending_free, 1);
}

static void
vtcon_get_console_size(struct vtcon_softc *sc, uint16_t *cols, uint16_t *rows)
{
	struct virtio_console_config concfg;

	KASSERT(sc->vtcon_flags & VTCON_FLAG_SIZE,
	    ("%s: size feature not negotiated", __func__));

	vtcon_read_config(sc, &concfg);

	*cols = concfg.cols;
	*rows = concfg.rows;
}

static void
vtcon_enable_interrupts(struct vtcon_softc *sc)
{
	struct vtcon_port *port;

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		virtqueue_enable_intr(sc->vtcon_ctrl_rxvq);

	TAILQ_FOREACH(port, &sc->vtcon_ports, vtcport_next)
		vtcon_port_enable_intr(port);
}

static void
vtcon_disable_interrupts(struct vtcon_softc *sc)
{
	struct vtcon_port *port;

	if (sc->vtcon_flags & VTCON_FLAG_MULTIPORT)
		virtqueue_disable_intr(sc->vtcon_ctrl_rxvq);

	TAILQ_FOREACH(port, &sc->vtcon_ports, vtcport_next)
		vtcon_port_disable_intr(port);
}
