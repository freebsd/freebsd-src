/*-
 * Copyright (c) 1988, 1993
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)logwtmp.c	8.1 (Berkeley) 6/4/93";
#else
static const char rcsid[] =
	"$Id: logwtmp.c,v 1.5 1997/09/04 22:38:59 pst Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libutil.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

void
trimdomain( char * fullhost, int hostsize )
{
    static char domain[MAXHOSTNAMELEN + 1];
    static int first = 1;
    char *s;

    if (first) {
        first = 0;
        if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
            (s = strchr(domain, '.')))
            (void) strcpy(domain, s + 1);
        else
            domain[0] = 0;
    }

    if (domain[0]) {
		s = fullhost;
        while ((fullhost = strchr(fullhost, '.'))) {
            if (!strcasecmp(fullhost + 1, domain)) {
		if ( fullhost - s  < hostsize ) {
               		*fullhost = '\0';    /* hit it and acceptable size*/
		}
                break;
            } else {
                fullhost++;
            }
        }
    }
}

void
logwtmp(line, name, host)
	const char *line;
	const char *name;
	const char *host;
{
	struct utmp ut;
	struct stat buf;
	char   fullhost[MAXHOSTNAMELEN + 1];
	char   *whost = fullhost;
	int fd;
	
	strncpy( whost, host, MAXHOSTNAMELEN );	
fullhost[MAXHOSTNAMELEN] = '\0';
	trimdomain( whost, UT_HOSTSIZE );
	host = whost;

	if (strlen(host) > UT_HOSTSIZE) {
		struct hostent *hp = gethostbyname(host);

		if (hp != NULL) {
			struct in_addr in;

			memmove(&in, hp->h_addr, sizeof(in));
			host = inet_ntoa(in);
		} else
			host = "invalid hostname";
	}

	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) < 0)
		return;
	if (fstat(fd, &buf) == 0) {
		(void) strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void) strncpy(ut.ut_name, name, sizeof(ut.ut_name));
		(void) strncpy(ut.ut_host, host, sizeof(ut.ut_host));
		(void) time(&ut.ut_time);
		if (write(fd, (char *)&ut, sizeof(struct utmp)) !=
		    sizeof(struct utmp))
			(void) ftruncate(fd, buf.st_size);
	}
	(void) close(fd);
}
