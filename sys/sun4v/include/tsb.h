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
 * $FreeBSD: src/sys/sun4v/include/tsb.h,v 1.4 2006/12/04 19:35:40 kmacy Exp $
 */

#ifndef	_MACHINE_TSB_H_
#define	_MACHINE_TSB_H_

#define MAX_TSB_INFO                     2

/*
 * Values for "tsb_ttesz_mask" bitmask.
 */
#define	TSB8K	(1 << TTE8K)
#define	TSB64K  (1 << TTE64K)
#define	TSB512K (1 << TTE512K)
#define	TSB4M   (1 << TTE4M)
#define	TSB32M  (1 << TTE32M)
#define	TSB256M (1 << TTE256M)

/*
 * Kernel TSBs
 */
#define TSB8K_INDEX           0
#define TSB4M_INDEX           1

extern hv_tsb_info_t kernel_td[MAX_TSB_INFO];

struct hv_tsb_info;


void tsb_init(struct hv_tsb_info *tsb, uint64_t *scratchval, uint64_t page_shift);

void tsb_deinit(struct hv_tsb_info *tsb);

void tsb_assert_invalid(struct hv_tsb_info *tsb, vm_offset_t va);

void tsb_set_tte(struct hv_tsb_info *tsb, vm_offset_t va, tte_t tte_data, uint64_t ctx);

void tsb_set_tte_real(struct hv_tsb_info *tsb, vm_offset_t index_va, 
		      vm_offset_t tag_va, tte_t tte_data, uint64_t ctx);

tte_t tsb_get_tte(struct hv_tsb_info *tsb, vm_offset_t va);

tte_t tsb_lookup_tte(vm_offset_t va, uint64_t context);

void tsb_clear(struct hv_tsb_info *tsb);

void tsb_clear_tte(struct hv_tsb_info *tsb, vm_offset_t va);

void tsb_clear_range(struct hv_tsb_info *tsb, vm_offset_t sva, vm_offset_t eva);

uint64_t tsb_set_scratchpad_kernel(struct hv_tsb_info *tsb);

uint64_t tsb_set_scratchpad_user(struct hv_tsb_info *tsb);

int tsb_size(struct hv_tsb_info *tsb);

int tsb_page_shift(pmap_t pmap);

#endif /* !_MACHINE_TSB_H_ */
