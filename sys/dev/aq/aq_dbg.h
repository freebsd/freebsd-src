/**
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3) The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file aq_dbg.h
 * Debug print macros & definitions.
 * @date 2017.12.07  @author roman.agafonov@aquantia.com
 */
#ifndef AQ_DBG_H
#define AQ_DBG_H

#include <sys/syslog.h>
#include <sys/systm.h>
/*
Debug levels:
0 - no debug
1 - important warnings
2 - debug prints
3 - trace function calls
4 - dump descriptor
*/

#define AQ_CFG_DEBUG_LVL   0x0

#define AQ_DBG_ERROR(string, args...) printf( "atlantic: " string "\n", ##args)

/* Debug stuff */
#if AQ_CFG_DEBUG_LVL > 0
#define AQ_DBG_WARNING(string, args...) printf( "atlantic: " string "\n", ##args)
#else
#define AQ_DBG_WARNING(string, ...)
#endif

#if AQ_CFG_DEBUG_LVL > 1
#define AQ_DBG_PRINT(string, args...) printf( "atlantic: " string "\n", ##args)
#else
#define AQ_DBG_PRINT(string, ...)
#endif

#if AQ_CFG_DEBUG_LVL > 2
#define AQ_DBG_ENTER() printf( "atlantic: %s() {\n", __func__)
#define AQ_DBG_ENTERA(s, args...) printf( "atlantic: %s(" s ") {\n", __func__, ##args)
#define AQ_DBG_EXIT(err) printf( "atlantic: } %s(), err=%d\n", __func__, err)
#else
#define AQ_DBG_ENTER()
#define AQ_DBG_ENTERA(s, args...)
#define AQ_DBG_EXIT(err)
#endif

#if AQ_CFG_DEBUG_LVL > 2
#define AQ_DBG_DUMP_DESC(desc) { \
            volatile u8 *raw = (volatile u8*)(desc); \
            printf( "07-00 %02X%02X%02X%02X %02X%02X%02X%02X 15-08 %02X%02X%02X%02X %02X%02X%02X%02X\n", \
                raw[7], raw[6], raw[5], raw[4], raw[3], raw[2], raw[1], raw[0], \
                raw[15], raw[14], raw[13], raw[12], raw[11], raw[10], raw[9], raw[8]); \
}\

#else
#define AQ_DBG_DUMP_DESC(desc)
#endif

typedef enum aq_debug_level
{
    lvl_error = LOG_ERR,
    lvl_warn = LOG_WARNING,
    lvl_trace = LOG_NOTICE,
    lvl_detail = LOG_INFO,
} aq_debug_level;

typedef enum aq_debug_category
{
    dbg_init    = 1,
    dbg_config  = 1 << 1,
    dbg_tx      = 1 << 2,
    dbg_rx      = 1 << 3,
    dbg_intr    = 1 << 4,
    dbg_fw      = 1 << 5,
} aq_debug_category;


#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

extern const aq_debug_level dbg_level_;
extern const u32 dbg_categories_;

#define log_base_(_lvl, _fmt, args...) printf( "atlantic: " _fmt "\n", ##args)

#if AQ_CFG_DEBUG_LVL > 0
#define trace_base_(_lvl, _cat, _fmt, args...) do { if (dbg_level_ >= _lvl && (_cat & dbg_categories_)) { printf( "atlantic: " _fmt " @%s,%d\n", ##args, __FILENAME__, __LINE__); }} while (0)
#else
#define trace_base_(_lvl, _cat, _fmt, ...) do {} while (0)
#endif // AQ_CFG_DEBUG_LVL > 0

#define aq_log_error(_fmt, args...)    log_base_(lvl_error, "[!] " _fmt, ##args)
#define aq_log_warn(_fmt, args...)     log_base_(lvl_warn, "/!\\ " _fmt, ##args)
#define aq_log(_fmt, args...)          log_base_(lvl_trace, _fmt, ##args)
#define aq_log_detail(_fmt, args...)   log_base_(lvl_detail, _fmt, ##args)

#define trace_error(_cat,_fmt, args...)   trace_base_(lvl_error, _cat, "[!] " _fmt, ##args)
#define trace_warn(_cat, _fmt, args...)   trace_base_(lvl_warn, _cat, "/!\\ " _fmt, ##args)
#define trace(_cat, _fmt, args...)   trace_base_(lvl_trace, _cat, _fmt, ##args)
#define trace_detail(_cat, _fmt, args...)   trace_base_(lvl_detail, _cat, _fmt, ##args)

void trace_aq_tx_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2]);
void trace_aq_rx_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2]);
void trace_aq_tx_context_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2]);
void DumpHex(const void* data, size_t size);

#endif // AQ_DBG_H
