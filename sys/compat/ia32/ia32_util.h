/*-
 * Copyright (c) 1998-1999 Andrew Gallatin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>


#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/cdefs.h>

struct ia32_ps_strings {
	u_int32_t ps_argvstr;	/* first of 0 or more argument strings */
	int	ps_nargvstr;	/* the number of argument strings */
	u_int32_t ps_envstr;	/* first of 0 or more environment strings */
	int	ps_nenvstr;	/* the number of environment strings */
};

#define IA32_USRSTACK	((1ul << 32) - PAGE_SIZE)
#define IA32_PS_STRINGS	(IA32_USRSTACK - sizeof(struct ia32_ps_strings))

static __inline caddr_t stackgap_init(void);
static __inline void *stackgap_alloc(caddr_t *, size_t);

static __inline caddr_t
stackgap_init()
{
#define	szsigcode (*(curproc->p_sysent->sv_szsigcode))
	return (caddr_t)(((caddr_t)IA32_PS_STRINGS) - szsigcode - SPARE_USRSPACE);
#undef szsigcode
}

static __inline void *
stackgap_alloc(sgp, sz)
	caddr_t *sgp;
	size_t   sz;
{
	void *p;

	p = (void *) *sgp;
	*sgp += ALIGN(sz);
	return p;
}


extern const char ia32_emul_path[];
int ia32_emul_find(struct thread *, caddr_t *, const char *, char *,
			char **, int);

#define CHECKALT(p, sgp, path, i)					\
	do {								\
		int _error;						\
									\
		_error = ia32_emul_find(p, sgp, ia32_emul_path, path,	\
					&path, i);			\
		if (_error == EFAULT)					\
			return (_error);				\
	} while (0)

#define	CHECKALTEXIST(p, sgp, path) CHECKALT((p), (sgp), (path), 0)
#define	CHECKALTCREAT(p, sgp, path) CHECKALT((p), (sgp), (path), 1)
