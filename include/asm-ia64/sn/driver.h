/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_DRIVER_H
#define _ASM_IA64_SN_DRIVER_H

#include <asm/sn/sgi.h>
#include <asm/types.h>

/*
** Interface for device driver handle management.
**
** These functions are mostly for use by the loadable driver code, and
** for use by I/O bus infrastructure code.
*/

typedef struct device_driver_s *device_driver_t;

/* == Driver thread priority support == */
typedef int ilvl_t;

#ifdef __cplusplus
extern "C" {
#endif

struct eframe_s;
struct piomap;
struct dmamap;

typedef __psunsigned_t iobush_t;

/* interrupt function */
typedef void	       *intr_arg_t;
typedef void		intr_func_f(intr_arg_t);
typedef intr_func_f    *intr_func_t;

#define	INTR_ARG(n)	((intr_arg_t)(__psunsigned_t)(n))

/* system interrupt resource handle -- returned from intr_alloc */
typedef struct intr_s *intr_t;
#define INTR_HANDLE_NONE ((intr_t)0)

/*
 * restore interrupt level value, returned from intr_block_level
 * for use with intr_unblock_level.
 */
typedef void *rlvl_t;


/* 
 * A basic, platform-independent description of I/O requirements for
 * a device. This structure is usually formed by lboot based on information 
 * in configuration files.  It contains information about PIO, DMA, and
 * interrupt requirements for a specific instance of a device.
 *
 * The pio description is currently unused.
 *
 * The dma description describes bandwidth characteristics and bandwidth
 * allocation requirements. (TBD)
 *
 * The Interrupt information describes the priority of interrupt, desired 
 * destination, policy (TBD), whether this is an error interrupt, etc.  
 * For now, interrupts are targeted to specific CPUs.
 */

typedef struct device_desc_s {
	/* pio description (currently none) */

	/* dma description */
	/* TBD: allocated badwidth requirements */

	/* interrupt description */
	vertex_hdl_t	intr_target;	/* Hardware locator string */
	int 		intr_policy;	/* TBD */
	ilvl_t		intr_swlevel;	/* software level for blocking intr */
	char		*intr_name;	/* name of interrupt, if any */

	int		flags;
} *device_desc_t;

/* flag values */
#define	D_INTR_ISERR	0x1		/* interrupt is for error handling */
#define D_IS_ASSOC	0x2		/* descriptor is associated with a dev */
#define D_INTR_NOTHREAD	0x4		/* Interrupt handler isn't threaded. */

#define INTR_SWLEVEL_NOTHREAD_DEFAULT 	0	/* Default
						 * Interrupt level in case of
						 * non-threaded interrupt 
						 * handlers
						 */
#endif /* _ASM_IA64_SN_DRIVER_H */
