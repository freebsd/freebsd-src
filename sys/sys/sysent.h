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
 * $FreeBSD: src/sys/sys/sysent.h,v 1.44 2004/04/07 04:19:49 imp Exp $
 */

#ifndef _SYS_SYSENT_H_
#define	_SYS_SYSENT_H_

struct thread;

typedef	int	sy_call_t(struct thread *, void *);

struct sysent {		/* system call table */
	int	sy_narg;	/* number of arguments */
	sy_call_t *sy_call;	/* implementing function */
};

#define SYF_ARGMASK	0x0000FFFF
#define SYF_MPSAFE	0x00010000

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
	int		(*sv_transtrap)(int, int);
					/* translate trap-to-signal mapping */
	int		(*sv_fixup)(register_t **, struct image_params *);
					/* stack fixup function */
	void		(*sv_sendsig)(void (*)(int), int, struct __sigset *,
			    u_long);	/* send signal */
	char 		*sv_sigcode;	/* start of sigtramp code */
	int 		*sv_szsigcode;	/* size of sigtramp code */
	void		(*sv_prepsyscall)(struct trapframe *, int *, u_int *,
			    caddr_t *);
	char		*sv_name;	/* name of binary type */
	int		(*sv_coredump)(struct thread *, struct vnode *, off_t);
					/* function to dump core, or NULL */
	int		(*sv_imgact_try)(struct image_params *);
	int		sv_minsigstksz;	/* minimum signal stack size */
	int		sv_pagesize;	/* pagesize */
	vm_offset_t	sv_minuser;	/* VM_MIN_ADDRESS */
	vm_offset_t	sv_maxuser;	/* VM_MAXUSER_ADDRESS */
	vm_offset_t	sv_usrstack;	/* USRSTACK */
	vm_offset_t	sv_psstrings;	/* PS_STRINGS */
	int		sv_stackprot;	/* vm protection for stack */
	register_t	*(*sv_copyout_strings)(struct image_params *);
	void		(*sv_setregs)(struct thread *, u_long, u_long, u_long);
	void		(*sv_fixlimits)(struct image_params *);
};

#ifdef _KERNEL
extern struct sysentvec aout_sysvec;
extern struct sysentvec elf_freebsd_sysvec;
extern struct sysentvec null_sysvec;
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
       evh, arg, offset, new_sysent, { 0, NULL }               \
};                                                             \
                                                               \
static moduledata_t name##_mod = {                             \
       #name,                                                  \
       syscall_module_handler,                                 \
       &name##_syscall_mod                                     \
};                                                             \
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)

#define SYSCALL_MODULE_HELPER(syscallname)              \
static int syscallname##_syscall = SYS_##syscallname;   \
static struct sysent syscallname##_sysent = {           \
    (sizeof(struct syscallname ## _args )               \
     / sizeof(register_t)),                             \
    (sy_call_t *)& syscallname                          \
};                                                      \
SYSCALL_MODULE(syscallname,                             \
    & syscallname##_syscall, & syscallname##_sysent,    \
    NULL, NULL);

int    syscall_register(int *offset, struct sysent *new_sysent,
	    struct sysent *old_sysent);
int    syscall_deregister(int *offset, struct sysent *old_sysent);
int    syscall_module_handler(struct module *mod, int what, void *arg);

#endif /* _KERNEL */

#endif /* !_SYS_SYSENT_H_ */
