/*
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
 * Copyright (c) 1995 Scott Bartram
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * from: svr4_util.h,v 1.5 1994/11/18 02:54:31 christos Exp
 * from: linux_util.h,v 1.2 1995/03/05 23:23:50 fvdl Exp
 * $FreeBSD$
 */

/*
 * This file is pretty much the same as Christos' svr4_util.h
 * (for now).
 */

#ifndef	_LINUX_UTIL_H_
#define	_LINUX_UTIL_H_

#include "opt_linux.h"

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/cdefs.h>

static __inline caddr_t stackgap_init(void);
static __inline void *stackgap_alloc(caddr_t *, size_t);

#define szsigcode (*(curthread->td_proc->p_sysent->sv_szsigcode))

static __inline caddr_t
stackgap_init()
{
	return (caddr_t)(PS_STRINGS - szsigcode - SPARE_USRSPACE);
}

static __inline void *
stackgap_alloc(sgp, sz)
	caddr_t	*sgp;
	size_t   sz;
{
	void *p = (void *) *sgp;

	sz = ALIGN(sz);
	if (*sgp + sz > (caddr_t)(PS_STRINGS - szsigcode))
		return NULL;
	*sgp += sz;
	return p;
}

extern const char linux_emul_path[];

int linux_emul_find __P((struct thread *, caddr_t *, const char *, char *,
			char **, int));

#define CHECKALT(p, sgp, path, i) 					\
	do {								\
		int _error;						\
									\
		_error = linux_emul_find(p, sgp, linux_emul_path, path,	\
		    &path, i);						\
		if (_error == EFAULT)					\
			return (_error);				\
	} while (0)

#define CHECKALTEXIST(p, sgp, path) CHECKALT(p, sgp, path, 0)
#define CHECKALTCREAT(p, sgp, path) CHECKALT(p, sgp, path, 1)

#define DUMMY(s)							\
int									\
linux_ ## s(struct thread *p, struct linux_ ## s ## _args *args)	\
{									\
	return (unsupported_msg(p, #s));				\
}									\
struct __hack

static __inline int
unsupported_msg(struct thread *td, const char *fname)
{

	printf("linux: syscall %s is obsoleted or not implemented (pid=%ld)\n",
	    fname, (long)td->td_proc->p_pid);
	return (ENOSYS);
}

#endif /* !_LINUX_UTIL_H_ */
