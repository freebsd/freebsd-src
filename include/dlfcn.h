/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	$Id$
 */

#ifndef _DLFCN_H_
#define	_DLFCN_H_
#include <sys/cdefs.h>

/*
 * Modes for dlopen().
 */
#define RTLD_LAZY	1	/* Bind function calls lazily */
#define RTLD_NOW	2	/* Bind function calls immediately */

/*
 * Special handle argument for dlsym().  It causes the search for the
 * symbol to begin in the next shared object after the one containing
 * the caller.
 */
#define RTLD_NEXT	((void *) -1)

/*
 * Structure filled in by dladdr().
 */
typedef struct dl_info {
	const char	*dli_fname;	/* Pathname of shared object */
	void		*dli_fbase;	/* Base address of shared object */
	const char	*dli_sname;	/* Name of nearest symbol */
	void		*dli_saddr;	/* Address of nearest symbol */
} Dl_info;

__BEGIN_DECLS
int dladdr __P((const void *, Dl_info *));
int dlclose __P((void *));
const char *dlerror __P((void));
void *dlopen __P((const char *, int));
void *dlsym __P((void *, const char *));
__END_DECLS

#endif /* !_DLFCN_H_ */
