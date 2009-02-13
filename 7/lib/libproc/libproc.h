/*-
 * Copyright (c) 2008 John Birrell (jb@freebsd.org)
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

#ifndef	_LIBPROC_H_
#define	_LIBPROC_H_

#include <gelf.h>

struct proc_handle;

typedef void (*proc_child_func)(void *);

/* Values returned by proc_state(). */
#define PS_IDLE		1
#define PS_STOP		2
#define PS_RUN		3
#define PS_UNDEAD	4
#define PS_DEAD		5
#define PS_LOST		6

typedef struct prmap {
	uintptr_t	pr_vaddr;	/* Virtual address. */
} prmap_t;

/* Function prototype definitions. */
__BEGIN_DECLS

const prmap_t *proc_addr2map(struct proc_handle *, uintptr_t);
const prmap_t *proc_name2map(struct proc_handle *, const char *);
char	*proc_objname(struct proc_handle *, uintptr_t, char *, size_t);
int	proc_addr2sym(struct proc_handle *, uintptr_t, char *, size_t, GElf_Sym *);
int	proc_attach(pid_t pid, int flags, struct proc_handle **pphdl);
int	proc_continue(struct proc_handle *);
int	proc_clearflags(struct proc_handle *, int);
int	proc_create(const char *, char * const *, proc_child_func *, void *,
	    struct proc_handle **);
int	proc_detach(struct proc_handle *);
int	proc_getflags(struct proc_handle *);
int	proc_name2sym(struct proc_handle *, const char *, const char *, GElf_Sym *);
int	proc_setflags(struct proc_handle *, int);
int	proc_state(struct proc_handle *);
int	proc_wait(struct proc_handle *);
pid_t	proc_getpid(struct proc_handle *);
void	proc_free(struct proc_handle *);

__END_DECLS

#endif /* !_LIBPROC_H_ */
