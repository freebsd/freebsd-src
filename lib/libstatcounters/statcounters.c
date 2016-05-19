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
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "statcounters.h"

// libstatcounter API
//////////////////////////////////////////////////////////////////////////////

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
DEFINE_GET_STAT_COUNTER(tagcache_write_hit,12,0);
DEFINE_GET_STAT_COUNTER(tagcache_write_miss,12,1);
DEFINE_GET_STAT_COUNTER(tagcache_read_hit,12,2);
DEFINE_GET_STAT_COUNTER(tagcache_read_miss,12,3);
DEFINE_GET_STAT_COUNTER(tagcache_evict,12,6);

// reset the hardware statcounters
void reset_statcounters (void)
{
    resetStatCounters();
}

// zero a statcounters_bank
void zero_statcounters (statcounters_bank_t * const cnt_bank)
{
    if (cnt_bank == NULL)
        errno = -1;
    else
    {
        cnt_bank->icache[WRITE_HIT]	    = 0;
        cnt_bank->icache[WRITE_MISS]	= 0;
        cnt_bank->icache[READ_HIT]	    = 0;
        cnt_bank->icache[READ_MISS]	    = 0;
        cnt_bank->icache[EVICT]	        = 0;
        cnt_bank->dcache[WRITE_HIT]	    = 0;
        cnt_bank->dcache[WRITE_MISS]	= 0;
        cnt_bank->dcache[READ_HIT]	    = 0;
        cnt_bank->dcache[READ_MISS]	    = 0;
        cnt_bank->dcache[EVICT]	        = 0;
        cnt_bank->l2cache[WRITE_HIT]	= 0;
        cnt_bank->l2cache[WRITE_MISS]   = 0;
        cnt_bank->l2cache[READ_HIT]	    = 0;
        cnt_bank->l2cache[READ_MISS]	= 0;
        cnt_bank->l2cache[EVICT]	    = 0;
        cnt_bank->tagcache[WRITE_HIT]	= 0;
        cnt_bank->tagcache[WRITE_MISS]	= 0;
        cnt_bank->tagcache[READ_HIT]	= 0;
        cnt_bank->tagcache[READ_MISS]	= 0;
        cnt_bank->tagcache[EVICT]	    = 0;
        cnt_bank->mipsmem[BYTE_READ]	= 0;
        cnt_bank->mipsmem[BYTE_WRITE]	= 0;
        cnt_bank->mipsmem[HWORD_READ]	= 0;
        cnt_bank->mipsmem[HWORD_WRITE]	= 0;
        cnt_bank->mipsmem[WORD_READ]	= 0;
        cnt_bank->mipsmem[WORD_WRITE]	= 0;
        cnt_bank->mipsmem[DWORD_READ]	= 0;
        cnt_bank->mipsmem[DWORD_WRITE]	= 0;
        cnt_bank->mipsmem[CAP_READ]	    = 0;
        cnt_bank->mipsmem[CAP_WRITE]	= 0;
        cnt_bank->dtlb_miss	            = 0;
        cnt_bank->itlb_miss	            = 0;
        cnt_bank->inst	                = 0;
        cnt_bank->cycle	                = 0;
    }
}

// sample hardware counters in a statcounters_bank
void sample_statcounters (statcounters_bank_t * const cnt_bank)
{
    if (cnt_bank == NULL)
        errno = -1;
    else
    {
        cnt_bank->icache[WRITE_HIT]	    = get_icache_write_hit_count();
        cnt_bank->icache[WRITE_MISS]	= get_icache_write_miss_count();
        cnt_bank->icache[READ_HIT]	    = get_icache_read_hit_count();
        cnt_bank->icache[READ_MISS]	    = get_icache_read_miss_count();
        cnt_bank->icache[EVICT]	        = get_icache_evict_count();
        cnt_bank->dcache[WRITE_HIT]	    = get_dcache_write_hit_count();
        cnt_bank->dcache[WRITE_MISS]	= get_dcache_write_miss_count();
        cnt_bank->dcache[READ_HIT]	    = get_dcache_read_hit_count();
        cnt_bank->dcache[READ_MISS]	    = get_dcache_read_miss_count();
        cnt_bank->dcache[EVICT]	        = get_dcache_evict_count();
        cnt_bank->l2cache[WRITE_HIT]	= get_l2cache_write_hit_count();
        cnt_bank->l2cache[WRITE_MISS]   = get_l2cache_write_miss_count();
        cnt_bank->l2cache[READ_HIT]	    = get_l2cache_read_hit_count();
        cnt_bank->l2cache[READ_MISS]	= get_l2cache_read_miss_count();
        cnt_bank->l2cache[EVICT]	    = get_l2cache_evict_count();
        cnt_bank->tagcache[WRITE_HIT]	= get_tagcache_write_hit_count();
        cnt_bank->tagcache[WRITE_MISS]	= get_tagcache_write_miss_count();
        cnt_bank->tagcache[READ_HIT]	= get_tagcache_read_hit_count();
        cnt_bank->tagcache[READ_MISS]	= get_tagcache_read_miss_count();
        cnt_bank->tagcache[EVICT]	    = get_tagcache_evict_count();
        cnt_bank->mipsmem[BYTE_READ]	= get_mem_byte_read_count();
        cnt_bank->mipsmem[BYTE_WRITE]	= get_mem_byte_write_count();
        cnt_bank->mipsmem[HWORD_READ]	= get_mem_hword_read_count();
        cnt_bank->mipsmem[HWORD_WRITE]	= get_mem_hword_write_count();
        cnt_bank->mipsmem[WORD_READ]	= get_mem_word_read_count();
        cnt_bank->mipsmem[WORD_WRITE]	= get_mem_word_write_count();
        cnt_bank->mipsmem[DWORD_READ]	= get_mem_dword_read_count();
        cnt_bank->mipsmem[DWORD_WRITE]	= get_mem_dword_write_count();
        cnt_bank->mipsmem[CAP_READ]	    = get_mem_cap_read_count();
        cnt_bank->mipsmem[CAP_WRITE]	= get_mem_cap_write_count();
        cnt_bank->dtlb_miss	            = get_dtlb_miss_count();
        cnt_bank->itlb_miss	            = get_itlb_miss_count();
        cnt_bank->inst	                = get_inst_count();
        cnt_bank->cycle	                = get_cycle_count();
    }
}

// diff two statcounters_banks into a third one
void diff_statcounters (
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs,
    statcounters_bank_t * const bd)
{
    if (bs == NULL || be == NULL || be == NULL)
        errno = -1;
    else
    {
        bd->itlb_miss    = be->itlb_miss - bs->itlb_miss;
        bd->dtlb_miss    = be->dtlb_miss - bs->dtlb_miss;
        bd->cycle        = be->cycle - bs->cycle;
        bd->inst         = be->inst - bs->inst;
        int i = 0;
        for (i = 0; i < MAX_MOD_CNT; i++)
        {
            bd->icache[i]    = be->icache[i] - bs->icache[i];
            bd->dcache[i]    = be->dcache[i] - bs->dcache[i];
            bd->l2cache[i]   = be->l2cache[i] - bs->l2cache[i];
            bd->mipsmem[i]   = be->mipsmem[i] - bs->mipsmem[i];
            bd->tagcache[i]  = be->tagcache[i] - bs->tagcache[i];
        }
    }
}

// dump a statcounters_bank in a file (csv or human readable)
void dump_statcounters (
    const statcounters_bank_t * const b,
    const char * const fname,
    const char * const fmt)
{
    if (b == NULL)
        errno = -1;
    else
    {
        FILE * fp = NULL;
        bool display_header = true;
        if (fname && access(fname, F_OK) != -1)
            display_header = false;
        if (fname && (fp = fopen(fname, "a")))
        {
            if (fmt && (strcmp(fmt,"csv") == 0))
            {
                if (display_header)
                {
                    fprintf(fp, "progname,");
                    fprintf(fp, "cycles,");
                    fprintf(fp, "instructions,");
                    fprintf(fp, "itlb_miss,");
                    fprintf(fp, "dtlb_miss,");
                    fprintf(fp, "icache_write_hit,");
                    fprintf(fp, "icache_write_miss,");
                    fprintf(fp, "icache_read_hit,");
                    fprintf(fp, "icache_read_miss,");
                    fprintf(fp, "icache_evict,");
                    fprintf(fp, "dcache_write_hit,");
                    fprintf(fp, "dcache_write_miss,");
                    fprintf(fp, "dcache_read_hit,");
                    fprintf(fp, "dcache_read_miss,");
                    fprintf(fp, "dcache_evict,");
                    fprintf(fp, "l2cache_write_hit,");
                    fprintf(fp, "l2cache_write_miss,");
                    fprintf(fp, "l2cache_read_hit,");
                    fprintf(fp, "l2cache_read_miss,");
                    fprintf(fp, "l2cache_evict,");
                    fprintf(fp, "tagcache_write_hit,");
                    fprintf(fp, "tagcache_write_miss,");
                    fprintf(fp, "tagcache_read_hit,");
                    fprintf(fp, "tagcache_read_miss,");
                    fprintf(fp, "tagcache_evict,");
                    fprintf(fp, "mipsmem_byte_read,");
                    fprintf(fp, "mipsmem_byte_write,");
                    fprintf(fp, "mipsmem_hword_read,");
                    fprintf(fp, "mipsmem_hword_write,");
                    fprintf(fp, "mipsmem_word_read,");
                    fprintf(fp, "mipsmem_word_write,");
                    fprintf(fp, "mipsmem_dword_read,");
                    fprintf(fp, "mipsmem_dword_write,");
                    fprintf(fp, "mipsmem_cap_read,");
                    fprintf(fp, "mipsmem_cap_write");
                    fprintf(fp, "\n");
                }
                fprintf(fp, "%s,",getprogname());
                fprintf(fp, "%lu,",b->cycle);
                fprintf(fp, "%lu,",b->inst);
                fprintf(fp, "%lu,",b->itlb_miss);
                fprintf(fp, "%lu,",b->dtlb_miss);
                fprintf(fp, "%lu,",b->icache[WRITE_HIT]);
                fprintf(fp, "%lu,",b->icache[WRITE_MISS]);
                fprintf(fp, "%lu,",b->icache[READ_HIT]);
                fprintf(fp, "%lu,",b->icache[READ_MISS]);
                fprintf(fp, "%lu,",b->icache[EVICT]);
                fprintf(fp, "%lu,",b->dcache[WRITE_HIT]);
                fprintf(fp, "%lu,",b->dcache[WRITE_MISS]);
                fprintf(fp, "%lu,",b->dcache[READ_HIT]);
                fprintf(fp, "%lu,",b->dcache[READ_MISS]);
                fprintf(fp, "%lu,",b->dcache[EVICT]);
                fprintf(fp, "%lu,",b->l2cache[WRITE_HIT]);
                fprintf(fp, "%lu,",b->l2cache[WRITE_MISS]);
                fprintf(fp, "%lu,",b->l2cache[READ_HIT]);
                fprintf(fp, "%lu,",b->l2cache[READ_MISS]);
                fprintf(fp, "%lu,",b->l2cache[EVICT]);
                fprintf(fp, "%lu,",b->tagcache[WRITE_HIT]);
                fprintf(fp, "%lu,",b->tagcache[WRITE_MISS]);
                fprintf(fp, "%lu,",b->tagcache[READ_HIT]);
                fprintf(fp, "%lu,",b->tagcache[READ_MISS]);
                fprintf(fp, "%lu,",b->tagcache[EVICT]);
                fprintf(fp, "%lu,",b->mipsmem[BYTE_READ]);
                fprintf(fp, "%lu,",b->mipsmem[BYTE_WRITE]);
                fprintf(fp, "%lu,",b->mipsmem[HWORD_READ]);
                fprintf(fp, "%lu,",b->mipsmem[HWORD_WRITE]);
                fprintf(fp, "%lu,",b->mipsmem[WORD_READ]);
                fprintf(fp, "%lu,",b->mipsmem[WORD_WRITE]);
                fprintf(fp, "%lu,",b->mipsmem[DWORD_READ]);
                fprintf(fp, "%lu,",b->mipsmem[DWORD_WRITE]);
                fprintf(fp, "%lu,",b->mipsmem[CAP_READ]);
                fprintf(fp, "%lu",b->mipsmem[CAP_WRITE]);
                fprintf(fp, "\n");
            }
            else
            {
                fprintf(fp, "===== %s =====\n",getprogname());
                fprintf(fp, "cycles:             \t%lu\n",b->cycle);
                fprintf(fp, "instructions:       \t%lu\n",b->inst);
                fprintf(fp, "itlb_miss:          \t%lu\n",b->itlb_miss);
                fprintf(fp, "dtlb_miss:          \t%lu\n",b->dtlb_miss);
                fprintf(fp, "\n");
                fprintf(fp, "icache_write_hit:   \t%lu\n",b->icache[WRITE_HIT]);
                fprintf(fp, "icache_write_miss:  \t%lu\n",b->icache[WRITE_MISS]);
                fprintf(fp, "icache_read_hit:    \t%lu\n",b->icache[READ_HIT]);
                fprintf(fp, "icache_read_miss:   \t%lu\n",b->icache[READ_MISS]);
                fprintf(fp, "icache_evict:       \t%lu\n",b->icache[EVICT]);
                fprintf(fp, "\n");
                fprintf(fp, "dcache_write_hit:   \t%lu\n",b->dcache[WRITE_HIT]);
                fprintf(fp, "dcache_write_miss:  \t%lu\n",b->dcache[WRITE_MISS]);
                fprintf(fp, "dcache_read_hit:    \t%lu\n",b->dcache[READ_HIT]);
                fprintf(fp, "dcache_read_miss:   \t%lu\n",b->dcache[READ_MISS]);
                fprintf(fp, "dcache_evict:       \t%lu\n",b->dcache[EVICT]);
                fprintf(fp, "\n");
                fprintf(fp, "l2cache_write_hit:  \t%lu\n",b->l2cache[WRITE_HIT]);
                fprintf(fp, "l2cache_write_miss: \t%lu\n",b->l2cache[WRITE_MISS]);
                fprintf(fp, "l2cache_read_hit:   \t%lu\n",b->l2cache[READ_HIT]);
                fprintf(fp, "l2cache_read_miss:  \t%lu\n",b->l2cache[READ_MISS]);
                fprintf(fp, "l2cache_evict:      \t%lu\n",b->l2cache[EVICT]);
                fprintf(fp, "\n");
                fprintf(fp, "tagcache_write_hit: \t%lu\n",b->tagcache[WRITE_HIT]);
                fprintf(fp, "tagcache_write_miss:\t%lu\n",b->tagcache[WRITE_MISS]);
                fprintf(fp, "tagcache_read_hit:  \t%lu\n",b->tagcache[READ_HIT]);
                fprintf(fp, "tagcache_read_miss: \t%lu\n",b->tagcache[READ_MISS]);
                fprintf(fp, "tagcache_evict:     \t%lu\n",b->tagcache[EVICT]);
                fprintf(fp, "\n");
                fprintf(fp, "mem_byte_read:      \t%lu\n",b->mipsmem[BYTE_READ]);
                fprintf(fp, "mem_byte_write:     \t%lu\n",b->mipsmem[BYTE_WRITE]);
                fprintf(fp, "mem_hword_read:     \t%lu\n",b->mipsmem[HWORD_READ]);
                fprintf(fp, "mem_hword_write:    \t%lu\n",b->mipsmem[HWORD_WRITE]);
                fprintf(fp, "mem_word_read:      \t%lu\n",b->mipsmem[WORD_READ]);
                fprintf(fp, "mem_word_write:     \t%lu\n",b->mipsmem[WORD_WRITE]);
                fprintf(fp, "mem_dword_read:     \t%lu\n",b->mipsmem[DWORD_READ]);
                fprintf(fp, "mem_dword_write:    \t%lu\n",b->mipsmem[DWORD_WRITE]);
                fprintf(fp, "mem_cap_read:       \t%lu\n",b->mipsmem[CAP_READ]);
                fprintf(fp, "mem_cap_write:      \t%lu\n",b->mipsmem[CAP_WRITE]);
                fprintf(fp, "\n");
            }
        }
    }
}

// C constructor / atexit interface
//////////////////////////////////////////////////////////////////////////////

static statcounters_bank_t start_cnt;
static statcounters_bank_t end_cnt;
static statcounters_bank_t diff_cnt;

static void end_sample (void);

__attribute__((constructor))
static void start_sample (void)
{
    atexit(end_sample);
    //printf("resetting stat counters\n");
    //resetStatCounters();
    //printf("initial sampling\n");
    sample_statcounters(&start_cnt);
}

//__attribute__((destructor)) static void end_sample (void)
static void end_sample (void)
{
    //printf("final sampling\n");
    sample_statcounters(&end_cnt); // TODO change the order of sampling to keep cycle sampled early
    diff_statcounters(&end_cnt,&start_cnt,&diff_cnt);
    dump_statcounters(&diff_cnt,getenv("STATCOUNTERS_OUTPUT"),getenv("STATCOUNTERS_FORMAT"));
}
