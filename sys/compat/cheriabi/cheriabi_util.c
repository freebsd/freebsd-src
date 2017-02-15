/*-
 * Copyright (c) 2017 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

#include <sys/types.h>
#include <sys/systm.h>

#include <compat/cheriabi/cheriabi_util.h>

int
cheriabi_copyinstrarg(struct thread *td, int syscall, int arg, char *buf,
    size_t len, size_t *done)
{
	struct chericap tmpcap;
	size_t length, offset;
	char *uaddr;

	/*
	 * XXX-BD: rely on fill_uap to have ensured the cap is valid and
	 * has some data in it;
	 */
	cheriabi_fetch_syscall_arg(td, &tmpcap, syscall, arg);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
	CHERI_CGETOFFSET(offset, CHERI_CR_CTEMP0);
	CHERI_CTOPTR(uaddr, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	return (copyinstr(uaddr, buf, MAX(length - offset, len), done));
}
