/*-
 * Copyright (c) 1998 Doug Rabson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_SGMAP_H_
#define _MACHINE_SGMAP_H_

struct sgmap;

typedef void sgmap_map_callback(void *arg, bus_addr_t ba, vm_offset_t pa);

vm_offset_t	sgmap_overflow_page(void);
struct sgmap	*sgmap_map_create(bus_addr_t sba, bus_addr_t eba,
			          sgmap_map_callback *map, void *arg);
void		sgmap_map_destroy(struct sgmap *sgmap);
bus_addr_t	sgmap_alloc_region(struct sgmap *sgmap,
				   bus_size_t size, bus_size_t boundary,
				   void **mhp);
void		sgmap_load_region(struct sgmap *sgmap, bus_addr_t sba,
				  vm_offset_t va, bus_size_t size);
void		sgmap_unload_region(struct sgmap *sgmap,
				    bus_addr_t sba, bus_size_t size);
void		sgmap_free_region(struct sgmap *sgmap, void *mh);

#endif /* !_MACHINE_SGMAP_H_ */
