/*-
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>.
 * Copyright (c) 2005 Marius Strobl <marius@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/libkern.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

int
ofw_bus_gen_setup_devinfo(struct ofw_bus_devinfo *obd, phandle_t node)
{

	if (obd == NULL)
		return (ENOMEM);
	/* The 'name' property is considered mandatory. */
	if ((OF_getprop_alloc(node, "name", 1, (void **)&obd->obd_name)) == -1)
		return (EINVAL);
	OF_getprop_alloc(node, "compatible", 1, (void **)&obd->obd_compat);
	OF_getprop_alloc(node, "device_type", 1, (void **)&obd->obd_type);
	OF_getprop_alloc(node, "model", 1, (void **)&obd->obd_model);
	OF_getprop_alloc(node, "status", 1, (void **)&obd->obd_status);
	obd->obd_node = node;
	return (0);
}

void
ofw_bus_gen_destroy_devinfo(struct ofw_bus_devinfo *obd)
{

	if (obd == NULL)
		return;
	if (obd->obd_compat != NULL)
		free(obd->obd_compat, M_OFWPROP);
	if (obd->obd_model != NULL)
		free(obd->obd_model, M_OFWPROP);
	if (obd->obd_name != NULL)
		free(obd->obd_name, M_OFWPROP);
	if (obd->obd_type != NULL)
		free(obd->obd_type, M_OFWPROP);
	if (obd->obd_status != NULL)
		free(obd->obd_status, M_OFWPROP);
}

int
ofw_bus_gen_child_pnpinfo_str(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{

	if (ofw_bus_get_name(child) != NULL) {
		strlcat(buf, "name=", buflen);
		strlcat(buf, ofw_bus_get_name(child), buflen);
	}

	if (ofw_bus_get_compat(child) != NULL) {
		strlcat(buf, " compat=", buflen);
		strlcat(buf, ofw_bus_get_compat(child), buflen);
	}
	return (0);
};

const char *
ofw_bus_gen_get_compat(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_compat);
}

const char *
ofw_bus_gen_get_model(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_model);
}

const char *
ofw_bus_gen_get_name(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_name);
}

phandle_t
ofw_bus_gen_get_node(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (0);
	return (obd->obd_node);
}

const char *
ofw_bus_gen_get_type(device_t bus, device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(bus, dev);
	if (obd == NULL)
		return (NULL);
	return (obd->obd_type);
}

const char *
ofw_bus_get_status(device_t dev)
{
	const struct ofw_bus_devinfo *obd;

	obd = OFW_BUS_GET_DEVINFO(device_get_parent(dev), dev);
	if (obd == NULL)
		return (NULL);

	return (obd->obd_status);
}

int
ofw_bus_status_okay(device_t dev)
{
	const char *status;

	status = ofw_bus_get_status(dev);
	if (status == NULL || strcmp(status, "okay") == 0)
		return (1);
	
	return (0);
}

int
ofw_bus_is_compatible(device_t dev, const char *onecompat)
{
	phandle_t node;
	const char *compat;
	int len, onelen, l;

	if ((compat = ofw_bus_get_compat(dev)) == NULL)
		return (0);

	if ((node = ofw_bus_get_node(dev)) == -1)
		return (0);

	/* Get total 'compatible' prop len */
	if ((len = OF_getproplen(node, "compatible")) <= 0)
		return (0);

	onelen = strlen(onecompat);

	while (len > 0) {
		if (strlen(compat) == onelen &&
		    strncasecmp(compat, onecompat, onelen) == 0)
			/* Found it. */
			return (1);

		/* Slide to the next sub-string. */
		l = strlen(compat) + 1;
		compat += l;
		len -= l;
	}
	return (0);
}

int
ofw_bus_is_compatible_strict(device_t dev, const char *compatible)
{
	const char *compat;
	size_t len;

	if ((compat = ofw_bus_get_compat(dev)) == NULL)
		return (0);

	len = strlen(compatible);
	if (strlen(compat) == len &&
	    strncasecmp(compat, compatible, len) == 0)
		return (1);

	return (0);
}

const struct ofw_compat_data *
ofw_bus_search_compatible(device_t dev, const struct ofw_compat_data *compat)
{

	if (compat == NULL)
		return NULL;

	for (; compat->ocd_str != NULL; ++compat) {
		if (ofw_bus_is_compatible(dev, compat->ocd_str))
			break;
	}

	return (compat);
}

int
ofw_bus_has_prop(device_t dev, const char *propname)
{
	phandle_t node;

	if ((node = ofw_bus_get_node(dev)) == -1)
		return (0);

	return (OF_hasprop(node, propname));
}

void
ofw_bus_setup_iinfo(phandle_t node, struct ofw_bus_iinfo *ii, int intrsz)
{
	pcell_t addrc;
	int msksz;

	if (OF_getencprop(node, "#address-cells", &addrc, sizeof(addrc)) == -1)
		addrc = 2;
	ii->opi_addrc = addrc * sizeof(pcell_t);

	ii->opi_imapsz = OF_getencprop_alloc(node, "interrupt-map", 1,
	    (void **)&ii->opi_imap);
	if (ii->opi_imapsz > 0) {
		msksz = OF_getencprop_alloc(node, "interrupt-map-mask", 1,
		    (void **)&ii->opi_imapmsk);
		/*
		 * Failure to get the mask is ignored; a full mask is used
		 * then.  We barf on bad mask sizes, however.
		 */
		if (msksz != -1 && msksz != ii->opi_addrc + intrsz)
			panic("ofw_bus_setup_iinfo: bad interrupt-map-mask "
			    "property!");
	}
}

int
ofw_bus_lookup_imap(phandle_t node, struct ofw_bus_iinfo *ii, void *reg,
    int regsz, void *pintr, int pintrsz, void *mintr, int mintrsz,
    phandle_t *iparent)
{
	uint8_t maskbuf[regsz + pintrsz];
	int rv;

	if (ii->opi_imapsz <= 0)
		return (0);
	KASSERT(regsz >= ii->opi_addrc,
	    ("ofw_bus_lookup_imap: register size too small: %d < %d",
		regsz, ii->opi_addrc));
	if (node != -1) {
		rv = OF_getencprop(node, "reg", reg, regsz);
		if (rv < regsz)
			panic("ofw_bus_lookup_imap: cannot get reg property");
	}
	return (ofw_bus_search_intrmap(pintr, pintrsz, reg, ii->opi_addrc,
	    ii->opi_imap, ii->opi_imapsz, ii->opi_imapmsk, maskbuf, mintr,
	    mintrsz, iparent));
}

/*
 * Map an interrupt using the firmware reg, interrupt-map and
 * interrupt-map-mask properties.
 * The interrupt property to be mapped must be of size intrsz, and pointed to
 * by intr.  The regs property of the node for which the mapping is done must
 * be passed as regs. This property is an array of register specifications;
 * the size of the address part of such a specification must be passed as
 * physsz.  Only the first element of the property is used.
 * imap and imapsz hold the interrupt mask and it's size.
 * imapmsk is a pointer to the interrupt-map-mask property, which must have
 * a size of physsz + intrsz; it may be NULL, in which case a full mask is
 * assumed.
 * maskbuf must point to a buffer of length physsz + intrsz.
 * The interrupt is returned in result, which must point to a buffer of length
 * rintrsz (which gives the expected size of the mapped interrupt).
 * Returns number of cells in the interrupt if a mapping was found, 0 otherwise.
 */
int
ofw_bus_search_intrmap(void *intr, int intrsz, void *regs, int physsz,
    void *imap, int imapsz, void *imapmsk, void *maskbuf, void *result,
    int rintrsz, phandle_t *iparent)
{
	phandle_t parent;
	uint8_t *ref = maskbuf;
	uint8_t *uiintr = intr;
	uint8_t *uiregs = regs;
	uint8_t *uiimapmsk = imapmsk;
	uint8_t *mptr;
	pcell_t pintrsz;
	int i, rsz, tsz;

	rsz = -1;
	if (imapmsk != NULL) {
		for (i = 0; i < physsz; i++)
			ref[i] = uiregs[i] & uiimapmsk[i];
		for (i = 0; i < intrsz; i++)
			ref[physsz + i] = uiintr[i] & uiimapmsk[physsz + i];
	} else {
		bcopy(regs, ref, physsz);
		bcopy(intr, ref + physsz, intrsz);
	}

	mptr = imap;
	i = imapsz;
	while (i > 0) {
		bcopy(mptr + physsz + intrsz, &parent, sizeof(parent));
		if (OF_searchencprop(OF_node_from_xref(parent),
		    "#interrupt-cells", &pintrsz, sizeof(pintrsz)) == -1)
			pintrsz = 1;	/* default */
		pintrsz *= sizeof(pcell_t);

		/* Compute the map stride size. */
		tsz = physsz + intrsz + sizeof(phandle_t) + pintrsz;
		KASSERT(i >= tsz, ("ofw_bus_search_intrmap: truncated map"));

		if (bcmp(ref, mptr, physsz + intrsz) == 0) {
			bcopy(mptr + physsz + intrsz + sizeof(parent),
			    result, MIN(rintrsz, pintrsz));

			if (iparent != NULL)
				*iparent = parent;
			return (pintrsz/sizeof(pcell_t));
		}
		mptr += tsz;
		i -= tsz;
	}
	return (0);
}

