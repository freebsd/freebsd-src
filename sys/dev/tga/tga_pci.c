/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Copyright (c) 2000 Andrew Miklic, Andrew Gallatin, and Thomas V. Crimi
 */

#include "opt_fb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/pc/bios.h>
#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/pc/vesa.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/tga.h>
#include <dev/tga/tga_pci.h>
#include <dev/fb/gfb.h>
#include <dev/gfb/gfb_pci.h>

static int tga_probe(device_t);
static int tga_attach(device_t);
static void tga_intr(void *);

static device_method_t tga_methods[] = {
	DEVMETHOD(device_probe, tga_probe),
	DEVMETHOD(device_attach, tga_attach),
	DEVMETHOD(device_detach, pcigfb_detach),
	{ 0, 0 }
};

static driver_t tga_driver = {
	"tga",
	tga_methods,
	sizeof(struct gfb_softc)
};

static devclass_t tga_devclass;

DRIVER_MODULE(tga, pci, tga_driver, tga_devclass, 0, 0);

static struct gfb_type tga_devs[] = {
	{ DEC_VENDORID, DEC_DEVICEID_TGA,
	"DEC TGA (21030) 2D Graphics Accelerator" },
	{ DEC_VENDORID, DEC_DEVICEID_TGA2,
	"DEC TGA2 (21130) 3D Graphics Accelerator" },
	{ 0, 0, NULL }
};

#ifdef FB_INSTALL_CDEV

static struct cdevsw tga_cdevsw = {
	/* open */	pcigfb_open,
	/* close */	pcigfb_close,
	/* read */	pcigfb_read,
	/* write */	pcigfb_write,
	/* ioctl */	pcigfb_ioctl,
	/* poll */	nopoll,
	/* mmap */	pcigfb_mmap,
	/* strategy */	nostrategy,
	/* name */	"tga",
	/* maj */	-1,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* kqfilter */	nokqfilter
};

#endif /* FB_INSTALL_CDEV */

static int
tga_probe(device_t dev)
{
	int error;
	gfb_type_t t;

	t = tga_devs;
	error = ENXIO;
	while(t->name != NULL) {
		if((pci_get_vendor(dev) == t->vendor_id) &&
		   (pci_get_device(dev) == t->device_id)) {
			device_set_desc(dev, t->name);
			error = 0;
			break;
		}
		t++;
	}
	return(error);
}

static int
tga_attach(device_t dev)
{
	gfb_softc_t sc;
	int unit, error, rid;

	error = 0;
	unit = device_get_unit(dev);
	sc = device_get_softc(dev);
	sc->driver_name = TGA_DRIVER_NAME;
	switch(pci_get_device(dev)) {
	case DEC_DEVICEID_TGA2:
		sc->model = 1;
		sc->type = KD_TGA2;
		break;
	case DEC_DEVICEID_TGA:
		sc->model = 0;
		sc->type = KD_TGA;
		break;
	default:
		device_printf(dev, "Unrecognized TGA type\n");
		goto fail;
	}
	if((error = pcigfb_attach(dev))) {
		goto fail;
	}
	sc->regs = sc->bhandle + TGA_MEM_CREGS;
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_TTY, tga_intr, sc,
		    &sc->intrhand);
	if(error) {
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}
	switch(sc->rev) {
	case 0x1:
	case 0x2:
	case 0x3:
		device_printf(dev, "TGA (21030) step %c\n", 'A' + sc->rev - 1);
		break;

	case 0x20:
		device_printf(dev, "TGA2 (21130) abstract software model\n");
		break;

	case 0x21:
	case 0x22:
		device_printf(dev, "TGA2 (21130) pass %d\n", sc->rev - 0x20);
		break;

	default:
		device_printf(dev, "Unknown stepping (0x%x)\n", sc->rev);
		break;
	}
#ifdef FB_INSTALL_CDEV
	sc->cdevsw = &tga_cdevsw;
	sc->devt = make_dev(sc->cdevsw, unit, UID_ROOT, GID_WHEEL, 0600,
	    "tga%x", unit);
#endif /* FB_INSTALL_CDEV */
	goto done;
fail:
	if(sc->intrhand != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->intrhand);
		sc->intrhand = NULL;
	}
	if(sc->irq != NULL) {
		rid = 0x0;
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->irq);
		sc->irq = NULL;
	}
	if(sc->res != NULL) {
		rid = GFB_MEM_BASE_RID;
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
		sc->res = NULL;
	}
	error = ENXIO;
done:
	return(error);
}

static void 
tga_intr(void *v)
{
	struct gfb_softc *sc = (struct gfb_softc *)v;
	u_int32_t reg;

	reg = READ_GFB_REGISTER(sc->adp, TGA_REG_SISR);
	if((reg & 0x00010001) != 0x00010001) {

		/* Odd. We never set any of the other interrupt enables. */
		if((reg & 0x1f) != 0) {

			/* Clear the mysterious pending interrupts. */
			WRITE_GFB_REGISTER(sc->adp, TGA_REG_SISR, (reg & 0x1f));
			GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_SISR, 1);

			/* This was our interrupt, even if we're puzzled as to
			 * why we got it.  Don't make the interrupt handler
			 * think it was a stray.
			 */
		}
	}

	/* Call the scheduled handler... */
	sc->gfbc->ramdac_intr(sc);

	/*
	   Clear interrupt field (this way, we will force a
	   memory error if we get an unexpected interrupt)...
	*/
	sc->gfbc->ramdac_intr = NULL;

	/* Disable the interrupt... */
	WRITE_GFB_REGISTER(sc->adp, TGA_REG_SISR, 0x00000001);
	GFB_REGISTER_WRITE_BARRIER(sc, TGA_REG_SISR, 1);
}
