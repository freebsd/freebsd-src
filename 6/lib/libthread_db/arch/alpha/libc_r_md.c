/*
 * Copyright (c) 2004 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/procfs.h>
#include <machine/setjmp.h>

void
libc_r_md_getgregs(jmp_buf jb, prgregset_t r)
{
	r->r_regs[R_V0] = jb->_jb[4];
	r->r_regs[R_T0] = jb->_jb[5];
	r->r_regs[R_T1] = jb->_jb[6];
	r->r_regs[R_T2] = jb->_jb[7];
	r->r_regs[R_T3] = jb->_jb[8];
	r->r_regs[R_T4] = jb->_jb[9];
	r->r_regs[R_T5] = jb->_jb[10];
	r->r_regs[R_T6] = jb->_jb[11];
	r->r_regs[R_T7] = jb->_jb[12];
	r->r_regs[R_S0] = jb->_jb[13];
	r->r_regs[R_S1] = jb->_jb[14];
	r->r_regs[R_S2] = jb->_jb[15];
	r->r_regs[R_S3] = jb->_jb[16];
	r->r_regs[R_S4] = jb->_jb[17];
	r->r_regs[R_S5] = jb->_jb[18];
	r->r_regs[R_S6] = jb->_jb[19];
	r->r_regs[R_A0] = jb->_jb[20];
	r->r_regs[R_A1] = jb->_jb[21];
	r->r_regs[R_A2] = jb->_jb[22];
	r->r_regs[R_A3] = jb->_jb[23];
	r->r_regs[R_A4] = jb->_jb[24];
	r->r_regs[R_A5] = jb->_jb[25];
	r->r_regs[R_T8] = jb->_jb[26];
	r->r_regs[R_T9] = jb->_jb[27];
	r->r_regs[R_T10] = jb->_jb[28];
	r->r_regs[R_T11] = jb->_jb[29];
	r->r_regs[R_RA] = jb->_jb[30];
	r->r_regs[R_T12] = jb->_jb[31];
	r->r_regs[R_AT] = jb->_jb[32];
	r->r_regs[R_GP] = jb->_jb[33];
	r->r_regs[R_SP] = jb->_jb[34];
	r->r_regs[R_ZERO] = jb->_jb[35];
}

void
libc_r_md_getfpregs(jmp_buf jb, prfpregset_t *r)
{
}
