/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	$Id: login.c,v 1.3 1994/04/07 17:47:36 ache Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)login.c	5.4 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/file.h>
#include <utmp.h>
#include <stdio.h>
#include <unistd.h>
#include <ttyent.h>

typedef struct utmp UTMP;

void
login(ut)
	UTMP *ut;
{
	register int fd;
	int tty;
	UTMP utmp;

	if ((fd = open(_PATH_UTMP, O_RDWR|O_CREAT, 0644)) >= 0) {
		if ((tty = ttyslot()) > 0) {
			(void)lseek(fd, (long)(tty * sizeof(UTMP)), L_SET);
			(void)write(fd, (char *)ut, sizeof(UTMP));
			(void)close(fd);
		} else {
			setttyent();
			for (tty = 0; getttyent(); tty++)
				;
			endttyent();
			(void)lseek(fd, (long)(tty * sizeof(UTMP)), L_SET);
			while (read(fd, (char *)&utmp, sizeof(UTMP)) == sizeof(UTMP)) {
				if (!utmp.ut_name[0]) {
					(void)lseek(fd, -(long)sizeof(UTMP), L_INCR);
					break;
				}
			}
			(void)write(fd, (char *)ut, sizeof(UTMP));
			(void)close(fd);
		}
	}
	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
		(void)write(fd, (char *)ut, sizeof(UTMP));
		(void)close(fd);
	}
}
