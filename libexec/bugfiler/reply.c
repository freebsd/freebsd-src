/*
 * Copyright (c) 1986, 1987, 1993
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

#ifndef lint
static char sccsid[] = "@(#)reply.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include <sys/param.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bug.h"
#include "extern.h"
#include "pathnames.h"

/*
 * reply --
 *	tell the user we got their silly little bug report
 */
void
reply()
{
	register char	*C,			/* traveling pointer */
			*to;			/* who we're replying to */
	register int	afd,			/* ack file descriptor */
			rval;			/* return value */
	FILE	*pf;				/* pipe pointer */

	if (mailhead[RPLY_TAG].found) {
		for (C = mailhead[RPLY_TAG].line + mailhead[RPLY_TAG].len;
		    *C != '\n' && (*C == ' ' || *C == '\t');++C);
		if (*C)
			goto gotone;
	}
	if (mailhead[FROM_TAG].found) {
		for (C = mailhead[FROM_TAG].line + mailhead[FROM_TAG].len;
		    *C != '\n' && (*C == ' ' || *C == '\t');++C);
		if (*C)
			goto gotone;
	}
	if (mailhead[CFROM_TAG].found) {
		for (C = mailhead[CFROM_TAG].line + mailhead[CFROM_TAG].len;
		     *C != '\n' && (*C == ' ' || *C == '\t');++C);
		if (*C)
			goto gotone;
	}
	return;

	/* if it's a foo <XXX>, get the XXX, else get foo (first string) */
gotone:	if (to = strchr(C, '<'))
		for (C = ++to;
		    *C != '\n' && *C != ' ' && *C != '\t' && *C != '>';++C);
	else {
		to = C;
		for (to = C++;*C != '\n' && *C != ' ' && *C != '\t';++C);
	}
	*C = EOS;

	if (!(pf = popen(MAIL_CMD, "w")))
		error("sendmail pipe failed.", CHN);

	fprintf(pf, "Reply-To: %s\nFrom: %s (Bugs Bunny)\nTo: %s\n",
	    BUGS_HOME, BUGS_HOME, to);
	if (mailhead[SUBJ_TAG].found)
		fprintf(pf, "Subject: Re:%s",
		    mailhead[SUBJ_TAG].line + mailhead[SUBJ_TAG].len);
	else
		fputs("Subject: Bug report acknowledgement.\n", pf);
	if (mailhead[DATE_TAG].found)
		fprintf(pf, "In-Acknowledgement-Of: Your message of %s",
		    mailhead[DATE_TAG].line + mailhead[DATE_TAG].len);
	if (mailhead[MSG_TAG].found)
		fprintf(pf, "\t\t%s", mailhead[MSG_TAG].line);
	fputs("Precedence: bulk\n\n", pf);	/* vacation(1) uses this... */
	fflush(pf);

	(void)sprintf(bfr, "%s/%s", dir, ACK_FILE);
	if ((afd = open(bfr, O_RDONLY, 0)) >= 0) {
		while ((rval = read(afd, bfr, sizeof(bfr))) != ERR && rval)
			(void)write(fileno(pf), bfr, rval);
		(void)close(afd);
	}
	pclose(pf);
}
