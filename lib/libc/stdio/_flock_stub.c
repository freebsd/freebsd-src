/*
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $Id: _flock_stub.c,v 1.1 1998/04/11 07:40:41 jb Exp $
 *
 */

#include <stdio.h>

/* Don't build this in libc_r, just libc: */
#ifndef	_THREAD_SAFE
/*
 * Declare weak references in case the application is not linked
 * with libpthread.
 */
#pragma weak flockfile=_flockfile_stub
#pragma weak _flockfile_debug=_flockfile_debug_stub
#pragma weak ftrylockfile=_ftrylockfile_stub
#pragma weak funlockfile=_funlockfile_stub

/*
 * This function is a stub for the _flockfile function in libpthread.
 */
void
_flockfile_stub(FILE *fp)
{
}

/*
 * This function is a stub for the _flockfile_debug function in libpthread.
 */
void
_flockfile_debug_stub(FILE *fp, char *fname, int lineno)
{
}

/*
 * This function is a stub for the _ftrylockfile function in libpthread.
 */
int
_ftrylockfile_stub(FILE *fp)
{
	return(0);
}

/*
 * This function is a stub for the _funlockfile function in libpthread.
 */
void
_funlockfile_stub(FILE *fp)
{
}
#endif
