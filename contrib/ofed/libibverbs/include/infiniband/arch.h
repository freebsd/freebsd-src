/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef INFINIBAND_ARCH_H
#define INFINIBAND_ARCH_H

#include <stdint.h>
#include <infiniband/endian.h>
#include <infiniband/byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

/*
 * Architecture-specific defines.  Currently, an architecture is
 * required to implement the following operations:
 *
 * mb() - memory barrier.  No loads or stores may be reordered across
 *     this macro by either the compiler or the CPU.
 * rmb() - read memory barrier.  No loads may be reordered across this
 *     macro by either the compiler or the CPU.
 * wmb() - write memory barrier.  No stores may be reordered across
 *     this macro by either the compiler or the CPU.
 * wc_wmb() - flush write combine buffers.  No write-combined writes
 *     will be reordered across this macro by either the compiler or
 *     the CPU.
 */

#if defined(__i386__)

#define mb()	 asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
#define rmb()	 mb()
#define wmb()	 asm volatile("" ::: "memory")
#define wc_wmb() mb()

#elif defined(__x86_64__)

/*
 * Only use lfence for mb() and rmb() because we don't care about
 * ordering against non-temporal stores (for now at least).
 */
#define mb()	 asm volatile("lfence" ::: "memory")
#define rmb()	 mb()
#define wmb()	 asm volatile("" ::: "memory")
#define wc_wmb() asm volatile("sfence" ::: "memory")

#elif defined(__PPC64__)

#define mb()	 asm volatile("sync" ::: "memory")
#define rmb()	 asm volatile("lwsync" ::: "memory")
#define wmb()	 mb()
#define wc_wmb() wmb()

#elif defined(__ia64__)

#define mb()	 asm volatile("mf" ::: "memory")
#define rmb()	 mb()
#define wmb()	 mb()
#define wc_wmb() asm volatile("fwb" ::: "memory")

#elif defined(__PPC__)

#define mb()	 asm volatile("sync" ::: "memory")
#define rmb()	 mb()
#define wmb()	 mb()
#define wc_wmb() wmb()

#elif defined(__sparc_v9__)

#define mb()	 asm volatile("membar #LoadLoad | #LoadStore | #StoreStore | #StoreLoad" ::: "memory")
#define rmb()	 asm volatile("membar #LoadLoad" ::: "memory")
#define wmb()	 asm volatile("membar #StoreStore" ::: "memory")
#define wc_wmb() wmb()

#elif defined(__sparc__)

#define mb()	 asm volatile("" ::: "memory")
#define rmb()	 mb()
#define wmb()	 mb()
#define wc_wmb() wmb()

#else

#warning No architecture specific defines found.  Using generic implementation.

#define mb()	 asm volatile("" ::: "memory")
#define rmb()	 mb()
#define wmb()	 mb()
#define wc_wmb() wmb()

#endif

#endif /* INFINIBAND_ARCH_H */
