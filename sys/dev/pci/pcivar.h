/*-
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
#define	_PCIVAR_H_

#include <sys/queue.h>

/* some PCI bus constants */
#define	PCI_MAXMAPS_0	6	/* max. no. of memory/port maps */
#define	PCI_MAXMAPS_1	2	/* max. no. of maps for PCI to PCI bridge */
#define	PCI_MAXMAPS_2	1	/* max. no. of maps for CardBus bridge */

typedef uint64_t pci_addr_t;

/* Interesting values for PCI power management */
struct pcicfg_pp {
    uint16_t	pp_cap;		/* PCI power management capabilities */
    uint8_t	pp_status;	/* config space address of PCI power status reg */
    uint8_t	pp_pmcsr;	/* config space address of PMCSR reg */
    uint8_t	pp_data;	/* config space address of PCI power data reg */
};
 
struct vpd_readonly {
    char	keyword[2];
    char	*value;
};

struct vpd_write {
    char	keyword[2];
    char	*value;
    int 	start;
    int 	len;
};

struct pcicfg_vpd {
    uint8_t	vpd_reg;	/* base register, + 2 for addr, + 4 data */
    char	vpd_cached;
    char	*vpd_ident;	/* string identifier */
    int 	vpd_rocnt;
    struct vpd_readonly *vpd_ros;
    int 	vpd_wcnt;
    struct vpd_write *vpd_w;
};

/* Interesting values for PCI MSI */
struct pcicfg_msi {
    uint16_t	msi_ctrl;	/* Message Control */
    uint8_t	msi_location;	/* Offset of MSI capability registers. */
    uint8_t	msi_msgnum;	/* Number of messages */
    int		msi_alloc;	/* Number of allocated messages. */
    uint64_t	msi_addr;	/* Contents of address register. */
    uint16_t	msi_data;	/* Contents of data register. */
    u_int	msi_handlers;
};

/* Interesting values for PCI MSI-X */
struct msix_vector {
    uint64_t	mv_address;	/* Contents of address register. */
    uint32_t	mv_data;	/* Contents of data register. */
    int		mv_irq;
};

struct msix_table_entry {
    u_int	mte_vector;	/* 1-based index into msix_vectors array. */
    u_int	mte_handlers;
};

struct pcicfg_msix {
    uint16_t	msix_ctrl;	/* Message Control */
    uint16_t	msix_msgnum;	/* Number of messages */
    uint8_t	msix_location;	/* Offset of MSI-X capability registers. */
    uint8_t	msix_table_bar;	/* BAR containing vector table. */
    uint8_t	msix_pba_bar;	/* BAR containing PBA. */
    uint32_t	msix_table_offset;
    uint32_t	msix_pba_offset;
    int		msix_alloc;	/* Number of allocated vectors. */
    int		msix_table_len;	/* Length of virtual table. */
    struct msix_table_entry *msix_table; /* Virtual table. */
    struct msix_vector *msix_vectors;	/* Array of allocated vectors. */
    struct resource *msix_table_res;	/* Resource containing vector table. */
    struct resource *msix_pba_res;	/* Resource containing PBA. */
};

/* Interesting values for HyperTransport */
struct pcicfg_ht {
    uint8_t	ht_msimap;	/* Offset of MSI mapping cap registers. */
    uint16_t	ht_msictrl;	/* MSI mapping control */
    uint64_t	ht_msiaddr;	/* MSI mapping base address */
};

/* config header information common to all header types */
typedef struct pcicfg {
    struct device *dev;		/* device which owns this */

    uint32_t	bar[PCI_MAXMAPS_0]; /* BARs */
    uint32_t	bios;		/* BIOS mapping */

    uint16_t	subvendor;	/* card vendor ID */
    uint16_t	subdevice;	/* card device ID, assigned by card vendor */
    uint16_t	vendor;		/* chip vendor ID */
    uint16_t	device;		/* chip device ID, assigned by chip vendor */

    uint16_t	cmdreg;		/* disable/enable chip and PCI options */
    uint16_t	statreg;	/* supported PCI features and error state */

    uint8_t	baseclass;	/* chip PCI class */
    uint8_t	subclass;	/* chip PCI subclass */
    uint8_t	progif;		/* chip PCI programming interface */
    uint8_t	revid;		/* chip revision ID */

    uint8_t	hdrtype;	/* chip config header type */
    uint8_t	cachelnsz;	/* cache line size in 4byte units */
    uint8_t	intpin;		/* PCI interrupt pin */
    uint8_t	intline;	/* interrupt line (IRQ for PC arch) */

    uint8_t	mingnt;		/* min. useful bus grant time in 250ns units */
    uint8_t	maxlat;		/* max. tolerated bus grant latency in 250ns */
    uint8_t	lattimer;	/* latency timer in units of 30ns bus cycles */

    uint8_t	mfdev;		/* multi-function device (from hdrtype reg) */
    uint8_t	nummaps;	/* actual number of PCI maps used */

    uint32_t	domain;		/* PCI domain */
    uint8_t	bus;		/* config space bus address */
    uint8_t	slot;		/* config space slot address */
    uint8_t	func;		/* config space function number */

    struct pcicfg_pp pp;	/* pci power management */
    struct pcicfg_vpd vpd;	/* pci vital product data */
    struct pcicfg_msi msi;	/* pci msi */
    struct pcicfg_msix msix;	/* pci msi-x */
    struct pcicfg_ht ht;	/* HyperTransport */
} pcicfgregs;

/* additional type 1 device config header information (PCI to PCI bridge) */

#define	PCI_PPBMEMBASE(h,l)  ((((pci_addr_t)(h) << 32) + ((l)<<16)) & ~0xfffff)
#define	PCI_PPBMEMLIMIT(h,l) ((((pci_addr_t)(h) << 32) + ((l)<<16)) | 0xfffff)
#define	PCI_PPBIOBASE(h,l)   ((((h)<<16) + ((l)<<8)) & ~0xfff)
#define	PCI_PPBIOLIMIT(h,l)  ((((h)<<16) + ((l)<<8)) | 0xfff)

typedef struct {
    pci_addr_t	pmembase;	/* base address of prefetchable memory */
    pci_addr_t	pmemlimit;	/* topmost address of prefetchable memory */
    uint32_t	membase;	/* base address of memory window */
    uint32_t	memlimit;	/* topmost address of memory window */
    uint32_t	iobase;		/* base address of port window */
    uint32_t	iolimit;	/* topmost address of port window */
    uint16_t	secstat;	/* secondary bus status register */
    uint16_t	bridgectl;	/* bridge control register */
    uint8_t	seclat;		/* CardBus latency timer */
} pcih1cfgregs;

/* additional type 2 device config header information (CardBus bridge) */

typedef struct {
    uint32_t	membase0;	/* base address of memory window */
    uint32_t	memlimit0;	/* topmost address of memory window */
    uint32_t	membase1;	/* base address of memory window */
    uint32_t	memlimit1;	/* topmost address of memory window */
    uint32_t	iobase0;	/* base address of port window */
    uint32_t	iolimit0;	/* topmost address of port window */
    uint32_t	iobase1;	/* base address of port window */
    uint32_t	iolimit1;	/* topmost address of port window */
    uint32_t	pccardif;	/* PC Card 16bit IF legacy more base addr. */
    uint16_t	secstat;	/* secondary bus status register */
    uint16_t	bridgectl;	/* bridge control register */
    uint8_t	seclat;		/* CardBus latency timer */
} pcih2cfgregs;

extern uint32_t pci_numdevs;

/* Only if the prerequisites are present */
#if defined(_SYS_BUS_H_) && defined(_SYS_PCIIO_H_)
struct pci_devinfo {
        STAILQ_ENTRY(pci_devinfo) pci_links;
	struct resource_list resources;
	pcicfgregs		cfg;
	struct pci_conf		conf;
};
#endif

#ifdef _SYS_BUS_H_

#include "pci_if.h"

/*
 * Define pci-specific resource flags for accessing memory via dense
 * or bwx memory spaces. These flags are ignored on i386.
 */
#define	PCI_RF_DENSE	0x10000
#define	PCI_RF_BWX	0x20000

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
    PCI_IVAR_DOMAIN,
    PCI_IVAR_BUS,
    PCI_IVAR_SLOT,
    PCI_IVAR_FUNCTION,
    PCI_IVAR_ETHADDR,
    PCI_IVAR_CMDREG,
    PCI_IVAR_CACHELNSZ,
    PCI_IVAR_MINGNT,
    PCI_IVAR_MAXLAT,
    PCI_IVAR_LATTIMER,
};

/*
 * Simplified accessors for pci devices
 */
#define	PCI_ACCESSOR(var, ivar, type)					\
	__BUS_ACCESSOR(pci, var, PCI, ivar, type)

PCI_ACCESSOR(subvendor,		SUBVENDOR,	uint16_t)
PCI_ACCESSOR(subdevice,		SUBDEVICE,	uint16_t)
PCI_ACCESSOR(vendor,		VENDOR,		uint16_t)
PCI_ACCESSOR(device,		DEVICE,		uint16_t)
PCI_ACCESSOR(devid,		DEVID,		uint32_t)
PCI_ACCESSOR(class,		CLASS,		uint8_t)
PCI_ACCESSOR(subclass,		SUBCLASS,	uint8_t)
PCI_ACCESSOR(progif,		PROGIF,		uint8_t)
PCI_ACCESSOR(revid,		REVID,		uint8_t)
PCI_ACCESSOR(intpin,		INTPIN,		uint8_t)
PCI_ACCESSOR(irq,		IRQ,		uint8_t)
PCI_ACCESSOR(domain,		DOMAIN,		uint32_t)
PCI_ACCESSOR(bus,		BUS,		uint8_t)
PCI_ACCESSOR(slot,		SLOT,		uint8_t)
PCI_ACCESSOR(function,		FUNCTION,	uint8_t)
PCI_ACCESSOR(ether,		ETHADDR,	uint8_t *)
PCI_ACCESSOR(cmdreg,		CMDREG,		uint8_t)
PCI_ACCESSOR(cachelnsz,		CACHELNSZ,	uint8_t)
PCI_ACCESSOR(mingnt,		MINGNT,		uint8_t)
PCI_ACCESSOR(maxlat,		MAXLAT,		uint8_t)
PCI_ACCESSOR(lattimer,		LATTIMER,	uint8_t)

#undef PCI_ACCESSOR

/*
 * Operations on configuration space.
 */
static __inline uint32_t
pci_read_config(device_t dev, int reg, int width)
{
    return PCI_READ_CONFIG(device_get_parent(dev), dev, reg, width);
}

static __inline void
pci_write_config(device_t dev, int reg, uint32_t val, int width)
{
    PCI_WRITE_CONFIG(device_get_parent(dev), dev, reg, val, width);
}

/*
 * Ivars for pci bridges.
 */

/*typedef enum pci_device_ivars pcib_device_ivars;*/
enum pcib_device_ivars {
	PCIB_IVAR_DOMAIN,
	PCIB_IVAR_BUS
};

#define	PCIB_ACCESSOR(var, ivar, type)					 \
    __BUS_ACCESSOR(pcib, var, PCIB, ivar, type)

PCIB_ACCESSOR(domain,		DOMAIN,		uint32_t)
PCIB_ACCESSOR(bus,		BUS,		uint32_t)

#undef PCIB_ACCESSOR

/*
 * PCI interrupt validation.  Invalid interrupt values such as 0 or 128
 * on i386 or other platforms should be mapped out in the MD pcireadconf
 * code and not here, since the only MI invalid IRQ is 255.
 */
#define	PCI_INVALID_IRQ		255
#define	PCI_INTERRUPT_VALID(x)	((x) != PCI_INVALID_IRQ)

/*
 * Convenience functions.
 *
 * These should be used in preference to manually manipulating
 * configuration space.
 */
static __inline int
pci_enable_busmaster(device_t dev)
{
    return(PCI_ENABLE_BUSMASTER(device_get_parent(dev), dev));
}

static __inline int
pci_disable_busmaster(device_t dev)
{
    return(PCI_DISABLE_BUSMASTER(device_get_parent(dev), dev));
}

static __inline int
pci_enable_io(device_t dev, int space)
{
    return(PCI_ENABLE_IO(device_get_parent(dev), dev, space));
}

static __inline int
pci_disable_io(device_t dev, int space)
{
    return(PCI_DISABLE_IO(device_get_parent(dev), dev, space));
}

static __inline int
pci_get_vpd_ident(device_t dev, const char **identptr)
{
    return(PCI_GET_VPD_IDENT(device_get_parent(dev), dev, identptr));
}

static __inline int
pci_get_vpd_readonly(device_t dev, const char *kw, const char **identptr)
{
    return(PCI_GET_VPD_READONLY(device_get_parent(dev), dev, kw, identptr));
}

/*
 * Check if the address range falls within the VGA defined address range(s)
 */
static __inline int
pci_is_vga_ioport_range(u_long start, u_long end)
{
 
	return (((start >= 0x3b0 && end <= 0x3bb) ||
	    (start >= 0x3c0 && end <= 0x3df)) ? 1 : 0);
}

static __inline int
pci_is_vga_memory_range(u_long start, u_long end)
{

	return ((start >= 0xa0000 && end <= 0xbffff) ? 1 : 0);
}

/*
 * PCI power states are as defined by ACPI:
 *
 * D0	State in which device is on and running.  It is receiving full
 *	power from the system and delivering full functionality to the user.
 * D1	Class-specific low-power state in which device context may or may not
 *	be lost.  Buses in D1 cannot do anything to the bus that would force
 *	devices on that bus to lose context.
 * D2	Class-specific low-power state in which device context may or may
 *	not be lost.  Attains greater power savings than D1.  Buses in D2
 *	can cause devices on that bus to lose some context.  Devices in D2
 *	must be prepared for the bus to be in D2 or higher.
 * D3	State in which the device is off and not running.  Device context is
 *	lost.  Power can be removed from the device.
 */
#define	PCI_POWERSTATE_D0	0
#define	PCI_POWERSTATE_D1	1
#define	PCI_POWERSTATE_D2	2
#define	PCI_POWERSTATE_D3	3
#define	PCI_POWERSTATE_UNKNOWN	-1

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

static __inline int
pci_find_extcap(device_t dev, int capability, int *capreg)
{
    return PCI_FIND_EXTCAP(device_get_parent(dev), dev, capability, capreg);
}

static __inline int
pci_alloc_msi(device_t dev, int *count)
{
    return (PCI_ALLOC_MSI(device_get_parent(dev), dev, count));
}

static __inline int
pci_alloc_msix(device_t dev, int *count)
{
    return (PCI_ALLOC_MSIX(device_get_parent(dev), dev, count));
}

static __inline int
pci_remap_msix(device_t dev, int count, const u_int *vectors)
{
    return (PCI_REMAP_MSIX(device_get_parent(dev), dev, count, vectors));
}

static __inline int
pci_release_msi(device_t dev)
{
    return (PCI_RELEASE_MSI(device_get_parent(dev), dev));
}

static __inline int
pci_msi_count(device_t dev)
{
    return (PCI_MSI_COUNT(device_get_parent(dev), dev));
}

static __inline int
pci_msix_count(device_t dev)
{
    return (PCI_MSIX_COUNT(device_get_parent(dev), dev));
}

device_t pci_find_bsf(uint8_t, uint8_t, uint8_t);
device_t pci_find_dbsf(uint32_t, uint8_t, uint8_t, uint8_t);
device_t pci_find_device(uint16_t, uint16_t);

/*
 * Can be used by MD code to request the PCI bus to re-map an MSI or
 * MSI-X message.
 */
int	pci_remap_msi_irq(device_t dev, u_int irq);

/* Can be used by drivers to manage the MSI-X table. */
int	pci_pending_msix(device_t dev, u_int index);

int	pci_msi_device_blacklisted(device_t dev);

void	pci_ht_map_msi(device_t dev, uint64_t addr);

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
extern uint32_t	pci_generation;

#endif /* _PCIVAR_H_ */
