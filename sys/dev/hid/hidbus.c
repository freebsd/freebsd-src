/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019-2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sx.h>

#define	HID_DEBUG_VAR	hid_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>

#include "hid_if.h"

#define	INPUT_EPOCH	global_epoch_preempt
#define	HID_RSIZE_MAX	1024

static hid_intr_t	hidbus_intr;

static device_probe_t	hidbus_probe;
static device_attach_t	hidbus_attach;
static device_detach_t	hidbus_detach;

struct hidbus_ivars {
	int32_t				usage;
	uint8_t				index;
	uint32_t			flags;
	uintptr_t			driver_info;    /* for internal use */
	struct mtx			*mtx;		/* child intr mtx */
	hid_intr_t			*intr_handler;	/* executed under mtx*/
	void				*intr_ctx;
	unsigned int			refcnt;		/* protected by mtx */
	struct epoch_context		epoch_ctx;
	CK_STAILQ_ENTRY(hidbus_ivars)	link;
};

struct hidbus_softc {
	device_t			dev;
	struct sx			sx;
	struct mtx			mtx;

	bool				nowrite;

	struct hid_rdesc_info		rdesc;
	bool				overloaded;
	int				nest;	/* Child attach nesting lvl */
	int				nauto;	/* Number of autochildren */

	CK_STAILQ_HEAD(, hidbus_ivars)	tlcs;
};

static int
hidbus_fill_rdesc_info(struct hid_rdesc_info *hri, const void *data,
    hid_size_t len)
{
	int error = 0;

	hri->data = __DECONST(void *, data);
	hri->len = len;

	/*
	 * If report descriptor is not available yet, set maximal
	 * report sizes high enough to allow hidraw to work.
	 */
	hri->isize = len == 0 ? HID_RSIZE_MAX :
	    hid_report_size_max(data, len, hid_input, &hri->iid);
	hri->osize = len == 0 ? HID_RSIZE_MAX :
	    hid_report_size_max(data, len, hid_output, &hri->oid);
	hri->fsize = len == 0 ? HID_RSIZE_MAX :
	    hid_report_size_max(data, len, hid_feature, &hri->fid);

	if (hri->isize > HID_RSIZE_MAX) {
		DPRINTF("input size is too large, %u bytes (truncating)\n",
		    hri->isize);
		hri->isize = HID_RSIZE_MAX;
		error = EOVERFLOW;
	}
	if (hri->osize > HID_RSIZE_MAX) {
		DPRINTF("output size is too large, %u bytes (truncating)\n",
		    hri->osize);
		hri->osize = HID_RSIZE_MAX;
		error = EOVERFLOW;
	}
	if (hri->fsize > HID_RSIZE_MAX) {
		DPRINTF("feature size is too large, %u bytes (truncating)\n",
		    hri->fsize);
		hri->fsize = HID_RSIZE_MAX;
		error = EOVERFLOW;
	}

	return (error);
}

int
hidbus_locate(const void *desc, hid_size_t size, int32_t u, enum hid_kind k,
    uint8_t tlc_index, uint8_t index, struct hid_location *loc,
    uint32_t *flags, uint8_t *id, struct hid_absinfo *ai)
{
	struct hid_data *d;
	struct hid_item h;
	int i;

	d = hid_start_parse(desc, size, 1 << k);
	HIDBUS_FOREACH_ITEM(d, &h, tlc_index) {
		for (i = 0; i < h.nusages; i++) {
			if (h.kind == k && h.usages[i] == u) {
				if (index--)
					break;
				if (loc != NULL)
					*loc = h.loc;
				if (flags != NULL)
					*flags = h.flags;
				if (id != NULL)
					*id = h.report_ID;
				if (ai != NULL && (h.flags&HIO_RELATIVE) == 0)
					*ai = (struct hid_absinfo) {
					    .max = h.logical_maximum,
					    .min = h.logical_minimum,
					    .res = hid_item_resolution(&h),
					};
				hid_end_parse(d);
				return (1);
			}
		}
	}
	if (loc != NULL)
		loc->size = 0;
	if (flags != NULL)
		*flags = 0;
	if (id != NULL)
		*id = 0;
	hid_end_parse(d);
	return (0);
}

bool
hidbus_is_collection(const void *desc, hid_size_t size, int32_t usage,
    uint8_t tlc_index)
{
	struct hid_data *d;
	struct hid_item h;
	bool ret = false;

	d = hid_start_parse(desc, size, 0);
	HIDBUS_FOREACH_ITEM(d, &h, tlc_index) {
		if (h.kind == hid_collection && h.usage == usage) {
			ret = true;
			break;
		}
	}
	hid_end_parse(d);
	return (ret);
}

static device_t
hidbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct hidbus_softc *sc = device_get_softc(dev);
	struct hidbus_ivars *tlc;
	device_t child;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
			return (child);

	tlc = malloc(sizeof(struct hidbus_ivars), M_DEVBUF, M_WAITOK | M_ZERO);
	tlc->mtx = &sc->mtx;
	device_set_ivars(child, tlc);
	sx_xlock(&sc->sx);
	CK_STAILQ_INSERT_TAIL(&sc->tlcs, tlc, link);
	sx_unlock(&sc->sx);

	return (child);
}

static int
hidbus_enumerate_children(device_t dev, const void* data, hid_size_t len)
{
	struct hidbus_softc *sc = device_get_softc(dev);
	struct hid_data *hd;
	struct hid_item hi;
	device_t child;
	uint8_t index = 0;

	if (data == NULL || len == 0)
		return (ENXIO);

	/* Add a child for each top level collection */
	hd = hid_start_parse(data, len, 1 << hid_input);
	while (hid_get_item(hd, &hi)) {
		if (hi.kind != hid_collection || hi.collevel != 1)
			continue;
		child = BUS_ADD_CHILD(dev, 0, NULL, -1);
		if (child == NULL) {
			device_printf(dev, "Could not add HID device\n");
			continue;
		}
		hidbus_set_index(child, index);
		hidbus_set_usage(child, hi.usage);
		hidbus_set_flags(child, HIDBUS_FLAG_AUTOCHILD);
		index++;
		DPRINTF("Add child TLC: 0x%04x:0x%04x\n",
		    HID_GET_USAGE_PAGE(hi.usage), HID_GET_USAGE(hi.usage));
	}
	hid_end_parse(hd);

	if (index == 0)
		return (ENXIO);

	sc->nauto = index;

	return (0);
}

static int
hidbus_attach_children(device_t dev)
{
	struct hidbus_softc *sc = device_get_softc(dev);
	int error;

	HID_INTR_SETUP(device_get_parent(dev), hidbus_intr, sc, &sc->rdesc);

	error = hidbus_enumerate_children(dev, sc->rdesc.data, sc->rdesc.len);
	if (error != 0)
		DPRINTF("failed to enumerate children: error %d\n", error);

	/*
	 * hidbus_attach_children() can recurse through device_identify->
	 * hid_set_report_descr() call sequence. Do not perform children
	 * attach twice in that case.
	 */
	sc->nest++;
	bus_generic_probe(dev);
	sc->nest--;
	if (sc->nest != 0)
		return (0);

	if (hid_is_keyboard(sc->rdesc.data, sc->rdesc.len) != 0)
		error = bus_generic_attach(dev);
	else
		error = bus_delayed_attach_children(dev);
	if (error != 0)
		device_printf(dev, "failed to attach child: error %d\n", error);

	return (error);
}

static int
hidbus_detach_children(device_t dev)
{
	device_t *children, bus;
	bool is_bus;
	int i, error;

	error = 0;

	is_bus = device_get_devclass(dev) == hidbus_devclass;
	bus = is_bus ? dev : device_get_parent(dev);

	KASSERT(device_get_devclass(bus) == hidbus_devclass,
	    ("Device is not hidbus or it's child"));

	if (is_bus) {
		/* If hidbus is passed, delete all children. */
		bus_generic_detach(bus);
		device_delete_children(bus);
	} else {
		/*
		 * If hidbus child is passed, delete all hidbus children
		 * except caller. Deleting the caller may result in deadlock.
		 */
		error = device_get_children(bus, &children, &i);
		if (error != 0)
			return (error);
		while (i-- > 0) {
			if (children[i] == dev)
				continue;
			DPRINTF("Delete child. index=%d (%s)\n",
			    hidbus_get_index(children[i]),
			    device_get_nameunit(children[i]));
			error = device_delete_child(bus, children[i]);
			if (error) {
				DPRINTF("Failed deleting %s\n",
				    device_get_nameunit(children[i]));
				break;
			}
		}
		free(children, M_TEMP);
	}

	HID_INTR_UNSETUP(device_get_parent(bus));

	return (error);
}

static int
hidbus_probe(device_t dev)
{

	device_set_desc(dev, "HID bus");

	/* Allow other subclasses to override this driver. */
	return (BUS_PROBE_GENERIC);
}

static int
hidbus_attach(device_t dev)
{
	struct hidbus_softc *sc = device_get_softc(dev);
	struct hid_device_info *devinfo = device_get_ivars(dev);
	void *d_ptr = NULL;
	hid_size_t d_len;
	int error;

	sc->dev = dev;
	CK_STAILQ_INIT(&sc->tlcs);
	mtx_init(&sc->mtx, "hidbus ivar lock", NULL, MTX_DEF);
	sx_init(&sc->sx, "hidbus ivar list lock");

	/*
	 * Ignore error. It is possible for non-HID device e.g. XBox360 gamepad
	 * to emulate HID through overloading of report descriptor.
	 */
	d_len = devinfo->rdescsize;
	if (d_len != 0) {
		d_ptr = malloc(d_len, M_DEVBUF, M_ZERO | M_WAITOK);
		error = hid_get_rdesc(dev, d_ptr, d_len);
		if (error != 0) {
			free(d_ptr, M_DEVBUF);
			d_len = 0;
			d_ptr = NULL;
		}
	}

	hidbus_fill_rdesc_info(&sc->rdesc, d_ptr, d_len);

	sc->nowrite = hid_test_quirk(devinfo, HQ_NOWRITE);

	error = hidbus_attach_children(dev);
	if (error != 0) {
		hidbus_detach(dev);
		return (ENXIO);
	}

	return (0);
}

static int
hidbus_detach(device_t dev)
{
	struct hidbus_softc *sc = device_get_softc(dev);

	hidbus_detach_children(dev);
	sx_destroy(&sc->sx);
	mtx_destroy(&sc->mtx);
	free(sc->rdesc.data, M_DEVBUF);

	return (0);
}

static void
hidbus_child_detached(device_t bus, device_t child)
{
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *tlc = device_get_ivars(child);

	KASSERT(tlc->refcnt == 0, ("Child device is running"));
	tlc->mtx = &sc->mtx;
	tlc->intr_handler = NULL;
	tlc->flags &= ~HIDBUS_FLAG_CAN_POLL;
}

/*
 * Epoch callback indicating tlc is safe to destroy
 */
static void
hidbus_ivar_dtor(epoch_context_t ctx)
{
	struct hidbus_ivars *tlc;

	tlc = __containerof(ctx, struct hidbus_ivars, epoch_ctx);
	free(tlc, M_DEVBUF);
}

static void
hidbus_child_deleted(device_t bus, device_t child)
{
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *tlc = device_get_ivars(child);

	sx_xlock(&sc->sx);
	KASSERT(tlc->refcnt == 0, ("Child device is running"));
	CK_STAILQ_REMOVE(&sc->tlcs, tlc, hidbus_ivars, link);
	sx_unlock(&sc->sx);
	epoch_call(INPUT_EPOCH, hidbus_ivar_dtor, &tlc->epoch_ctx);
}

static int
hidbus_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *tlc = device_get_ivars(child);

	switch (which) {
	case HIDBUS_IVAR_INDEX:
		*result = tlc->index;
		break;
	case HIDBUS_IVAR_USAGE:
		*result = tlc->usage;
		break;
	case HIDBUS_IVAR_FLAGS:
		*result = tlc->flags;
		break;
	case HIDBUS_IVAR_DRIVER_INFO:
		*result = tlc->driver_info;
		break;
	case HIDBUS_IVAR_LOCK:
		*result = (uintptr_t)(tlc->mtx == &sc->mtx ? NULL : tlc->mtx);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
hidbus_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *tlc = device_get_ivars(child);

	switch (which) {
	case HIDBUS_IVAR_INDEX:
		tlc->index = value;
		break;
	case HIDBUS_IVAR_USAGE:
		tlc->usage = value;
		break;
	case HIDBUS_IVAR_FLAGS:
		tlc->flags = value;
		if ((value & HIDBUS_FLAG_CAN_POLL) != 0)
			HID_INTR_SETUP(
			    device_get_parent(bus), NULL, NULL, NULL);
		break;
	case HIDBUS_IVAR_DRIVER_INFO:
		tlc->driver_info = value;
		break;
	case HIDBUS_IVAR_LOCK:
		tlc->mtx = (struct mtx *)value == NULL ?
		    &sc->mtx : (struct mtx *)value;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/* Location hint for devctl(8) */
static int
hidbus_child_location_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{
	struct hidbus_ivars *tlc = device_get_ivars(child);

	snprintf(buf, buflen, "index=%hhu", tlc->index);
        return (0);
}

/* PnP information for devctl(8) */
static int
hidbus_child_pnpinfo_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{
	struct hidbus_ivars *tlc = device_get_ivars(child);
	struct hid_device_info *devinfo = device_get_ivars(bus);

	snprintf(buf, buflen, "page=0x%04x usage=0x%04x bus=0x%02hx "
	    "vendor=0x%04hx product=0x%04hx version=0x%04hx%s%s",
	    HID_GET_USAGE_PAGE(tlc->usage), HID_GET_USAGE(tlc->usage),
	    devinfo->idBus, devinfo->idVendor, devinfo->idProduct,
	    devinfo->idVersion, devinfo->idPnP[0] == '\0' ? "" : " _HID=",
	    devinfo->idPnP[0] == '\0' ? "" : devinfo->idPnP);
	return (0);
}

void
hidbus_set_desc(device_t child, const char *suffix)
{
	device_t bus = device_get_parent(child);
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hid_device_info *devinfo = device_get_ivars(bus);
	struct hidbus_ivars *tlc = device_get_ivars(child);
	char buf[80];

	/* Do not add NULL suffix or if device name already contains it. */
	if (suffix != NULL && strcasestr(devinfo->name, suffix) == NULL &&
	    (sc->nauto > 1 || (tlc->flags & HIDBUS_FLAG_AUTOCHILD) == 0)) {
		snprintf(buf, sizeof(buf), "%s %s", devinfo->name, suffix);
		device_set_desc_copy(child, buf);
	} else
		device_set_desc(child, devinfo->name);
}

device_t
hidbus_find_child(device_t bus, int32_t usage)
{
	device_t *children, child;
	int ccount, i;

	GIANT_REQUIRED;

	/* Get a list of all hidbus children */
	if (device_get_children(bus, &children, &ccount) != 0)
		return (NULL);

	/* Scan through to find required TLC */
	for (i = 0, child = NULL; i < ccount; i++) {
		if (hidbus_get_usage(children[i]) == usage) {
			child = children[i];
			break;
		}
	}
	free(children, M_TEMP);

	return (child);
}

void
hidbus_intr(void *context, void *buf, hid_size_t len)
{
	struct hidbus_softc *sc = context;
	struct hidbus_ivars *tlc;
	struct epoch_tracker et;

	/*
	 * Broadcast input report to all subscribers.
	 * TODO: Add check for input report ID.
	 *
	 * Relock mutex on every TLC item as we can't hold any locks over whole
	 * TLC list here due to LOR with open()/close() handlers.
	 */
	if (!HID_IN_POLLING_MODE())
		epoch_enter_preempt(INPUT_EPOCH, &et);
	CK_STAILQ_FOREACH(tlc, &sc->tlcs, link) {
		if (tlc->refcnt == 0 || tlc->intr_handler == NULL)
			continue;
		if (HID_IN_POLLING_MODE()) {
			if ((tlc->flags & HIDBUS_FLAG_CAN_POLL) != 0)
				tlc->intr_handler(tlc->intr_ctx, buf, len);
		} else {
			mtx_lock(tlc->mtx);
			tlc->intr_handler(tlc->intr_ctx, buf, len);
			mtx_unlock(tlc->mtx);
		}
	}
	if (!HID_IN_POLLING_MODE())
		epoch_exit_preempt(INPUT_EPOCH, &et);
}

void
hidbus_set_intr(device_t child, hid_intr_t *handler, void *context)
{
	struct hidbus_ivars *tlc = device_get_ivars(child);

	tlc->intr_handler = handler;
	tlc->intr_ctx = context;
}

int
hidbus_intr_start(device_t child)
{
	device_t bus = device_get_parent(child);
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *ivar = device_get_ivars(child);
	struct hidbus_ivars *tlc;
	bool refcnted = false;
	int error;

	if (sx_xlock_sig(&sc->sx) != 0)
		return (EINTR);
	CK_STAILQ_FOREACH(tlc, &sc->tlcs, link) {
		refcnted |= (tlc->refcnt != 0);
		if (tlc == ivar) {
			mtx_lock(tlc->mtx);
			++tlc->refcnt;
			mtx_unlock(tlc->mtx);
		}
	}
	error = refcnted ? 0 : HID_INTR_START(device_get_parent(bus));
	sx_unlock(&sc->sx);

	return (error);
}

int
hidbus_intr_stop(device_t child)
{
	device_t bus = device_get_parent(child);
	struct hidbus_softc *sc = device_get_softc(bus);
	struct hidbus_ivars *ivar = device_get_ivars(child);
	struct hidbus_ivars *tlc;
	bool refcnted = false;
	int error;

	if (sx_xlock_sig(&sc->sx) != 0)
		return (EINTR);
	CK_STAILQ_FOREACH(tlc, &sc->tlcs, link) {
		if (tlc == ivar) {
			mtx_lock(tlc->mtx);
			MPASS(tlc->refcnt != 0);
			--tlc->refcnt;
			mtx_unlock(tlc->mtx);
		}
		refcnted |= (tlc->refcnt != 0);
	}
	error = refcnted ? 0 : HID_INTR_STOP(device_get_parent(bus));
	sx_unlock(&sc->sx);

	return (error);
}

void
hidbus_intr_poll(device_t child)
{
	device_t bus = device_get_parent(child);

	HID_INTR_POLL(device_get_parent(bus));
}

struct hid_rdesc_info *
hidbus_get_rdesc_info(device_t child)
{
	device_t bus = device_get_parent(child);
	struct hidbus_softc *sc = device_get_softc(bus);

	return (&sc->rdesc);
}

/*
 * HID interface.
 *
 * Hidbus as well as any hidbus child can be passed as first arg.
 */

/* Read cached report descriptor */
int
hid_get_report_descr(device_t dev, void **data, hid_size_t *len)
{
	device_t bus;
	struct hidbus_softc *sc;

	bus = device_get_devclass(dev) == hidbus_devclass ?
	    dev : device_get_parent(dev);
	sc = device_get_softc(bus);

	/*
	 * Do not send request to a transport backend.
	 * Use cached report descriptor instead of it.
	 */
	if (sc->rdesc.data == NULL || sc->rdesc.len == 0)
		return (ENXIO);

	if (data != NULL)
		*data = sc->rdesc.data;
	if (len != NULL)
		*len = sc->rdesc.len;

	return (0);
}

/*
 * Replace cached report descriptor with top level driver provided one.
 *
 * It deletes all hidbus children except caller and enumerates them again after
 * new descriptor has been registered. Currently it can not be called from
 * autoenumerated (by report's TLC) child device context as it results in child
 * duplication. To overcome this limitation hid_set_report_descr() should be
 * called from device_identify driver's handler with hidbus itself passed as
 * 'device_t dev' parameter.
 */
int
hid_set_report_descr(device_t dev, const void *data, hid_size_t len)
{
	struct hid_rdesc_info rdesc;
	device_t bus;
	struct hidbus_softc *sc;
	bool is_bus;
	int error;

	GIANT_REQUIRED;

	is_bus = device_get_devclass(dev) == hidbus_devclass;
	bus = is_bus ? dev : device_get_parent(dev);
	sc = device_get_softc(bus);

	/*
	 * Do not overload already overloaded report descriptor in
	 * device_identify handler. It causes infinite recursion loop.
	 */
	if (is_bus && sc->overloaded)
		return(0);

	DPRINTFN(5, "len=%d\n", len);
	DPRINTFN(5, "data = %*D\n", len, data, " ");

	error = hidbus_fill_rdesc_info(&rdesc, data, len);
	if (error != 0)
		return (error);

	error = hidbus_detach_children(dev);
	if (error != 0)
		return(error);

	/* Make private copy to handle a case of dynamicaly allocated data. */
	rdesc.data = malloc(len, M_DEVBUF, M_ZERO | M_WAITOK);
	bcopy(data, rdesc.data, len);
	sc->overloaded = true;
	free(sc->rdesc.data, M_DEVBUF);
	bcopy(&rdesc, &sc->rdesc, sizeof(struct hid_rdesc_info));

	error = hidbus_attach_children(bus);

	return (error);
}

static int
hidbus_write(device_t dev, const void *data, hid_size_t len)
{
	struct hidbus_softc *sc;
	uint8_t id;

	sc = device_get_softc(dev);
	/*
	 * Output interrupt endpoint is often optional. If HID device
	 * does not provide it, send reports via control pipe.
	 */
	if (sc->nowrite) {
		/* try to extract the ID byte */
		id = (sc->rdesc.oid & (len > 0)) ? *(const uint8_t*)data : 0;
		return (hid_set_report(dev, data, len, HID_OUTPUT_REPORT, id));
	}

	return (hid_write(dev, data, len));
}

/*------------------------------------------------------------------------*
 *	hidbus_lookup_id
 *
 * This functions takes an array of "struct hid_device_id" and tries
 * to match the entries with the information in "struct hid_device_info".
 *
 * Return values:
 * NULL: No match found.
 * Else: Pointer to matching entry.
 *------------------------------------------------------------------------*/
const struct hid_device_id *
hidbus_lookup_id(device_t dev, const struct hid_device_id *id, int nitems_id)
{
	const struct hid_device_id *id_end;
	const struct hid_device_info *info;
	int32_t usage;
	bool is_child;

	if (id == NULL) {
		goto done;
	}

	id_end = id + nitems_id;
	info = hid_get_device_info(dev);
	is_child = device_get_devclass(dev) != hidbus_devclass;
	if (is_child)
		usage = hidbus_get_usage(dev);

	/*
	 * Keep on matching array entries until we find a match or
	 * until we reach the end of the matching array:
	 */
	for (; id != id_end; id++) {

		if (is_child && (id->match_flag_page) &&
		    (id->page != HID_GET_USAGE_PAGE(usage))) {
			continue;
		}
		if (is_child && (id->match_flag_usage) &&
		    (id->usage != HID_GET_USAGE(usage))) {
			continue;
		}
		if ((id->match_flag_bus) &&
		    (id->idBus != info->idBus)) {
			continue;
		}
		if ((id->match_flag_vendor) &&
		    (id->idVendor != info->idVendor)) {
			continue;
		}
		if ((id->match_flag_product) &&
		    (id->idProduct != info->idProduct)) {
			continue;
		}
		if ((id->match_flag_ver_lo) &&
		    (id->idVersion_lo > info->idVersion)) {
			continue;
		}
		if ((id->match_flag_ver_hi) &&
		    (id->idVersion_hi < info->idVersion)) {
			continue;
		}
		if (id->match_flag_pnp &&
		    strncmp(id->idPnP, info->idPnP, HID_PNP_ID_SIZE) != 0) {
			continue;
		}
		/* We found a match! */
		return (id);
	}

done:
	return (NULL);
}

/*------------------------------------------------------------------------*
 *	hidbus_lookup_driver_info - factored out code
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
int
hidbus_lookup_driver_info(device_t child, const struct hid_device_id *id,
    int nitems_id)
{

	id = hidbus_lookup_id(child, id, nitems_id);
	if (id) {
		/* copy driver info */
		hidbus_set_driver_info(child, id->driver_info);
		return (0);
	}
	return (ENXIO);
}

const struct hid_device_info *
hid_get_device_info(device_t dev)
{
	device_t bus;

	bus = device_get_devclass(dev) == hidbus_devclass ?
	    dev : device_get_parent(dev);

	return (device_get_ivars(bus));
}

static device_method_t hidbus_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		hidbus_probe),
	DEVMETHOD(device_attach,	hidbus_attach),
	DEVMETHOD(device_detach,	hidbus_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* bus interface */
	DEVMETHOD(bus_add_child,	hidbus_add_child),
	DEVMETHOD(bus_child_detached,	hidbus_child_detached),
	DEVMETHOD(bus_child_deleted,	hidbus_child_deleted),
	DEVMETHOD(bus_read_ivar,	hidbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	hidbus_write_ivar),
	DEVMETHOD(bus_child_pnpinfo_str,hidbus_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str,hidbus_child_location_str),

	/* hid interface */
	DEVMETHOD(hid_get_rdesc,	hid_get_rdesc),
	DEVMETHOD(hid_read,		hid_read),
	DEVMETHOD(hid_write,		hidbus_write),
	DEVMETHOD(hid_get_report,	hid_get_report),
	DEVMETHOD(hid_set_report,	hid_set_report),
	DEVMETHOD(hid_set_idle,		hid_set_idle),
	DEVMETHOD(hid_set_protocol,	hid_set_protocol),
	DEVMETHOD(hid_ioctl,		hid_ioctl),

	DEVMETHOD_END
};

devclass_t hidbus_devclass;
driver_t hidbus_driver = {
	"hidbus",
	hidbus_methods,
	sizeof(struct hidbus_softc),
};

MODULE_DEPEND(hidbus, hid, 1, 1, 1);
MODULE_VERSION(hidbus, 1);
DRIVER_MODULE(hidbus, iichid, hidbus_driver, hidbus_devclass, 0, 0);
DRIVER_MODULE(hidbus, usbhid, hidbus_driver, hidbus_devclass, 0, 0);
