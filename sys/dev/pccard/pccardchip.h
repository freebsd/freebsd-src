/*	$NetBSD: pcmciachip.h,v 1.4 1999/10/15 06:07:32 haya Exp $	*/
/* $FreeBSD: src/sys/dev/pccard/pccardchip.h,v 1.6 2000/01/10 06:58:17 imp Exp $ */

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#ifndef _PCCARD_PCCARDCHIP_H_
#define	_PCCARD_PCCARDCHIP_H_

#if 0
#include <machine/bus.h>

struct pccard_function;
struct pccard_mem_handle;
struct pccard_io_handle;

/* interfaces for pccard to call the chipset */

typedef struct pccard_chip_functions *pccard_chipset_tag_t;
typedef void *pccard_chipset_handle_t;
#endif
typedef int pccard_mem_handle_t;

#define	PCCARD_MEM_ATTR		1
#define	PCCARD_MEM_COMMON	2

#define	PCCARD_WIDTH_AUTO	0
#define	PCCARD_WIDTH_IO8	1
#define	PCCARD_WIDTH_IO16	2

#if 0
struct pccard_chip_functions {
	/* memory space allocation */
	int	(*mem_alloc)(pccard_chipset_handle_t, bus_size_t,
		    struct pccard_mem_handle *);
	void	(*mem_free)(pccard_chipset_handle_t,
		    struct pccard_mem_handle *);

	/* memory space window mapping */
	int	(*mem_map)(pccard_chipset_handle_t, int, bus_addr_t,
		    bus_size_t, struct pccard_mem_handle *,
		    bus_addr_t *, int *);
	void	(*mem_unmap)(pccard_chipset_handle_t, int);

	/* I/O space allocation */
	int	(*io_alloc) (pccard_chipset_handle_t, bus_addr_t,
		    bus_size_t, bus_size_t, struct pccard_io_handle *);
	void	(*io_free) (pccard_chipset_handle_t,
		    struct pccard_io_handle *);

	/* I/O space window mapping */
	int	(*io_map) (pccard_chipset_handle_t, int, bus_addr_t,
		    bus_size_t, struct pccard_io_handle *, int *);
	void	(*io_unmap) (pccard_chipset_handle_t, int);

	/* interrupt glue */
	void	*(*intr_establish) (pccard_chipset_handle_t,
		    struct pccard_function *, int, int (*)(void *), void *);
	void	(*intr_disestablish) (pccard_chipset_handle_t, void *);

	/* card enable/disable */
	void	(*socket_enable) (pccard_chipset_handle_t);
	void	(*socket_disable) (pccard_chipset_handle_t);

	/* card detection */
	int (*card_detect)(pccard_chipset_handle_t);  
};

/* Memory space functions. */
#define pccard_chip_mem_alloc(tag, handle, size, pcmhp)			\
	((*(tag)->mem_alloc)((handle), (size), (pcmhp)))

#define pccard_chip_mem_free(tag, handle, pcmhp)			\
	((*(tag)->mem_free)((handle), (pcmhp)))

#define pccard_chip_mem_map(tag, handle, kind, card_addr, size, pcmhp,	\
	    offsetp, windowp)						\
	((*(tag)->mem_map)((handle), (kind), (card_addr), (size), (pcmhp), \
	    (offsetp), (windowp)))

#define pccard_chip_mem_unmap(tag, handle, window)			\
	((*(tag)->mem_unmap)((handle), (window)))

/* I/O space functions. */
#define pccard_chip_io_alloc(tag, handle, start, size, align, pcihp)	\
	((*(tag)->io_alloc)((handle), (start), (size), (align), (pcihp)))

#define pccard_chip_io_free(tag, handle, pcihp)				\
	((*(tag)->io_free)((handle), (pcihp)))

#define pccard_chip_io_map(tag, handle, width, card_addr, size, pcihp,	\
	    windowp) \
	((*(tag)->io_map)((handle), (width), (card_addr), (size), (pcihp), \
	    (windowp)))

#define pccard_chip_io_unmap(tag, handle, window)			\
	((*(tag)->io_unmap)((handle), (window)))

/* Interrupt functions. */
#define pccard_chip_intr_establish(tag, handle, pf, ipl, fct, arg)	\
	((*(tag)->intr_establish)((handle), (pf), (ipl), (fct), (arg)))

#define pccard_chip_intr_disestablish(tag, handle, ih)			\
	((*(tag)->intr_disestablish)((handle), (ih)))

/* Socket functions. */
#define	pccard_chip_socket_enable(tag, handle)				\
	((*(tag)->socket_enable)((handle)))
#define	pccard_chip_socket_disable(tag, handle)				\
	((*(tag)->socket_disable)((handle)))

struct pccardbus_attach_args {
	char *paa_busname;	/* Bus name */
	pccard_chipset_tag_t pct;
	pccard_chipset_handle_t pch;
	bus_addr_t iobase;		/* start i/o space allocation here */
	bus_size_t iosize;		/* size of the i/o space range */
};

#endif /* 0 */

#endif /* _PCCARD_PCCARDCHIP_H_ */
