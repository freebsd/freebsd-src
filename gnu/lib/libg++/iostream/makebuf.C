/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// Modified for GNU iostream by Per Bothner 1991.

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "%W% (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

#include "ioprivate.h"
#include "fstream.h"
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Allocate a file buffer, or switch to unbuffered I/O.
 * Per the ANSI C standard, ALL tty devices default to line buffered.
 *
 * As a side effect, we set __SOPT or __SNPT (en/dis-able fseek
 * optimisation) right after the _fstat() that finds the buffer size.
 */
int filebuf::doallocate()
{
    register size_t size, couldbetty;
    register char *p;
    struct stat st;

    if (fd() < 0 || _fstat(fd(), &st) < 0) {
	couldbetty = 0;
	size = _G_BUFSIZ;
#if 0
	/* do not try to optimise fseek() */
	fp->_flags |= __SNPT;
#endif
    } else {
	couldbetty = S_ISCHR(st.st_mode);
#if _G_HAVE_ST_BLKSIZE
	size = st.st_blksize <= 0 ? _G_BUFSIZ : st.st_blksize;
#else
	size = _G_BUFSIZ;
#endif
    }
#ifdef USE_MALLOC_BUF
    if ((p = malloc(size)) == NULL) {
	unbuffered(1);
//	fp->_bf._base = fp->_p = fp->_nbuf;
//	fp->_bf._size = 1;
	return EOF;
    }
#else
    p = ALLOC_BUF(size);
#endif
    setb(p, p+size, 1);
    if (couldbetty && _isatty(fd()))
	_flags |= _S_LINE_BUF;
    return 1;
}
