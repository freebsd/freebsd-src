/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PCIIO_H
#define _ASM_SN_PCI_PCIIO_H

/*
 * pciio.h -- platform-independent PCI interface
 */

#ifdef __KERNEL__
#include <linux/config.h>
#include <linux/ioport.h>
#include <asm/sn/ioerror.h>
#include <asm/sn/driver.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#else
#include <linux/config.h>
#include <linux/ioport.h>
#include <ioerror.h>
#include <driver.h>
#include <hcl.h>
#endif

#ifndef __ASSEMBLY__

#ifdef __KERNEL__
#include <asm/sn/dmamap.h>
#include <asm/sn/alenlist.h>
#else
#include <dmamap.h>
#include <alenlist.h>
#endif

typedef int pciio_vendor_id_t;

#define PCIIO_VENDOR_ID_NONE	(-1)

typedef int pciio_device_id_t;

#define PCIIO_DEVICE_ID_NONE	(-1)

typedef uint8_t pciio_bus_t;       /* PCI bus number (0..255) */
typedef uint8_t pciio_slot_t;      /* PCI slot number (0..31, 255) */
typedef uint8_t pciio_function_t;  /* PCI func number (0..7, 255) */

#define	PCIIO_SLOTS		((pciio_slot_t)32)
#define	PCIIO_FUNCS		((pciio_function_t)8)

#define	PCIIO_SLOT_NONE		((pciio_slot_t)255)
#define	PCIIO_FUNC_NONE		((pciio_function_t)255)

typedef int pciio_intr_line_t;		/* PCI interrupt line(s) */

#define PCIIO_INTR_LINE(n)      (0x1 << (n))
#define PCIIO_INTR_LINE_A	(0x1)
#define PCIIO_INTR_LINE_B	(0x2)
#define PCIIO_INTR_LINE_C	(0x4)
#define PCIIO_INTR_LINE_D	(0x8)

typedef int pciio_space_t;		/* PCI address space designation */

#define PCIIO_SPACE_NONE	(0)
#define	PCIIO_SPACE_ROM		(1)
#define PCIIO_SPACE_IO		(2)
/*	PCIIO_SPACE_		(3) */
#define PCIIO_SPACE_MEM		(4)
#define PCIIO_SPACE_MEM32	(5)
#define PCIIO_SPACE_MEM64	(6)
#define PCIIO_SPACE_CFG		(7)
#define PCIIO_SPACE_WIN0	(8)
#define PCIIO_SPACE_WIN(n)	(PCIIO_SPACE_WIN0+(n))	/* 8..13 */
/*	PCIIO_SPACE_		(14) */
#define PCIIO_SPACE_BAD		(15)

#if 1	/* does anyone really use these? */
#define PCIIO_SPACE_USER0	(20)
#define PCIIO_SPACE_USER(n)	(PCIIO_SPACE_USER0+(n))	/* 20 .. ? */
#endif

/*
 * PCI_NOWHERE is the error value returned in
 * place of a PCI address when there is no
 * corresponding address.
 */
#define	PCI_NOWHERE		(0)

/*
 *    Acceptable flag bits for pciio service calls
 *
 * PCIIO_FIXED: require that mappings be established
 *	using fixed sharable resources; address
 *	translation results will be permanently
 *	available. (PIOMAP_FIXED and DMAMAP_FIXED are
 *	the same numeric value and are acceptable).
 * PCIIO_NOSLEEP: if any part of the operation would
 *	sleep waiting for resoruces, return an error
 *	instead. (PIOMAP_NOSLEEP and DMAMAP_NOSLEEP are
 *	the same numeric value and are acceptable).
 * PCIIO_INPLACE: when operating on alenlist structures,
 *	reuse the source alenlist rather than creating a
 *	new one. (PIOMAP_INPLACE and DMAMAP_INPLACE are
 *	the same numeric value and are acceptable).
 *
 * PCIIO_DMA_CMD: configure this stream as a
 *	generic "command" stream. Generally this
 *	means turn off prefetchers and write
 *	gatherers, and whatever else might be
 *	necessary to make command ring DMAs
 *	work as expected.
 * PCIIO_DMA_DATA: configure this stream as a
 *	generic "data" stream. Generally, this
 *	means turning on prefetchers and write
 *	gatherers, and anything else that might
 *	increase the DMA throughput (short of
 *	using "high priority" or "real time"
 *	resources that may lower overall system
 *	performance).
 * PCIIO_DMA_A64: this device is capable of
 *	using 64-bit DMA addresses. Unless this
 *	flag is specified, it is assumed that
 *	the DMA address must be in the low 4G
 *	of PCI space.
 * PCIIO_PREFETCH: if there are prefetchers
 *	available, they can be turned on.
 * PCIIO_NOPREFETCH: any prefetchers along
 *	the dma path should be turned off.
 * PCIIO_WRITE_GATHER: if there are write gatherers
 *	available, they can be turned on.
 * PCIIO_NOWRITE_GATHER: any write gatherers along
 *	the dma path should be turned off.
 *
 * PCIIO_BYTE_STREAM: the DMA stream represents a group
 *	of ordered bytes. Arrange all byte swapping
 *	hardware so that the bytes land in the correct
 *	order. This is a common setting for data
 *	channels, but is NOT implied by PCIIO_DMA_DATA.
 * PCIIO_WORD_VALUES: the DMA stream is used to
 *	communicate quantities stored in multiple bytes,
 *	and the device doing the DMA is little-endian;
 *	arrange any swapping hardware so that
 *	32-bit-wide values are maintained. This is a
 *	common setting for command rings that contain
 *	DMA addresses and counts, but is NOT implied by
 *	PCIIO_DMA_CMD. CPU Accesses to 16-bit fields
 *	must have their address xor-ed with 2, and
 *	accesses to individual bytes must have their
 *	addresses xor-ed with 3 relative to what the
 *	device expects.
 *
 * NOTE: any "provider specific" flags that
 * conflict with the generic flags will
 * override the generic flags, locally
 * at that provider.
 *
 * Also, note that PCI-generic flags (PCIIO_) are
 * in bits 0-14. The upper bits, 15-31, are reserved
 * for PCI implementation-specific flags.
 */

#define	PCIIO_FIXED		DMAMAP_FIXED
#define	PCIIO_NOSLEEP		DMAMAP_NOSLEEP
#define	PCIIO_INPLACE		DMAMAP_INPLACE

#define PCIIO_DMA_CMD		0x0010
#define PCIIO_DMA_DATA		0x0020
#define PCIIO_DMA_A64		0x0040

#define PCIIO_WRITE_GATHER	0x0100
#define PCIIO_NOWRITE_GATHER	0x0200
#define PCIIO_PREFETCH		0x0400
#define PCIIO_NOPREFETCH	0x0800

/* Requesting an endianness setting that the
 * underlieing hardware can not support
 * WILL result in a failure to allocate
 * dmamaps or complete a dmatrans.
 */
#define	PCIIO_BYTE_STREAM	0x1000	/* set BYTE SWAP for "byte stream" */
#define	PCIIO_WORD_VALUES	0x2000	/* set BYTE SWAP for "word values" */

/*
 * Interface to deal with PCI endianness.
 * The driver calls pciio_endian_set once, supplying the actual endianness of
 * the device and the desired endianness.  On SGI systems, only use LITTLE if
 * dealing with a driver that does software swizzling.  Most of the time,
 * it's preferable to request BIG.  The return value indicates the endianness
 * that is actually achieved.  On systems that support hardware swizzling,
 * the achieved endianness will be the desired endianness.  On systems without
 * swizzle hardware, the achieved endianness will be the device's endianness.
 */
typedef enum pciio_endian_e {
    PCIDMA_ENDIAN_BIG,
    PCIDMA_ENDIAN_LITTLE
} pciio_endian_t;

/*
 * handles of various sorts
 */
typedef struct pciio_piomap_s *pciio_piomap_t;
typedef struct pciio_dmamap_s *pciio_dmamap_t;
typedef struct pciio_intr_s *pciio_intr_t;
typedef struct pciio_info_s *pciio_info_t;
typedef struct pciio_piospace_s *pciio_piospace_t;
typedef struct pciio_win_info_s *pciio_win_info_t;
typedef struct pciio_win_map_s *pciio_win_map_t;
typedef struct pciio_win_alloc_s *pciio_win_alloc_t;

/* PIO MANAGEMENT */

/*
 *    A NOTE ON PCI PIO ADDRESSES
 *
 *      PCI supports three different address spaces: CFG
 *      space, MEM space and I/O space. Further, each
 *      card always accepts CFG accesses at an address
 *      based on which slot it is attached to, but can
 *      decode up to six address ranges.
 *
 *      Assignment of the base address registers for all
 *      PCI devices is handled centrally; most commonly,
 *      device drivers will want to talk to offsets
 *      within one or another of the address ranges. In
 *      order to do this, which of these "address
 *      spaces" the PIO is directed into must be encoded
 *      in the flag word.
 *
 *      We reserve the right to defer allocation of PCI
 *      address space for a device window until the
 *      driver makes a piomap_alloc or piotrans_addr
 *      request.
 *
 *      If a device driver mucks with its device's base
 *      registers through a PIO mapping to CFG space,
 *      results of further PIO through the corresponding
 *      window are UNDEFINED.
 *
 *      Windows are named by the index in the base
 *      address register set for the device of the
 *      desired register; IN THE CASE OF 64 BIT base
 *      registers, the index should be to the word of
 *      the register that contains the mapping type
 *      bits; since the PCI CFG space is natively
 *      organized little-endian fashion, this is the
 *      first of the two words.
 *
 *      AT THE MOMENT, any required corrections for
 *      endianness are the responsibility of the device
 *      driver; not all platforms support control in
 *      hardware of byteswapping hardware. We anticipate
 *      providing flag bits to the PIO and DMA
 *      management interfaces to request different
 *      configurations of byteswapping hardware.
 *
 *      PIO Accesses to CFG space via the "Bridge" ASIC
 *      used in IP30 platforms preserve the native byte
 *      significance within the 32-bit word; byte
 *      addresses for single byte accesses need to be
 *      XORed with 3, and addresses for 16-bit accesses
 *      need to be XORed with 2.
 *
 *      The IOC3 used on IP30, and other SGI PCI devices
 *      as well, require use of 32-bit accesses to their
 *      configuration space registers. Any potential PCI
 *      bus providers need to be aware of this requirement.
 */

#define PCIIO_PIOMAP_CFG	(0x1)
#define PCIIO_PIOMAP_MEM	(0x2)
#define PCIIO_PIOMAP_IO		(0x4)
#define PCIIO_PIOMAP_WIN(n)	(0x8+(n))

typedef pciio_piomap_t
pciio_piomap_alloc_f    (vertex_hdl_t dev,	/* set up mapping for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 pciio_space_t space,	/* which address space */
			 iopaddr_t pcipio_addr,		/* starting address */
			 size_t byte_count,
			 size_t byte_count_max,		/* maximum size of a mapping */
			 unsigned flags);	/* defined in sys/pio.h */

typedef void
pciio_piomap_free_f     (pciio_piomap_t pciio_piomap);

typedef caddr_t
pciio_piomap_addr_f     (pciio_piomap_t pciio_piomap,	/* mapping resources */
			 iopaddr_t pciio_addr,	/* map for this pcipio address */
			 size_t byte_count);	/* map this many bytes */

typedef void
pciio_piomap_done_f     (pciio_piomap_t pciio_piomap);

typedef caddr_t
pciio_piotrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 pciio_space_t space,	/* which address space */
			 iopaddr_t pciio_addr,	/* starting address */
			 size_t byte_count,	/* map this many bytes */
			 unsigned flags);

typedef caddr_t
pciio_pio_addr_f        (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 pciio_space_t space,	/* which address space */
			 iopaddr_t pciio_addr,	/* starting address */
			 size_t byte_count,	/* map this many bytes */
			 pciio_piomap_t *mapp,	/* in case a piomap was needed */
			 unsigned flags);

typedef iopaddr_t
pciio_piospace_alloc_f  (vertex_hdl_t dev,	/* PIO space for this device */
			 device_desc_t dev_desc,	/* Device descriptor   */
			 pciio_space_t space,	/* which address space  */
			 size_t byte_count,	/* Number of bytes of space */
			 size_t alignment);	/* Alignment of allocation  */

typedef void
pciio_piospace_free_f   (vertex_hdl_t dev,	/* Device freeing space */
			 pciio_space_t space,	/* Which space is freed */
			 iopaddr_t pci_addr,	/* Address being freed */
			 size_t size);	/* Size freed           */

/* DMA MANAGEMENT */

typedef pciio_dmamap_t
pciio_dmamap_alloc_f    (vertex_hdl_t dev,	/* set up mappings for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 size_t byte_count_max,		/* max size of a mapping */
			 unsigned flags);	/* defined in dma.h */

typedef void
pciio_dmamap_free_f     (pciio_dmamap_t dmamap);

typedef iopaddr_t
pciio_dmamap_addr_f     (pciio_dmamap_t dmamap,		/* use these mapping resources */
			 paddr_t paddr,	/* map for this address */
			 size_t byte_count);	/* map this many bytes */

typedef void
pciio_dmamap_done_f     (pciio_dmamap_t dmamap);

typedef iopaddr_t
pciio_dmatrans_addr_f   (vertex_hdl_t dev,	/* translate for this device */
			 device_desc_t dev_desc,	/* device descriptor */
			 paddr_t paddr,	/* system physical address */
			 size_t byte_count,	/* length */
			 unsigned flags);	/* defined in dma.h */

typedef void
pciio_dmamap_drain_f	(pciio_dmamap_t map);

typedef void
pciio_dmaaddr_drain_f	(vertex_hdl_t vhdl,
			 paddr_t addr,
			 size_t bytes);

typedef void
pciio_dmalist_drain_f	(vertex_hdl_t vhdl,
			 alenlist_t list);

/* INTERRUPT MANAGEMENT */

typedef pciio_intr_t
pciio_intr_alloc_f      (vertex_hdl_t dev,	/* which PCI device */
			 device_desc_t dev_desc,	/* device descriptor */
			 pciio_intr_line_t lines,	/* which line(s) will be used */
			 vertex_hdl_t owner_dev);	/* owner of this intr */

typedef void
pciio_intr_free_f       (pciio_intr_t intr_hdl);

typedef int
pciio_intr_connect_f    (pciio_intr_t intr_hdl, intr_func_t intr_func, intr_arg_t intr_arg);	/* pciio intr resource handle */

typedef void
pciio_intr_disconnect_f (pciio_intr_t intr_hdl);

typedef vertex_hdl_t
pciio_intr_cpu_get_f    (pciio_intr_t intr_hdl);	/* pciio intr resource handle */

/* CONFIGURATION MANAGEMENT */

typedef void
pciio_provider_startup_f (vertex_hdl_t pciio_provider);

typedef void
pciio_provider_shutdown_f (vertex_hdl_t pciio_provider);

typedef int	
pciio_reset_f		(vertex_hdl_t conn);	/* pci connection point */

typedef pciio_endian_t			/* actual endianness */
pciio_endian_set_f      (vertex_hdl_t dev,	/* specify endianness for this device */
			 pciio_endian_t device_end,	/* endianness of device */
			 pciio_endian_t desired_end);	/* desired endianness */

typedef uint64_t
pciio_config_get_f	(vertex_hdl_t conn,	/* pci connection point */
			 unsigned reg,		/* register byte offset */
			 unsigned size);	/* width in bytes (1..4) */

typedef void
pciio_config_set_f	(vertex_hdl_t conn,	/* pci connection point */
			 unsigned reg,		/* register byte offset */
			 unsigned size,		/* width in bytes (1..4) */
			 uint64_t value);	/* value to store */

typedef int
pciio_error_devenable_f (vertex_hdl_t pconn_vhdl, int error_code);

typedef pciio_slot_t
pciio_error_extract_f	(vertex_hdl_t vhdl,
			 pciio_space_t *spacep,
			 iopaddr_t *addrp);

typedef void
pciio_driver_reg_callback_f	(vertex_hdl_t conn,
				int key1,
				int key2,
				int error);

typedef void
pciio_driver_unreg_callback_f	(vertex_hdl_t conn, /* pci connection point */
				 int key1,
				 int key2,
				 int error);

typedef int
pciio_device_unregister_f	(vertex_hdl_t conn);

typedef int
pciio_dma_enabled_f		(vertex_hdl_t conn);

/*
 * Adapters that provide a PCI interface adhere to this software interface.
 */
typedef struct pciio_provider_s {
    /* PIO MANAGEMENT */
    pciio_piomap_alloc_f   *piomap_alloc;
    pciio_piomap_free_f    *piomap_free;
    pciio_piomap_addr_f    *piomap_addr;
    pciio_piomap_done_f    *piomap_done;
    pciio_piotrans_addr_f  *piotrans_addr;
    pciio_piospace_alloc_f *piospace_alloc;
    pciio_piospace_free_f  *piospace_free;

    /* DMA MANAGEMENT */
    pciio_dmamap_alloc_f   *dmamap_alloc;
    pciio_dmamap_free_f    *dmamap_free;
    pciio_dmamap_addr_f    *dmamap_addr;
    pciio_dmamap_done_f    *dmamap_done;
    pciio_dmatrans_addr_f  *dmatrans_addr;
    pciio_dmamap_drain_f   *dmamap_drain;
    pciio_dmaaddr_drain_f  *dmaaddr_drain;
    pciio_dmalist_drain_f  *dmalist_drain;

    /* INTERRUPT MANAGEMENT */
    pciio_intr_alloc_f     *intr_alloc;
    pciio_intr_free_f      *intr_free;
    pciio_intr_connect_f   *intr_connect;
    pciio_intr_disconnect_f *intr_disconnect;
    pciio_intr_cpu_get_f   *intr_cpu_get;

    /* CONFIGURATION MANAGEMENT */
    pciio_provider_startup_f *provider_startup;
    pciio_provider_shutdown_f *provider_shutdown;
    pciio_reset_f	   *reset;
    pciio_endian_set_f     *endian_set;
    pciio_config_get_f	   *config_get;
    pciio_config_set_f	   *config_set;

    /* Error handling interface */
    pciio_error_devenable_f *error_devenable;
    pciio_error_extract_f *error_extract;

    /* Callback support */
    pciio_driver_reg_callback_f *driver_reg_callback;
    pciio_driver_unreg_callback_f *driver_unreg_callback;
    pciio_device_unregister_f 	*device_unregister;
    pciio_dma_enabled_f		*dma_enabled;
} pciio_provider_t;

/* PCI devices use these standard PCI provider interfaces */
extern pciio_piomap_alloc_f pciio_piomap_alloc;
extern pciio_piomap_free_f pciio_piomap_free;
extern pciio_piomap_addr_f pciio_piomap_addr;
extern pciio_piomap_done_f pciio_piomap_done;
extern pciio_piotrans_addr_f pciio_piotrans_addr;
extern pciio_pio_addr_f pciio_pio_addr;
extern pciio_piospace_alloc_f pciio_piospace_alloc;
extern pciio_piospace_free_f pciio_piospace_free;
extern pciio_dmamap_alloc_f pciio_dmamap_alloc;
extern pciio_dmamap_free_f pciio_dmamap_free;
extern pciio_dmamap_addr_f pciio_dmamap_addr;
extern pciio_dmamap_done_f pciio_dmamap_done;
extern pciio_dmatrans_addr_f pciio_dmatrans_addr;
extern pciio_dmamap_drain_f pciio_dmamap_drain;
extern pciio_dmaaddr_drain_f pciio_dmaaddr_drain;
extern pciio_dmalist_drain_f pciio_dmalist_drain;
extern pciio_intr_alloc_f pciio_intr_alloc;
extern pciio_intr_free_f pciio_intr_free;
extern pciio_intr_connect_f pciio_intr_connect;
extern pciio_intr_disconnect_f pciio_intr_disconnect;
extern pciio_intr_cpu_get_f pciio_intr_cpu_get;
extern pciio_provider_startup_f pciio_provider_startup;
extern pciio_provider_shutdown_f pciio_provider_shutdown;
extern pciio_reset_f pciio_reset;
extern pciio_endian_set_f pciio_endian_set;
extern pciio_config_get_f pciio_config_get;
extern pciio_config_set_f pciio_config_set;

/* Widgetdev in the IOERROR structure is encoded as follows.
 *	+---------------------------+
 *	| slot (7:3) | function(2:0)|
 *	+---------------------------+
 * Following are the convenience interfaces to get at form
 * a widgetdev or to break it into its constituents.
 */

#define PCIIO_WIDGETDEV_SLOT_SHFT		3
#define PCIIO_WIDGETDEV_SLOT_MASK		0x1f
#define PCIIO_WIDGETDEV_FUNC_MASK		0x7

#define pciio_widgetdev_create(slot,func)       \
        (((slot) << PCIIO_WIDGETDEV_SLOT_SHFT) + (func))

#define pciio_widgetdev_slot_get(wdev)		\
	(((wdev) >> PCIIO_WIDGETDEV_SLOT_SHFT) & PCIIO_WIDGETDEV_SLOT_MASK)

#define pciio_widgetdev_func_get(wdev)		\
	((wdev) & PCIIO_WIDGETDEV_FUNC_MASK)


/* Generic PCI card initialization interface
 */

extern int
pciio_driver_register  (pciio_vendor_id_t vendor_id,	/* card's vendor number */
			pciio_device_id_t device_id,	/* card's device number */
			char *driver_prefix,	/* driver prefix */
			unsigned flags);

extern void
pciio_error_register   (vertex_hdl_t pconn,	/* which slot */
			error_handler_f *efunc,	/* function to call */
			error_handler_arg_t einfo);	/* first parameter */

extern void             pciio_driver_unregister(char *driver_prefix);

typedef void		pciio_iter_f(vertex_hdl_t pconn);	/* a connect point */

/* Interfaces used by PCI Bus Providers to talk to
 * the Generic PCI layer.
 */
extern vertex_hdl_t
pciio_device_register  (vertex_hdl_t connectpt,	/* vertex at center of bus */
			vertex_hdl_t master,	/* card's master ASIC (pci provider) */
			pciio_slot_t slot,	/* card's slot (0..?) */
			pciio_function_t func,	/* card's func (0..?) */
			pciio_vendor_id_t vendor,	/* card's vendor number */
			pciio_device_id_t device);	/* card's device number */

extern void
pciio_device_unregister(vertex_hdl_t connectpt);

extern pciio_info_t
pciio_device_info_new  (pciio_info_t pciio_info,	/* preallocated info struct */
			vertex_hdl_t master,	/* card's master ASIC (pci provider) */
			pciio_slot_t slot,	/* card's slot (0..?) */
			pciio_function_t func,	/* card's func (0..?) */
			pciio_vendor_id_t vendor,	/* card's vendor number */
			pciio_device_id_t device);	/* card's device number */

extern void
pciio_device_info_free(pciio_info_t pciio_info);

extern vertex_hdl_t
pciio_device_info_register(
			vertex_hdl_t connectpt,	/* vertex at center of bus */
			pciio_info_t pciio_info);	/* details about conn point */

extern void
pciio_device_info_unregister(
			vertex_hdl_t connectpt,	/* vertex at center of bus */
			pciio_info_t pciio_info);	/* details about conn point */


extern int              
pciio_device_attach(
			vertex_hdl_t pcicard,   /* vertex created by pciio_device_register */
			int drv_flags);
extern int
pciio_device_detach(
			vertex_hdl_t pcicard,   /* vertex created by pciio_device_register */
                        int drv_flags);


/* create and initialize empty window mapping resource */
extern pciio_win_map_t
pciio_device_win_map_new(pciio_win_map_t win_map,	/* preallocated win map structure */
			 size_t region_size,		/* size of region to be tracked */
			 size_t page_size);		/* allocation page size */

/* destroy window mapping resource freeing up ancillary resources */
extern void
pciio_device_win_map_free(pciio_win_map_t win_map);	/* preallocated win map structure */

/* populate window mapping with free range of addresses */
extern void
pciio_device_win_populate(pciio_win_map_t win_map,	/* win map */
			  iopaddr_t ioaddr,		/* base address of free range */
			  size_t size);			/* size of free range */

/* allocate window from mapping resource */
extern iopaddr_t
pciio_device_win_alloc(struct resource * res,
		       pciio_win_alloc_t win_alloc,	/* opaque allocation cookie */
		       size_t start,			/* start unit, or 0 */
		       size_t size,			/* size of allocation */
		       size_t align);			/* alignment of allocation */

/* free previously allocated window */
extern void
pciio_device_win_free(pciio_win_alloc_t win_alloc);	/* opaque allocation cookie */


/*
 * Generic PCI interface, for use with all PCI providers
 * and all PCI devices.
 */

/* Generic PCI interrupt interfaces */
extern vertex_hdl_t     pciio_intr_dev_get(pciio_intr_t pciio_intr);
extern vertex_hdl_t     pciio_intr_cpu_get(pciio_intr_t pciio_intr);

/* Generic PCI pio interfaces */
extern vertex_hdl_t     pciio_pio_dev_get(pciio_piomap_t pciio_piomap);
extern pciio_slot_t     pciio_pio_slot_get(pciio_piomap_t pciio_piomap);
extern pciio_space_t    pciio_pio_space_get(pciio_piomap_t pciio_piomap);
extern iopaddr_t        pciio_pio_pciaddr_get(pciio_piomap_t pciio_piomap);
extern ulong            pciio_pio_mapsz_get(pciio_piomap_t pciio_piomap);
extern caddr_t          pciio_pio_kvaddr_get(pciio_piomap_t pciio_piomap);

/* Generic PCI dma interfaces */
extern vertex_hdl_t     pciio_dma_dev_get(pciio_dmamap_t pciio_dmamap);

/* Register/unregister PCI providers and get implementation handle */
extern void             pciio_provider_register(vertex_hdl_t provider, pciio_provider_t *pciio_fns);
extern void             pciio_provider_unregister(vertex_hdl_t provider);
extern pciio_provider_t *pciio_provider_fns_get(vertex_hdl_t provider);

/* Generic pci slot information access interface */
extern pciio_info_t     pciio_info_chk(vertex_hdl_t vhdl);
extern pciio_info_t     pciio_info_get(vertex_hdl_t vhdl);
extern pciio_info_t     pciio_hostinfo_get(vertex_hdl_t vhdl);
extern void             pciio_info_set(vertex_hdl_t vhdl, pciio_info_t widget_info);
extern vertex_hdl_t     pciio_info_dev_get(pciio_info_t pciio_info);
extern vertex_hdl_t     pciio_info_hostdev_get(pciio_info_t pciio_info);
extern pciio_bus_t	pciio_info_bus_get(pciio_info_t pciio_info);
extern pciio_slot_t     pciio_info_slot_get(pciio_info_t pciio_info);
extern pciio_function_t	pciio_info_function_get(pciio_info_t pciio_info);
extern pciio_vendor_id_t pciio_info_vendor_id_get(pciio_info_t pciio_info);
extern pciio_device_id_t pciio_info_device_id_get(pciio_info_t pciio_info);
extern vertex_hdl_t     pciio_info_master_get(pciio_info_t pciio_info);
extern arbitrary_info_t pciio_info_mfast_get(pciio_info_t pciio_info);
extern pciio_provider_t *pciio_info_pops_get(pciio_info_t pciio_info);
extern error_handler_f *pciio_info_efunc_get(pciio_info_t);
extern error_handler_arg_t *pciio_info_einfo_get(pciio_info_t);
extern pciio_space_t	pciio_info_bar_space_get(pciio_info_t, int);
extern iopaddr_t	pciio_info_bar_base_get(pciio_info_t, int);
extern size_t		pciio_info_bar_size_get(pciio_info_t, int);
extern iopaddr_t	pciio_info_rom_base_get(pciio_info_t);
extern size_t		pciio_info_rom_size_get(pciio_info_t);
extern int		pciio_info_type1_get(pciio_info_t);
extern int              pciio_error_handler(vertex_hdl_t, int, ioerror_mode_t, ioerror_t *);
extern int		pciio_dma_enabled(vertex_hdl_t);

/**
 * sn_pci_set_vchan - Set the requested Virtual Channel bits into the mapped DMA 
 *                    address.
 * @pci_dev: pci device pointer
 * @addr: mapped dma address
 * @vchan: Virtual Channel to use 0 or 1.
 *
 * Set the Virtual Channel bit in the mapped dma address.
 */
static inline int
sn_pci_set_vchan(struct pci_dev *pci_dev,
	dma_addr_t *addr,
	int vchan)
{

	if (vchan > 1) {
		return -1;
	}

	if (!(*addr >> 32))	/* Using a mask here would be cleaner */
		return 0;	/* but this generates better code */

	if (vchan == 1) {
		/* Set Bit 57 */
		*addr |= (1UL << 57);
	} else {
		/* Clear Bit 57 */
		*addr &= ~(1UL << 57);
	}

	return 0;
}

#endif				/* C or C++ */


/*
 * Prototypes
 */

int snia_badaddr_val(volatile void *addr, int len, volatile void *ptr);
nasid_t snia_get_console_nasid(void);
nasid_t snia_get_master_baseio_nasid(void);
void snia_ioerror_dump(char *name, int error_code, int error_mode, ioerror_t *ioerror);
int snia_pcibr_rrb_alloc(struct pci_dev *pci_dev, int *count_vchan0, int *count_vchan1);
pciio_endian_t snia_pciio_endian_set(struct pci_dev *pci_dev,
	pciio_endian_t device_end, pciio_endian_t desired_end);
iopaddr_t snia_pciio_dmatrans_addr(struct pci_dev *pci_dev, device_desc_t dev_desc,
	paddr_t paddr, size_t byte_count, unsigned flags);
pciio_dmamap_t snia_pciio_dmamap_alloc(struct pci_dev *pci_dev,
	device_desc_t dev_desc, size_t byte_count_max, unsigned flags);
void snia_pciio_dmamap_free(pciio_dmamap_t pciio_dmamap);
iopaddr_t snia_pciio_dmamap_addr(pciio_dmamap_t pciio_dmamap, paddr_t paddr,
	size_t byte_count);
void snia_pciio_dmamap_done(pciio_dmamap_t pciio_dmamap);
void *snia_kmem_zalloc(size_t size);
void snia_kmem_free(void *ptr, size_t size);
void *snia_kmem_alloc_node(register size_t size, cnodeid_t node);

#endif				/* _ASM_SN_PCI_PCIIO_H */
