/*-
 * Copyright (c) 2010 Joerg Wunsch
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

enum tnt4882reg {
	dir = 0x00,
	cdor = 0x00,
	isr1 = 0x02,
	imr1 = 0x02,
	isr2 = 0x04,
	imr2 = 0x04,
	accwr = 0x05,
	spsr = 0x06,
	spmr = 0x06,
	intr = 0x07,
	adsr = 0x08,
	admr = 0x08,
	cnt2 = 0x09,
	cptr = 0x0a,
	auxmr = 0x0a,
	tauxcr = 0x0a,	/* 9914 mode register */
	cnt3 = 0x0b,
	adr0 = 0x0c,
	adr = 0x0c,
	hssel = 0x0d,
	adr1 = 0x0e,
	eosr = 0x0e,
	sts1 = 0x10,
	cfg = 0x10,
	dsr = 0x11,
	sh_cnt = 0x11,
	imr3 = 0x12,
	hier = 0x13,
	cnt0 = 0x14,
	misc = 0x15,
	cnt1 = 0x16,
	csr = 0x17,
	keyreg = 0x17,
	fifob = 0x18,
	fifoa = 0x19,
	isr3 = 0x1a,
	ccr = 0x1a,
	sasr = 0x1b,
	dcr = 0x1b,
	sts2 = 0x1c,
	cmdr = 0x1c,
	isr0 = 0x1d,
	imr0 = 0x1d,
	timer = 0x1e,
	bsr = 0x1f,
	bcr = 0x1f
};

