/*
 * Copyright (c) 1995 Steven Wallace
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Steven Wallace.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/sysent.h>
#include <sys/signalvar.h>
#include <i386/ibcs2/ibcs2_syscall.h>

extern int bsd_to_ibcs2_sig[];
extern int bsd_to_ibcs2_errno[];
extern struct sysent ibcs2_sysent[IBCS2_SYS_MAXSYSCALL];
extern int szsigcode;
extern char sigcode[];

struct sysentvec ibcs2_svr3_sysvec = {
        sizeof (ibcs2_sysent) / sizeof (ibcs2_sysent[0]),
        ibcs2_sysent,
        0xFF,
        NSIG,
        bsd_to_ibcs2_sig,
        ELAST,
        bsd_to_ibcs2_errno,
	0,		/* fixup */
	sendsig,
	sigcode,	/* use generic trampoline */
	&szsigcode,	/* use generic trampoline size */
	0,		/* prepsyscall */
	"IBCS2 COFF"
};
