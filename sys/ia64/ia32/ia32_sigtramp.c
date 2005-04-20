/*-
 * Copyright (c) 2002 Doug Rabson
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

char ia32_sigcode[] = {
	0xff, 0x54, 0x24, 0x10,		/* call *SIGF_HANDLER(%esp) */
	0x8d, 0x44, 0x24, 0x14,		/* lea SIGF_UC(%esp),%eax */
	0x50,				/* pushl %eax */
	0xf7, 0x40, 0x54, 0x00, 0x00, 0x02, 0x02, /* testl $PSL_VM,UC_EFLAGS(%eax) */
	0x75, 0x03,			/* jne 9f */
	0x8e, 0x68, 0x14,		/* movl UC_GS(%eax),%gs */
	0xb8, 0x57, 0x01, 0x00, 0x00,	/* 9: movl $SYS_sigreturn,%eax */
	0x50,				/* pushl %eax */
	0xcd, 0x80,			/* int $0x80 */
	0xeb, 0xfe,			/* 0: jmp 0b */
	0
};
int sz_ia32_sigcode = sizeof(ia32_sigcode);
