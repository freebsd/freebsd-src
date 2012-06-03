/*-
 * Copyright (c) 2012 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#ifndef _SYS_BUSDMA_H_
#define	_SYS_BUSDMA_H_

#include <machine/_bus.h>

struct busdma_tag;
typedef struct busdma_tag *busdma_tag_t;

struct busdma_md;
typedef struct busdma_md *busdma_md_t;

struct busdma_mtag {
	vm_paddr_t	dmt_minaddr;
	vm_paddr_t	dmt_maxaddr;
	vm_size_t	dmt_maxsz;
	vm_paddr_t	dmt_align;
	vm_paddr_t	dmt_bndry;
};
typedef struct busdma_mtag *busdma_mtag_t;

/*
 * busdma_tag_create
 * returns:		errno value
 * arguments:
 *	dev		device for which the created tag is a root tag.
 *	align		alignment requirements of the DMA segments.
 *	bndry		address boundary constraints for DMA.
 *	maxaddr		largest address that can be handled by the device.
 *	maxsz		maximum total DMA size allowed.
 *	nsegs		maximum number of DMA segments allowed.
 *	maxsegsz	maximum size of a single DMA segment.
 *	flags		flags that control the behaviour of the operation.
 *	tag_p		address in which to return the newly created tag.
 */
int busdma_tag_create(device_t dev, bus_addr_t align, bus_addr_t bndry,
    bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p);

/*
 * busdma_tag_derive
 * returns:		errno value
 * arguments:
 *	tag		the root tag that is to be derived from.
 *	align		alignment requirements of the DMA segments.
 *	bndry		address boundary constraints for DMA.
 *	maxaddr		largest address that can be handled by the device.
 *	maxsz		maximum total DMA size allowed.
 *	nsegs		maximum number of DMA segments allowed.
 *	maxsegsz	maximum size of a single DMA segment.
 *	flags		flags that control the behaviour of the operation.
 *	tag_p		address in which to return the newly created tag.
 */
int busdma_tag_derive(busdma_tag_t tag, bus_addr_t align, bus_addr_t bndry,
    bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p);

/*
 * busdma_tag_destroy
 * returns:		errno value
 * arguments:
 *	tag		the tag to be destroyed.
 */
int busdma_tag_destroy(busdma_tag_t tag);

/*
 *
 */
typedef void (*busdma_callback_f)(void *, busdma_md_t, int);

int busdma_md_create(busdma_tag_t tag, u_int flags, busdma_md_t *md_p);
int busdma_md_destroy(busdma_md_t md);
int busdma_md_load_linear(busdma_md_t md, void *buf, size_t size,
    busdma_callback_f cb, void *arg, u_int flags);
int busdma_md_load_mbuf(busdma_md_t md, void *buf, size_t size);
int busdma_md_load_uio(busdma_md_t md, void *buf, size_t size);
int busdma_md_unload(busdma_md_t md);
u_int busdma_md_get_nsegs(busdma_md_t md);
bus_addr_t busdma_md_get_busaddr(busdma_md_t md, u_int idx);
vm_paddr_t busdma_md_get_paddr(busdma_md_t md, u_int idx);
vm_offset_t busdma_md_get_vaddr(busdma_md_t md, u_int idx);
vm_size_t busdma_md_get_size(busdma_md_t md, u_int idx);

static __inline void *
busdma_md_get_pointer(busdma_md_t md, u_int idx)
{
	return ((void *)(uintptr_t)busdma_md_get_vaddr(md, idx));
}

/*
 * busdma_mem_alloc
 * returns:		errno value
 * arguments:
 *	tag		the tag providing the constraints.
 *	flags		flags that control the behaviour of the operation.
 *	md_p		address in which to return the memory descriptor.
 */
int busdma_mem_alloc(busdma_tag_t tag, u_int flags, busdma_md_t *md_p);

/*
 * busdma_mem_free
 * returns:		errno value
 * arguments:
 *	tag		the memory descriptor of the memory to be freed.
 */
int busdma_mem_free(busdma_md_t md);

int busdma_start(busdma_md_t md, u_int);
int busdma_stop(busdma_md_t md);
void busdma_sync(busdma_md_t md, u_int);
void busdma_sync_range(busdma_md_t md, u_int, vm_paddr_t, vm_size_t);

#define	BUSDMA_SYNC_READ	0x1
#define	BUSDMA_SYNC_WRITE	0x2
#define	BUSDMA_SYNC_BEFORE	0x0
#define	BUSDMA_SYNC_AFTER	0x4

#define	BUSDMA_SYNC_PREREAD	(BUSDMA_SYNC_BEFORE | BUSDMA_SYNC_READ)
#define	BUSDMA_SYNC_PREWRITE	(BUSDMA_SYNC_BEFORE | BUSDMA_SYNC_WRITE)
#define	BUSDMA_SYNC_POSTREAD	(BUSDMA_SYNC_AFTER | BUSDMA_SYNC_READ)
#define	BUSDMA_SYNC_POSTWRITE	(BUSDMA_SYNC_AFTER | BUSDMA_SYNC_WRITE)

#endif /* _SYS_BUSDMA_H_ */
