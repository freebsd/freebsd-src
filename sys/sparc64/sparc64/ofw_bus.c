/*-
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
 * $FreeBSD: src/sys/sparc64/sparc64/ofw_bus.c,v 1.12 2005/01/07 02:29:23 imp Exp $
 */

/*
 * Open Firmware bus support code that is (hopefully) independent from the
 * used hardware.
 * Maybe this should go into dev/ofw/; there may however be sparc specific
 * bits left.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>

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
		KASSERT(i >= tsz, ("ofw_bus_search_intrmap: truncated map"));
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
