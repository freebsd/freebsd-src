/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * $FreeBSD$
 */

#ifndef	_SVR4_UTIL_H_
#define	_SVR4_UTIL_H_

/*#include <compat/common/compat_util.h>*/
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/cdefs.h>
#include <sys/uio.h>

#ifdef DEBUG_SVR4
#define DPRINTF(a)	uprintf a;
#else
#define DPRINTF(a)
#endif

int	svr4_emul_find(struct thread *, char *, enum uio_seg, char **, int);

#define CHECKALT(td, upath, pathp, i)					\
	do {								\
		int _error;						\
									\
		_error = svr4_emul_find(td, upath, UIO_USERSPACE, pathp, i); \
		if (*(pathp) == NULL)					\
			return (_error);				\
	} while (0)

#define CHECKALTEXIST(td, upath, pathp) CHECKALT(td, upath, pathp, 0)
#define CHECKALTCREAT(td, upath, pathp) CHECKALT(td, upath, pathp, 1)

#endif /* !_SVR4_UTIL_H_ */
