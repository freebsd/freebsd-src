/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_vga.h"
#include "opt_fb.h"
#include "opt_syscons.h"	/* should be removed in the future, XXX */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/fbio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#ifdef __i386__
#include <machine/pc/bios.h>
#endif

#include <dev/fb/fbreg.h>
#include <dev/fb/vgareg.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

#define VGA_SOFTC(unit)		\
	((vga_softc_t *)devclass_get_softc(isavga_devclass, unit))

static devclass_t	isavga_devclass;

#ifdef FB_INSTALL_CDEV

static d_open_t		isavga_open;
static d_close_t	isavga_close;
static d_read_t		isavga_read;
static d_write_t	isavga_write;
static d_ioctl_t	isavga_ioctl;
static d_mmap_t		isavga_mmap;

static struct cdevsw isavga_cdevsw = {
	.d_open =	isavga_open,
	.d_close =	isavga_close,
	.d_read =	isavga_read,
	.d_write =	isavga_write,
	.d_ioctl =	isavga_ioctl,
	.d_mmap =	isavga_mmap,
	.d_name =	VGA_DRIVER_NAME,
	.d_maj =	-1,
};

#endif /* FB_INSTALL_CDEV */

static void
isavga_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, VGA_DRIVER_NAME, 0);
}

static int
isavga_probe(device_t dev)
{
	video_adapter_t adp;
	int error;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	device_set_desc(dev, "Generic ISA VGA");
	error = vga_probe_unit(device_get_unit(dev), &adp, device_get_flags(dev));
	if (error == 0) {
		bus_set_resource(dev, SYS_RES_IOPORT, 0,
				 adp.va_io_base, adp.va_io_size);
		bus_set_resource(dev, SYS_RES_MEMORY, 0,
				 adp.va_mem_base, adp.va_mem_size);
#if 0
		isa_set_port(dev, adp.va_io_base);
		isa_set_portsize(dev, adp.va_io_size);
		isa_set_maddr(dev, adp.va_mem_base);
		isa_set_msize(dev, adp.va_mem_size);
#endif
	}
	return error;
}

static int
isavga_attach(device_t dev)
{
	vga_softc_t *sc;
	int unit;
	int rid;
	int error;

	unit = device_get_unit(dev);
	sc = device_get_softc(dev);

	rid = 0;
	bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, 0, RF_ACTIVE | RF_SHAREABLE);
	rid = 0;
	bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				 0, ~0, 0, RF_ACTIVE | RF_SHAREABLE);

	error = vga_attach_unit(unit, sc, device_get_flags(dev));
	if (error)
		return error;

#ifdef FB_INSTALL_CDEV
	/* attach a virtual frame buffer device */
	error = fb_attach(makedev(0, VGA_MKMINOR(unit)), sc->adp, &isavga_cdevsw);
	if (error)
		return error;
#endif /* FB_INSTALL_CDEV */

	if (bootverbose)
		(*vidsw[sc->adp->va_index]->diag)(sc->adp, bootverbose);

#if experimental
	device_add_child(dev, "fb", -1);
	bus_generic_attach(dev);
#endif

	return 0;
}

#ifdef FB_INSTALL_CDEV

static int
isavga_open(dev_t dev, int flag, int mode, struct thread *td)
{
	return vga_open(dev, VGA_SOFTC(VGA_UNIT(dev)), flag, mode, td);
}

static int
isavga_close(dev_t dev, int flag, int mode, struct thread *td)
{
	return vga_close(dev, VGA_SOFTC(VGA_UNIT(dev)), flag, mode, td);
}

static int
isavga_read(dev_t dev, struct uio *uio, int flag)
{
	return vga_read(dev, VGA_SOFTC(VGA_UNIT(dev)), uio, flag);
}

static int
isavga_write(dev_t dev, struct uio *uio, int flag)
{
	return vga_write(dev, VGA_SOFTC(VGA_UNIT(dev)), uio, flag);
}

static int
isavga_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
	return vga_ioctl(dev, VGA_SOFTC(VGA_UNIT(dev)), cmd, arg, flag, td);
}

static int
isavga_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int prot)
{
	return vga_mmap(dev, VGA_SOFTC(VGA_UNIT(dev)), offset, paddr, prot);
}

#endif /* FB_INSTALL_CDEV */

static device_method_t isavga_methods[] = {
	DEVMETHOD(device_identify,	isavga_identify),
	DEVMETHOD(device_probe,		isavga_probe),
	DEVMETHOD(device_attach,	isavga_attach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	{ 0, 0 }
};

static driver_t isavga_driver = {
	VGA_DRIVER_NAME,
	isavga_methods,
	sizeof(vga_softc_t),
};

DRIVER_MODULE(vga, isa, isavga_driver, isavga_devclass, 0, 0);
