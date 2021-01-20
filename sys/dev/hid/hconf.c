/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2020 Andriy Gapon <avg@FreeBSD.org>
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

/*
 * Digitizer configuration top-level collection support.
 * https://docs.microsoft.com/en-us/windows-hardware/design/component-guidelines/windows-precision-touchpad-required-hid-top-level-collections
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sx.h>

#define	HID_DEBUG_VAR	hconf_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>

#include <dev/hid/hconf.h>

#ifdef HID_DEBUG
static int hconf_debug = 0;

static SYSCTL_NODE(_hw_hid, OID_AUTO, hconf, CTLFLAG_RW, 0,
    "Digitizer configuration top-level collection");
SYSCTL_INT(_hw_hid_hconf, OID_AUTO, debug, CTLFLAG_RWTUN,
    &hconf_debug, 1, "Debug level");
#endif

enum feature_control_type {
	INPUT_MODE = 0,
	SURFACE_SWITCH,
	BUTTONS_SWITCH,
	CONTROLS_COUNT
};

struct feature_control_descr {
	const char	*name;
	const char	*descr;
	uint16_t	usage;
	u_int		value;
} feature_control_descrs[] = {
	[INPUT_MODE] = {
		.name = "input_mode",
		.descr = "HID device input mode: 0 = mouse, 3 = touchpad",
		.usage = HUD_INPUT_MODE,
		.value = HCONF_INPUT_MODE_MOUSE,
	},
	[SURFACE_SWITCH] = {
		.name = "surface_switch",
		.descr = "Enable / disable switch for surface: 1 = on, 0 = off",
		.usage = HUD_SURFACE_SWITCH,
		.value = 1,
	},
	[BUTTONS_SWITCH] = {
		.name = "buttons_switch",
		.descr = "Enable / disable switch for buttons: 1 = on, 0 = off",
		.usage = HUD_BUTTONS_SWITCH,
		.value = 1,
	},
};

struct feature_control {
	u_int			val;
	struct hid_location	loc;
	hid_size_t		rlen;
	uint8_t			rid;
};

struct hconf_softc {
	device_t		dev;
	struct sx		lock;

	struct feature_control	feature_controls[CONTROLS_COUNT];
};

static device_probe_t		hconf_probe;
static device_attach_t		hconf_attach;
static device_detach_t		hconf_detach;
static device_resume_t		hconf_resume;

static devclass_t hconf_devclass;

static device_method_t hconf_methods[] = {

	DEVMETHOD(device_probe,		hconf_probe),
	DEVMETHOD(device_attach,	hconf_attach),
	DEVMETHOD(device_detach,	hconf_detach),
	DEVMETHOD(device_resume,	hconf_resume),

	DEVMETHOD_END
};

static driver_t hconf_driver = {
	.name = "hconf",
	.methods = hconf_methods,
	.size = sizeof(struct hconf_softc),
};

static const struct hid_device_id hconf_devs[] = {
	{ HID_TLC(HUP_DIGITIZERS, HUD_CONFIG) },
};

static int
hconf_set_feature_control(struct hconf_softc *sc, int ctrl_id, u_int val)
{
	struct feature_control *fc;
	uint8_t *fbuf;
	int error;
	int i;

	KASSERT(ctrl_id >= 0 && ctrl_id < CONTROLS_COUNT,
	    ("impossible ctrl id %d", ctrl_id));
	fc = &sc->feature_controls[ctrl_id];
	if (fc->rlen <= 1)
		return (ENXIO);

	fbuf = malloc(fc->rlen, M_TEMP, M_WAITOK | M_ZERO);
	sx_xlock(&sc->lock);

	/*
	 * Assume the report is write-only. Then we have to check for other
	 * controls that may share the same report and set their bits as well.
	 */
	bzero(fbuf + 1, fc->rlen - 1);
	for (i = 0; i < nitems(sc->feature_controls); i++) {
		struct feature_control *ofc = &sc->feature_controls[i];

		/* Skip unrelated report IDs. */
		if (ofc->rid != fc->rid)
			continue;
		KASSERT(fc->rlen == ofc->rlen,
		    ("different lengths for report %d: %d vs %d\n",
		    fc->rid, fc->rlen, ofc->rlen));
		hid_put_udata(fbuf + 1, ofc->rlen - 1, &ofc->loc,
		    i == ctrl_id ? val : ofc->val);
	}

	fbuf[0] = fc->rid;

	error = hid_set_report(sc->dev, fbuf, fc->rlen,
	    HID_FEATURE_REPORT, fc->rid);
	if (error == 0)
		fc->val = val;

	sx_unlock(&sc->lock);
	free(fbuf, M_TEMP);

	return (error);
}

static int
hconf_feature_control_handler(SYSCTL_HANDLER_ARGS)
{
	struct feature_control *fc;
	struct hconf_softc *sc = arg1;
	int ctrl_id = arg2;
	u_int value;
	int error;

	if (ctrl_id < 0 || ctrl_id >= CONTROLS_COUNT)
		return (ENXIO);

	fc = &sc->feature_controls[ctrl_id];
	value = fc->val;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = hconf_set_feature_control(sc, ctrl_id, value);
	if (error != 0) {
		DPRINTF("Failed to set %s: %d\n",
		    feature_control_descrs[ctrl_id].name, error);
	}
	return (0);
}


static int
hconf_parse_feature(struct feature_control *fc, uint8_t tlc_index,
    uint16_t usage, void *d_ptr, hid_size_t d_len)
{
	uint32_t flags;

	if (!hidbus_locate(d_ptr, d_len, HID_USAGE2(HUP_DIGITIZERS, usage),
	    hid_feature, tlc_index, 0, &fc->loc, &flags, &fc->rid, NULL))
		return (ENOENT);

	if ((flags & (HIO_VARIABLE | HIO_RELATIVE)) != HIO_VARIABLE)
		return (EINVAL);

	fc->rlen = hid_report_size(d_ptr, d_len, hid_feature, fc->rid);
	return (0);
}

static int
hconf_probe(device_t dev)
{
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, hconf_devs);
	if (error != 0)
		return (error);

	hidbus_set_desc(dev, "Configuration");

	return (BUS_PROBE_DEFAULT);
}

static int
hconf_attach(device_t dev)
{
	struct hconf_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	void *d_ptr;
	hid_size_t d_len;
	uint8_t tlc_index;
	int error;
	int i;

	error = hid_get_report_descr(dev, &d_ptr, &d_len);
	if (error) {
		device_printf(dev, "could not retrieve report descriptor from "
		    "device: %d\n", error);
		return (ENXIO);
	}

	sc->dev = dev;
	sx_init(&sc->lock, device_get_nameunit(dev));

	tlc_index = hidbus_get_index(dev);
	for (i = 0; i < nitems(sc->feature_controls); i++) {
		(void)hconf_parse_feature(&sc->feature_controls[i], tlc_index,
		    feature_control_descrs[i].usage, d_ptr, d_len);
		if (sc->feature_controls[i].rlen > 1) {
			SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
			    feature_control_descrs[i].name,
			    CTLTYPE_UINT | CTLFLAG_RW,
			    sc, i, hconf_feature_control_handler, "I",
			    feature_control_descrs[i].descr);
		}
		sc->feature_controls[i].val = feature_control_descrs[i].value;
	}

	return (0);
}

static int
hconf_detach(device_t dev)
{
	struct hconf_softc *sc = device_get_softc(dev);

	sx_destroy(&sc->lock);

	return (0);
}

static int
hconf_resume(device_t dev)
{
	struct hconf_softc *sc = device_get_softc(dev);
	int error;
	int i;

	for (i = 0; i < nitems(sc->feature_controls); i++) {
		if (sc->feature_controls[i].rlen < 2)
			continue;
		/* Do not update usages to default value */
		if (sc->feature_controls[i].val ==
		    feature_control_descrs[i].value)
			continue;
		error = hconf_set_feature_control(sc, i,
		    sc->feature_controls[i].val);
		if (error != 0) {
			DPRINTF("Failed to restore %s: %d\n",
			    feature_control_descrs[i].name, error);
		}
	}

	return (0);
}

int
hconf_set_input_mode(device_t dev, enum hconf_input_mode mode)
{
	struct hconf_softc *sc = device_get_softc(dev);

	return (hconf_set_feature_control(sc, INPUT_MODE, mode));
}

DRIVER_MODULE(hconf, hidbus, hconf_driver, hconf_devclass, NULL, 0);
MODULE_DEPEND(hconf, hidbus, 1, 1, 1);
MODULE_DEPEND(hconf, hid, 1, 1, 1);
MODULE_VERSION(hconf, 1);
HID_PNP_INFO(hconf_devs);
