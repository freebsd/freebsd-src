/*-
 * Copyright (c) 2001 Doug Rabson
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

struct ia64_unwind_table;

struct ia64_unwind_table *ia64_add_unwind_table(u_int64_t *base,
						u_int64_t *start,
						u_int64_t *end);
void ia64_free_unwind_table(struct ia64_unwind_table *ut);
struct ia64_unwind_state *ia64_create_unwind_state(struct trapframe *framep);
void ia64_free_unwind_state(struct ia64_unwind_state *us);
u_int64_t ia64_unwind_state_get_ip(struct ia64_unwind_state *us);
u_int64_t ia64_unwind_state_get_sp(struct ia64_unwind_state *us);
u_int64_t ia64_unwind_state_get_cfm(struct ia64_unwind_state *us);
u_int64_t *ia64_unwind_state_get_bsp(struct ia64_unwind_state *us);
int ia64_unwind_state_previous_frame(struct ia64_unwind_state *us);
