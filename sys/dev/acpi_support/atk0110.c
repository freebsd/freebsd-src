/*	$NetBSD: atk0110.c,v 1.4 2010/02/11 06:54:57 cnst Exp $	*/
/*	$OpenBSD: atk0110.c,v 1.1 2009/07/23 01:38:16 cnst Exp $	*/

/*
 * Copyright (c) 2009, 2010 Constantine A. Murenin <cnst++@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/_inttypes.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

/*
 * ASUSTeK AI Booster (ACPI ASOC ATK0110).
 *
 * This code was originally written for OpenBSD after the techniques
 * described in the Linux's asus_atk0110.c and FreeBSD's Takanori Watanabe's
 * acpi_aiboost.c were verified to be accurate on the actual hardware kindly
 * provided by Sam Fourman Jr.  It was subsequently ported from OpenBSD to
 * DragonFly BSD, to NetBSD's sysmon_envsys(9) and to FreeBSD's sysctl(9).
 *
 *				  -- Constantine A. Murenin <http://cnst.su/>
 */

#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("aibs");
ACPI_SERIAL_DECL(aibs, "aibs");

#define AIBS_MORE_SENSORS
#define AIBS_VERBOSE

enum aibs_type {
	AIBS_VOLT,
	AIBS_TEMP,
	AIBS_FAN
};

struct aibs_sensor {
	ACPI_INTEGER	v;
	ACPI_INTEGER	i;
	ACPI_INTEGER	l;
	ACPI_INTEGER	h;
	enum aibs_type	t;
};

struct aibs_softc {
	struct device		*sc_dev;
	ACPI_HANDLE		sc_ah;

	struct aibs_sensor	*sc_asens_volt;
	struct aibs_sensor	*sc_asens_temp;
	struct aibs_sensor	*sc_asens_fan;
};

static int aibs_probe(device_t);
static int aibs_attach(device_t);
static int aibs_detach(device_t);
static int aibs_sysctl(SYSCTL_HANDLER_ARGS);

static void aibs_attach_sif(struct aibs_softc *, enum aibs_type);

static device_method_t aibs_methods[] = {
	DEVMETHOD(device_probe,		aibs_probe),
	DEVMETHOD(device_attach,	aibs_attach),
	DEVMETHOD(device_detach,	aibs_detach),
	{ NULL, NULL }
};

static driver_t aibs_driver = {
	"aibs",
	aibs_methods,
	sizeof(struct aibs_softc)
};

static devclass_t aibs_devclass;

DRIVER_MODULE(aibs, acpi, aibs_driver, aibs_devclass, NULL, NULL);


static char* aibs_hids[] = {
	"ATK0110",
	NULL
};

static int
aibs_probe(device_t dev)
{
	if (acpi_disabled("aibs") ||
	    ACPI_ID_PROBE(device_get_parent(dev), dev, aibs_hids) == NULL)
		return ENXIO;

	device_set_desc(dev, "ASUSTeK AI Booster (ACPI ASOC ATK0110)");
	return 0;
}

static int
aibs_attach(device_t dev)
{
	struct aibs_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;
	sc->sc_ah = acpi_get_handle(dev);

	aibs_attach_sif(sc, AIBS_VOLT);
	aibs_attach_sif(sc, AIBS_TEMP);
	aibs_attach_sif(sc, AIBS_FAN);

	return 0;
}

static void
aibs_attach_sif(struct aibs_softc *sc, enum aibs_type st)
{
	ACPI_STATUS		s;
	ACPI_BUFFER		b;
	ACPI_OBJECT		*bp, *o;
	int			i, n;
	const char		*node;
	char			name[] = "?SIF";
	struct aibs_sensor	*as;
	struct sysctl_oid	*so;

	switch (st) {
	case AIBS_VOLT:
		node = "volt";
		name[0] = 'V';
		break;
	case AIBS_TEMP:
		node = "temp";
		name[0] = 'T';
		break;
	case AIBS_FAN:
		node = "fan";
		name[0] = 'F';
		break;
	default:
		return;
	}

	b.Length = ACPI_ALLOCATE_BUFFER;
	s = AcpiEvaluateObjectTyped(sc->sc_ah, name, NULL, &b,
	    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(s)) {
		device_printf(sc->sc_dev, "%s not found\n", name);
		return;
	}

	bp = b.Pointer;
	o = bp->Package.Elements;
	if (o[0].Type != ACPI_TYPE_INTEGER) {
		device_printf(sc->sc_dev, "%s[0]: invalid type\n", name);
		AcpiOsFree(b.Pointer);
		return;
	}

	n = o[0].Integer.Value;
	if (bp->Package.Count - 1 < n) {
		device_printf(sc->sc_dev, "%s: invalid package\n", name);
		AcpiOsFree(b.Pointer);
		return;
	} else if (bp->Package.Count - 1 > n) {
		int on = n;

#ifdef AIBS_MORE_SENSORS
		n = bp->Package.Count - 1;
#endif
		device_printf(sc->sc_dev, "%s: malformed package: %i/%i"
		    ", assume %i\n", name, on, bp->Package.Count - 1, n);
	}
	if (n < 1) {
		device_printf(sc->sc_dev, "%s: no members in the package\n",
		    name);
		AcpiOsFree(b.Pointer);
		return;
	}

	as = malloc(sizeof(*as) * n, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (as == NULL) {
		device_printf(sc->sc_dev, "%s: malloc fail\n", name);
		AcpiOsFree(b.Pointer);
		return;
	}
	switch (st) {
	case AIBS_VOLT:
		sc->sc_asens_volt = as;
		break;
	case AIBS_TEMP:
		sc->sc_asens_temp = as;
		break;
	case AIBS_FAN:
		sc->sc_asens_fan = as;
		break;
	}

	/* sysctl subtree for sensors of this type */
	so = SYSCTL_ADD_NODE(device_get_sysctl_ctx(sc->sc_dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev)), st,
	    node, CTLFLAG_RD, NULL, NULL);

	for (i = 0, o++; i < n; i++, o++) {
		ACPI_OBJECT	*oi;
		char		si[3];
		const char	*desc;

		/* acpica5 automatically evaluates the referenced package */
		if (o[0].Type != ACPI_TYPE_PACKAGE) {
			device_printf(sc->sc_dev,
			    "%s: %i: not a package: %i type\n",
			    name, i, o[0].Type);
			continue;
		}
		oi = o[0].Package.Elements;
		if (o[0].Package.Count != 5 ||
		    oi[0].Type != ACPI_TYPE_INTEGER ||
		    oi[1].Type != ACPI_TYPE_STRING ||
		    oi[2].Type != ACPI_TYPE_INTEGER ||
		    oi[3].Type != ACPI_TYPE_INTEGER ||
		    oi[4].Type != ACPI_TYPE_INTEGER) {
			device_printf(sc->sc_dev,
			    "%s: %i: invalid package\n",
			    name, i);
			continue;
		}
		as[i].i = oi[0].Integer.Value;
		desc = oi[1].String.Pointer;
		as[i].l = oi[2].Integer.Value;
		as[i].h = oi[3].Integer.Value;
		as[i].t = st;
#ifdef AIBS_VERBOSE
		device_printf(sc->sc_dev, "%c%i: "
		    "0x%08"PRIx64" %20s %5"PRIi64" / %5"PRIi64"  "
		    "0x%"PRIx64"\n",
		    name[0], i,
		    as[i].i, desc, (int64_t)as[i].l, (int64_t)as[i].h,
		    oi[4].Integer.Value);
#endif
		snprintf(si, sizeof(si), "%i", i);
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->sc_dev),
		    SYSCTL_CHILDREN(so), i, si, CTLTYPE_OPAQUE | CTLFLAG_RD,
		    sc, st, aibs_sysctl, st == AIBS_TEMP ? "IK" : "I", desc);
	}

	AcpiOsFree(b.Pointer);
}

static int
aibs_detach(device_t dev)
{
	struct aibs_softc	*sc = device_get_softc(dev);

	if (sc->sc_asens_volt != NULL)
		free(sc->sc_asens_volt, M_DEVBUF);
	if (sc->sc_asens_temp != NULL)
		free(sc->sc_asens_temp, M_DEVBUF);
	if (sc->sc_asens_fan != NULL)
		free(sc->sc_asens_fan, M_DEVBUF);
	return 0;
}

#ifdef AIBS_VERBOSE
#define ddevice_printf(x...) device_printf(x)
#else
#define ddevice_printf(x...)
#endif

static int
aibs_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct aibs_softc	*sc = arg1;
	enum aibs_type		st = arg2;
	int			i = oidp->oid_number;
	ACPI_STATUS		rs;
	ACPI_OBJECT		p, *bp;
	ACPI_OBJECT_LIST	mp;
	ACPI_BUFFER		b;
	char			*name;
	struct aibs_sensor	*as;
	ACPI_INTEGER		v, l, h;
	int			so[3];

	switch (st) {
	case AIBS_VOLT:
		name = "RVLT";
		as = sc->sc_asens_volt;
		break;
	case AIBS_TEMP:
		name = "RTMP";
		as = sc->sc_asens_temp;
		break;
	case AIBS_FAN:
		name = "RFAN";
		as = sc->sc_asens_fan;
		break;
	default:
		return ENOENT;
	}
	if (as == NULL)
		return ENOENT;
	l = as[i].l;
	h = as[i].h;
	p.Type = ACPI_TYPE_INTEGER;
	p.Integer.Value = as[i].i;
	mp.Count = 1;
	mp.Pointer = &p;
	b.Length = ACPI_ALLOCATE_BUFFER;
	ACPI_SERIAL_BEGIN(aibs);
	rs = AcpiEvaluateObjectTyped(sc->sc_ah, name, &mp, &b,
	    ACPI_TYPE_INTEGER);
	if (ACPI_FAILURE(rs)) {
		ddevice_printf(sc->sc_dev,
		    "%s: %i: evaluation failed\n",
		    name, i);
		ACPI_SERIAL_END(aibs);
		return EIO;
	}
	bp = b.Pointer;
	v = bp->Integer.Value;
	AcpiOsFree(b.Pointer);
	ACPI_SERIAL_END(aibs);

	switch (st) {
	case AIBS_VOLT:
		break;
	case AIBS_TEMP:
		v += 2732;
		l += 2732;
		h += 2732;
		break;
	case AIBS_FAN:
		break;
	}
	so[0] = v;
	so[1] = l;
	so[2] = h;
	return sysctl_handle_opaque(oidp, &so, sizeof(so), req);
}
