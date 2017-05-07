/*-
 * Copyright (c) 2016-2017 Alexandre Joannou
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
#include "statcounters.h"

// low level rdhwr access to counters
//////////////////////////////////////////////////////////////////////////////

// TODO the itlbmiss/dtlbmiss/cycle/inst counters are not reset with that
static inline void resetStatCounters (void)
{
    __asm __volatile(".word (0x1F << 26) | (0x0 << 21) | (0x0 << 16) | (0x7 << 11) | (0x0 << 6) | (0x3B)");
}

#define DEFINE_GET_STAT_COUNTER(name,X,Y)   \
static inline uint64_t get_##name##_count (void)   \
{                                           \
    uint64_t ret;                           \
    __asm __volatile(".word (0x1f << 26) | (0x0 << 21) | (12 << 16) | ("#X" << 11) | ( "#Y"  << 6) | 0x3b\n\tmove %0,$12" : "=r" (ret) :: "$12"); \
    return ret;                             \
}

// available modules, module type, associated rdhw primary selector
//----------------------------------------------------------------------------
// instruction cache, CacheCore, 8
// data cache, CacheCore, 9
// level 2 cache, CacheCore, 10
// memory operations, MIPSMem, 11
// tag cache, CacheCore, 12
// level 2 cache master, MasterStats, 13
// tag cache master, MasterStats, 14

// module type : CacheCore
// available counter, associated rdhwr secondary selector
//----------------------------------------------------------------------------
// write hit, 0
// write miss, 1
// read hit, 2
// read miss, 3
// prefetch hit, 4
// prefetch miss, 5
// eviction, 6
// eviction due to prefetch, 7
// write of tag set, 8
// read of tag set, 9
enum
{
    STATCOUNTERS_WRITE_HIT     = 0,
    STATCOUNTERS_WRITE_MISS    = 1,
    STATCOUNTERS_READ_HIT      = 2,
    STATCOUNTERS_READ_MISS     = 3,
    STATCOUNTERS_PFTCH_HIT     = 4,
    STATCOUNTERS_PFTCH_MISS    = 5,
    STATCOUNTERS_EVICT         = 6,
    STATCOUNTERS_PFTCH_EVICT   = 7,
    STATCOUNTERS_SET_TAG_WRITE = 8,
    STATCOUNTERS_SET_TAG_READ  = 9
};
//----------------------------------------------------------------------------
// module type : MIPSMem
// available counter, associated rdhwr secondary selector
//----------------------------------------------------------------------------
// byte read, 0
// byte write, 1
// half word read, 2
// half word write, 3
// word read, 4
// word write, 5
// double word read, 6
// double word write, 7
// capability read, 8
// capability write, 9
enum
{
    STATCOUNTERS_BYTE_READ   = 0,
    STATCOUNTERS_BYTE_WRITE  = 1,
    STATCOUNTERS_HWORD_READ  = 2,
    STATCOUNTERS_HWORD_WRITE = 3,
    STATCOUNTERS_WORD_READ   = 4,
    STATCOUNTERS_WORD_WRITE  = 5,
    STATCOUNTERS_DWORD_READ  = 6,
    STATCOUNTERS_DWORD_WRITE = 7,
    STATCOUNTERS_CAP_READ    = 8,
    STATCOUNTERS_CAP_WRITE   = 9
};
//----------------------------------------------------------------------------
// module type : MasterStats
// available counter, associated rdhwr secondary selector
//----------------------------------------------------------------------------
// read request, 0
// write request, 1
// write request flit, 2
// read response, 3
// read response flit, 4
// write response, 5
enum
{
    STATCOUNTERS_READ_REQ       = 0,
    STATCOUNTERS_WRITE_REQ      = 1,
    STATCOUNTERS_WRITE_REQ_FLIT = 2,
    STATCOUNTERS_READ_RSP       = 3,
    STATCOUNTERS_READ_RSP_FLIT  = 4,
    STATCOUNTERS_WRITE_RSP      = 5
};

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
DEFINE_GET_STAT_COUNTER(dcache_set_tag_write,9,8);
DEFINE_GET_STAT_COUNTER(dcache_set_tag_read,9,9);
DEFINE_GET_STAT_COUNTER(l2cache_write_hit,10,0);
DEFINE_GET_STAT_COUNTER(l2cache_write_miss,10,1);
DEFINE_GET_STAT_COUNTER(l2cache_read_hit,10,2);
DEFINE_GET_STAT_COUNTER(l2cache_read_miss,10,3);
DEFINE_GET_STAT_COUNTER(l2cache_evict,10,6);
DEFINE_GET_STAT_COUNTER(l2cache_set_tag_write,10,8);
DEFINE_GET_STAT_COUNTER(l2cache_set_tag_read,10,9);
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
DEFINE_GET_STAT_COUNTER(l2cachemaster_read_req,13,0);
DEFINE_GET_STAT_COUNTER(l2cachemaster_write_req,13,1);
DEFINE_GET_STAT_COUNTER(l2cachemaster_write_req_flit,13,2);
DEFINE_GET_STAT_COUNTER(l2cachemaster_read_rsp,13,3);
DEFINE_GET_STAT_COUNTER(l2cachemaster_read_rsp_flit,13,4);
DEFINE_GET_STAT_COUNTER(l2cachemaster_write_rsp,13,5);
DEFINE_GET_STAT_COUNTER(tagcachemaster_read_req,14,0);
DEFINE_GET_STAT_COUNTER(tagcachemaster_write_req,14,1);
DEFINE_GET_STAT_COUNTER(tagcachemaster_write_req_flit,14,2);
DEFINE_GET_STAT_COUNTER(tagcachemaster_read_rsp,14,3);
DEFINE_GET_STAT_COUNTER(tagcachemaster_read_rsp_flit,14,4);
DEFINE_GET_STAT_COUNTER(tagcachemaster_write_rsp,14,5);

// helper functions

#define ARCHNAME_BUFF_SZ 32

static void getarchname (char * restrict archname, const char * const dflt)
{
    FILE *fp;
    char name[ARCHNAME_BUFF_SZ];
    fp = popen("uname -m", "r");
    if (fp != NULL && fgets(name, sizeof(name)-1, fp) !=NULL)
    {
        char * tmp = strdup(name);
        char * ret = strsep(&tmp,"\n");
        strncpy(archname,ret,ARCHNAME_BUFF_SZ);
        free(tmp);
    }
    else
        strncpy(archname,dflt,ARCHNAME_BUFF_SZ);
    pclose(fp);
}

// libstatcounters API
//////////////////////////////////////////////////////////////////////////////

// reset the hardware statcounters
void reset_statcounters (void)
{
    statcounters_reset();
}
void statcounters_reset (void)
{
    resetStatCounters();
}

// zero a statcounters_bank
void zero_statcounters (statcounters_bank_t * const cnt_bank)
{
    statcounters_zero(cnt_bank);
}
int statcounters_zero (statcounters_bank_t * const cnt_bank)
{
    if (cnt_bank == NULL)
        return -1;
    memset(cnt_bank, 0, sizeof(statcounters_bank_t));
    return 0;
}

// sample hardware counters in a statcounters_bank
void sample_statcounters (statcounters_bank_t * const cnt_bank)
{
    statcounters_sample(cnt_bank);
}
int statcounters_sample (statcounters_bank_t * const cnt_bank)
{
    if (cnt_bank == NULL)
        return -1;
    cnt_bank->icache[STATCOUNTERS_WRITE_HIT]              = get_icache_write_hit_count();
    cnt_bank->icache[STATCOUNTERS_WRITE_MISS]             = get_icache_write_miss_count();
    cnt_bank->icache[STATCOUNTERS_READ_HIT]               = get_icache_read_hit_count();
    cnt_bank->icache[STATCOUNTERS_READ_MISS]              = get_icache_read_miss_count();
    cnt_bank->icache[STATCOUNTERS_EVICT]                  = get_icache_evict_count();
    cnt_bank->dcache[STATCOUNTERS_WRITE_HIT]              = get_dcache_write_hit_count();
    cnt_bank->dcache[STATCOUNTERS_WRITE_MISS]             = get_dcache_write_miss_count();
    cnt_bank->dcache[STATCOUNTERS_READ_HIT]               = get_dcache_read_hit_count();
    cnt_bank->dcache[STATCOUNTERS_READ_MISS]              = get_dcache_read_miss_count();
    cnt_bank->dcache[STATCOUNTERS_EVICT]                  = get_dcache_evict_count();
    cnt_bank->dcache[STATCOUNTERS_SET_TAG_WRITE]          = get_dcache_set_tag_write_count();
    cnt_bank->dcache[STATCOUNTERS_SET_TAG_READ]           = get_dcache_set_tag_read_count();
    cnt_bank->l2cache[STATCOUNTERS_WRITE_HIT]             = get_l2cache_write_hit_count();
    cnt_bank->l2cache[STATCOUNTERS_WRITE_MISS]            = get_l2cache_write_miss_count();
    cnt_bank->l2cache[STATCOUNTERS_READ_HIT]              = get_l2cache_read_hit_count();
    cnt_bank->l2cache[STATCOUNTERS_READ_MISS]             = get_l2cache_read_miss_count();
    cnt_bank->l2cache[STATCOUNTERS_EVICT]                 = get_l2cache_evict_count();
    cnt_bank->l2cache[STATCOUNTERS_SET_TAG_WRITE]         = get_l2cache_set_tag_write_count();
    cnt_bank->l2cache[STATCOUNTERS_SET_TAG_READ]          = get_l2cache_set_tag_read_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_READ_REQ]        = get_l2cachemaster_read_req_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_WRITE_REQ]       = get_l2cachemaster_write_req_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_WRITE_REQ_FLIT]  = get_l2cachemaster_write_req_flit_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_READ_RSP]        = get_l2cachemaster_read_rsp_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_READ_RSP_FLIT]   = get_l2cachemaster_read_rsp_flit_count();
    cnt_bank->l2cachemaster[STATCOUNTERS_WRITE_RSP]       = get_l2cachemaster_write_rsp_count();
    cnt_bank->tagcache[STATCOUNTERS_WRITE_HIT]            = get_tagcache_write_hit_count();
    cnt_bank->tagcache[STATCOUNTERS_WRITE_MISS]           = get_tagcache_write_miss_count();
    cnt_bank->tagcache[STATCOUNTERS_READ_HIT]             = get_tagcache_read_hit_count();
    cnt_bank->tagcache[STATCOUNTERS_READ_MISS]            = get_tagcache_read_miss_count();
    cnt_bank->tagcache[STATCOUNTERS_EVICT]                = get_tagcache_evict_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_READ_REQ]       = get_tagcachemaster_read_req_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_WRITE_REQ]      = get_tagcachemaster_write_req_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_WRITE_REQ_FLIT] = get_tagcachemaster_write_req_flit_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_READ_RSP]       = get_tagcachemaster_read_rsp_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_READ_RSP_FLIT]  = get_tagcachemaster_read_rsp_flit_count();
    cnt_bank->tagcachemaster[STATCOUNTERS_WRITE_RSP]      = get_tagcachemaster_write_rsp_count();
    cnt_bank->mipsmem[STATCOUNTERS_BYTE_READ]             = get_mem_byte_read_count();
    cnt_bank->mipsmem[STATCOUNTERS_BYTE_WRITE]            = get_mem_byte_write_count();
    cnt_bank->mipsmem[STATCOUNTERS_HWORD_READ]            = get_mem_hword_read_count();
    cnt_bank->mipsmem[STATCOUNTERS_HWORD_WRITE]           = get_mem_hword_write_count();
    cnt_bank->mipsmem[STATCOUNTERS_WORD_READ]             = get_mem_word_read_count();
    cnt_bank->mipsmem[STATCOUNTERS_WORD_WRITE]            = get_mem_word_write_count();
    cnt_bank->mipsmem[STATCOUNTERS_DWORD_READ]            = get_mem_dword_read_count();
    cnt_bank->mipsmem[STATCOUNTERS_DWORD_WRITE]           = get_mem_dword_write_count();
    cnt_bank->mipsmem[STATCOUNTERS_CAP_READ]              = get_mem_cap_read_count();
    cnt_bank->mipsmem[STATCOUNTERS_CAP_WRITE]             = get_mem_cap_write_count();
    cnt_bank->dtlb_miss                                   = get_dtlb_miss_count();
    cnt_bank->itlb_miss                                   = get_itlb_miss_count();
    cnt_bank->inst                                        = get_inst_count();
    cnt_bank->cycle                                       = get_cycle_count();
    return 0;
}

// diff two statcounters_banks into a third one
void diff_statcounters (
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs,
    statcounters_bank_t * const bd)
{
    statcounters_diff(bd,be,bs);
}
int statcounters_diff (
    statcounters_bank_t * const bd,
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs)
{
    if (bd == NULL || be == NULL || bs == NULL)
        return -1;
    bd->itlb_miss    = be->itlb_miss - bs->itlb_miss;
    bd->dtlb_miss    = be->dtlb_miss - bs->dtlb_miss;
    bd->cycle        = be->cycle - bs->cycle;
    bd->inst         = be->inst - bs->inst;
    for (int i = 0; i < STATCOUNTERS_MAX_MOD_CNT; i++)
    {
        bd->icache[i]         = be->icache[i] - bs->icache[i];
        bd->dcache[i]         = be->dcache[i] - bs->dcache[i];
        bd->l2cache[i]        = be->l2cache[i] - bs->l2cache[i];
        bd->mipsmem[i]        = be->mipsmem[i] - bs->mipsmem[i];
        bd->tagcache[i]       = be->tagcache[i] - bs->tagcache[i];
        bd->l2cachemaster[i]  = be->l2cachemaster[i] - bs->l2cachemaster[i];
        bd->tagcachemaster[i] = be->tagcachemaster[i] - bs->tagcachemaster[i];
    }
    return 0;
}

// dump a statcounters_bank in a file (csv or human readable)
void dump_statcounters (
    const statcounters_bank_t * const b,
    const char * const fname,
    const char * const fmt)
{
    FILE * fp = NULL;
    bool display_header = true;
    statcounters_fmt_flag_t flg = HUMAN_READABLE;
    if (fname && access(fname, F_OK) != -1)
        display_header = false;
    if (fname && (fp = fopen(fname, "a")))
    {
        if (fmt && (strcmp(fmt,"csv") == 0))
        {
            if (display_header) flg = CSV_HEADER;
            else flg = CSV_NOHEADER;
        }
        char tmp[ARCHNAME_BUFF_SZ];
	getarchname(tmp,"unknown_arch");
        statcounters_dump(b,getprogname(),tmp,fp,flg);
        fclose(fp);
    }
}
int statcounters_dump (
    const statcounters_bank_t * const b,
    const char * const progname,
    const char * const archname,
    FILE * const fp,
    const statcounters_fmt_flag_t fmt_flg)
{
    if (b == NULL || fp == NULL)
        return -1;
    switch (fmt_flg)
    {
        case CSV_HEADER:
            fprintf(fp, "progname,");
            fprintf(fp, "archname,");
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
            fprintf(fp, "dcache_set_tag_write,");
            fprintf(fp, "dcache_set_tag_read,");
            fprintf(fp, "l2cache_write_hit,");
            fprintf(fp, "l2cache_write_miss,");
            fprintf(fp, "l2cache_read_hit,");
            fprintf(fp, "l2cache_read_miss,");
            fprintf(fp, "l2cache_evict,");
            fprintf(fp, "l2cache_set_tag_write,");
            fprintf(fp, "l2cache_set_tag_read,");
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
            fprintf(fp, "mipsmem_cap_write,");
            fprintf(fp, "l2cachemaster_read_req,");
            fprintf(fp, "l2cachemaster_write_req,");
            fprintf(fp, "l2cachemaster_write_req_flit,");
            fprintf(fp, "l2cachemaster_read_rsp,");
            fprintf(fp, "l2cachemaster_read_rsp_flit,");
            fprintf(fp, "l2cachemaster_write_rsp,");
            fprintf(fp, "tagcachemaster_read_req,");
            fprintf(fp, "tagcachemaster_write_req,");
            fprintf(fp, "tagcachemaster_write_req_flit,");
            fprintf(fp, "tagcachemaster_read_rsp,");
            fprintf(fp, "tagcachemaster_read_rsp_flit,");
            fprintf(fp, "tagcachemaster_write_rsp");
            fprintf(fp, "\n");
        case CSV_NOHEADER:
            fprintf(fp, "%s,",progname);
            fprintf(fp, "%s,",archname);
            fprintf(fp, "%lu,",b->cycle);
            fprintf(fp, "%lu,",b->inst);
            fprintf(fp, "%lu,",b->itlb_miss);
            fprintf(fp, "%lu,",b->dtlb_miss);
            fprintf(fp, "%lu,",b->icache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "%lu,",b->icache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "%lu,",b->icache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "%lu,",b->icache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "%lu,",b->icache[STATCOUNTERS_EVICT]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_EVICT]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_SET_TAG_WRITE]);
            fprintf(fp, "%lu,",b->dcache[STATCOUNTERS_SET_TAG_READ]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_EVICT]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_SET_TAG_WRITE]);
            fprintf(fp, "%lu,",b->l2cache[STATCOUNTERS_SET_TAG_READ]);
            fprintf(fp, "%lu,",b->tagcache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "%lu,",b->tagcache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "%lu,",b->tagcache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "%lu,",b->tagcache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "%lu,",b->tagcache[STATCOUNTERS_EVICT]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_BYTE_READ]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_BYTE_WRITE]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_HWORD_READ]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_HWORD_WRITE]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_WORD_READ]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_WORD_WRITE]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_DWORD_READ]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_DWORD_WRITE]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_CAP_READ]);
            fprintf(fp, "%lu,",b->mipsmem[STATCOUNTERS_CAP_WRITE]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_READ_REQ]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_WRITE_REQ]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_WRITE_REQ_FLIT]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_READ_RSP]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_READ_RSP_FLIT]);
            fprintf(fp, "%lu,",b->l2cachemaster[STATCOUNTERS_WRITE_RSP]);
            fprintf(fp, "%lu,",b->tagcachemaster[STATCOUNTERS_READ_REQ]);
            fprintf(fp, "%lu,",b->tagcachemaster[STATCOUNTERS_WRITE_REQ]);
            fprintf(fp, "%lu,",b->tagcachemaster[STATCOUNTERS_WRITE_REQ_FLIT]);
            fprintf(fp, "%lu,",b->tagcachemaster[STATCOUNTERS_READ_RSP]);
            fprintf(fp, "%lu,",b->tagcachemaster[STATCOUNTERS_READ_RSP_FLIT]);
            fprintf(fp, "%lu",b->tagcachemaster[STATCOUNTERS_WRITE_RSP]);
            fprintf(fp, "\n");
            break;
        case HUMAN_READABLE:
        default:
            fprintf(fp, "===== %s -- %s =====\n",progname, archname);
            fprintf(fp, "cycles:                       \t%lu\n",b->cycle);
            fprintf(fp, "instructions:                 \t%lu\n",b->inst);
            fprintf(fp, "itlb_miss:                    \t%lu\n",b->itlb_miss);
            fprintf(fp, "dtlb_miss:                    \t%lu\n",b->dtlb_miss);
            fprintf(fp, "\n");
            fprintf(fp, "icache_write_hit:             \t%lu\n",b->icache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "icache_write_miss:            \t%lu\n",b->icache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "icache_read_hit:              \t%lu\n",b->icache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "icache_read_miss:             \t%lu\n",b->icache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "icache_evict:                 \t%lu\n",b->icache[STATCOUNTERS_EVICT]);
            fprintf(fp, "\n");
            fprintf(fp, "dcache_write_hit:             \t%lu\n",b->dcache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "dcache_write_miss:            \t%lu\n",b->dcache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "dcache_read_hit:              \t%lu\n",b->dcache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "dcache_read_miss:             \t%lu\n",b->dcache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "dcache_evict:                 \t%lu\n",b->dcache[STATCOUNTERS_EVICT]);
            fprintf(fp, "dcache_set_tag_write:         \t%lu\n",b->dcache[STATCOUNTERS_SET_TAG_WRITE]);
            fprintf(fp, "dcache_set_tag_read:          \t%lu\n",b->dcache[STATCOUNTERS_SET_TAG_READ]);
            fprintf(fp, "\n");
            fprintf(fp, "l2cache_write_hit:            \t%lu\n",b->l2cache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "l2cache_write_miss:           \t%lu\n",b->l2cache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "l2cache_read_hit:             \t%lu\n",b->l2cache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "l2cache_read_miss:            \t%lu\n",b->l2cache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "l2cache_evict:                \t%lu\n",b->l2cache[STATCOUNTERS_EVICT]);
            fprintf(fp, "l2cache_set_tag_write:        \t%lu\n",b->l2cache[STATCOUNTERS_SET_TAG_WRITE]);
            fprintf(fp, "l2cache_set_tag_read:         \t%lu\n",b->l2cache[STATCOUNTERS_SET_TAG_READ]);
            fprintf(fp, "\n");
            fprintf(fp, "tagcache_write_hit:           \t%lu\n",b->tagcache[STATCOUNTERS_WRITE_HIT]);
            fprintf(fp, "tagcache_write_miss:          \t%lu\n",b->tagcache[STATCOUNTERS_WRITE_MISS]);
            fprintf(fp, "tagcache_read_hit:            \t%lu\n",b->tagcache[STATCOUNTERS_READ_HIT]);
            fprintf(fp, "tagcache_read_miss:           \t%lu\n",b->tagcache[STATCOUNTERS_READ_MISS]);
            fprintf(fp, "tagcache_evict:               \t%lu\n",b->tagcache[STATCOUNTERS_EVICT]);
            fprintf(fp, "\n");
            fprintf(fp, "mem_byte_read:                \t%lu\n",b->mipsmem[STATCOUNTERS_BYTE_READ]);
            fprintf(fp, "mem_byte_write:               \t%lu\n",b->mipsmem[STATCOUNTERS_BYTE_WRITE]);
            fprintf(fp, "mem_hword_read:               \t%lu\n",b->mipsmem[STATCOUNTERS_HWORD_READ]);
            fprintf(fp, "mem_hword_write:              \t%lu\n",b->mipsmem[STATCOUNTERS_HWORD_WRITE]);
            fprintf(fp, "mem_word_read:                \t%lu\n",b->mipsmem[STATCOUNTERS_WORD_READ]);
            fprintf(fp, "mem_word_write:               \t%lu\n",b->mipsmem[STATCOUNTERS_WORD_WRITE]);
            fprintf(fp, "mem_dword_read:               \t%lu\n",b->mipsmem[STATCOUNTERS_DWORD_READ]);
            fprintf(fp, "mem_dword_write:              \t%lu\n",b->mipsmem[STATCOUNTERS_DWORD_WRITE]);
            fprintf(fp, "mem_cap_read:                 \t%lu\n",b->mipsmem[STATCOUNTERS_CAP_READ]);
            fprintf(fp, "mem_cap_write:                \t%lu\n",b->mipsmem[STATCOUNTERS_CAP_WRITE]);
            fprintf(fp, "\n");
            fprintf(fp, "l2cachemaster_read_req:       \t%lu\n",b->l2cachemaster[STATCOUNTERS_READ_REQ]);
            fprintf(fp, "l2cachemaster_write_req:      \t%lu\n",b->l2cachemaster[STATCOUNTERS_WRITE_REQ]);
            fprintf(fp, "l2cachemaster_write_req_flit: \t%lu\n",b->l2cachemaster[STATCOUNTERS_WRITE_REQ_FLIT]);
            fprintf(fp, "l2cachemaster_read_rsp:       \t%lu\n",b->l2cachemaster[STATCOUNTERS_READ_RSP]);
            fprintf(fp, "l2cachemaster_read_rsp_flit:  \t%lu\n",b->l2cachemaster[STATCOUNTERS_READ_RSP_FLIT]);
            fprintf(fp, "l2cachemaster_write_rsp:      \t%lu\n",b->l2cachemaster[STATCOUNTERS_WRITE_RSP]);
            fprintf(fp, "\n");
            fprintf(fp, "tagcachemaster_read_req:      \t%lu\n",b->tagcachemaster[STATCOUNTERS_READ_REQ]);
            fprintf(fp, "tagcachemaster_write_req:     \t%lu\n",b->tagcachemaster[STATCOUNTERS_WRITE_REQ]);
            fprintf(fp, "tagcachemaster_write_req_flit:\t%lu\n",b->tagcachemaster[STATCOUNTERS_WRITE_REQ_FLIT]);
            fprintf(fp, "tagcachemaster_read_rsp:      \t%lu\n",b->tagcachemaster[STATCOUNTERS_READ_RSP]);
            fprintf(fp, "tagcachemaster_read_rsp_flit: \t%lu\n",b->tagcachemaster[STATCOUNTERS_READ_RSP_FLIT]);
            fprintf(fp, "tagcachemaster_write_rsp:     \t%lu\n",b->tagcachemaster[STATCOUNTERS_WRITE_RSP]);
            fprintf(fp, "\n");
            break;
    }
    return 0;
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
    // registering exit function
    atexit(end_sample);
    // initial sampling
    statcounters_sample(&start_cnt);
}

//__attribute__((destructor)) static void end_sample (void)
static void end_sample (void)
{
    // final sampling
    statcounters_sample(&end_cnt); // TODO change the order of sampling to keep cycle sampled early
    // compute difference between samples
    statcounters_diff(&diff_cnt, &end_cnt, &start_cnt);
    // preparing call to dumping function
    FILE * fp = NULL;
    const char * const pname = getenv("STATCOUNTERS_PROGNAME");
    char pname_arg[128];
    const char * const aname = getenv("STATCOUNTERS_ARCHNAME");
    char aname_arg[ARCHNAME_BUFF_SZ];
    const char * const fname = getenv("STATCOUNTERS_OUTPUT");
    const char * const fmt = getenv("STATCOUNTERS_FORMAT");
    bool display_header = true;
    statcounters_fmt_flag_t fmt_flg = HUMAN_READABLE;
    if (fname && access(fname, F_OK) != -1)
        display_header = false;
    if (fname && (fp = fopen(fname, "a")))
    {
        if (fmt && (strcmp(fmt,"csv") == 0))
        {
            if (display_header) fmt_flg = CSV_HEADER;
            else fmt_flg = CSV_NOHEADER;
        }
	if (pname) strncpy(pname_arg,pname,128);
	else strncpy(pname_arg,getprogname(),128);
	if (aname) strncpy(aname_arg,aname,32);
	else getarchname(aname_arg,"unknown_arch");
	statcounters_dump(&diff_cnt,pname_arg,aname_arg,fp,fmt_flg);
        fclose(fp);
    }
}
