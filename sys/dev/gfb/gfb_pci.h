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
 * Copyright (c) 2000 Andrew Miklic
 *
 * $FreeBSD$
 */

#ifndef _PCI_GFB_H_
#define _PCI_GFB_H_

#define GFB_MEM_BASE_RID	0x10

typedef u_int32_t gfb_reg_t;

/* GFB register access macros */

#define BUFADDR(adp, off)					\
/*	(adp)->va_buffer = (adp)->va_mem_base +			\
			   (((adp)->va_buffer -			\
			     (adp)->va_mem_base +		\
			     (adp)->va_buffer_size) %		\
			    (adp)->va_mem_size),		\
	(adp)->va_buffer_alias = ((adp)->va_buffer -		\
				  (adp)->va_mem_base) /		\
				 (adp)->va_buffer_size,		\
	(adp)->va_window = (adp)->va_buffer +			\
			   (((adp)->va_window -			\
			     (adp)->va_mem_base) /		\
			    (adp)->va_buffer_alias),		\
*/	((adp)->va_window + (adp)->va_window_orig +		\
	 ((vm_offset_t)(off) << 2L))

#define REGADDR(adp, reg)					\
/*	(adp)->va_buffer = (adp)->va_mem_base +			\
			   (((adp)->va_buffer -			\
			     (adp)->va_mem_base +		\
			     (adp)->va_buffer_size) %		\
			    (adp)->va_mem_size),		\
	(adp)->va_buffer_alias = ((adp)->va_buffer -		\
				  (adp)->va_mem_base) /		\
				 (adp)->va_buffer_size,		\
	(adp)->va_registers = (adp)->va_buffer +		\
			      (((adp)->va_registers -		\
				(adp)->va_mem_base) /		\
			       (adp)->va_buffer_alias),		\
*/	((adp)->va_registers + ((vm_offset_t)(reg) << 2L))

#define READ_GFB_REGISTER(adp, reg) 				\
	(*(u_int32_t *)(REGADDR(adp, reg)))
#define WRITE_GFB_REGISTER(adp, reg, val)			\
	(*(u_int32_t*)(REGADDR(adp, reg)) = (val))
#define GFB_REGISTER_WRITE_BARRIER(sc, reg, nregs)		\
	bus_space_barrier((sc)->btag, (sc)->regs, ((reg) << 2),	\
			  4 * (nregs), BUS_SPACE_BARRIER_WRITE)
#define GFB_REGISTER_READ_BARRIER(sc, reg, nregs)		\
	bus_space_barrier((sc)->btag, (sc)->regs, ((reg) << 2), \
			  4 * (nregs), BUS_SPACE_BARRIER_READ)
#define GFB_REGISTER_READWRITE_BARRIER(sc, reg, nregs)		\
	bus_space_barrier((sc)->btag, (sc)->regs, ((reg) << 2),	\
			  4 * (nregs),				\
			  BUS_SPACE_BARRIER_READ |		\
			  BUS_SPACE_BARRIER_WRITE)

#define READ_GFB_BUFFER(adp, reg) 				\
	(*(u_int32_t *)(BUFADDR(adp, reg)))
#define WRITE_GFB_BUFFER(adp, reg, val)				\
	(*(u_int32_t*)(BUFADDR(adp, reg)) = (val))

/*int pcigfb_probe(device_t);*/
int pcigfb_attach(device_t);
int pcigfb_detach(device_t);

#ifdef FB_INSTALL_CDEV

d_open_t pcigfb_open;
d_close_t pcigfb_close;
d_read_t pcigfb_read;
d_write_t pcigfb_write;
d_ioctl_t pcigfb_ioctl;
d_mmap_t pcigfb_mmap;

#endif /*FB_INSTALL_CDEV*/

typedef struct gfb_type {
	int vendor_id;
	int device_id;
	char* name;
} *gfb_type_t;

#endif /* _PCI_GFB_H_ */
