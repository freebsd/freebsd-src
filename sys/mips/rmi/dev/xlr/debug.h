/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD
 * $FreeBSD$
 */
#ifndef _RMI_DEBUG_H_
#define _RMI_DEBUG_H_

#include <machine/atomic.h>

enum {
	//cacheline 0
	MSGRNG_INT,
	MSGRNG_PIC_INT,
	MSGRNG_MSG,
	MSGRNG_EXIT_STATUS,
	MSGRNG_MSG_CYCLES,
	//cacheline 1
	NETIF_TX = 8,
	NETIF_RX,
	NETIF_TX_COMPLETE,
	NETIF_TX_COMPLETE_TX,
	NETIF_RX_CYCLES,
	NETIF_TX_COMPLETE_CYCLES,
	NETIF_TX_CYCLES,
	NETIF_TIMER_START_Q,
	//NETIF_REG_FRIN,
	//NETIF_INT_REG,
	//cacheline 2
	REPLENISH_ENTER = 16,
	REPLENISH_ENTER_COUNT,
	REPLENISH_CPU,
	REPLENISH_FRIN,
	REPLENISH_CYCLES,
	NETIF_STACK_TX,
	NETIF_START_Q,
	NETIF_STOP_Q,
	//cacheline 3
	USER_MAC_START = 24,
	USER_MAC_INT = 24,
	USER_MAC_TX_COMPLETE,
	USER_MAC_RX,
	USER_MAC_POLL,
	USER_MAC_TX,
	USER_MAC_TX_FAIL,
	USER_MAC_TX_COUNT,
	USER_MAC_FRIN,
	//cacheline 4
	USER_MAC_TX_FAIL_GMAC_CREDITS = 32,
	USER_MAC_DO_PAGE_FAULT,
	USER_MAC_UPDATE_TLB,
	USER_MAC_UPDATE_BIGTLB,
	USER_MAC_UPDATE_TLB_PFN0,
	USER_MAC_UPDATE_TLB_PFN1,

	XLR_MAX_COUNTERS = 40
};
extern int xlr_counters[MAXCPU][XLR_MAX_COUNTERS];
extern __uint32_t msgrng_msg_cycles;

#ifdef ENABLE_DEBUG
#define xlr_inc_counter(x) atomic_add_int(&xlr_counters[PCPU_GET(cpuid)][(x)], 1)
#define xlr_dec_counter(x) atomic_subtract_int(&xlr_counters[PCPU_GET(cpuid)][(x)], 1)
#define xlr_set_counter(x, value) atomic_set_int(&xlr_counters[PCPU_GET(cpuid)][(x)], (value))
#define xlr_get_counter(x) (&xlr_counters[0][(x)])

#else				/* default mode */

#define xlr_inc_counter(x)
#define xlr_dec_counter(x)
#define xlr_set_counter(x, value)
#define xlr_get_counter(x)

#endif

#define dbg_msg(fmt, args...) printf(fmt, ##args)
#define dbg_panic(fmt, args...) panic(fmt, ##args)

#endif
