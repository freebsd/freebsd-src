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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "statcounters.h"

static uint64_t start_cycle;
static uint64_t start_inst;
static uint64_t start_itlb_miss;
static uint64_t start_dtlb_miss;
static uint64_t start_icache_write_hit;
static uint64_t start_icache_write_miss;
static uint64_t start_icache_read_hit;
static uint64_t start_icache_read_miss;
static uint64_t start_icache_evict;
static uint64_t start_dcache_write_hit;
static uint64_t start_dcache_write_miss;
static uint64_t start_dcache_read_hit;
static uint64_t start_dcache_read_miss;
static uint64_t start_dcache_evict;
static uint64_t start_l2cache_write_hit;
static uint64_t start_l2cache_write_miss;
static uint64_t start_l2cache_read_hit;
static uint64_t start_l2cache_read_miss;
static uint64_t start_l2cache_evict;
static uint64_t start_mem_byte_read;
static uint64_t start_mem_byte_write;
static uint64_t start_mem_hword_read;
static uint64_t start_mem_hword_write;
static uint64_t start_mem_word_read;
static uint64_t start_mem_word_write;
static uint64_t start_mem_dword_read;
static uint64_t start_mem_dword_write;
static uint64_t start_mem_cap_read;
static uint64_t start_mem_cap_write;

static uint64_t end_cycle;
static uint64_t end_inst;
static uint64_t end_itlb_miss;
static uint64_t end_dtlb_miss;
static uint64_t end_icache_write_hit;
static uint64_t end_icache_write_miss;
static uint64_t end_icache_read_hit;
static uint64_t end_icache_read_miss;
static uint64_t end_icache_evict;
static uint64_t end_dcache_write_hit;
static uint64_t end_dcache_write_miss;
static uint64_t end_dcache_read_hit;
static uint64_t end_dcache_read_miss;
static uint64_t end_dcache_evict;
static uint64_t end_l2cache_write_hit;
static uint64_t end_l2cache_write_miss;
static uint64_t end_l2cache_read_hit;
static uint64_t end_l2cache_read_miss;
static uint64_t end_l2cache_evict;
static uint64_t end_mem_byte_read;
static uint64_t end_mem_byte_write;
static uint64_t end_mem_hword_read;
static uint64_t end_mem_hword_write;
static uint64_t end_mem_word_read;
static uint64_t end_mem_word_write;
static uint64_t end_mem_dword_read;
static uint64_t end_mem_dword_write;
static uint64_t end_mem_cap_read;
static uint64_t end_mem_cap_write;

DEFINE_GET_STAT_COUNTER(cycle,2,0);
DEFINE_GET_STAT_COUNTER(inst,4,0);
DEFINE_GET_STAT_COUNTER(itlb_miss,5,0);
DEFINE_GET_STAT_COUNTER(dtlb_miss,6,0);
DEFINE_GET_STAT_COUNTER(icache_write_hit,8,0);
DEFINE_GET_STAT_COUNTER(icache_write_miss,8,1);
DEFINE_GET_STAT_COUNTER(icache_read_hit,8,2);
DEFINE_GET_STAT_COUNTER(icache_read_miss,8,3);
DEFINE_GET_STAT_COUNTER(icache_evict,8,6);
DEFINE_GET_STAT_COUNTER(dcache_write_hit,9,0);
DEFINE_GET_STAT_COUNTER(dcache_write_miss,9,1);
DEFINE_GET_STAT_COUNTER(dcache_read_hit,9,2);
DEFINE_GET_STAT_COUNTER(dcache_read_miss,9,3);
DEFINE_GET_STAT_COUNTER(dcache_evict,9,6);
DEFINE_GET_STAT_COUNTER(l2cache_write_hit,10,0);
DEFINE_GET_STAT_COUNTER(l2cache_write_miss,10,1);
DEFINE_GET_STAT_COUNTER(l2cache_read_hit,10,2);
DEFINE_GET_STAT_COUNTER(l2cache_read_miss,10,3);
DEFINE_GET_STAT_COUNTER(l2cache_evict,10,6);
DEFINE_GET_STAT_COUNTER(mem_byte_read,11,0);
DEFINE_GET_STAT_COUNTER(mem_byte_write,11,1);
DEFINE_GET_STAT_COUNTER(mem_hword_read,11,2);
DEFINE_GET_STAT_COUNTER(mem_hword_write,11,3);
DEFINE_GET_STAT_COUNTER(mem_word_read,11,4);
DEFINE_GET_STAT_COUNTER(mem_word_write,11,5);
DEFINE_GET_STAT_COUNTER(mem_dword_read,11,6);
DEFINE_GET_STAT_COUNTER(mem_dword_write,11,7);
DEFINE_GET_STAT_COUNTER(mem_cap_read,11,8);
DEFINE_GET_STAT_COUNTER(mem_cap_write,11,9);

static void end_sample (void);

__attribute__((constructor))
static void start_sample (void)
{
    atexit(end_sample);
    //printf("resetting stat counters\n");
    resetStatCounters();
    //printf("initial sampling\n");
    start_icache_write_hit      = get_icache_write_hit_count();
    start_icache_write_miss     = get_icache_write_miss_count();
    start_icache_read_hit       = get_icache_read_hit_count();
    start_icache_read_miss      = get_icache_read_miss_count();
    start_icache_evict          = get_icache_evict_count();
    start_dcache_write_hit      = get_dcache_write_hit_count();
    start_dcache_write_miss     = get_dcache_write_miss_count();
    start_dcache_read_hit       = get_dcache_read_hit_count();
    start_dcache_read_miss      = get_dcache_read_miss_count();
    start_dcache_evict          = get_dcache_evict_count();
    start_l2cache_write_hit     = get_l2cache_write_hit_count();
    start_l2cache_write_miss    = get_l2cache_write_miss_count();
    start_l2cache_read_hit      = get_l2cache_read_hit_count();
    start_l2cache_read_miss     = get_l2cache_read_miss_count();
    start_l2cache_evict         = get_l2cache_evict_count();
    start_mem_byte_read         = get_mem_byte_read_count();
    start_mem_byte_write        = get_mem_byte_write_count();
    start_mem_hword_read        = get_mem_hword_read_count();
    start_mem_hword_write       = get_mem_hword_write_count();
    start_mem_word_read         = get_mem_word_read_count();
    start_mem_word_write        = get_mem_word_write_count();
    start_mem_dword_read        = get_mem_dword_read_count();
    start_mem_dword_write       = get_mem_dword_write_count();
    start_mem_cap_read          = get_mem_cap_read_count();
    start_mem_cap_write         = get_mem_cap_write_count();
    start_dtlb_miss             = get_dtlb_miss_count();
    start_itlb_miss             = get_itlb_miss_count();
    start_inst                  = get_inst_count();
    start_cycle                 = get_cycle_count();
}

//__attribute__((destructor)) static void end_sample (void)
static void end_sample (void)
{
    //printf("final sampling\n");
    end_cycle               = get_cycle_count();
    end_inst                = get_inst_count();
    end_itlb_miss           = get_itlb_miss_count();
    end_dtlb_miss           = get_dtlb_miss_count();
    end_icache_write_hit    = get_icache_write_hit_count();
    end_icache_write_miss   = get_icache_write_miss_count();
    end_icache_read_hit     = get_icache_read_hit_count();
    end_icache_read_miss    = get_icache_read_miss_count();
    end_icache_evict        = get_icache_evict_count();
    end_dcache_write_hit    = get_dcache_write_hit_count();
    end_dcache_write_miss   = get_dcache_write_miss_count();
    end_dcache_read_hit     = get_dcache_read_hit_count();
    end_dcache_read_miss    = get_dcache_read_miss_count();
    end_dcache_evict        = get_dcache_evict_count();
    end_l2cache_write_hit   = get_l2cache_write_hit_count();
    end_l2cache_write_miss  = get_l2cache_write_miss_count();
    end_l2cache_read_hit    = get_l2cache_read_hit_count();
    end_l2cache_read_miss   = get_l2cache_read_miss_count();
    end_l2cache_evict       = get_l2cache_evict_count();
    end_mem_byte_read       = get_mem_byte_read_count();
    end_mem_byte_write      = get_mem_byte_write_count();
    end_mem_hword_read      = get_mem_hword_read_count();
    end_mem_hword_write     = get_mem_hword_write_count();
    end_mem_word_read       = get_mem_word_read_count();
    end_mem_word_write      = get_mem_word_write_count();
    end_mem_dword_read      = get_mem_dword_read_count();
    end_mem_dword_write     = get_mem_dword_write_count();
    end_mem_cap_read        = get_mem_cap_read_count();
    end_mem_cap_write       = get_mem_cap_write_count();
    if (getenv("GATHER_STATCOUNTERS"))
    {
        fprintf(stderr, "cycles:\t%lu\n",end_cycle-start_cycle);
        fprintf(stderr, "instructions:\t%lu\n",end_inst-start_inst);
        fprintf(stderr, "itlb_miss:\t%lu\n",end_itlb_miss-start_itlb_miss);
        fprintf(stderr, "dtlb_miss:\t%lu\n",end_dtlb_miss-start_dtlb_miss);
        fprintf(stderr, "icache_write_hit:\t%lu\n",end_icache_write_hit-start_icache_write_hit);
        fprintf(stderr, "icache_write_miss:\t%lu\n",end_icache_write_miss-start_icache_write_miss);
        fprintf(stderr, "icache_read_hit:\t%lu\n",end_icache_read_hit-start_icache_read_hit);
        fprintf(stderr, "icache_read_miss:\t%lu\n",end_icache_read_miss-start_icache_read_miss);
        fprintf(stderr, "icache_evict:\t%lu\n",end_icache_evict-start_icache_evict);
        fprintf(stderr, "dcache_write_hit:\t%lu\n",end_dcache_write_hit-start_dcache_write_hit);
        fprintf(stderr, "dcache_write_miss:\t%lu\n",end_dcache_write_miss-start_dcache_write_miss);
        fprintf(stderr, "dcache_read_hit:\t%lu\n",end_dcache_read_hit-start_dcache_read_hit);
        fprintf(stderr, "dcache_read_miss:\t%lu\n",end_dcache_read_miss-start_dcache_read_miss);
        fprintf(stderr, "dcache_evict:\t%lu\n",end_dcache_evict-start_dcache_evict);
        fprintf(stderr, "l2cache_write_hit:\t%lu\n",end_l2cache_write_hit-start_l2cache_write_hit);
        fprintf(stderr, "l2cache_write_miss:\t%lu\n",end_l2cache_write_miss-start_l2cache_write_miss);
        fprintf(stderr, "l2cache_read_hit:\t%lu\n",end_l2cache_read_hit-start_l2cache_read_hit);
        fprintf(stderr, "l2cache_read_miss:\t%lu\n",end_l2cache_read_miss-start_l2cache_read_miss);
        fprintf(stderr, "l2cache_evict:\t%lu\n",end_l2cache_evict-start_l2cache_evict);
        fprintf(stderr, "mem_byte_read:\t%lu\n",end_mem_byte_read-start_mem_byte_read);
        fprintf(stderr, "mem_byte_write:\t%lu\n",end_mem_byte_write-start_mem_byte_write);
        fprintf(stderr, "mem_hword_read:\t%lu\n",end_mem_hword_read-start_mem_hword_read);
        fprintf(stderr, "mem_hword_read:\t%lu\n",end_mem_hword_write-start_mem_hword_write);
        fprintf(stderr, "mem_word_read:\t%lu\n",end_mem_word_read-start_mem_word_read);
        fprintf(stderr, "mem_word_read:\t%lu\n",end_mem_word_write-start_mem_word_write);
        fprintf(stderr, "mem_dword_read:\t%lu\n",end_mem_dword_read-start_mem_dword_read);
        fprintf(stderr, "mem_dword_read:\t%lu\n",end_mem_dword_write-start_mem_dword_write);
        fprintf(stderr, "mem_cap_read:\t%lu\n",end_mem_cap_read-start_mem_cap_read);
        fprintf(stderr, "mem_cap_read:\t%lu\n",end_mem_cap_write-start_mem_cap_write);
    }
}

