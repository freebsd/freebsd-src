/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/ia64/include/unwind.h,v 1.7.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_UNWIND_H_
#define	_MACHINE_UNWIND_H_

struct pcb;
struct trapframe;
struct uwx_env;

struct unw_regstate {
	struct pcb	*pcb;
	struct trapframe *frame;
	struct uwx_env	*env;
	uint64_t	keyval[8];
};

int unw_create_from_pcb(struct unw_regstate *s, struct pcb *pcb);
int unw_create_from_frame(struct unw_regstate *s, struct trapframe *tf);
void unw_delete(struct unw_regstate *s);
int unw_step(struct unw_regstate *s);

int unw_get_bsp(struct unw_regstate *s, uint64_t *r);
int unw_get_cfm(struct unw_regstate *s, uint64_t *r);
int unw_get_ip(struct unw_regstate *s, uint64_t *r);
int unw_get_sp(struct unw_regstate *s, uint64_t *r);

int unw_table_add(uint64_t, uint64_t, uint64_t);
void unw_table_remove(uint64_t);

#endif /* _MACHINE_UNWIND_H_ */
