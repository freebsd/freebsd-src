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

struct busdma_mem;
typedef struct busdma_mem *busdma_mem_t;

/*
 * busdma_tag_create
 * returns:		errno value
 * arguments:
 *	dev		device for which the created tag is a root tag.
 *	maxaddr		largest address that can be handled by the device.
 *	align		alignment requirements of the DMA segments.
 *	bndry		address boundary constraints for DMA.
 *	maxsz		maximum total DMA size allowed.
 *	nsegs		maximum number of DMA segments allowed.
 *	maxsegsz	maximum size of a single DMA segment.
 *	flags		flags that control the behaviour of the operation.
 *	tag_p		address in which to return the newly created tag.
 */
int busdma_tag_create(device_t dev, bus_addr_t maxaddr, bus_addr_t align,
    bus_addr_t bndry, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p);

/*
 * busdma_tag_derive
 * returns:		errno value
 * arguments:
 *	tag		the root tag that is to be derived from.
 *	maxaddr		largest address that can be handled by the device.
 *	align		alignment requirements of the DMA segments.
 *	bndry		address boundary constraints for DMA.
 *	maxsz		maximum total DMA size allowed.
 *	nsegs		maximum number of DMA segments allowed.
 *	maxsegsz	maximum size of a single DMA segment.
 *	flags		flags that control the behaviour of the operation.
 *	tag_p		address in which to return the newly created tag.
 */
int busdma_tag_derive(busdma_tag_t tag, bus_addr_t maxaddr, bus_addr_t align,
    bus_addr_t bndry, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p);

/*
 * busdma_mem_alloc
 * returns:		errno value
 * arguments:
 *	tag		the tag providing the constraints.
 *	flags		flags that control the behaviour of the operation.
 *	mem_p		address in which to return the memory descriptor.
 */
int busdma_mem_alloc(busdma_tag_t tag, u_int flags, busdma_mem_t *mem_p);

/*
 * busdma_mem_get_seg_addr
 * returns:		kernel virtual address of the specified segment.
 * arguments:
 *	mem		the DMA memory allocated or mapped.
 *	idx		the segment index, starting at 0.
 */
vm_offset_t busdma_mem_get_seg_addr(busdma_mem_t tag, u_int idx);

/*
 * busdma_mem_get_seg_busaddr
 * returns:		(virtual) bus address of the specified segment.
 * arguments:
 *	mem		the BMA memory allocated to mapped.
 *	idx		the segment index, starting at 0.
 */
bus_addr_t busdma_mem_get_seg_busaddr(busdma_mem_t mem, u_int idx);

#endif /* _SYS_BUSDMA_H_ */
