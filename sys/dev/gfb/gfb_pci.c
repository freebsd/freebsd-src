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
#include <dev/fb/gfb.h>
#include <dev/gfb/gfb_pci.h>

#ifdef __alpha__

#include <machine/rpb.h>
#include <machine/cpu.h>

#endif /* __alpha__ */

#include "opt_fb.h"

static devclass_t gfb_devclass;

#ifdef __alpha__

extern void sccnattach(void);

#endif /* __alpha__ */

extern struct gfb_font bold8x16;
extern struct gfb_softc *gfb_device_softcs[2][16];

int
pcigfb_attach(device_t dev)
{
	int s;
	gfb_softc_t sc;
	video_adapter_t *adp;
	int unit, flags, error, rid, va_index;
#ifdef __alpha__
	struct ctb *ctb;
#endif /* __alpha__ */

	s = splimp();
	error = 0;
	unit = device_get_unit(dev);
	flags = device_get_flags(dev);
	sc = device_get_softc(dev);
	sc->rev = pci_get_revid(dev);
	rid = GFB_MEM_BASE_RID;
	sc->res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1,
	    RF_ACTIVE|PCI_RF_DENSE);
	if(sc->res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		goto fail;
	}
	sc->btag = rman_get_bustag(sc->res);
	sc->bhandle = rman_get_bushandle(sc->res);

	/* Allocate interrupt (irq)... */
	rid = 0x0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE|RF_ACTIVE);
	if(sc->irq == NULL) {
		device_printf(dev, "Couldn't map interrupt\n");
		goto fail;
	}
	if((va_index = vid_find_adapter(sc->driver_name, unit)) < 0) {
		sc->adp = (video_adapter_t *)malloc(sizeof(video_adapter_t),
		    M_DEVBUF, M_NOWAIT);
		adp = sc->adp;
		bzero(adp, sizeof(video_adapter_t));
		vid_init_struct(adp, sc->driver_name, sc->type, unit);
		if(vid_register(adp) < 0) {
			free(sc->adp, M_DEVBUF);
			goto fail;
		}
		adp->va_flags |= V_ADP_REGISTERED;
		adp->va_model = sc->model;
		adp->va_mem_base = (vm_offset_t)rman_get_virtual(sc->res);
		adp->va_mem_size = rman_get_end(sc->res) -
		    rman_get_start(sc->res);
		adp->va_io_base = NULL;
		adp->va_io_size = 0;
		adp->va_crtc_addr = NULL;
		gfb_device_softcs[sc->model][unit] = sc;
		sc->gfbc = (struct gfb_conf *)malloc(sizeof(struct gfb_conf),
		    M_DEVBUF, M_NOWAIT);
		bzero(sc->gfbc, sizeof(struct gfb_conf));
		if((*vidsw[adp->va_index]->init)(unit, adp, flags)) {
			device_printf(dev, "Couldn't initialize adapter\n");
			vid_unregister(adp);
			gfb_device_softcs[sc->model][unit] = NULL;
			free(sc->gfbc, M_DEVBUF);
			free(sc->adp, M_DEVBUF);
			goto fail;
		}
		sc->gfbc->palette.red =
		    (u_char *)malloc(sc->gfbc->palette.count, M_DEVBUF,
		    M_NOWAIT);
		sc->gfbc->palette.green =
		    (u_char *)malloc(sc->gfbc->palette.count, M_DEVBUF,
		    M_NOWAIT);
		sc->gfbc->palette.blue =
		    (u_char *)malloc(sc->gfbc->palette.count, M_DEVBUF,
		    M_NOWAIT);
		sc->gfbc->cursor_palette.red =
		    (u_char *)malloc(sc->gfbc->cursor_palette.count, M_DEVBUF,
		    M_NOWAIT);
		sc->gfbc->cursor_palette.green =
		    (u_char *)malloc(sc->gfbc->cursor_palette.count, M_DEVBUF,
		    M_NOWAIT);
		sc->gfbc->cursor_palette.blue =
		    (u_char *)malloc(sc->gfbc->cursor_palette.count, M_DEVBUF,
		    M_NOWAIT);
		if(gfb_init(unit, adp, flags)) {
			device_printf(dev, "Couldn't initialize framebuffer\n");
			vid_unregister(adp);
			gfb_device_softcs[sc->model][unit] = NULL;
			free(sc->gfbc->cursor_palette.blue, M_DEVBUF);
			free(sc->gfbc->cursor_palette.green, M_DEVBUF);
			free(sc->gfbc->cursor_palette.red, M_DEVBUF);
			free(sc->gfbc->palette.blue, M_DEVBUF);
			free(sc->gfbc->palette.green, M_DEVBUF);
			free(sc->gfbc->palette.red, M_DEVBUF);
			free(sc->gfbc, M_DEVBUF);
			free(sc->adp, M_DEVBUF);
			goto fail;
		}
	} else {
		(*vidsw[va_index]->probe)(unit, &adp, (void *)sc->driver_name,
		    flags);
		sc->adp = adp;
		sc->gfbc = gfb_device_softcs[sc->model][unit]->gfbc;
		gfb_device_softcs[sc->model][unit] = sc;
	}

	/*
	   This is a back-door for PCI devices--since FreeBSD no longer supports
	   PCI configuration-space accesses during the *configure() phase for
	   video adapters, we cannot identify a PCI device as the console during
	   the first call to sccnattach(). There must be a second chance for PCI
	   adapters to be recognized as the console, and this is it...
	*/
#ifdef __alpha__
	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);
	if (ctb->ctb_term_type == 3) /* Display adapter */
		sccnattach();
#endif /* __alpha__ */

	device_printf(dev, "Board type %s\n", sc->gfbc->name);
	device_printf(dev, "%d x %d, %dbpp, %s RAMDAC\n",
	       sc->adp->va_info.vi_width, sc->adp->va_info.vi_height,
	       sc->adp->va_info.vi_depth, sc->gfbc->ramdac_name);
#ifdef FB_INSTALL_CDEV
	/* attach a virtual frame buffer device */
	error = fb_attach(makedev(0, unit), sc->adp, sc->cdevsw);
	if(error)
		goto fail;
	if(bootverbose)
		(*vidsw[sc->adp->va_index]->diag)(sc->adp, bootverbose);
#if experimental
	device_add_child(dev, "fb", -1);
	bus_generic_attach(dev);
#endif /*experimental*/
#endif /*FB_INSTALL_CDEV*/
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
	splx(s);
	return(error);
}

int
pcigfb_detach(device_t dev)
{
	struct gfb_softc *sc;
	int rid;

	sc = device_get_softc(dev);
#ifdef FB_INSTALL_CDEV
	destroy_dev(sc->devt);
#endif /*FB_INSTALL_CDEV*/
	bus_teardown_intr(dev, sc->irq, sc->intrhand);
	rid = 0x0;
	bus_release_resource(dev, SYS_RES_IRQ, rid, sc->irq);
	rid = GFB_MEM_BASE_RID;
	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
	return(0);
}

#ifdef FB_INSTALL_CDEV

int
pcigfb_open(dev_t dev, int flag, int mode, struct proc *p)
{
	struct gfb_softc *sc;
	int error;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));

	if(sc == NULL)
		error = ENXIO;
	else if(mode & (O_CREAT | O_APPEND | O_TRUNC))
		error = ENODEV;
	else
		error = genfbopen(&sc->gensc, sc->adp, flag, mode, p);
	return(error);
}

int
pcigfb_close(dev_t dev, int flag, int mode, struct proc *p)
{
	struct gfb_softc *sc;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));
	return genfbclose(&sc->gensc, sc->adp, flag, mode, p);
}

int
pcigfb_read(dev_t dev, struct uio *uio, int flag)
{
	struct gfb_softc *sc;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));
	return genfbread(&sc->gensc, sc->adp, uio, flag);
}

int
pcigfb_write(dev_t dev, struct uio *uio, int flag)
{
	struct gfb_softc *sc;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));
	return genfbwrite(&sc->gensc, sc->adp, uio, flag);
}

int
pcigfb_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *p)
{
	struct gfb_softc *sc;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));
	return genfbioctl(&sc->gensc, sc->adp, cmd, arg, flag, p);
}

int
pcigfb_mmap(dev_t dev, vm_offset_t offset, int prot)
{
	struct gfb_softc *sc;

	sc = (struct gfb_softc *)devclass_get_softc(gfb_devclass, minor(dev));
	return genfbmmap(&sc->gensc, sc->adp, offset, prot);
}

#endif /*FB_INSTALL_CDEV*/
