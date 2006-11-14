/*	$NetBSD: puc.c,v 1.7 2000/07/29 17:43:38 jlam Exp $	*/

/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2000 M. Warner Losh.  All rights reserved.
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

/*-
 * Copyright (c) 1996, 1998, 1999
 *	Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_puc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define PUC_ENTRAILS	1
#include <dev/puc/pucvar.h>

extern const struct puc_device_description puc_devices[];

int puc_config_win877(struct puc_softc *);

static const struct puc_device_description *
puc_find_description(uint32_t vend, uint32_t prod, uint32_t svend, 
    uint32_t sprod)
{
	int i;

#define checkreg(val, index) \
    (((val) & puc_devices[i].rmask[(index)]) == puc_devices[i].rval[(index)])

	for (i = 0; puc_devices[i].name != NULL; i++) {
		if (checkreg(vend, PUC_REG_VEND) &&
		    checkreg(prod, PUC_REG_PROD) &&
		    checkreg(svend, PUC_REG_SVEND) &&
		    checkreg(sprod, PUC_REG_SPROD))
			return (&puc_devices[i]);
	}

#undef checkreg

	return (NULL);
}

static int
puc_pci_probe(device_t dev)
{
	uint32_t v1, v2, d1, d2;
	const struct puc_device_description *desc;

	if ((pci_read_config(dev, PCIR_HDRTYPE, 1) & PCIM_HDRTYPE) != 0)
		return (ENXIO);

	v1 = pci_read_config(dev, PCIR_VENDOR, 2);
	d1 = pci_read_config(dev, PCIR_DEVICE, 2);
	v2 = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	d2 = pci_read_config(dev, PCIR_SUBDEV_0, 2);

	desc = puc_find_description(v1, d1, v2, d2);
	if (desc == NULL)
		return (ENXIO);
	device_set_desc(dev, desc->name);
	return (BUS_PROBE_DEFAULT);
}

static int
puc_pci_attach(device_t dev)
{
	uint32_t v1, v2, d1, d2;

	v1 = pci_read_config(dev, PCIR_VENDOR, 2);
	d1 = pci_read_config(dev, PCIR_DEVICE, 2);
	v2 = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	d2 = pci_read_config(dev, PCIR_SUBDEV_0, 2);
	return (puc_attach(dev, puc_find_description(v1, d1, v2, d2)));
}

static device_method_t puc_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		puc_pci_probe),
    DEVMETHOD(device_attach,		puc_pci_attach),

    DEVMETHOD(bus_alloc_resource,	puc_alloc_resource),
    DEVMETHOD(bus_release_resource,	puc_release_resource),
    DEVMETHOD(bus_get_resource,		puc_get_resource),
    DEVMETHOD(bus_read_ivar,		puc_read_ivar),
    DEVMETHOD(bus_setup_intr,		puc_setup_intr),
    DEVMETHOD(bus_teardown_intr,	puc_teardown_intr),
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_driver_added,		bus_generic_driver_added),
    { 0, 0 }
};

static driver_t puc_pci_driver = {
	"puc",
	puc_pci_methods,
	sizeof(struct puc_softc),
};

DRIVER_MODULE(puc, pci, puc_pci_driver, puc_devclass, 0, 0);
DRIVER_MODULE(puc, cardbus, puc_pci_driver, puc_devclass, 0, 0);


#define rdspio(indx)		(bus_space_write_1(bst, bsh, efir, indx), \
				bus_space_read_1(bst, bsh, efdr))
#define wrspio(indx,data)	(bus_space_write_1(bst, bsh, efir, indx), \
				bus_space_write_1(bst, bsh, efdr, data))

#ifdef PUC_DEBUG
static void
puc_print_win877(bus_space_tag_t bst, bus_space_handle_t bsh, u_int efir,
	u_int efdr)
{
	u_char cr00, cr01, cr04, cr09, cr0d, cr14, cr15, cr16, cr17;
	u_char cr18, cr19, cr24, cr25, cr28, cr2c, cr31, cr32;

	cr00 = rdspio(0x00);
	cr01 = rdspio(0x01);
	cr04 = rdspio(0x04);
	cr09 = rdspio(0x09);
	cr0d = rdspio(0x0d);
	cr14 = rdspio(0x14);
	cr15 = rdspio(0x15);
	cr16 = rdspio(0x16);
	cr17 = rdspio(0x17);
	cr18 = rdspio(0x18);
	cr19 = rdspio(0x19);
	cr24 = rdspio(0x24);
	cr25 = rdspio(0x25);
	cr28 = rdspio(0x28);
	cr2c = rdspio(0x2c);
	cr31 = rdspio(0x31);
	cr32 = rdspio(0x32);
	printf("877T: cr00 %x, cr01 %x, cr04 %x, cr09 %x, cr0d %x, cr14 %x, "
	    "cr15 %x, cr16 %x, cr17 %x, cr18 %x, cr19 %x, cr24 %x, cr25 %x, "
	    "cr28 %x, cr2c %x, cr31 %x, cr32 %x\n", cr00, cr01, cr04, cr09,
	    cr0d, cr14, cr15, cr16, cr17,
	    cr18, cr19, cr24, cr25, cr28, cr2c, cr31, cr32);
}
#endif

int
puc_config_win877(struct puc_softc *sc)
{
	u_char val;
	u_int efir, efdr;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
        struct resource *res;

	res = sc->sc_bar_mappings[0].res;

	bst = rman_get_bustag(res);
	bsh = rman_get_bushandle(res);

	/* configure the first W83877TF */
	bus_space_write_1(bst, bsh, 0x250, 0x89);
	efir = 0x251;
	efdr = 0x252;
	val = rdspio(0x09) & 0x0f;
	if (val != 0x0c) {
		printf("conf_win877: Oops not a W83877TF\n");
		return (ENXIO);
	}

#ifdef PUC_DEBUG
	printf("before: ");
	puc_print_win877(bst, bsh, efir, efdr);
#endif

	val = rdspio(0x16);
	val |= 0x04;
	wrspio(0x16, val);
	val &= ~0x04;
	wrspio(0x16, val);

	wrspio(0x24, 0x2e8 >> 2);
	wrspio(0x25, 0x2f8 >> 2);
	wrspio(0x17, 0x03);
	wrspio(0x28, 0x43);

#ifdef PUC_DEBUG
	printf("after: ");
	puc_print_win877(bst, bsh, efir, efdr);
#endif

	bus_space_write_1(bst, bsh, 0x250, 0xaa);

	/* configure the second W83877TF */
	bus_space_write_1(bst, bsh, 0x3f0, 0x87);
	bus_space_write_1(bst, bsh, 0x3f0, 0x87);
	efir = 0x3f0;
	efdr = 0x3f1;
	val = rdspio(0x09) & 0x0f;
	if (val != 0x0c) {
		printf("conf_win877: Oops not a W83877TF\n");
		return(ENXIO);
	}

#ifdef PUC_DEBUG
	printf("before: ");
	puc_print_win877(bst, bsh, efir, efdr);
#endif

	val = rdspio(0x16);
	val |= 0x04;
	wrspio(0x16, val);
	val &= ~0x04;
	wrspio(0x16, val);

	wrspio(0x24, 0x3e8 >> 2);
	wrspio(0x25, 0x3f8 >> 2);
	wrspio(0x17, 0x03);
	wrspio(0x28, 0x43);

#ifdef PUC_DEBUG
	printf("after: ");
	puc_print_win877(bst, bsh, efir, efdr);
#endif

	bus_space_write_1(bst, bsh, 0x3f0, 0xaa);
	return (0);
}

#undef rdspio
#undef wrspio

