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

/*
 * This is more or less arbitrary, except for the stack space consumed by
 * the segments array. Choose more than ((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1),
 * since in practice we could be map pages more than once.
 */
#define	BUS_DMAMAP_NSEGS	64

struct bus_dmamap_res {
	struct resource		*dr_res;
	bus_size_t		dr_used;
	SLIST_ENTRY(bus_dmamap_res)	dr_link;
};

struct bus_dmamap {
	TAILQ_ENTRY(bus_dmamap)	dm_maplruq;
	SLIST_HEAD(, bus_dmamap_res)	dm_reslist;
	int			dm_onq;
	int			dm_loaded;
};

static __inline void
sparc64_dmamap_init(struct bus_dmamap *m)
{
	SLIST_INIT(&m->dm_reslist);
}

#endif /* !_MACHINE_BUS_PRIVATE_H_ */
