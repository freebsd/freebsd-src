/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

/* $Id$ */

#ifndef _VPS_MD_H
#define _VPS_MD_H

#define VPS_ARCH_I386

#define VPS_MD_SNAPCTX_NPAGES		10240 /* 40 MB */

#define VPS_MD_DUMPHDR_PTRSIZE		VPS_DUMPH_32BIT

#define VPS_MD_DUMPHDR_BYTEORDER	VPS_DUMPH_LSB

extern struct sysentvec elf32_freebsd_sysvec; 

#ifdef _VPS_MD_FUNCTIONS

inline
static void
vps_md_syscallret(struct thread *td, struct syscall_args *sa)
{

	/* re-setting tf_eax */
	td->td_frame->tf_eax = sa->code;
	DBGCORE("%s: td=%p tf_eax=%p\n",
	    __func__, td, (void*)td->td_frame->tf_eax);
}

#endif /* _VPS_MD_FUNCTIONS */

#endif /* _VPS_MD_H */

/* EOF */
