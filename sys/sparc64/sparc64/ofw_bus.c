/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: $NetBSD: ofw_machdep.c,v 1.16 2001/07/20 00:07:14 eeh Exp $
 *
 * $FreeBSD$
 */

/*
 * OpenFirmware bus support code that is (hopefully) independent from the used
 * hardware.
 * Maybe this should go into dev/ofw/; there may however be sparc specific
 * bits left.
 */

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <ofw/openfirm.h>

#include <machine/ofw_bus.h>

static int
ofw_bus_searchprop(phandle_t node, char *propname, void *buf, int buflen)
{
	int rv;

	for (; node != 0; node = OF_parent(node)) {
		if ((rv = OF_getprop(node, propname, buf, buflen)) != -1)
			return (rv);
	}
	return (-1);
}

#ifdef OFW_NEWPCI

void
ofw_bus_setup_iinfo(phandle_t node, struct ofw_bus_iinfo *ii, int intrsz)
{
	pcell_t addrc;
	int msksz;

	if (OF_getprop(node, "#address-cells", &addrc, sizeof(addrc)) == -1)
		addrc = 2;
	ii->opi_addrc = addrc * sizeof(pcell_t);

	ii->opi_imapsz = OF_getprop_alloc(node, "interrupt-map", 1,
	    (void **)&ii->opi_imap);
	if (ii->opi_imapsz > 0) {
		msksz = OF_getprop_alloc(node, "interrupt-map-mask", 1,
		    (void **)&ii->opi_imapmsk);
		/*
		 * Failure to get the mask is ignored; a full mask is used then.
		 * Barf on bad mask sizes, however.
		 */
		if (msksz != -1 && msksz != ii->opi_addrc + intrsz) {
			panic("ofw_bus_setup_iinfo: bad interrupt-map-mask "
			    "property!");
		}
	}

}

int
ofw_bus_lookup_imap(phandle_t node, struct ofw_bus_iinfo *ii, void *reg,
    int regsz, void *pintr, int pintrsz, void *mintr, int mintrsz,
    void *maskbuf)
{
	int rv;

	if (ii->opi_imapsz <= 0)
		return (0);
	KASSERT(regsz >= ii->opi_addrc,
	    ("ofw_bus_lookup_imap: register size too small: %d < %d",
		regsz, ii->opi_addrc));
	rv = OF_getprop(node, "reg", reg, regsz);
	if (rv < regsz)
		panic("ofw_bus_lookup_imap: could not get reg property");
	return (ofw_bus_search_intrmap(pintr, pintrsz, reg, ii->opi_addrc,
	    ii->opi_imap, ii->opi_imapsz, ii->opi_imapmsk, maskbuf, mintr,
	    mintrsz));
}

/*
 * Map an interrupt using the firmware reg, interrupt-map and
 * interrupt-map-mask properties.
 * The interrupt property to be mapped must be of size intrsz, and pointed to
 * by intr. The regs property of the node for which the mapping is done must
 * be passed as regs. This property is an array of register specifications;
 * the size of the address part of such a specification must be passed as
 * physsz. Only the first element of the property is used.
 * imap and imapsz hold the interrupt mask and it's size.
 * imapmsk is a pointer to the interrupt-map-mask property, which must have
 * a size of physsz + intrsz; it may be NULL, in which case a full mask is
 * assumed.
 * maskbuf must point to a buffer of length physsz + intrsz.
 * The interrupt is returned in result, which must point to a buffer of length
 * rintrsz (which gives the expected size of the mapped interrupt).
 * Returns 1 if a mapping was found, 0 otherwise.
 */
int
ofw_bus_search_intrmap(void *intr, int intrsz, void *regs, int physsz,
    void *imap, int imapsz, void *imapmsk, void *maskbuf, void *result,
    int rintrsz)
{
	phandle_t parent;
	u_int8_t *ref = maskbuf;
	u_int8_t *uiintr = intr;
	u_int8_t *uiregs = regs;
	u_int8_t *uiimapmsk = imapmsk;
	u_int8_t *mptr;
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
	tsz = physsz + intrsz + sizeof(phandle_t) + rintrsz;
	while (i > 0) {
		KASSERT(i >= tsz, ("ofw_bus_find_intr: truncated map"));
		bcopy(mptr + physsz + intrsz, &parent, sizeof(parent));
		if (ofw_bus_searchprop(parent, "#interrupt-cells",
		    &pintrsz, sizeof(pintrsz)) == -1)
			pintrsz = 1;	/* default */
		pintrsz *= sizeof(pcell_t);
		if (pintrsz != rintrsz)
			panic("ofw_bus_search_intrmap: expected interrupt cell "
			    "size incorrect: %d != %d", rintrsz, pintrsz);
		if (bcmp(ref, mptr, physsz + intrsz) == 0) {
			bcopy(mptr + physsz + intrsz + sizeof(parent),
			    result, rintrsz);
			return (1);
		}
		mptr += tsz;
		i -= tsz;
	}
	return (0);
}

#else
/*
 * Map an interrupt using the firmware reg, interrupt-map and
 * interrupt-map-mask properties.
 * The interrupt is returned in *result, which is malloc()'ed. The size of
 * the interrupt specifiaction is returned.
 */
static int
ofw_bus_find_intr(u_int8_t *intr, int intrsz, u_int8_t *regs, int physsz,
    u_int8_t *imap, int imapsz, u_int8_t *imapmsk, u_int8_t **result)
{
	phandle_t parent;
	char *ref;
	u_int8_t *mptr;
	pcell_t pintrsz;
	int i, rsz, tsz;

	rsz = -1;
	ref = malloc(physsz + intrsz, M_TEMP, M_WAITOK);
	if (imapmsk != NULL) {
		for (i = 0; i < physsz; i++)
			ref[i] = regs[i] & imapmsk[i];
		for (i = 0; i < intrsz; i++)
			ref[physsz + i] = intr[i] & imapmsk[physsz + i];
	} else {
		bcopy(regs, ref, physsz);
		bcopy(intr, ref + physsz, intrsz);
	}
	mptr = imap;
	i = imapsz;
	while (i > 0) {
		KASSERT(i >= physsz + sizeof(parent),
		    ("ofw_bus_find_intr: truncated map"));
		bcopy(mptr + physsz + intrsz, &parent, sizeof(parent));
		if (ofw_bus_searchprop(parent, "#interrupt-cells",
		    &pintrsz, sizeof(pintrsz)) == -1)
			pintrsz = 1;	/* default */
		pintrsz *= sizeof(pcell_t);
		KASSERT(i >= physsz + intrsz + sizeof(parent) +
		    pintrsz, ("ofw_bus_find_intr: truncated map"));
		if (bcmp(ref, mptr, physsz + intrsz) == 0) {
			*result = malloc(pintrsz, M_OFWPROP, M_WAITOK);
			bcopy(mptr + physsz + intrsz + sizeof(parent),
			    *result, pintrsz);
			rsz = pintrsz;
			break;
		}
		tsz = physsz + intrsz + sizeof(phandle_t) + pintrsz;
		mptr += tsz;
		i -= tsz;
	}
	free(ref, M_TEMP);
	return (rsz);
}

/*
 * Apply the OpenFirmware algorithm for mapping an interrupt. First, the
 * 'interrupts' and 'reg' properties are retrieved; those are matched against
 * the interrupt map of the next higher node. If there is no match or no such
 * propery, we go to the next higher node, using the 'reg' property of the node
 * that was just processed unusccessfully.
 * When a match occurs, we should continue to search, using the new interrupt
 * specification that was just found; this is currently not performed
 * (see below).
 * When the root node is reached with at least one successful mapping performed,
 * and the format is right, the interrupt number is returned.
 *
 * This should work for all bus systems.
 */
u_int32_t
ofw_bus_route_intr(phandle_t node, int intrp, obr_callback_t *cb, void *cookie)
{
	u_int8_t *reg, *intr, *tintr, *imap, *imapmsk;
	phandle_t parent;
	pcell_t addrc, ic;
	u_int32_t rv;
	int regsz, tisz, isz, imapsz, found;

	found = 0;
	reg = imap = imapmsk = NULL;
	if (intrp == ORIP_NOINT) {
		isz = OF_getprop_alloc(node, "interrupts", 1, (void **)&intr);
		if (isz < 0)
			return (ORIR_NOTFOUND);
	} else {
		ic = intrp;
		isz = sizeof(ic);
		intr = malloc(isz, M_OFWPROP, M_WAITOK);
		bcopy(&ic, intr, isz);
	}
	/*
	 * Note that apparently, remapping at multiple levels is allowed;
	 * however, this causes problems with EBus at least, and seems to never
	 * be needed, so we disable it for now (*sigh*).
	 */
	for (parent = OF_parent(node); parent != 0 && !found;
	     parent = OF_parent(node = parent)) {
		if (reg != NULL)
			free(reg, M_OFWPROP);
		regsz = OF_getprop_alloc(node, "reg", 1, (void **)&reg);
		if (regsz < 0)
			panic("ofw_bus_route_intr: could not get reg property");
		imapsz = OF_getprop_alloc(parent, "interrupt-map", 1,
		    (void **)&imap);
		if (imapsz == -1) {
			/*
			 * Use the callback to allow caller-specific workarounds
			 * for firmware bugs (missing properties).
			 */
			if (cb != NULL) {
				tisz = cb(parent, intr, isz, reg, regsz, &tintr,
				    &found, cookie);
				if (tisz != -1) {
					isz = tisz;
					free(intr, M_OFWPROP);
					intr = tintr;
				}
			}
			continue;
		}
		if (OF_getprop(parent, "#address-cells", &addrc,
		    sizeof(addrc)) == -1)
			addrc = 2;
		addrc *= sizeof(pcell_t);
		/*
		 * Failures to get the mask are ignored; a full mask is assumed
		 * in this case.
		 */
		OF_getprop_alloc(parent, "interrupt-map-mask", 1,
		    (void **)&imapmsk);
		tisz = ofw_bus_find_intr(intr, isz, reg, addrc, imap, imapsz,
		    imapmsk, &tintr);
		if (tisz != -1) {
			found = 1;
			isz = tisz;
			free(intr, M_OFWPROP);
			intr = tintr;
		}
		free(imap, M_OFWPROP);
		if (imapmsk != NULL)
			free(imapmsk, M_OFWPROP);
	}
	if (reg != NULL)
		free(reg, M_OFWPROP);
#if 0
	/*
	 * Obviously there are some boxes that don't require mapping at all,
	 * for example the U30, which has no interrupt maps for children of
	 * the root PCI bus.
	 */
	if (!found) {
		if (intrp != ORIP_NOINT)
			return (ORIR_NOTFOUND);
		panic("ofw_bus_route_intr: 'interrupts' property, but no "
		    "mapping found");
	}
#endif
	KASSERT(isz == sizeof(u_int32_t),
	    ("ofw_bus_route_intr: bad interrupt spec size %d", isz));
	bcopy(intr, &rv, sizeof(rv));
	free(intr, M_OFWPROP);
	return (rv);
}
#endif /* OFW_NEWPCI */
