/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 *
 */

#ifndef _PCIVAR_H_
#define _PCIVAR_H_

#include <sys/queue.h>

/* some PCI bus constants */

#define PCI_BUSMAX	255	/* highest supported bus number */
#define PCI_SLOTMAX	31	/* highest supported slot number */
#define PCI_FUNCMAX	7	/* highest supported function number */
#define PCI_REGMAX	255	/* highest supported config register addr. */

#define PCI_MAXMAPS_0	6	/* max. no. of memory/port maps */
#define PCI_MAXMAPS_1	2	/* max. no. of maps for PCI to PCI bridge */
#define PCI_MAXMAPS_2	1	/* max. no. of maps for CardBus bridge */

/* pci_addr_t covers this system's PCI bus address space: 32 or 64 bit */

#ifdef PCI_A64
typedef u_int64_t pci_addr_t;	/* u_int64_t for system with 64bit addresses */
#else
typedef u_int32_t pci_addr_t;	/* u_int64_t for system with 64bit addresses */
#endif

/* config header information common to all header types */

typedef struct pcicfg {
    struct device *dev;		/* device which owns this */

    u_int16_t	subvendor;	/* card vendor ID */
    u_int16_t	subdevice;	/* card device ID, assigned by card vendor */
    u_int16_t	vendor;		/* chip vendor ID */
    u_int16_t	device;		/* chip device ID, assigned by chip vendor */

    u_int16_t	cmdreg;		/* disable/enable chip and PCI options */
    u_int16_t	statreg;	/* supported PCI features and error state */

    u_int8_t	baseclass;	/* chip PCI class */
    u_int8_t	subclass;	/* chip PCI subclass */
    u_int8_t	progif;		/* chip PCI programming interface */
    u_int8_t	revid;		/* chip revision ID */

    u_int8_t	hdrtype;	/* chip config header type */
    u_int8_t	cachelnsz;	/* cache line size in 4byte units */
    u_int8_t	intpin;		/* PCI interrupt pin */
    u_int8_t	intline;	/* interrupt line (IRQ for PC arch) */

    u_int8_t	mingnt;		/* min. useful bus grant time in 250ns units */
    u_int8_t	maxlat;		/* max. tolerated bus grant latency in 250ns */
    u_int8_t	lattimer;	/* latency timer in units of 30ns bus cycles */

    u_int8_t	mfdev;		/* multi-function device (from hdrtype reg) */
    u_int8_t	nummaps;	/* actual number of PCI maps used */

    u_int8_t	bus;		/* config space bus address */
    u_int8_t	slot;		/* config space slot address */
    u_int8_t	func;		/* config space function number */

    u_int16_t	pp_cap;		/* PCI power management capabilities */
    u_int8_t	pp_status;	/* config space address of PCI power status reg */
    u_int8_t	pp_pmcsr;	/* config space address of PMCSR reg */
    u_int8_t	pp_data;	/* config space address of PCI power data reg */
    
} pcicfgregs;

/* additional type 1 device config header information (PCI to PCI bridge) */

#ifdef PCI_A64
#define PCI_PPBMEMBASE(h,l)  ((((pci_addr_t)(h) << 32) + ((l)<<16)) & ~0xfffff)
#define PCI_PPBMEMLIMIT(h,l) ((((pci_addr_t)(h) << 32) + ((l)<<16)) | 0xfffff)
#else
#define PCI_PPBMEMBASE(h,l)  (((l)<<16) & ~0xfffff)
#define PCI_PPBMEMLIMIT(h,l) (((l)<<16) | 0xfffff)
#endif /* PCI_A64 */

#define PCI_PPBIOBASE(h,l)   ((((h)<<16) + ((l)<<8)) & ~0xfff)
#define PCI_PPBIOLIMIT(h,l)  ((((h)<<16) + ((l)<<8)) | 0xfff)

typedef struct {
    pci_addr_t	pmembase;	/* base address of prefetchable memory */
    pci_addr_t	pmemlimit;	/* topmost address of prefetchable memory */
    u_int32_t	membase;	/* base address of memory window */
    u_int32_t	memlimit;	/* topmost address of memory window */
    u_int32_t	iobase;		/* base address of port window */
    u_int32_t	iolimit;	/* topmost address of port window */
    u_int16_t	secstat;	/* secondary bus status register */
    u_int16_t	bridgectl;	/* bridge control register */
    u_int8_t	seclat;		/* CardBus latency timer */
} pcih1cfgregs;

/* additional type 2 device config header information (CardBus bridge) */

typedef struct {
    u_int32_t	membase0;	/* base address of memory window */
    u_int32_t	memlimit0;	/* topmost address of memory window */
    u_int32_t	membase1;	/* base address of memory window */
    u_int32_t	memlimit1;	/* topmost address of memory window */
    u_int32_t	iobase0;	/* base address of port window */
    u_int32_t	iolimit0;	/* topmost address of port window */
    u_int32_t	iobase1;	/* base address of port window */
    u_int32_t	iolimit1;	/* topmost address of port window */
    u_int32_t	pccardif;	/* PC Card 16bit IF legacy more base addr. */
    u_int16_t	secstat;	/* secondary bus status register */
    u_int16_t	bridgectl;	/* bridge control register */
    u_int8_t	seclat;		/* CardBus latency timer */
} pcih2cfgregs;

extern u_int32_t pci_numdevs;

/* Only if the prerequisites are present */
#if defined(_SYS_BUS_H_) && defined(_SYS_PCIIO_H_)
struct pci_devinfo {
        STAILQ_ENTRY(pci_devinfo) pci_links;
	struct resource_list resources;
	pcicfgregs		cfg;
	struct pci_conf		conf;
};
#endif

#ifdef __alpha__
vm_offset_t pci_cvt_to_dense (vm_offset_t);
vm_offset_t pci_cvt_to_bwx (vm_offset_t);
#endif /* __alpha__ */

#ifdef _SYS_BUS_H_

#include "pci_if.h"

/*
 * Define pci-specific resource flags for accessing memory via dense
 * or bwx memory spaces. These flags are ignored on i386.
 */
#define PCI_RF_DENSE	0x10000
#define PCI_RF_BWX	0x20000

enum pci_device_ivars {
    PCI_IVAR_SUBVENDOR,
    PCI_IVAR_SUBDEVICE,
    PCI_IVAR_VENDOR,
    PCI_IVAR_DEVICE,
    PCI_IVAR_DEVID,
    PCI_IVAR_CLASS,
    PCI_IVAR_SUBCLASS,
    PCI_IVAR_PROGIF,
    PCI_IVAR_REVID,
    PCI_IVAR_INTPIN,
    PCI_IVAR_IRQ,
    PCI_IVAR_BUS,
    PCI_IVAR_SLOT,
    PCI_IVAR_FUNCTION,
};

/*
 * Simplified accessors for pci devices
 */
#define PCI_ACCESSOR(var, ivar, type)						\
	__BUS_ACCESSOR(pci, var, PCI, ivar, type)

PCI_ACCESSOR(subvendor,		SUBVENDOR,	u_int16_t)
PCI_ACCESSOR(subdevice,		SUBDEVICE,	u_int16_t)
PCI_ACCESSOR(vendor,		VENDOR,		u_int16_t)
PCI_ACCESSOR(device,		DEVICE,		u_int16_t)
PCI_ACCESSOR(devid,		DEVID,		u_int32_t)
PCI_ACCESSOR(class,		CLASS,		u_int8_t)
PCI_ACCESSOR(subclass,		SUBCLASS,	u_int8_t)
PCI_ACCESSOR(progif,		PROGIF,		u_int8_t)
PCI_ACCESSOR(revid,		REVID,		u_int8_t)
PCI_ACCESSOR(intpin,		INTPIN,		u_int8_t)
PCI_ACCESSOR(irq,		IRQ,		u_int8_t)
PCI_ACCESSOR(bus,		BUS,		u_int8_t)
PCI_ACCESSOR(slot,		SLOT,		u_int8_t)
PCI_ACCESSOR(function,		FUNCTION,	u_int8_t)

#undef PCI_ACCESSOR

/*
 * Operations on configuration space.
 */
static __inline u_int32_t
pci_read_config(device_t dev, int reg, int width)
{
    return PCI_READ_CONFIG(device_get_parent(dev), dev, reg, width);
}

static __inline void
pci_write_config(device_t dev, int reg, u_int32_t val, int width)
{
    PCI_WRITE_CONFIG(device_get_parent(dev), dev, reg, val, width);
}

/*
 * Ivars for pci bridges.
 */

/*typedef enum pci_device_ivars pcib_device_ivars;*/
enum pcib_device_ivars {
	PCIB_IVAR_BUS
};

#define PCIB_ACCESSOR(A, B, T)						 \
									 \
static __inline T pcib_get_ ## A(device_t dev)				 \
{									 \
    uintptr_t v;							 \
    BUS_READ_IVAR(device_get_parent(dev), dev, PCIB_IVAR_ ## B, &v);	 \
    return (T) v;							 \
}									 \
									 \
static __inline void pcib_set_ ## A(device_t dev, T t)			 \
{									 \
    uintptr_t v = (uintptr_t) t;					 \
    BUS_WRITE_IVAR(device_get_parent(dev), dev, PCIB_IVAR_ ## B, v);	 \
}

PCIB_ACCESSOR(bus,		BUS,		u_int32_t)

#undef PCIB_ACCESSOR

/*
 * PCI interrupt validation.
 */
#define PCI_INTERRUPT_VALID(x)	(((x) != 0) && ((x) != 255))

/*
 * Convenience functions.
 *
 * These should be used in preference to manually manipulating
 * configuration space.
 */
static __inline void
pci_enable_busmaster(device_t dev)
{
    PCI_ENABLE_BUSMASTER(device_get_parent(dev), dev);
}

static __inline void
pci_disable_busmaster(device_t dev)
{
    PCI_DISABLE_BUSMASTER(device_get_parent(dev), dev);
}

static __inline void
pci_enable_io(device_t dev, int space)
{
    PCI_ENABLE_IO(device_get_parent(dev), dev, space);
}

static __inline void
pci_disable_io(device_t dev, int space)
{
    PCI_DISABLE_IO(device_get_parent(dev), dev, space);
}

/*
 * PCI power states are as defined by ACPI:
 *
 * D0	State in which device is on and running.  It is receiving full
 *	power from the system and delivering full functionality to the user.
 * D1	Class-specific low-power state in which device context may or may not
 *	be lost.  Buses in D1 cannot do anything to the bus that would force
 *	devices on that bus to loose context.
 * D2	Class-specific low-power state in which device context may or may
 *	not be lost.  Attains greater power savings than D1.  Buses in D2
 *	can cause devices on that bus to loose some context.  Devices in D2
 *	must be prepared for the bus to be in D2 or higher.
 * D3	State in which the device is off and not running.  Device context is
 *	lost.  Power can be removed from the device.
 */
#define PCI_POWERSTATE_D0	0
#define PCI_POWERSTATE_D1	1
#define PCI_POWERSTATE_D2	2
#define PCI_POWERSTATE_D3	3
#define PCI_POWERSTATE_UNKNOWN	-1

static __inline int
pci_set_powerstate(device_t dev, int state)
{
    return PCI_SET_POWERSTATE(device_get_parent(dev), dev, state);
}

static __inline int
pci_get_powerstate(device_t dev)
{
    return PCI_GET_POWERSTATE(device_get_parent(dev), dev);
}

device_t pci_find_bsf(u_int8_t, u_int8_t, u_int8_t);
device_t pci_find_device(u_int16_t, u_int16_t);
#endif	/* _SYS_BUS_H_ */

/*
 * cdev switch for control device, initialised in generic PCI code
 */
extern struct cdevsw pcicdev;

/*
 * List of all PCI devices, generation count for the list.
 */
STAILQ_HEAD(devlist, pci_devinfo);

extern struct devlist	pci_devq;
extern u_int32_t	pci_generation;

#endif /* _PCIVAR_H_ */
