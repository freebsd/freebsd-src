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
    void	*hdrspec;	/* pointer to header type specific data */

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

    u_int8_t	secondarybus;	/* bus on secondary side of bridge, if any */
    u_int8_t	subordinatebus;	/* topmost bus number behind bridge, if any */
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

/* externally visible functions */

const char *pci_ata_match(struct device *dev);
const char *pci_usb_match(struct device *dev);
const char *pci_vga_match(struct device *dev);
const char *pci_chip_match(struct device *dev);

/* low level PCI config register functions provided by pcibus.c */

int pci_cfgread (pcicfgregs *cfg, int reg, int bytes);
void pci_cfgwrite (pcicfgregs *cfg, int reg, int data, int bytes);

#ifdef __alpha__
vm_offset_t pci_cvt_to_dense (vm_offset_t);
vm_offset_t pci_cvt_to_bwx (vm_offset_t);
#endif /* __alpha__ */

/* low level devlist operations for the 2.2 compatibility code in pci.c */
pcicfgregs * pci_devlist_get_parent(pcicfgregs *cfg);

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
	PCI_IVAR_SECONDARYBUS,
	PCI_IVAR_SUBORDINATEBUS,
};

/*
 * Simplified accessors for pci devices
 */
#define PCI_ACCESSOR(A, B, T)						\
									\
static __inline T pci_get_ ## A(device_t dev)				\
{									\
	uintptr_t v;							\
	BUS_READ_IVAR(device_get_parent(dev), dev, PCI_IVAR_ ## B, &v);	\
	return (T) v;							\
}									\
									\
static __inline void pci_set_ ## A(device_t dev, T t)			\
{									\
	uintptr_t v = (uintptr_t) t;					\
	BUS_WRITE_IVAR(device_get_parent(dev), dev, PCI_IVAR_ ## B, v);	\
}

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
PCI_ACCESSOR(secondarybus,	SECONDARYBUS,	u_int8_t)
PCI_ACCESSOR(subordinatebus,	SUBORDINATEBUS,	u_int8_t)

#undef PCI_ACCESSOR

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
	BUS_READ_IVAR(device_get_parent(dev), dev, PCIB_IVAR_ ## B, &v); \
	return (T) v;							 \
}									 \
									 \
static __inline void pcib_set_ ## A(device_t dev, T t)			 \
{									 \
	uintptr_t v = (uintptr_t) t;					 \
	BUS_WRITE_IVAR(device_get_parent(dev), dev, PCIB_IVAR_ ## B, v); \
}

PCIB_ACCESSOR(bus,		BUS,		u_int32_t)

#undef PCIB_ACCESSOR

#endif

/* for compatibility to FreeBSD-2.2 and 3.x versions of PCI code */

#if defined(_KERNEL) && !defined(KLD_MODULE)
#include "opt_compat_oldpci.h"
#endif

#ifdef COMPAT_OLDPCI

/* all this is going some day */

typedef pcicfgregs *pcici_t;
typedef unsigned pcidi_t;
typedef void pci_inthand_t(void *arg);

#define pci_max_burst_len (3)

/* just copied from old PCI code for now ... */

struct pci_device {
    char*    pd_name;
    const char*  (*pd_probe ) (pcici_t tag, pcidi_t type);
    void   (*pd_attach) (pcici_t tag, int     unit);
    u_long  *pd_count;
    int    (*pd_shutdown) (int, int);
};

#ifdef __i386__
typedef u_short pci_port_t;
#else
typedef u_int pci_port_t;
#endif

u_long pci_conf_read (pcici_t tag, u_long reg);
void pci_conf_write (pcici_t tag, u_long reg, u_long data);
int pci_map_port (pcici_t tag, u_long reg, pci_port_t* pa);
int pci_map_mem (pcici_t tag, u_long reg, vm_offset_t* va, vm_offset_t* pa);
int pci_map_int (pcici_t tag, pci_inthand_t *handler, void *arg,
		 intrmask_t *maskptr);
int pci_map_int_right(pcici_t cfg, pci_inthand_t *handler, void *arg,
		      intrmask_t *maskptr, u_int flags);
int pci_unmap_int (pcici_t tag);

pcici_t pci_get_parent_from_tag(pcici_t tag);
int     pci_get_bus_from_tag(pcici_t tag);

struct module;
int compat_pci_handler (struct module *, int, void *);
#define COMPAT_PCI_DRIVER(name, pcidata)				\
static moduledata_t name##_mod = {					\
	#name,								\
	compat_pci_handler,						\
	&pcidata							\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_ANY)
#endif /* COMPAT_OLDPCI */

#endif /* _PCIVAR_H_ */
