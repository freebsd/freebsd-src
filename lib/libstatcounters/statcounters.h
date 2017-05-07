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

#ifndef STATCOUNTERS_H
#define STATCOUNTERS_H

#ifndef __has_extension
#define __has_extension(x) 0
#endif
#if __has_extension(attribute_deprecated_with_message)
#define DEPRECATED(x) __attribute__((deprecated(x)))
#else
#define DEPRECATED(x) __attribute__((deprecated))
#endif

// counters bank
#define STATCOUNTERS_MAX_MOD_CNT 10
typedef struct statcounters_bank
{
    uint64_t itlb_miss;
    uint64_t dtlb_miss;
    uint64_t cycle;
    uint64_t inst;
    uint64_t icache[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t dcache[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t l2cache[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t mipsmem[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t tagcache[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t l2cachemaster[STATCOUNTERS_MAX_MOD_CNT];
    uint64_t tagcachemaster[STATCOUNTERS_MAX_MOD_CNT];
} statcounters_bank_t;

// format flags
typedef enum
{
    HUMAN_READABLE,
    CSV_HEADER,
    CSV_NOHEADER
} statcounters_fmt_flag_t;

// reset statcounters XXX this literally resets the hardware counters (allowed
// from user space for convenience but need not to be abused to be usefull)
void reset_statcounters (void) DEPRECATED("use statcounters_reset instead");
void statcounters_reset (void);
// zero a statcounters_bank
void zero_statcounters (statcounters_bank_t * const cnt_bank) DEPRECATED("use statcounters_zero instead");
int statcounters_zero (statcounters_bank_t * const cnt_bank);
// sample hardware counters in a statcounters_bank
void sample_statcounters (statcounters_bank_t * const cnt_bank) DEPRECATED("use statcounters_sample instead");
int statcounters_sample (statcounters_bank_t * const cnt_bank);
// diff two statcounters_banks into a third one
void diff_statcounters (
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs,
    statcounters_bank_t * const bd) DEPRECATED("use statcounters_diff instead -- arguments order changed");
int statcounters_diff (
    statcounters_bank_t * const bd,
    const statcounters_bank_t * const be,
    const statcounters_bank_t * const bs);
// dump a statcounters_bank in a file (csv or human readable)
void dump_statcounters (
    const statcounters_bank_t * const b,
    const char * const fname,
    const char * const fmt) DEPRECATED("use statcounters_dump instead -- arguments changed");
int statcounters_dump (
    const statcounters_bank_t * const b,
    const char * const progname,
    const char * const archname,
    FILE * const fp,
    const statcounters_fmt_flag_t fmt_flg);

#endif
