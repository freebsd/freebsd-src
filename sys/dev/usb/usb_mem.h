/*	$NetBSD: usb_mem.h,v 1.9 1999/10/13 08:10:58 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__NetBSD__) || defined(__OpenBSD__)
typedef struct usb_dma_block {
	bus_dma_tag_t tag;
	bus_dmamap_t map;
        caddr_t kaddr;
        bus_dma_segment_t segs[1];
        int nsegs;
        size_t size;
        size_t align;
	int fullblock;
	LIST_ENTRY(usb_dma_block) next;
} usb_dma_block_t;

#define DMAADDR(dma, offset) ((dma)->block->segs[0].ds_addr + (dma)->offs + (offset))
#define KERNADDR(dma, offset) ((void *)((dma)->block->kaddr + (dma)->offs) + (offset))

usbd_status	usb_allocmem(usbd_bus_handle,size_t,size_t, usb_dma_t *);
void		usb_freemem(usbd_bus_handle, usb_dma_t *);

#elif defined(__FreeBSD__)

/* 
 * FreeBSD does not have special functions for dma memory, so let's keep it
 * simple for now.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>       /* for vtophys */

#define		usb_allocmem(t,s,a,p)	(*(p) = malloc(s, M_USB, M_NOWAIT), (*(p) == NULL? USBD_NOMEM: USBD_NORMAL_COMPLETION))
#define		usb_freemem(t,p)	(free(*(p), M_USB))

#ifdef __alpha__
#define DMAADDR(dma, offset)	(alpha_XXX_dmamap((vm_offset_t) *(dma) + (offset)))
#else
#define DMAADDR(dma, offset)	(vtophys(*(dma) + (offset)))
#endif
#define KERNADDR(dma, offset)	((void *) (*(dma) + (offset)))
#endif

