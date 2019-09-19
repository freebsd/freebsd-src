/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Alex Richardson
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef	_DEV_HWPMC_HWPMC_BERI_H_
#define	_DEV_HWPMC_HWPMC_BERI_H_

#define STATCOUNTER_ITEM(name, X, Y)				\
static inline uint64_t statcounters_get_##name##_count(void)	\
{								\
	uint64_t ret;						\
	__asm __volatile(					\
			".word (0x1f << 26) | (0x0 << 21) |	\
				(12 << 16) | ("#X" << 11) |	\
				( "#Y"  << 6) | 0x3b\n\t"	\
			"move %0,$12" : "=r" (ret) :: "$12");	\
	return (ret);						\
}

STATCOUNTER_ITEM(cycle,2,0)
STATCOUNTER_ITEM(inst,4,0)
STATCOUNTER_ITEM(inst_user,4,1)
STATCOUNTER_ITEM(inst_kernel,4,2)
STATCOUNTER_ITEM(imprecise_setbounds,4,3)
STATCOUNTER_ITEM(unrepresentable_caps,4,4)
STATCOUNTER_ITEM(itlb_miss,5,0)
STATCOUNTER_ITEM(dtlb_miss,6,0)
STATCOUNTER_ITEM(icache_write_hit,8,0)
STATCOUNTER_ITEM(icache_write_miss,8,1)
STATCOUNTER_ITEM(icache_read_hit,8,2)
STATCOUNTER_ITEM(icache_read_miss,8,3)
STATCOUNTER_ITEM(icache_evict,8,6)
STATCOUNTER_ITEM(dcache_write_hit,9,0)
STATCOUNTER_ITEM(dcache_write_miss,9,1)
STATCOUNTER_ITEM(dcache_read_hit,9,2)
STATCOUNTER_ITEM(dcache_read_miss,9,3)
STATCOUNTER_ITEM(dcache_evict,9,6)
STATCOUNTER_ITEM(dcache_set_tag_write,9,8)
STATCOUNTER_ITEM(dcache_set_tag_read,9,9)
STATCOUNTER_ITEM(l2cache_write_hit,10,0)
STATCOUNTER_ITEM(l2cache_write_miss,10,1)
STATCOUNTER_ITEM(l2cache_read_hit,10,2)
STATCOUNTER_ITEM(l2cache_read_miss,10,3)
STATCOUNTER_ITEM(l2cache_evict,10,6)
STATCOUNTER_ITEM(l2cache_set_tag_write,10,8)
STATCOUNTER_ITEM(l2cache_set_tag_read,10,9)
STATCOUNTER_ITEM(mem_byte_read,11,0)
STATCOUNTER_ITEM(mem_byte_write,11,1)
STATCOUNTER_ITEM(mem_hword_read,11,2)
STATCOUNTER_ITEM(mem_hword_write,11,3)
STATCOUNTER_ITEM(mem_word_read,11,4)
STATCOUNTER_ITEM(mem_word_write,11,5)
STATCOUNTER_ITEM(mem_dword_read,11,6)
STATCOUNTER_ITEM(mem_dword_write,11,7)
STATCOUNTER_ITEM(mem_cap_read,11,8)
STATCOUNTER_ITEM(mem_cap_write,11,9)
STATCOUNTER_ITEM(mem_cap_read_tag_set,11,10)
STATCOUNTER_ITEM(mem_cap_write_tag_set,11,11)
STATCOUNTER_ITEM(tagcache_write_hit,12,0)
STATCOUNTER_ITEM(tagcache_write_miss,12,1)
STATCOUNTER_ITEM(tagcache_read_hit,12,2)
STATCOUNTER_ITEM(tagcache_read_miss,12,3)
STATCOUNTER_ITEM(tagcache_evict,12,6)
STATCOUNTER_ITEM(l2cachemaster_read_req,13,0)
STATCOUNTER_ITEM(l2cachemaster_write_req,13,1)
STATCOUNTER_ITEM(l2cachemaster_write_req_flit,13,2)
STATCOUNTER_ITEM(l2cachemaster_read_rsp,13,3)
STATCOUNTER_ITEM(l2cachemaster_read_rsp_flit,13,4)
STATCOUNTER_ITEM(l2cachemaster_write_rsp,13,5)
STATCOUNTER_ITEM(tagcachemaster_read_req,14,0)
STATCOUNTER_ITEM(tagcachemaster_write_req,14,1)
STATCOUNTER_ITEM(tagcachemaster_write_req_flit,14,2)
STATCOUNTER_ITEM(tagcachemaster_read_rsp,14,3)
STATCOUNTER_ITEM(tagcachemaster_read_rsp_flit,14,4)
STATCOUNTER_ITEM(tagcachemaster_write_rsp,14,5)

#endif	/* !_DEV_HWPMC_HWPMC_BERI_H_ */
