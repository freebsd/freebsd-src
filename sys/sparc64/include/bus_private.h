/*-
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: FreeBSD: src/sys/i386/i386/busdma_machdep.c,v 1.25 2002/01/05
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_BUS_PRIVATE_H_
#define	_MACHINE_BUS_PRIVATE_H_

#include <sys/queue.h>

#define	BUS_DMAMAP_NSEGS	((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1)

struct bus_dmamap {
	bus_dma_tag_t	dmat;
	void		*buf;		/* unmapped buffer pointer */
	bus_size_t	buflen;		/* unmapped buffer length */
	bus_addr_t	start;		/* start of mapped region */
	struct resource *res;		/* associated resource */
	bus_size_t	dvmaresv;	/* reseved DVMA memory */
	STAILQ_ENTRY(bus_dmamap)	maplruq;
	int		onq;
};

#endif /* !_MACHINE_BUS_PRIVATE_H_ */
