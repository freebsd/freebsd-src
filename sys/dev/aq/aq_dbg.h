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

#include <sys/systm.h>
#include <sys/syslog.h>

#include "aq_device.h"
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
	volatile uint8_t *raw = (volatile uint8_t*)(desc); \
	printf( "07-00 %02X%02X%02X%02X %02X%02X%02X%02X 15-08 %02X%02X%02X%02X %02X%02X%02X%02X\n", \
	    raw[7], raw[6], raw[5], raw[4], raw[3], raw[2], raw[1], raw[0], \
	    raw[15], raw[14], raw[13], raw[12], raw[11], raw[10], raw[9], raw[8]); \
}\

#else
#define AQ_DBG_DUMP_DESC(desc)
#endif

enum aq_debug_level
{
	lvl_error = LOG_ERR,
	lvl_warn = LOG_WARNING,
	lvl_trace = LOG_NOTICE,
	lvl_detail = LOG_INFO,
};

enum aq_debug_category
{
	dbg_init    = 1,
	dbg_config  = 1 << 1,
	dbg_tx      = 1 << 2,
	dbg_rx      = 1 << 3,
	dbg_intr    = 1 << 4,
	dbg_fw      = 1 << 5,
};


#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define AQ_DBG_LEVEL_DEFAULT		lvl_error
#define AQ_DBG_CATEGORIES_DEFAULT	(dbg_init | dbg_config | dbg_tx |	\
					 dbg_rx | dbg_intr | dbg_fw)

/* NULL until aq_if_attach_pre() wires it up; traces run before that. */
#define AQ_DBG_SOFTC(_hw)	((struct aq_dev *)(_hw)->aq_dev)

#define aq_log_base(_hw, _lvl, _fmt, args...) do {			\
	const struct aq_dev *_sc = AQ_DBG_SOFTC(_hw);			\
									\
	if (_sc != NULL && _sc->dbg_level >= (_lvl))			\
		device_printf((_hw)->dev, _fmt "\n", ##args);		\
} while (0)

#define aq_trace_base(_hw, _lvl, _cat, _fmt, args...) do {		\
	const struct aq_dev *_sc = AQ_DBG_SOFTC(_hw);			\
									\
	if (_sc != NULL && _sc->dbg_level >= (_lvl) &&			\
	    ((_cat) & _sc->dbg_categories))				\
		device_printf((_hw)->dev, _fmt " @%s,%d\n", ##args,	\
		    __FILENAME__, __LINE__);				\
} while (0)

#define aq_log_warn(_hw, _fmt, args...)					\
	aq_log_base(_hw, lvl_warn, "/!\\ " _fmt, ##args)
#define aq_log(_hw, _fmt, args...)					\
	aq_log_base(_hw, lvl_trace, _fmt, ##args)
#define aq_log_detail(_hw, _fmt, args...)				\
	aq_log_base(_hw, lvl_detail, _fmt, ##args)

#define trace_error(_hw, _cat, _fmt, args...)				\
	aq_trace_base(_hw, lvl_error, _cat, "[!] " _fmt, ##args)
#define trace_warn(_hw, _cat, _fmt, args...)				\
	aq_trace_base(_hw, lvl_warn, _cat, "/!\\ " _fmt, ##args)
#define trace(_hw, _cat, _fmt, args...)					\
	aq_trace_base(_hw, lvl_trace, _cat, _fmt, ##args)
#define trace_detail(_hw, _cat, _fmt, args...)				\
	aq_trace_base(_hw, lvl_detail, _cat, _fmt, ##args)

#if AQ_CFG_DEBUG_LVL > 2
void trace_aq_tx_descr(struct aq_hw *hw, int ring_idx, unsigned int pointer,
    volatile uint64_t descr[2]);
void trace_aq_rx_descr(struct aq_hw *hw, int ring_idx, unsigned int pointer,
    volatile uint64_t descr[2]);
void trace_aq_tx_context_descr(struct aq_hw *hw, int ring_idx,
    unsigned int pointer, volatile uint64_t descr[2]);
#else
#define trace_aq_tx_descr(...)		((void)0)
#define trace_aq_rx_descr(...)		((void)0)
#define trace_aq_tx_context_descr(...)	((void)0)
#endif

#endif // AQ_DBG_H
