/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	from: @(#)stat.c	7.1 (Berkeley) 5/5/91
 *	$Id: stat.c,v 1.2 1993/10/16 19:31:37 rgrimes Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include "saio.h"

#ifndef SMALL
fstat(fd, sb)
	int fd;
	struct stat *sb;
{
	register struct iob *io;

	fd -= 3;
	if (fd < 0 || fd >= SOPEN_MAX ||
	    ((io = &iob[fd])->i_flgs & F_ALLOC) == 0) {
		errno = EBADF;
		return (-1);
	}
	/* only important stuff */
	sb->st_mode = io->i_ino.di_mode;
	sb->st_uid = io->i_ino.di_uid;
	sb->st_gid = io->i_ino.di_gid;
	sb->st_size = io->i_ino.di_size;
	return (0);
}

stat(str, sb)
	const char *str;
	struct stat *sb;
{
	int fd, rv;

	fd = open(str, 0);
	if (fd < 0)
		return(-1);
	rv = fstat(fd, sb);
	close(fd);
	return(rv);
}
#endif SMALL
