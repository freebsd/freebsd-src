/*-
 * Copyright (c) 2006 Kip Macy
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
 * $FreeBSD: src/sys/sun4v/include/tte_hash.h,v 1.4 2006/11/22 04:33:34 kmacy Exp $
 */

#ifndef	_MACHINE_TTE_HASH_H_
#define	_MACHINE_TTE_HASH_H_

#define HASH_ENTRY_SHIFT   2
#define HASH_ENTRIES       ((1 << HASH_ENTRY_SHIFT) - 1)
#define THE_SHIFT          (TTE_SHIFT + HASH_ENTRY_SHIFT)  /* size of TSB entry * #entries */
#define TH_COLLISION_SHIFT 47                          /* bit 47 will never be set for a valid tag */
#define TH_COLLISION       (1UL << TH_COLLISION_SHIFT)  
#define TH_INVALID_SHIFT   46                          /* bit 47 will never be set for a valid tag */
#define TH_INVALID         (1UL << TH_INVALID_SHIFT)  


struct tte_hash;
typedef struct tte_hash *tte_hash_t;

void tte_hash_init(void);

void tte_hash_clear(tte_hash_t hash);

tte_t tte_hash_clear_bits(tte_hash_t hash, vm_offset_t va, uint64_t flags);

tte_hash_t tte_hash_kernel_create(vm_offset_t, uint16_t, vm_paddr_t);

tte_hash_t tte_hash_create(uint64_t context, uint64_t *scratchval);

void tte_hash_destroy(tte_hash_t th);

tte_t tte_hash_delete(tte_hash_t hash, vm_offset_t va);

void tte_hash_delete_all(tte_hash_t hash);

void tte_hash_insert(tte_hash_t hash, vm_offset_t va, tte_t data);

tte_t tte_hash_lookup(tte_hash_t hash, vm_offset_t va);

tte_t tte_hash_lookup_nolock(tte_hash_t hash, vm_offset_t va);

tte_hash_t tte_hash_reset(tte_hash_t hash, uint64_t *scratchval);

uint64_t tte_hash_set_scratchpad_kernel(tte_hash_t th);

uint64_t tte_hash_set_scratchpad_user(tte_hash_t th, uint64_t context);

tte_t tte_hash_update(tte_hash_t hash, vm_offset_t va, tte_t tte_data);

int tte_hash_needs_resize(tte_hash_t th);

tte_hash_t  tte_hash_resize(tte_hash_t th);

#endif /* _MACHINE_TTE_HASH_H_ */
