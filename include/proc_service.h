/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
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
 * $FreeBSD$
 */

#ifndef _PROC_SERVICE_H_
#define _PROC_SERVICE_H_

#include <sys/types.h>
#include <sys/procfs.h>

struct ps_prochandle;

typedef enum
{
    PS_OK,
    PS_ERR,
    PS_BADPID,
    PS_BADLID,
    PS_BADADDR,
    PS_NOSYM,
    PS_NOFREGS
} ps_err_e;

/*
 * Every program that links libthread_db must provide a set of process control
 * primitives to access memory and registers in the target process, to start
 * and to stop the target process, and to look up symbols in the target process.
 */

#if 0
ps_err_e ps_pdmodel(struct ps_prochandle *ph, int *data_model);
#endif
ps_err_e ps_pglobal_lookup(struct ps_prochandle *ph, const char *object_name,
		const char *sym_name , psaddr_t *sym_addr);
#if 0
ps_err_e ps_pglobal_sym(struct ps_prochandle *ph, const char *object_name,
		const char *sym_name , ps_sym_t *sym);
ps_err_e ps_pread(struct ps_prochandle *ph, psaddr_t addr, void *buf,
		size_t size);
ps_err_e ps_pwrite(struct ps_prochandle *ph, psaddr_t addr, const void *buf,
		size_t size);
#endif
ps_err_e ps_pdread(struct ps_prochandle *ph, psaddr_t addr, void *buf,
		size_t size);
ps_err_e ps_pdwrite(struct ps_prochandle *ph, psaddr_t addr, const void *buf,
		size_t size);
ps_err_e ps_ptread(struct ps_prochandle *ph, psaddr_t addr, void *buf,
		size_t size);
ps_err_e ps_ptwrite(struct ps_prochandle *ph, psaddr_t addr, const void *buf,
		size_t size);
ps_err_e ps_pstop(struct ps_prochandle *ph);
ps_err_e ps_pcontinue(struct ps_prochandle *ph);
ps_err_e ps_lstop(struct ps_prochandle *ph, lwpid_t lwpid);
ps_err_e ps_lcontinue(struct ps_prochandle *ph, lwpid_t lwpid);
ps_err_e ps_lgetregs(struct ps_prochandle *ph, lwpid_t lwpid,
		prgregset_t gregset);
ps_err_e ps_lsetregs(struct ps_prochandle *ph, lwpid_t lwpid,
		const prgregset_t gregset);
ps_err_e ps_lgetfpregs(struct ps_prochandle *ph, lwpid_t lwpid,
		prfpregset_t *fpregset);
ps_err_e ps_lsetfpregs(struct ps_prochandle *ph, lwpid_t lwpid,
		const prfpregset_t *fpregset);
#if 0
ps_err_e ps_pauxv(struct ps_prochandle *ph, const auxv_t **auxp);
ps_err_e ps_kill(struct ps_prochandle *ph, int sig);
ps_err_e ps_lrolltoaddr(struct ps_prochandle *ph, lwpid_t lwpid,
		psaddr_t go_addr, psaddr_t stop_addr);
#endif
void ps_plog(const char *fmt, ...);
pid_t ps_getpid (struct ps_prochandle *ph);
 
#endif
