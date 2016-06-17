/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PCIIO_PRIVATE_H
#define _ASM_SN_PCI_PCIIO_PRIVATE_H

#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pci_defs.h>

/*
 * pciio_private.h -- private definitions for pciio
 * PCI drivers should NOT include this file.
 */

#ident "sys/PCI/pciio_private: $Revision: 1.13 $"

/*
 * All PCI providers set up PIO using this information.
 */
struct pciio_piomap_s {
    unsigned                pp_flags;	/* PCIIO_PIOMAP flags */
    vertex_hdl_t            pp_dev;	/* associated pci card */
    pciio_slot_t            pp_slot;	/* which slot the card is in */
    pciio_space_t           pp_space;	/* which address space */
    iopaddr_t               pp_pciaddr;		/* starting offset of mapping */
    size_t                  pp_mapsz;	/* size of this mapping */
    caddr_t                 pp_kvaddr;	/* kernel virtual address to use */
};

/*
 * All PCI providers set up DMA using this information.
 */
struct pciio_dmamap_s {
    unsigned                pd_flags;	/* PCIIO_DMAMAP flags */
    vertex_hdl_t            pd_dev;	/* associated pci card */
    pciio_slot_t            pd_slot;	/* which slot the card is in */
};

/*
 * All PCI providers set up interrupts using this information.
 */

struct pciio_intr_s {
    unsigned                pi_flags;	/* PCIIO_INTR flags */
    vertex_hdl_t            pi_dev;	/* associated pci card */
    device_desc_t	    pi_dev_desc;	/* override device descriptor */
    pciio_intr_line_t       pi_lines;	/* which interrupt line(s) */
    intr_func_t             pi_func;	/* handler function (when connected) */
    intr_arg_t              pi_arg;	/* handler parameter (when connected) */
    cpuid_t                 pi_mustruncpu; /* Where we must run. */
    int                     pi_irq;     /* IRQ assigned */
    int                     pi_cpu;     /* cpu assigned */
};

/* PCIIO_INTR (pi_flags) flags */
#define PCIIO_INTR_CONNECTED	1	/* interrupt handler/thread has been connected */
#define PCIIO_INTR_NOTHREAD	2	/* interrupt handler wants to be called at interrupt level */

/*
 * Some PCI provider implementations keep track of PCI window Base Address
 * Register (BAR) address range assignment via the rmalloc()/rmfree() arena
 * management routines.  These implementations use the following data
 * structure for each allocation address space (e.g. memory, I/O, small
 * window, etc.).
 *
 * The ``page size'' encodes the minimum allocation unit and must be a power
 * of 2.  The main use of this allocation ``page size'' is to control the
 * number of free address ranges that the mapping allocation software will
 * need to track.  Smaller values will allow more efficient use of the address
 * ranges but will result in much larger allocation map structures ...  For
 * instance, if we want to manage allocations for a 256MB address range,
 * choosing a 1MB allocation page size will result in up to 1MB being wasted
 * for allocation requests smaller than 1MB.  The worst case allocation
 * pattern for the allocation software to track would be a pattern of 1MB
 * allocated, 1MB free.  This results in the need to track up to 128 free
 * ranges.
 */
struct pciio_win_map_s {
	struct map	*wm_map;	/* window address map */
	int		wm_page_size;	/* allocation ``page size'' */
};

/*
 * Opaque structure used to keep track of window allocation information.
 */
struct pciio_win_alloc_s {
	struct resource *wa_resource;   /* window map allocation resource */
	unsigned long	wa_base;	/* allocation starting page number */
	size_t		wa_pages;	/* number of pages in allocation */
};

/*
 * Each PCI Card has one of these.
 */

struct pciio_info_s {
    char                   *c_fingerprint;
    vertex_hdl_t            c_vertex;	/* back pointer to vertex */
    pciio_bus_t             c_bus;	/* which bus the card is in */
    pciio_slot_t            c_slot;	/* which slot the card is in */
    pciio_function_t        c_func;	/* which func (on multi-func cards) */
    pciio_vendor_id_t       c_vendor;	/* PCI card "vendor" code */
    pciio_device_id_t       c_device;	/* PCI card "device" code */
    vertex_hdl_t            c_master;	/* PCI bus provider */
    arbitrary_info_t        c_mfast;	/* cached fastinfo from c_master */
    pciio_provider_t       *c_pops;	/* cached provider from c_master */
    error_handler_f        *c_efunc;	/* error handling function */
    error_handler_arg_t     c_einfo;	/* first parameter for efunc */

    struct pciio_win_info_s {           /* state of BASE regs */
        pciio_space_t           w_space;
        iopaddr_t               w_base;
        size_t                  w_size;
        int                     w_devio_index;   /* DevIO[] register used to
                                                    access this window */
	struct pciio_win_alloc_s w_win_alloc;    /* window allocation cookie */
    }                       c_window[PCI_CFG_BASE_ADDRS + 1];
#define c_rwindow	c_window[PCI_CFG_BASE_ADDRS]	/* EXPANSION ROM window */
#define c_rbase		c_rwindow.w_base		/* EXPANSION ROM base addr */
#define c_rsize		c_rwindow.w_size		/* EXPANSION ROM size (bytes) */
    pciio_piospace_t	    c_piospace;	/* additional I/O spaces allocated */
};

extern char             pciio_info_fingerprint[];
#endif				/* _ASM_SN_PCI_PCIIO_PRIVATE_H */
