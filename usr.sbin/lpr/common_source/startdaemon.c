/*
 * Copyright (c) 1983 Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)startdaemon.c	5.7 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * Tell the printer daemon that there are new files in the spool directory.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "lp.local.h"
#include "pathnames.h"

startdaemon(printer)
	char *printer;
{
	struct sockaddr_un sun;
	register int s, n;
	char buf[BUFSIZ];
	static void perr();

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		perr("socket");
		return(0);
	}
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, _PATH_SOCKETNAME);
	if (connect(s, (struct sockaddr *)&sun, strlen(sun.sun_path) + 2) < 0) {
		perr("connect");
		(void) close(s);
		return(0);
	}
	(void) sprintf(buf, "\1%s\n", printer);
	n = strlen(buf);
	if (write(s, buf, n) != n) {
		perr("write");
		(void) close(s);
		return(0);
	}
	if (read(s, buf, 1) == 1) {
		if (buf[0] == '\0') {		/* everything is OK */
			(void) close(s);
			return(1);
		}
		putchar(buf[0]);
	}
	while ((n = read(s, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, n, stdout);
	(void) close(s);
	return(0);
}

static void
perr(msg)
	char *msg;
{
	extern int errno;
	extern char *name;
	char *strerror();

	(void)printf("%s: %s: %s\n", name, msg, strerror(errno));
}
