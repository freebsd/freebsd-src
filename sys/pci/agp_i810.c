/*-
 * Copyright (c) 2000 Doug Rabson
 * Copyright (c) 2000 Ruslan Ermilov
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
 *
 *	$FreeBSD$
 */

/*
 * Fixes for 830/845G support: David Dawes <dawes@xfree86.org>
 */

#include "opt_bus.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/agppriv.h>
#include <pci/agpreg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

MALLOC_DECLARE(M_AGP);

#define READ1(off)	bus_space_read_1(sc->bst, sc->bsh, off)
#define READ4(off)	bus_space_read_4(sc->bst, sc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(sc->bst, sc->bsh, off, v)

#define CHIP_I810 0	/* i810/i815 */
#define CHIP_I830 1	/* i830/i845 */

struct agp_i810_softc {
	struct agp_softc agp;
	u_int32_t initial_aperture;	/* aperture size at startup */
	struct agp_gatt *gatt;
	int chiptype;			/* i810-like or i830 */
	u_int32_t dcache_size;		/* i810 only */
	u_int32_t stolen;		/* number of i830/845 gtt entries for stolen memory */
	device_t bdev;			/* bridge device */
	struct resource *regs;		/* memory mapped GC registers */
	bus_space_tag_t bst;		/* bus_space tag */
	bus_space_handle_t bsh;		/* bus_space handle */
};

static const char*
agp_i810_match(device_t dev)
{
	if (pci_get_class(dev) != PCIC_DISPLAY
	    || pci_get_subclass(dev) != PCIS_DISPLAY_VGA)
		return NULL;

	switch (pci_get_devid(dev)) {
	case 0x71218086:
		return ("Intel 82810 (i810 GMCH) SVGA controller");

	case 0x71238086:
		return ("Intel 82810-DC100 (i810-DC100 GMCH) SVGA controller");

	case 0x71258086:
		return ("Intel 82810E (i810E GMCH) SVGA controller");

	case 0x11328086:
		return ("Intel 82815 (i815 GMCH) SVGA controller");

	case 0x35778086:
		return ("Intel 82830 (i830M GMCH) SVGA controller");

	case 0x25628086:
		return ("Intel 82845 (i845 GMCH) SVGA controller");
	};

	return NULL;
}

/*
 * Find bridge device.
 */
static device_t
agp_i810_find_bridge(device_t dev)
{
	device_t *children, child;
	int nchildren, i;
	u_int32_t devid;

	/*
	 * Calculate bridge device's ID.
	 */
	devid = pci_get_devid(dev);
	switch (devid) {
	case 0x71218086:
	case 0x71238086:
	case 0x71258086:
		devid -= 0x10000;
		break;

	case 0x11328086:
	case 0x35778086:
	case 0x25628086:
		devid -= 0x20000;
		break;
	};
	if (device_get_children(device_get_parent(dev), &children, &nchildren))
		return 0;

	for (i = 0; i < nchildren; i++) {
		child = children[i];

		if (pci_get_devid(child) == devid) {
			free(children, M_TEMP);
			return child;
		}
	}
	free(children, M_TEMP);
	return 0;
}

static int
agp_i810_probe(device_t dev)
{
	const char *desc;

	desc = agp_i810_match(dev);
	if (desc) {
		device_t bdev;
		u_int8_t smram;
		int devid = pci_get_devid(dev);

		bdev = agp_i810_find_bridge(dev);
		if (!bdev) {
			if (bootverbose)
				printf("I810: can't find bridge device\n");
			return ENXIO;
		}

		/*
		 * checking whether internal graphics device has been activated.
		 */
		if ( (devid != 0x35778086 ) &&
		     (devid != 0x25628086 ) ) {
			smram = pci_read_config(bdev, AGP_I810_SMRAM, 1);
			if ((smram & AGP_I810_SMRAM_GMS)
			    == AGP_I810_SMRAM_GMS_DISABLED) {
				if (bootverbose)
					printf("I810: disabled, not probing\n");
				return ENXIO;
			}
		} else {	/* I830MG */
			unsigned int gcc1;
			gcc1 = pci_read_config(bdev, AGP_I830_GCC1, 1);
			if ((gcc1 & AGP_I830_GCC1_DEV2) == AGP_I830_GCC1_DEV2_DISABLED) {
				if (bootverbose)
					printf("I830: disabled, not probing\n");
					return ENXIO;
			}
		}

		device_verbose(dev);
		device_set_desc(dev, desc);
		return 0;
	}

	return ENXIO;
}

static int
agp_i810_attach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	int error, rid;

	sc->bdev = agp_i810_find_bridge(dev);
	if (!sc->bdev)
		return ENOENT;

	error = agp_generic_attach(dev);
	if (error)
		return error;

	switch (pci_get_devid(dev)) {
	case 0x71218086:
	case 0x71238086:
	case 0x71258086:
	case 0x11328086:
		sc->chiptype = CHIP_I810;
	case 0x35778086:
	case 0x25628086:
		sc->chiptype = CHIP_I830;
	};

	/* Same for i810 and i830 */
	rid = AGP_I810_MMADR;
	sc->regs = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				      0, ~0, 1, RF_ACTIVE);
	if (!sc->regs) {
		agp_generic_detach(dev);
		return ENOMEM;
	}
	sc->bst = rman_get_bustag(sc->regs);
	sc->bsh = rman_get_bushandle(sc->regs);

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	gatt = malloc( sizeof(struct agp_gatt), M_AGP, M_NOWAIT);
	if (!gatt) {
 		agp_generic_detach(dev);
 		return ENOMEM;
	}
	sc->gatt = gatt;

	gatt->ag_entries = AGP_GET_APERTURE(dev) >> AGP_PAGE_SHIFT;

	if ( sc->chiptype == CHIP_I810 ) {
		/* Some i810s have on-chip memory called dcache */
		if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
			sc->dcache_size = 4 * 1024 * 1024;
		else
			sc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		gatt->ag_virtual = contigmalloc( 64 * 1024, M_AGP, 0, 
					0, ~0, PAGE_SIZE, 0);
		if (!gatt->ag_virtual) {
			if (bootverbose)
				device_printf(dev, "contiguous allocation failed\n");
			free(gatt, M_AGP);
			agp_generic_detach(dev);
			return ENOMEM;
		}
		bzero(gatt->ag_virtual, gatt->ag_entries * sizeof(u_int32_t));
	
		gatt->ag_physical = vtophys((vm_offset_t) gatt->ag_virtual);
		agp_flush_cache();
		/* Install the GATT. */
		WRITE4(AGP_I810_PGTBL_CTL, gatt->ag_physical | 1);
	} else {
		/* The i830 automatically initializes the 128k gatt on boot. */
		unsigned int gcc1, pgtblctl;
		
		gcc1 = pci_read_config(sc->bdev, AGP_I830_GCC1, 1);
		switch (gcc1 & AGP_I830_GCC1_GMS) {
			case AGP_I830_GCC1_GMS_STOLEN_512:
				sc->stolen = (512 - 132) * 1024 / 4096;
				break;
			case AGP_I830_GCC1_GMS_STOLEN_1024: 
				sc->stolen = (1024 - 132) * 1024 / 4096;
				break;
			case AGP_I830_GCC1_GMS_STOLEN_8192: 
				sc->stolen = (8192 - 132) * 1024 / 4096;
				break;
			default:
				sc->stolen = 0;
				device_printf(dev, "unknown memory configuration, disabling\n");
				agp_generic_detach(dev);
				return EINVAL;
		}
		if (sc->stolen > 0)
			device_printf(dev, "detected %dk stolen memory\n", sc->stolen * 4);
		device_printf(dev, "aperture size is %dM\n", sc->initial_aperture / 1024 / 1024);

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
#if 0
		device_printf(dev, "PGTBL_CTL is 0x%08x\n", pgtblctl);
#endif
		pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	}

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();

	return 0;
}

static int
agp_i810_detach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	int error;

	error = agp_generic_detach(dev);
	if (error)
		return error;

	/* Clear the GATT base. */
	if ( sc->chiptype == CHIP_I810 ) {
		WRITE4(AGP_I810_PGTBL_CTL, 0);
	} else {
		unsigned int pgtblctl;
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl &= ~1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);
	}

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(dev, sc->initial_aperture);

	if ( sc->chiptype == CHIP_I810 ) {
		contigfree(sc->gatt->ag_virtual, 64 * 1024, M_AGP);
	}
	free(sc->gatt, M_AGP);

	bus_release_resource(dev, SYS_RES_MEMORY,
			     AGP_I810_MMADR, sc->regs);

	return 0;
}

static u_int32_t
agp_i810_get_aperture(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if ( sc->chiptype == CHIP_I810 ) {
		u_int16_t miscc;
		miscc = pci_read_config(sc->bdev, AGP_I810_MISCC, 2);
		if ((miscc & AGP_I810_MISCC_WINSIZE) == AGP_I810_MISCC_WINSIZE_32)
			return 32 * 1024 * 1024;
		else
			return 64 * 1024 * 1024;
	} else {	/* I830 */
		unsigned int gcc1;

		gcc1 = pci_read_config(sc->bdev, AGP_I830_GCC1, 2);
		if ((gcc1 & AGP_I830_GCC1_GMASIZE) == AGP_I830_GCC1_GMASIZE_64)
			return 64 * 1024 * 1024;
		else
			return 128 * 1024 * 1024;
	}
}

static int
agp_i810_set_aperture(device_t dev, u_int32_t aperture)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	u_int16_t miscc;

	if ( sc->chiptype == CHIP_I810 ) {
		/*
		 * Double check for sanity.
		 */
		if (aperture != 32 * 1024 * 1024 && aperture != 64 * 1024 * 1024) {
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}
	
		miscc = pci_read_config(sc->bdev, AGP_I810_MISCC, 2);
		miscc &= ~AGP_I810_MISCC_WINSIZE;
		if (aperture == 32 * 1024 * 1024)
			miscc |= AGP_I810_MISCC_WINSIZE_32;
		else
			miscc |= AGP_I810_MISCC_WINSIZE_64;
	
		pci_write_config(sc->bdev, AGP_I810_MISCC, miscc, 2);
	} else {	/* I830 */
		unsigned int gcc1;

		if (aperture != 64 * 1024 * 1024 && aperture != 128 * 1024 * 1024) {
			device_printf(dev, "bad aperture size %d\n", aperture);
			return EINVAL;
		}
		gcc1 = pci_read_config(sc->bdev, AGP_I830_GCC1, 2);
		gcc1 &= ~AGP_I830_GCC1_GMASIZE;
		if (aperture == 64 * 1024 * 1024)
			gcc1 |= AGP_I830_GCC1_GMASIZE_64;
		else
			gcc1 |= AGP_I830_GCC1_GMASIZE_128;

		pci_write_config(sc->bdev, AGP_I830_GCC1, gcc1, 2);
	}

	return 0;
}

static int
agp_i810_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT)) {
		device_printf(dev, "failed: offset is 0x%08x, shift is %d, entries is %d\n", offset, AGP_PAGE_SHIFT, sc->gatt->ag_entries);
		return EINVAL;
	}

	if ( sc->chiptype == CHIP_I830 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to bind into stolen memory");
			return EINVAL;
		}
	}

	WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4, physical | 1);
	return 0;
}

static int
agp_i810_unbind_page(device_t dev, int offset)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	if ( sc->chiptype == CHIP_I830 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to unbind from stolen memory");
			return EINVAL;
		}
	}

	WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4, 0);
	return 0;
}

/*
 * Writing via memory mapped registers already flushes all TLBs.
 */
static void
agp_i810_flush_tlb(device_t dev)
{
}

static int
agp_i810_enable(device_t dev, u_int32_t mode)
{

	return 0;
}

static struct agp_memory *
agp_i810_alloc_memory(device_t dev, int type, vm_size_t size)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	struct agp_memory *mem;

	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return 0;

	if (sc->agp.as_allocated + size > sc->agp.as_maxmem)
		return 0;

	if (type == 1) {
		/*
		 * Mapping local DRAM into GATT.
		 */
		if ( sc->chiptype == CHIP_I830 )
			return 0;
		if (size != sc->dcache_size)
			return 0;
	} else if (type == 2) {
		/*
		 * Bogus mapping of a single page for the hardware cursor.
		 */
		if (size != AGP_PAGE_SIZE)
			return 0;
	}

	mem = malloc(sizeof *mem, M_AGP, M_WAITOK);
	mem->am_id = sc->agp.as_nextid++;
	mem->am_size = size;
	mem->am_type = type;
	if (type != 1)
		mem->am_obj = vm_object_allocate(OBJT_DEFAULT,
						 atop(round_page(size)));
	else
		mem->am_obj = 0;

	if (type == 2) {
		/*
		 * Allocate and wire down the page now so that we can
		 * get its physical address.
		 */
		vm_page_t m;
		m = vm_page_grab(mem->am_obj, 0, VM_ALLOC_ZERO|VM_ALLOC_RETRY);
		if ((m->flags & PG_ZERO) == 0)
			vm_page_zero_fill(m);
		vm_page_wire(m);
		mem->am_physical = VM_PAGE_TO_PHYS(m);
		vm_page_wakeup(m);
	} else {
		mem->am_physical = 0;
	}

	mem->am_offset = 0;
	mem->am_is_bound = 0;
	TAILQ_INSERT_TAIL(&sc->agp.as_memory, mem, am_link);
	sc->agp.as_allocated += size;

	return mem;
}

static int
agp_i810_free_memory(device_t dev, struct agp_memory *mem)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (mem->am_is_bound)
		return EBUSY;

	if (mem->am_type == 2) {
		/*
		 * Unwire the page which we wired in alloc_memory.
		 */
		vm_page_t m = vm_page_lookup(mem->am_obj, 0);
		vm_page_unwire(m, 0);
	}

	sc->agp.as_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->agp.as_memory, mem, am_link);
	if (mem->am_obj)
		vm_object_deallocate(mem->am_obj);
	free(mem, M_AGP);
	return 0;
}

static int
agp_i810_bind_memory(device_t dev, struct agp_memory *mem,
		     vm_offset_t offset)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	vm_offset_t i;

	if (mem->am_type != 1)
		return agp_generic_bind_memory(dev, mem, offset);

	if ( sc->chiptype == CHIP_I830 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		WRITE4(AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4,
		       i | 3);
	}

	return 0;
}

static int
agp_i810_unbind_memory(device_t dev, struct agp_memory *mem)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	vm_offset_t i;

	if (mem->am_type != 1)
		return agp_generic_unbind_memory(dev, mem);

	if ( sc->chiptype == CHIP_I830 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		WRITE4(AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, 0);

	return 0;
}

static device_method_t agp_i810_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		agp_i810_probe),
	DEVMETHOD(device_attach,	agp_i810_attach),
	DEVMETHOD(device_detach,	agp_i810_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_i810_get_aperture),
	DEVMETHOD(agp_set_aperture,	agp_i810_set_aperture),
	DEVMETHOD(agp_bind_page,	agp_i810_bind_page),
	DEVMETHOD(agp_unbind_page,	agp_i810_unbind_page),
	DEVMETHOD(agp_flush_tlb,	agp_i810_flush_tlb),
	DEVMETHOD(agp_enable,		agp_i810_enable),
	DEVMETHOD(agp_alloc_memory,	agp_i810_alloc_memory),
	DEVMETHOD(agp_free_memory,	agp_i810_free_memory),
	DEVMETHOD(agp_bind_memory,	agp_i810_bind_memory),
	DEVMETHOD(agp_unbind_memory,	agp_i810_unbind_memory),

	{ 0, 0 }
};

static driver_t agp_i810_driver = {
	"agp",
	agp_i810_methods,
	sizeof(struct agp_i810_softc),
};

static devclass_t agp_devclass;

DRIVER_MODULE(agp_i810, pci, agp_i810_driver, agp_devclass, 0, 0);
