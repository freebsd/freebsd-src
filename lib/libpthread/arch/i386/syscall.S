/* ==== syscall.S ============================================================
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 1993 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Description : Machine dependent syscalls for i386/i486/i586
 *
 *  1.00 93/08/26 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#if defined(SYSLIBC_SCCS) && !defined(lint)
        .asciz "@(#)syscall.s   5.1 (Berkeley) 4/23/90"
#endif /* SYSLIBC_SCCS and not lint */

#include <sys/syscall.h>

#define	SYSCALL(x)					\
	.globl _machdep_sys_/**/x;		\
									\
_machdep_sys_/**/x:;				\
									\
	lea	SYS_/**/x, %eax;			\
	.byte 0x9a; .long 0; .word 7;	\
	jb	1b;							\
	ret;							

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

1:
	neg		%eax
	ret

/* ==========================================================================
 * machdep_sys_write()
 */
SYSCALL(write)

/* ==========================================================================
 * machdep_sys_read()
 */
SYSCALL(read)

/* ==========================================================================
 * machdep_sys_open()
 */
SYSCALL(open)

/* ==========================================================================
 * machdep_sys_close()
 */
SYSCALL(close)

/* ==========================================================================
 * machdep_sys_fcntl()
 */
SYSCALL(fcntl)

/* ==========================================================================
 * machdep_sys_lseek()
 */
SYSCALL(lseek)

/* ========================================================================== 
 * Nonstandard calls used to make the system work
 *
 * ==========================================================================
 * machdep_sys_select()
 */
SYSCALL(select)

/* ========================================================================== 
 * Berkeley socket stuff
 * 
 * ==========================================================================
 * machdep_sys_socket()
 */
SYSCALL(socket)

/* ==========================================================================
 * machdep_sys_bind()
 */
SYSCALL(bind)

/* ==========================================================================
 * machdep_sys_connect()
 */
SYSCALL(connect)

/* ==========================================================================
 * machdep_sys_accept()
 */
SYSCALL(accept)

/* ==========================================================================
 * machdep_sys_listen()
 */
SYSCALL(listen)

/* ==========================================================================
 * machdep_sys_getsockopt()
 */
SYSCALL(getsockopt)

/* ==========================================================================
 * machdep_sys_readv()
 */
SYSCALL(readv)

/* ==========================================================================
 * machdep_sys_writev()
 */
SYSCALL(writev)

/* ==========================================================================
 * machdep_sys_getpeername()
 */
SYSCALL(getpeername)

/* ==========================================================================
 * machdep_sys_getsockname()
 */
SYSCALL(getsockname)

