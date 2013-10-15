/*-
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Ed Schouten
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/intel/intel_reg.h>

#define	PCI_VENDOR_INTEL	0x8086
#define	PCI_DEVICE_I965GM	0x2a02

static int intel_probe(device_t);
static int intel_attach(device_t);

static device_method_t intel_methods[] = {
	DEVMETHOD(device_probe,		intel_probe),
	DEVMETHOD(device_attach,	intel_attach),
	{ 0, 0 }
};

static vd_init_t	intel_vtinit;
static vd_bitbltchr_t	intel_vtbitbltchr;

static struct vt_driver intel_vtops = {
	.vd_init	= intel_vtinit,
	.vd_bitbltchr	= intel_vtbitbltchr,
	/* Prefer to use KMS, so GENERIC - 10 */
	.vd_priority	= VD_PRIORITY_GENERIC - 10,
};

struct intel_softc {
	bus_space_tag_t		i_reg_tag;
	bus_space_handle_t	i_reg_handle;
};

#define	REG_READ(sc, ofs)	\
	bus_space_read_4(sc->i_reg_tag, sc->i_reg_handle, ofs)
#define	REG_WRITE(sc, ofs, val)	\
	bus_space_write_4(sc->i_reg_tag, sc->i_reg_handle, ofs, val)

static devclass_t intel_devclass;
static driver_t intel_driver = {
	"vt_intel",
	intel_methods,
	sizeof(struct intel_softc)
};

static void
intel_initialize_graphics(struct vt_device *vd)
{
	struct intel_softc *sc = vd->vd_softc;
	uint32_t x;

	/* Disable VGA. */
	/* XXX: Doesn't work? Region returns 0xffffffff! */
	x = REG_READ(sc, INTEL_VB_VGACNTRL);
	x |= INTEL_VB_VGACNTRL_DISABLE;
	REG_WRITE(sc, INTEL_VB_VGACNTRL, x);
}

static int
intel_probe(device_t dev)
{

	if (pci_get_vendor(dev) != PCI_VENDOR_INTEL)
		return (ENXIO);
	if (pci_get_class(dev) != PCIC_DISPLAY)
		return (ENXIO);
	
	switch (pci_get_device(dev)) {
	case PCI_DEVICE_I965GM:
		device_set_desc(dev, "Intel 965GM");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
intel_attach(device_t dev)
{
	struct intel_softc *sc = device_get_softc(dev);
	struct resource *res;
	int rid;

	rid = 0x0a;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);
	sc->i_reg_tag = rman_get_bustag(res);
	sc->i_reg_handle = rman_get_bushandle(res);

	/* XXX */
	vt_allocate(&intel_vtops, sc);
	return (0);
}

static int
intel_vtinit(struct vt_device *vd)
{

	vd->vd_width = 1024;
	vd->vd_height = 768;
	intel_initialize_graphics(vd);

	return (0);
}

static void
intel_vtbitbltchr(struct vt_device *vd, const uint8_t *src,
    vt_axis_t top, vt_axis_t left, unsigned int width, unsigned int height,
    term_color_t fg, term_color_t bg)
{
}

DRIVER_MODULE(intel, pci, intel_driver, intel_devclass, 0, 0);
