/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Wojciech A. Koszek
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/syscallsubr.h>
#include <sys/syslog.h>

#include <compat/freebsd32/freebsd32_proto.h>

int
freebsd32_abort2(struct thread *td, struct freebsd32_abort2_args *uap)
{
	void *uargs[16];
	void *uargsp;
	uint32_t *uargsptr;
	uint32_t ptr;
	int i, nargs;

	nargs = uap->nargs;
	if (nargs < 0 || nargs > nitems(uargs))
		nargs = -1;
	uargsp = NULL;
	if (nargs > 0) {
		if (uap->args != NULL) {
			uargsptr = uap->args;
			for (i = 0; i < nargs; i++) {
				if (fueword32(uargsptr + i, &ptr) != 0) {
					nargs = -1;
					break;
				} else
					uargs[i] = (void *)(uintptr_t)ptr;
			}
			if (nargs > 0)
				uargsp = &uargs;
		} else
			nargs = -1;
	}
	return (kern_abort2(td, uap->why, nargs, uargsp));
}
