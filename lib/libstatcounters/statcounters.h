/*-
 * Copyright (c) 2016 Alexandre Joannou
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef STATCOUNTERS_H
#define STATCOUNTERS_H

// available modules
#define ICACHE  8   //CacheCore
#define DCACHE  9   //CacheCore
#define L2CACHE 10  //CacheCore
#define MIPSMEM 11  //MIPSMem

// CacheCore counters
#define WRITE_HIT   0
#define WRITE_MISS  1
#define READ_HIT    2
#define READ_MISS   3
#define PFTCH_HIT   4
#define PFTCH_MISS  5
#define EVICT       6
#define PFTCH_EVICT 7

// MIPSMem counters
#define BYTE_READ   0
#define BYTE_WRITE  1
#define HWORD_READ  2
#define HWORD_WRITE 3
#define WORD_READ   4
#define WORD_WRITE  5
#define DWORD_READ  6
#define DWORD_WRITE 7
#define CAP_READ    8
#define CAP_WRITE   9

#define eval(X) X

static inline void resetStatCounters (void)
{
  __asm __volatile(".word (0x1F << 26) | (0x0 << 21) | (0x0 << 16) | (0x7 << 11) | (0x0 << 6) | (0x3B)");
}

#include <inttypes.h>
#define DEFINE_GET_STAT_COUNTER(name,X,Y)       \
static inline uint64_t get_##name##_count (void) \
{                                               \
  uint64_t ret;                                  \
  __asm __volatile(".word (0x1f << 26) | (0x0 << 21) | (12 << 16) | ("#X" << 11) | ( "#Y"  << 6) | 0x3b\n\tmove %0,$12" : "=r" (ret) :: "$12"); \
  return ret;                                   \
}

#endif
