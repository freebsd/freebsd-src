/*-
 * Copyright (c) 1982, 1988, 1991 The Regents of the University of California.
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
 * $FreeBSD: src/sys/sys/sysent.h,v 1.27.2.2 2000/05/16 06:58:05 dillon Exp $
 */

#ifndef _SYS_SYSENT_H_
#define	_SYS_SYSENT_H_

struct proc;

typedef	int	sy_call_t __P((struct proc *, void *));

struct sysent {		/* system call table */
	int	sy_narg;	/* number of arguments */
	sy_call_t *sy_call;	/* implementing function */
};

#define SYF_ARGMASK	0x0000FFFF
#define SYF_MPSAFE	0x00010000

#define SCARG(p,k)	((p)->k)	/* get arg from args pointer */
  /* placeholder till we integrate rest of lite2 syscallargs changes XXX */

struct image_params;
struct __sigset;
struct trapframe;
struct vnode;

struct sysentvec {
	int		sv_size;	/* number of entries */
	struct sysent	*sv_table;	/* pointer to sysent */
	u_int		sv_mask;	/* optional mask to index */
	int		sv_sigsize;	/* size of signal translation table */
	int		*sv_sigtbl;	/* signal translation table */
	int		sv_errsize;	/* size of errno translation table */
	int 		*sv_errtbl;	/* errno translation table */
	int		(*sv_transtrap) __P((int, int));
					/* translate trap-to-signal mapping */
	int		(*sv_fixup) __P((register_t **, struct image_params *));
					/* stack fixup function */
	void		(*sv_sendsig) __P((void (*)(int), int,
					   struct __sigset *, u_long));
					/* send signal */
	char 		*sv_sigcode;	/* start of sigtramp code */
	int 		*sv_szsigcode;	/* size of sigtramp code */
	void		(*sv_prepsyscall) __P((struct trapframe *, int *,
					       u_int *, caddr_t *));
	char		*sv_name;	/* name of binary type */
	int		(*sv_coredump) __P((struct proc *, struct vnode *,
					    off_t));
					/* function to dump core, or NULL */
	int		(*sv_imgact_try) __P((struct image_params *));
};

#ifdef _KERNEL
extern struct sysentvec aout_sysvec;
extern struct sysent sysent[];

#define NO_SYSCALL (-1)

struct module;

struct syscall_module_data {
       int     (*chainevh)(struct module *, int, void *); /* next handler */
       void    *chainarg;      /* arg for next event handler */
       int     *offset;         /* offset into sysent */
       struct  sysent *new_sysent; /* new sysent */
       struct  sysent old_sysent; /* old sysent */
};

#define SYSCALL_MODULE(name, offset, new_sysent, evh, arg)     \
static struct syscall_module_data name##_syscall_mod = {       \
       evh, arg, offset, new_sysent                            \
};                                                             \
                                                               \
static moduledata_t name##_mod = {                             \
       #name,                                                  \
       syscall_module_handler,                                 \
       &name##_syscall_mod                                     \
};                                                             \
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)

int    syscall_register __P((int *offset, struct sysent *new_sysent,
                             struct sysent *old_sysent));
int    syscall_deregister __P((int *offset, struct sysent *old_sysent));
int    syscall_module_handler __P((struct module *mod, int what, void *arg));

#endif /* _KERNEL */

#endif /* !_SYS_SYSENT_H_ */
