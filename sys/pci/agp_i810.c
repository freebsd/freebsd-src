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
 */

/*
 * Fixes for 830/845G support: David Dawes <dawes@xfree86.org>
 * 852GM/855GM/865G support added by David Dawes <dawes@xfree86.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/pci/agp_i810.c,v 1.41.2.2 2007/12/01 16:24:23 remko Exp $");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <pci/agppriv.h>
#include <pci/agpreg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/md_var.h>
#include <sys/rman.h>

MALLOC_DECLARE(M_AGP);

enum {
	CHIP_I810,	/* i810/i815 */
	CHIP_I830,	/* 830M/845G */
	CHIP_I855,	/* 852GM/855GM/865G */
	CHIP_I915,	/* 915G/915GM */
	CHIP_I965,	/* G965 */
	CHIP_G33,	/* G33/Q33/Q35 */
};

/* The i810 through i855 have the registers at BAR 1, and the GATT gets
 * allocated by us.  The i915 has registers in BAR 0 and the GATT is at the
 * start of the stolen memory, and should only be accessed by the OS through
 * BAR 3.  The G965 has registers and GATT in the same BAR (0) -- first 512KB
 * is registers, second 512KB is GATT.
 */
static struct resource_spec agp_i810_res_spec[] = {
	{ SYS_RES_MEMORY, AGP_I810_MMADR, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

static struct resource_spec agp_i915_res_spec[] = {
	{ SYS_RES_MEMORY, AGP_I915_MMADR, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_MEMORY, AGP_I915_GTTADR, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

static struct resource_spec agp_i965_res_spec[] = {
	{ SYS_RES_MEMORY, AGP_I965_GTTMMADR, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

struct agp_i810_softc {
	struct agp_softc agp;
	u_int32_t initial_aperture;	/* aperture size at startup */
	struct agp_gatt *gatt;
	int chiptype;			/* i810-like or i830 */
	u_int32_t dcache_size;		/* i810 only */
	u_int32_t stolen;		/* number of i830/845 gtt entries for stolen memory */
	device_t bdev;			/* bridge device */

	void *argb_cursor;		/* contigmalloc area for ARGB cursor */

	struct resource_spec * sc_res_spec;
	struct resource *sc_res[2];
};

/* For adding new devices, devid is the id of the graphics controller
 * (pci:0:2:0, for example).  The placeholder (usually at pci:0:2:1) for the
 * second head should never be added.  The bridge_offset is the offset to
 * subtract from devid to get the id of the hostb that the device is on.
 */
static const struct agp_i810_match {
	int devid;
	int chiptype;
	int bridge_offset;
	char *name;
} agp_i810_matches[] = {
	{0x71218086, CHIP_I810, 0x00010000,
	    "Intel 82810 (i810 GMCH) SVGA controller"},
	{0x71238086, CHIP_I810, 0x00010000,
	    "Intel 82810-DC100 (i810-DC100 GMCH) SVGA controller"},
	{0x71258086, CHIP_I810, 0x00010000,
	    "Intel 82810E (i810E GMCH) SVGA controller"},
	{0x11328086, CHIP_I810, 0x00020000,
	    "Intel 82815 (i815 GMCH) SVGA controller"},
	{0x35778086, CHIP_I830, 0x00020000,
	    "Intel 82830M (830M GMCH) SVGA controller"},
	{0x25628086, CHIP_I830, 0x00020000,
	    "Intel 82845M (845M GMCH) SVGA controller"},
	{0x35828086, CHIP_I855, 0x00020000,
	    "Intel 82852/5"},
	{0x25728086, CHIP_I855, 0x00020000,
	    "Intel 82865G (865G GMCH) SVGA controller"},
	{0x25828086, CHIP_I915, 0x00020000,
	    "Intel 82915G (915G GMCH) SVGA controller"},
	{0x258A8086, CHIP_I915, 0x00020000,
	    "Intel E7221 SVGA controller"},
	{0x25928086, CHIP_I915, 0x00020000,
	    "Intel 82915GM (915GM GMCH) SVGA controller"},
	{0x27728086, CHIP_I915, 0x00020000,
	    "Intel 82945G (945G GMCH) SVGA controller"},
	{0x27A28086, CHIP_I915, 0x00020000,
	    "Intel 82945GM (945GM GMCH) SVGA controller"},
	{0x27A28086, CHIP_I915, 0x00020000,
	    "Intel 945GME SVGA controller"},
	{0x29728086, CHIP_I965, 0x00020000,
	    "Intel 946GZ SVGA controller"},
	{0x29828086, CHIP_I965, 0x00020000,
	    "Intel G965 SVGA controller"},
	{0x29928086, CHIP_I965, 0x00020000,
	    "Intel Q965 SVGA controller"},
	{0x29a28086, CHIP_I965, 0x00020000,
	    "Intel G965 SVGA controller"},
/*
	{0x29b28086, CHIP_G33, 0x00020000,
	    "Intel Q35 SVGA controller"},
	{0x29c28086, CHIP_G33, 0x00020000,
	    "Intel G33 SVGA controller"},
	{0x29d28086, CHIP_G33, 0x00020000,
	    "Intel Q33 SVGA controller"},
*/
	{0x2a028086, CHIP_I965, 0x00020000,
	    "Intel GM965 SVGA controller"},
	{0x2a128086, CHIP_I965, 0x00020000,
	    "Intel GME965 SVGA controller"},
	{0, 0, 0, NULL}
};

static const struct agp_i810_match*
agp_i810_match(device_t dev)
{
	int i, devid;

	if (pci_get_class(dev) != PCIC_DISPLAY
	    || pci_get_subclass(dev) != PCIS_DISPLAY_VGA)
		return NULL;

	devid = pci_get_devid(dev);
	for (i = 0; agp_i810_matches[i].devid != 0; i++) {
		if (agp_i810_matches[i].devid == devid)
		    break;
	}
	if (agp_i810_matches[i].devid == 0)
		return NULL;
	else
		return &agp_i810_matches[i];
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
	const struct agp_i810_match *match;
  
	match = agp_i810_match(dev);
	devid = match->devid - match->bridge_offset;

	if (device_get_children(device_get_parent(device_get_parent(dev)),
	    &children, &nchildren))
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

static void
agp_i810_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "agp", -1) == NULL &&
	    agp_i810_match(parent))
		device_add_child(parent, "agp", -1);
}

static int
agp_i810_probe(device_t dev)
{
	device_t bdev;
	const struct agp_i810_match *match;
	u_int8_t smram;
	int gcc1, deven;

	if (resource_disabled("agp", device_get_unit(dev)))
		return (ENXIO);
	match = agp_i810_match(dev);
	if (match == NULL)
		return ENXIO;

	bdev = agp_i810_find_bridge(dev);
	if (!bdev) {
		if (bootverbose)
			printf("I810: can't find bridge device\n");
		return ENXIO;
	}

	/*
	 * checking whether internal graphics device has been activated.
	 */
	switch (match->chiptype) {
	case CHIP_I810:
		smram = pci_read_config(bdev, AGP_I810_SMRAM, 1);
		if ((smram & AGP_I810_SMRAM_GMS) ==
		    AGP_I810_SMRAM_GMS_DISABLED) {
			if (bootverbose)
				printf("I810: disabled, not probing\n");
			return ENXIO;
		}
		break;
	case CHIP_I830:
	case CHIP_I855:
		gcc1 = pci_read_config(bdev, AGP_I830_GCC1, 1);
		if ((gcc1 & AGP_I830_GCC1_DEV2) ==
		    AGP_I830_GCC1_DEV2_DISABLED) {
			if (bootverbose)
				printf("I830: disabled, not probing\n");
			return ENXIO;
		}
		break;
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
		deven = pci_read_config(bdev, AGP_I915_DEVEN, 4);
		if ((deven & AGP_I915_DEVEN_D2F0) ==
		    AGP_I915_DEVEN_D2F0_DISABLED) {
			if (bootverbose)
				printf("I915: disabled, not probing\n");
			return ENXIO;
		}
		break;
	}

	if (match->devid == 0x35828086) {
		switch (pci_read_config(dev, AGP_I85X_CAPID, 1)) {
		case AGP_I855_GME:
			device_set_desc(dev,
			    "Intel 82855GME (855GME GMCH) SVGA controller");
			break;
		case AGP_I855_GM:
			device_set_desc(dev,
			    "Intel 82855GM (855GM GMCH) SVGA controller");
			break;
		case AGP_I852_GME:
			device_set_desc(dev,
			    "Intel 82852GME (852GME GMCH) SVGA controller");
			break;
		case AGP_I852_GM:
			device_set_desc(dev,
			    "Intel 82852GM (852GM GMCH) SVGA controller");
			break;
		default:
			device_set_desc(dev,
			    "Intel 8285xM (85xGM GMCH) SVGA controller");
			break;
		}
	} else {
		device_set_desc(dev, match->name);
	}

	return BUS_PROBE_DEFAULT;
}

static void
agp_i810_dump_regs(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	device_printf(dev, "AGP_I810_PGTBL_CTL: %08x\n",
	    bus_read_4(sc->sc_res[0], AGP_I810_PGTBL_CTL));

	switch (sc->chiptype) {
	case CHIP_I810:
		device_printf(dev, "AGP_I810_MISCC: 0x%04x\n",
		    pci_read_config(sc->bdev, AGP_I810_MISCC, 2));
		break;
	case CHIP_I830:
		device_printf(dev, "AGP_I830_GCC1: 0x%02x\n",
		    pci_read_config(sc->bdev, AGP_I830_GCC1, 1));
		break;
	case CHIP_I855:
		device_printf(dev, "AGP_I855_GCC1: 0x%02x\n",
		    pci_read_config(sc->bdev, AGP_I855_GCC1, 1));
		break;
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
		device_printf(dev, "AGP_I855_GCC1: 0x%02x\n",
		    pci_read_config(sc->bdev, AGP_I855_GCC1, 1));
		device_printf(dev, "AGP_I915_MSAC: 0x%02x\n",
		    pci_read_config(sc->bdev, AGP_I915_MSAC, 1));
		break;
	}
	device_printf(dev, "Aperture resource size: %d bytes\n",
	    AGP_GET_APERTURE(dev));
}

static int
agp_i810_attach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	const struct agp_i810_match *match;
	int error;

	sc->bdev = agp_i810_find_bridge(dev);
	if (!sc->bdev)
		return ENOENT;

	match = agp_i810_match(dev);
	sc->chiptype = match->chiptype;

	switch (sc->chiptype) {
	case CHIP_I810:
	case CHIP_I830:
	case CHIP_I855:
		sc->sc_res_spec = agp_i810_res_spec;
		agp_set_aperture_resource(dev, AGP_APBASE);
		break;
	case CHIP_I915:
	case CHIP_G33:
		sc->sc_res_spec = agp_i915_res_spec;
		agp_set_aperture_resource(dev, AGP_I915_GMADR);
		break;
	case CHIP_I965:
		sc->sc_res_spec = agp_i965_res_spec;
		agp_set_aperture_resource(dev, AGP_I915_GMADR);
		break;
	}

	error = agp_generic_attach(dev);
	if (error)
		return error;

	if (sc->chiptype != CHIP_I965 && sc->chiptype != CHIP_G33 &&
	    ptoa((vm_paddr_t)Maxmem) > 0xfffffffful)
	{
		device_printf(dev, "agp_i810.c does not support physical "
		    "memory above 4GB.\n");
		return ENOENT;
	}

	if (bus_alloc_resources(dev, sc->sc_res_spec, sc->sc_res)) {
		agp_generic_detach(dev);
		return ENODEV;
	}

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	gatt = malloc( sizeof(struct agp_gatt), M_AGP, M_NOWAIT);
	if (!gatt) {
		bus_release_resources(dev, sc->sc_res_spec, sc->sc_res);
 		agp_generic_detach(dev);
 		return ENOMEM;
	}
	sc->gatt = gatt;

	gatt->ag_entries = AGP_GET_APERTURE(dev) >> AGP_PAGE_SHIFT;

	if ( sc->chiptype == CHIP_I810 ) {
		/* Some i810s have on-chip memory called dcache */
		if (bus_read_1(sc->sc_res[0], AGP_I810_DRT) &
		    AGP_I810_DRT_POPULATED)
			sc->dcache_size = 4 * 1024 * 1024;
		else
			sc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		gatt->ag_virtual = contigmalloc( 64 * 1024, M_AGP, 0, 
					0, ~0, PAGE_SIZE, 0);
		if (!gatt->ag_virtual) {
			if (bootverbose)
				device_printf(dev, "contiguous allocation failed\n");
			bus_release_resources(dev, sc->sc_res_spec,
			    sc->sc_res);
			free(gatt, M_AGP);
			agp_generic_detach(dev);
			return ENOMEM;
		}
		bzero(gatt->ag_virtual, gatt->ag_entries * sizeof(u_int32_t));
	
		gatt->ag_physical = vtophys((vm_offset_t) gatt->ag_virtual);
		agp_flush_cache();
		/* Install the GATT. */
		bus_write_4(sc->sc_res[0], AGP_I810_PGTBL_CTL,
		    gatt->ag_physical | 1);
	} else if ( sc->chiptype == CHIP_I830 ) {
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
				bus_release_resources(dev, sc->sc_res_spec,
				    sc->sc_res);
				free(gatt, M_AGP);
				agp_generic_detach(dev);
				return EINVAL;
		}
		if (sc->stolen > 0) {
			device_printf(dev, "detected %dk stolen memory\n",
			    sc->stolen * 4);
		}
		device_printf(dev, "aperture size is %dM\n",
		    sc->initial_aperture / 1024 / 1024);

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = bus_read_4(sc->sc_res[0], AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		bus_write_4(sc->sc_res[0], AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	} else if (sc->chiptype == CHIP_I855 || sc->chiptype == CHIP_I915 ||
	    sc->chiptype == CHIP_I965 || sc->chiptype == CHIP_G33) {
		unsigned int gcc1, pgtblctl, stolen, gtt_size;

		/* Stolen memory is set up at the beginning of the aperture by
		 * the BIOS, consisting of the GATT followed by 4kb for the
		 * BIOS display.
		 */
		switch (sc->chiptype) {
		case CHIP_I855:
			gtt_size = 128;
			break;
		case CHIP_I915:
			gtt_size = 256;
			break;
		case CHIP_I965:
		case CHIP_G33:
			switch (bus_read_4(sc->sc_res[0], AGP_I810_PGTBL_CTL) &
			    AGP_I810_PGTBL_SIZE_MASK) {
			case AGP_I810_PGTBL_SIZE_128KB:
				gtt_size = 128;
				break;
			case AGP_I810_PGTBL_SIZE_256KB:
				gtt_size = 256;
				break;
			case AGP_I810_PGTBL_SIZE_512KB:
				gtt_size = 512;
				break;
			default:
				device_printf(dev, "Bad PGTBL size\n");
				bus_release_resources(dev, sc->sc_res_spec,
				    sc->sc_res);
				free(gatt, M_AGP);
				agp_generic_detach(dev);
				return EINVAL;
			}
			break;
		default:
			device_printf(dev, "Bad chiptype\n");
			bus_release_resources(dev, sc->sc_res_spec,
			    sc->sc_res);
			free(gatt, M_AGP);
			agp_generic_detach(dev);
			return EINVAL;
		}

		/* GCC1 is called MGGC on i915+ */
		gcc1 = pci_read_config(sc->bdev, AGP_I855_GCC1, 1);
		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I855_GCC1_GMS_STOLEN_1M:
			stolen = 1024;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_4M:
			stolen = 4096;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_8M:
			stolen = 8192;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_16M:
			stolen = 16384;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_32M:
			stolen = 32768;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_48M:
			stolen = 49152;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_64M:
			stolen = 65536;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_128M:
			stolen = 128 * 1024;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_256M:
			stolen = 256 * 1024;
			break;
		default:
			device_printf(dev, "unknown memory configuration, "
			    "disabling\n");
			bus_release_resources(dev, sc->sc_res_spec,
			    sc->sc_res);
			free(gatt, M_AGP);
			agp_generic_detach(dev);
			return EINVAL;
		}
		sc->stolen = (stolen - gtt_size - 4) * 1024 / 4096;
		if (sc->stolen > 0)
			device_printf(dev, "detected %dk stolen memory\n", sc->stolen * 4);
		device_printf(dev, "aperture size is %dM\n", sc->initial_aperture / 1024 / 1024);

		/* GATT address is already in there, make sure it's enabled */
		pgtblctl = bus_read_4(sc->sc_res[0], AGP_I810_PGTBL_CTL);
		pgtblctl |= 1;
		bus_write_4(sc->sc_res[0], AGP_I810_PGTBL_CTL, pgtblctl);

		gatt->ag_physical = pgtblctl & ~1;
	}

	if (0)
		agp_i810_dump_regs(dev);

	return 0;
}

static int
agp_i810_detach(device_t dev)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	agp_free_cdev(dev);

	/* Clear the GATT base. */
	if ( sc->chiptype == CHIP_I810 ) {
		bus_write_4(sc->sc_res[0], AGP_I810_PGTBL_CTL, 0);
	} else {
		unsigned int pgtblctl;
		pgtblctl = bus_read_4(sc->sc_res[0], AGP_I810_PGTBL_CTL);
		pgtblctl &= ~1;
		bus_write_4(sc->sc_res[0], AGP_I810_PGTBL_CTL, pgtblctl);
	}

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(dev, sc->initial_aperture);

	if ( sc->chiptype == CHIP_I810 ) {
		contigfree(sc->gatt->ag_virtual, 64 * 1024, M_AGP);
	}
	free(sc->gatt, M_AGP);

	bus_release_resources(dev, sc->sc_res_spec, sc->sc_res);
	agp_free_res(dev);

	return 0;
}

/**
 * Sets the PCI resource size of the aperture on i830-class and below chipsets,
 * while returning failure on later chipsets when an actual change is
 * requested.
 *
 * This whole function is likely bogus, as the kernel would probably need to
 * reconfigure the placement of the AGP aperture if a larger size is requested,
 * which doesn't happen currently.
 */
static int
agp_i810_set_aperture(device_t dev, u_int32_t aperture)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	u_int16_t miscc, gcc1;

	switch (sc->chiptype) {
	case CHIP_I810:
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
		break;
	case CHIP_I830:
		if (aperture != 64 * 1024 * 1024 &&
		    aperture != 128 * 1024 * 1024) {
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
		break;
	case CHIP_I855:
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
		return agp_generic_set_aperture(dev, aperture);
	}

	return 0;
}

/**
 * Writes a GTT entry mapping the page at the given offset from the beginning
 * of the aperture to the given physical address.
 */
static void
agp_i810_write_gtt_entry(device_t dev, int offset, vm_offset_t physical,
    int enabled)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	u_int32_t pte;

	pte = (u_int32_t)physical | 1;
	if (sc->chiptype == CHIP_I965 || sc->chiptype == CHIP_G33) {
		pte |= (physical & 0x0000000f00000000ull) >> 28;
	} else {
		/* If we do actually have memory above 4GB on an older system,
		 * crash cleanly rather than scribble on system memory,
		 * so we know we need to fix it.
		 */
		KASSERT((pte & 0x0000000f00000000ull) == 0,
		    (">4GB physical address in agp"));
	}

	switch (sc->chiptype) {
	case CHIP_I810:
	case CHIP_I830:
	case CHIP_I855:
		bus_write_4(sc->sc_res[0],
		    AGP_I810_GTT + (offset >> AGP_PAGE_SHIFT) * 4, pte);
		break;
	case CHIP_I915:
	case CHIP_G33:
		bus_write_4(sc->sc_res[1],
		    (offset >> AGP_PAGE_SHIFT) * 4, pte);
		break;
	case CHIP_I965:
		bus_write_4(sc->sc_res[0],
		    (offset >> AGP_PAGE_SHIFT) * 4 + (512 * 1024), pte);
		break;
	}
}

static int
agp_i810_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT)) {
		device_printf(dev, "failed: offset is 0x%08x, shift is %d, entries is %d\n", offset, AGP_PAGE_SHIFT, sc->gatt->ag_entries);
		return EINVAL;
	}

	if ( sc->chiptype != CHIP_I810 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to bind into stolen memory");
			return EINVAL;
		}
	}

	agp_i810_write_gtt_entry(dev, offset, physical, 1);

	return 0;
}

static int
agp_i810_unbind_page(device_t dev, int offset)
{
	struct agp_i810_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	if ( sc->chiptype != CHIP_I810 ) {
		if ( (offset >> AGP_PAGE_SHIFT) < sc->stolen ) {
			device_printf(dev, "trying to unbind from stolen memory");
			return EINVAL;
		}
	}

	agp_i810_write_gtt_entry(dev, offset, 0, 0);

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
		if ( sc->chiptype != CHIP_I810 )
			return 0;
		if (size != sc->dcache_size)
			return 0;
	} else if (type == 2) {
		/*
		 * Type 2 is the contiguous physical memory type, that hands
		 * back a physical address.  This is used for cursors on i810.
		 * Hand back as many single pages with physical as the user
		 * wants, but only allow one larger allocation (ARGB cursor)
		 * for simplicity.
		 */
		if (size != AGP_PAGE_SIZE) {
			if (sc->argb_cursor != NULL)
				return 0;

			/* Allocate memory for ARGB cursor, if we can. */
			sc->argb_cursor = contigmalloc(size, M_AGP,
			   0, 0, ~0, PAGE_SIZE, 0);
			if (sc->argb_cursor == NULL)
				return 0;
		}
	}

	mem = malloc(sizeof *mem, M_AGP, M_WAITOK);
	mem->am_id = sc->agp.as_nextid++;
	mem->am_size = size;
	mem->am_type = type;
	if (type != 1 && (type != 2 || size == AGP_PAGE_SIZE))
		mem->am_obj = vm_object_allocate(OBJT_DEFAULT,
						 atop(round_page(size)));
	else
		mem->am_obj = 0;

	if (type == 2) {
		if (size == AGP_PAGE_SIZE) {
			/*
			 * Allocate and wire down the page now so that we can
			 * get its physical address.
			 */
			vm_page_t m;
	
			VM_OBJECT_LOCK(mem->am_obj);
			m = vm_page_grab(mem->am_obj, 0, VM_ALLOC_NOBUSY |
			    VM_ALLOC_WIRED | VM_ALLOC_ZERO | VM_ALLOC_RETRY);
			VM_OBJECT_UNLOCK(mem->am_obj);
			mem->am_physical = VM_PAGE_TO_PHYS(m);
		} else {
			/* Our allocation is already nicely wired down for us.
			 * Just grab the physical address.
			 */
			mem->am_physical = vtophys(sc->argb_cursor);
		}
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
		if (mem->am_size == AGP_PAGE_SIZE) {
			/*
			 * Unwire the page which we wired in alloc_memory.
			 */
			vm_page_t m;
	
			VM_OBJECT_LOCK(mem->am_obj);
			m = vm_page_lookup(mem->am_obj, 0);
			VM_OBJECT_UNLOCK(mem->am_obj);
			vm_page_lock_queues();
			vm_page_unwire(m, 0);
			vm_page_unlock_queues();
		} else {
			contigfree(sc->argb_cursor, mem->am_size, M_AGP);
			sc->argb_cursor = NULL;
		}
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

	/* Do some sanity checks first. */
	if (offset < 0 || (offset & (AGP_PAGE_SIZE - 1)) != 0 ||
	    offset + mem->am_size > AGP_GET_APERTURE(dev)) {
		device_printf(dev, "binding memory at bad offset %#x\n",
		    (int)offset);
		return EINVAL;
	}

	if (mem->am_type == 2 && mem->am_size != AGP_PAGE_SIZE) {
		mtx_lock(&sc->agp.as_lock);
		if (mem->am_is_bound) {
			mtx_unlock(&sc->agp.as_lock);
			return EINVAL;
		}
		/* The memory's already wired down, just stick it in the GTT. */
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
			agp_i810_write_gtt_entry(dev, offset + i,
			    mem->am_physical + i, 1);
		}
		agp_flush_cache();
		mem->am_offset = offset;
		mem->am_is_bound = 1;
		mtx_unlock(&sc->agp.as_lock);
		return 0;
	}

	if (mem->am_type != 1)
		return agp_generic_bind_memory(dev, mem, offset);

	if ( sc->chiptype != CHIP_I810 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		bus_write_4(sc->sc_res[0],
		    AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, i | 3);
	}

	return 0;
}

static int
agp_i810_unbind_memory(device_t dev, struct agp_memory *mem)
{
	struct agp_i810_softc *sc = device_get_softc(dev);
	vm_offset_t i;

	if (mem->am_type == 2 && mem->am_size != AGP_PAGE_SIZE) {
		mtx_lock(&sc->agp.as_lock);
		if (!mem->am_is_bound) {
			mtx_unlock(&sc->agp.as_lock);
			return EINVAL;
		}

		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
			agp_i810_write_gtt_entry(dev, mem->am_offset + i,
			    0, 0);
		}
		agp_flush_cache();
		mem->am_is_bound = 0;
		mtx_unlock(&sc->agp.as_lock);
		return 0;
	}

	if (mem->am_type != 1)
		return agp_generic_unbind_memory(dev, mem);

	if ( sc->chiptype != CHIP_I810 )
		return EINVAL;

	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		bus_write_4(sc->sc_res[0],
		    AGP_I810_GTT + (i >> AGP_PAGE_SHIFT) * 4, 0);
	}

	return 0;
}

static device_method_t agp_i810_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	agp_i810_identify),
	DEVMETHOD(device_probe,		agp_i810_probe),
	DEVMETHOD(device_attach,	agp_i810_attach),
	DEVMETHOD(device_detach,	agp_i810_detach),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_generic_get_aperture),
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

DRIVER_MODULE(agp_i810, vgapci, agp_i810_driver, agp_devclass, 0, 0);
MODULE_DEPEND(agp_i810, agp, 1, 1, 1);
MODULE_DEPEND(agp_i810, pci, 1, 1, 1);
