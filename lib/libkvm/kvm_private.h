/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *	@(#)kvm_private.h	8.1 (Berkeley) 6/4/93
 * $FreeBSD$
 */

struct __kvm {
	/*
	 * a string to be prepended to error messages
	 * provided for compatibility with sun's interface
	 * if this value is null, errors are saved in errbuf[]
	 */
	const char *program;
	char	*errp;		/* XXX this can probably go away */
	char	errbuf[_POSIX2_LINE_MAX];
#define ISALIVE(kd) ((kd)->vmfd >= 0)
	int	pmfd;		/* physical memory file (or crashdump) */
	int	vmfd;		/* virtual memory file (-1 if crashdump) */
	int	unused;		/* was: swap file (e.g., /dev/drum) */
	int	nlfd;		/* namelist file (e.g., /kernel) */
	struct kinfo_proc *procbase;
	char	*argspc;	/* (dynamic) storage for argv strings */
	int	arglen;		/* length of the above */
	char	**argv;		/* (dynamic) storage for argv pointers */
	int	argc;		/* length of above (not actual # present) */
	char	*argbuf;	/* (dynamic) temporary storage */
	/*
	 * Kernel virtual address translation state.  This only gets filled
	 * in for dead kernels; otherwise, the running kernel (i.e. kmem)
	 * will do the translations for us.  It could be big, so we
	 * only allocate it if necessary.
	 */
	struct vmstate *vmst;
};

/*
 * Functions used internally by kvm, but across kvm modules.
 */
void	 _kvm_err __P((kvm_t *kd, const char *program, const char *fmt, ...)) __printflike(3, 4);
void	 _kvm_freeprocs(kvm_t *kd);
void	 _kvm_freevtop(kvm_t *);
int	 _kvm_initvtop(kvm_t *);
int	 _kvm_kvatop(kvm_t *, u_long, u_long *);
void	*_kvm_malloc(kvm_t *kd, size_t);
void	*_kvm_realloc(kvm_t *kd, void *, size_t);
void	 _kvm_syserr
	    __P((kvm_t *kd, const char *program, const char *fmt, ...)) __printflike(3, 4);
int	 _kvm_uvatop(kvm_t *, const struct proc *, u_long, u_long *);
