/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2009 Neelkanth Natu
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

#ifndef _SB_SCD_H_
#define	_SB_SCD_H_

#define	NUM_INTSRC		64	/* total number of interrupt sources */

uint64_t	sb_zbbus_cycle_count(void);
uint64_t	sb_cpu_speed(void);
void		sb_system_reset(void);

int		sb_route_intsrc(int src);
void		sb_enable_intsrc(int cpu, int src);
void		sb_disable_intsrc(int cpu, int src);
uint64_t	sb_read_intsrc_mask(int cpu);
void		sb_write_intsrc_mask(int cpu, uint64_t mask);
void		sb_write_intmap(int cpu, int intsrc, int intrnum);
int		sb_read_intmap(int cpu, int intsrc);

#ifdef SMP
#define	INTSRC_MAILBOX3		29
void		sb_set_mailbox(int cpuid, uint64_t val);
void		sb_clear_mailbox(int cpuid, uint64_t val);
#endif

#endif	/* _SB_SCD_H_ */
