/*
 * Copyright (c) 1983 Eric P. Allman
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

#ifndef lint
static char sccsid[] = "@(#)collect.c	8.14 (Berkeley) 4/18/94";
#endif /* not lint */

# include <errno.h>
# include "sendmail.h"

/*
**  COLLECT -- read & parse message header & make temp file.
**
**	Creates a temporary file name and copies the standard
**	input to that file.  Leading UNIX-style "From" lines are
**	stripped off (after important information is extracted).
**
**	Parameters:
**		smtpmode -- if set, we are running SMTP: give an RFC821
**			style message to say we are ready to collect
**			input, and never ignore a single dot to mean
**			end of message.
**		requeueflag -- this message will be requeued later, so
**			don't do final processing on it.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Temp file is created and filled.
**		The from person may be set.
*/

char	*CollectErrorMessage;
bool	CollectErrno;

collect(smtpmode, requeueflag, e)
	bool smtpmode;
	bool requeueflag;
	register ENVELOPE *e;
{
	register FILE *tf;
	bool ignrdot = smtpmode ? FALSE : IgnrDot;
	time_t dbto = smtpmode ? TimeOuts.to_datablock : 0;
	char buf[MAXLINE], buf2[MAXLINE];
	register char *workbuf, *freebuf;
	bool inputerr = FALSE;
	extern char *hvalue();
	extern bool isheader(), flusheol();

	CollectErrorMessage = NULL;
	CollectErrno = 0;

	/*
	**  Create the temp file name and create the file.
	*/

	e->e_df = queuename(e, 'd');
	e->e_df = newstr(e->e_df);
	if ((tf = dfopen(e->e_df, O_WRONLY|O_CREAT|O_TRUNC, FileMode)) == NULL)
	{
		syserr("Cannot create %s", e->e_df);
		NoReturn = TRUE;
		finis();
	}

	/*
	**  Tell ARPANET to go ahead.
	*/

	if (smtpmode)
		message("354 Enter mail, end with \".\" on a line by itself");

	/* set global timer to monitor progress */
	sfgetset(dbto);

	/*
	**  Try to read a UNIX-style From line
	*/

	if (sfgets(buf, MAXLINE, InChannel, dbto,
			"initial message read") == NULL)
		goto readerr;
	fixcrlf(buf, FALSE);
# ifndef NOTUNIX
	if (!SaveFrom && strncmp(buf, "From ", 5) == 0)
	{
		if (!flusheol(buf, InChannel))
			goto readerr;
		eatfrom(buf, e);
		if (sfgets(buf, MAXLINE, InChannel, dbto,
				"message header read") == NULL)
			goto readerr;
		fixcrlf(buf, FALSE);
	}
# endif /* NOTUNIX */

	/*
	**  Copy InChannel to temp file & do message editing.
	**	To keep certain mailers from getting confused,
	**	and to keep the output clean, lines that look
	**	like UNIX "From" lines are deleted in the header.
	*/

	workbuf = buf;		/* `workbuf' contains a header field */
	freebuf = buf2;		/* `freebuf' can be used for read-ahead */
	for (;;)
	{
		char *curbuf;
		int curbuffree;
		register int curbuflen;
		char *p;

		/* first, see if the header is over */
		if (!isheader(workbuf))
		{
			fixcrlf(workbuf, TRUE);
			break;
		}

		/* if the line is too long, throw the rest away */
		if (!flusheol(workbuf, InChannel))
			goto readerr;

		/* it's okay to toss '\n' now (flusheol() needed it) */
		fixcrlf(workbuf, TRUE);

		curbuf = workbuf;
		curbuflen = strlen(curbuf);
		curbuffree = MAXLINE - curbuflen;
		p = curbuf + curbuflen;

		/* get the rest of this field */
		for (;;)
		{
			int clen;

			if (sfgets(freebuf, MAXLINE, InChannel, dbto,
					"message header read") == NULL)
			{
				freebuf[0] = '\0';
				break;
			}

			/* is this a continuation line? */
			if (*freebuf != ' ' && *freebuf != '\t')
				break;

			if (!flusheol(freebuf, InChannel))
				goto readerr;

			fixcrlf(freebuf, TRUE);
			clen = strlen(freebuf) + 1;

			/* if insufficient room, dynamically allocate buffer */
			if (clen >= curbuffree)
			{
				/* reallocate buffer */
				int nbuflen = ((p - curbuf) + clen) * 2;
				char *nbuf = xalloc(nbuflen);

				p = nbuf + curbuflen;
				curbuffree = nbuflen - curbuflen;
				bcopy(curbuf, nbuf, curbuflen);
				if (curbuf != buf && curbuf != buf2)
					free(curbuf);
				curbuf = nbuf;
			}
			*p++ = '\n';
			bcopy(freebuf, p, clen - 1);
			p += clen - 1;
			curbuffree -= clen;
			curbuflen += clen;
		}
		*p++ = '\0';

		e->e_msgsize += curbuflen;

		/*
		**  The working buffer now becomes the free buffer, since
		**  the free buffer contains a new header field.
		**
		**  This is premature, since we still havent called
		**  chompheader() to process the field we just created
		**  (so the call to chompheader() will use `freebuf').
		**  This convolution is necessary so that if we break out
		**  of the loop due to H_EOH, `workbuf' will always be
		**  the next unprocessed buffer.
		*/

		{
			register char *tmp = workbuf;
			workbuf = freebuf;
			freebuf = tmp;
		}

		/*
		**  Snarf header away.
		*/

		if (bitset(H_EOH, chompheader(curbuf, FALSE, e)))
			break;

		/*
		**  If the buffer was dynamically allocated, free it.
		*/

		if (curbuf != buf && curbuf != buf2)
			free(curbuf);
	}

	if (tTd(30, 1))
		printf("EOH\n");

	if (*workbuf == '\0')
	{
		/* throw away a blank line */
		if (sfgets(buf, MAXLINE, InChannel, dbto,
				"message separator read") == NULL)
			goto readerr;
	}
	else if (workbuf == buf2)	/* guarantee `buf' contains data */
		(void) strcpy(buf, buf2);

	/*
	**  Collect the body of the message.
	*/

	for (;;)
	{
		register char *bp = buf;

		fixcrlf(buf, TRUE);

		/* check for end-of-message */
		if (!ignrdot && buf[0] == '.' && (buf[1] == '\n' || buf[1] == '\0'))
			break;

		/* check for transparent dot */
		if ((OpMode == MD_SMTP || OpMode == MD_DAEMON) &&
		    bp[0] == '.' && bp[1] == '.')
			bp++;

		/*
		**  Figure message length, output the line to the temp
		**  file, and insert a newline if missing.
		*/

		e->e_msgsize += strlen(bp) + 1;
		fputs(bp, tf);
		fputs("\n", tf);
		if (ferror(tf))
			tferror(tf, e);
		if (sfgets(buf, MAXLINE, InChannel, dbto,
				"message body read") == NULL)
			goto readerr;
	}

	if (feof(InChannel) || ferror(InChannel))
	{
readerr:
		if (tTd(30, 1))
			printf("collect: read error\n");
		inputerr = TRUE;
	}

	/* reset global timer */
	sfgetset((time_t) 0);

	if (fflush(tf) != 0)
		tferror(tf, e);
	if (fsync(fileno(tf)) < 0 || fclose(tf) < 0)
	{
		tferror(tf, e);
		finis();
	}

	if (CollectErrorMessage != NULL && Errors <= 0)
	{
		if (CollectErrno != 0)
		{
			errno = CollectErrno;
			syserr(CollectErrorMessage, e->e_df);
			finis();
		}
		usrerr(CollectErrorMessage);
	}
	else if (inputerr && (OpMode == MD_SMTP || OpMode == MD_DAEMON))
	{
		/* An EOF when running SMTP is an error */
		char *host;
		char *problem;

		host = RealHostName;
		if (host == NULL)
			host = "localhost";

		if (feof(InChannel))
			problem = "unexpected close";
		else if (ferror(InChannel))
			problem = "I/O error";
		else
			problem = "read timeout";
# ifdef LOG
		if (LogLevel > 0 && feof(InChannel))
			syslog(LOG_NOTICE,
			    "collect: %s on connection from %s, sender=%s: %s\n",
			    problem, host, e->e_from.q_paddr, errstring(errno));
# endif
		if (feof(InChannel))
			usrerr("451 collect: %s on connection from %s, from=%s",
				problem, host, e->e_from.q_paddr);
		else
			syserr("451 collect: %s on connection from %s, from=%s",
				problem, host, e->e_from.q_paddr);

		/* don't return an error indication */
		e->e_to = NULL;
		e->e_flags &= ~EF_FATALERRS;
		e->e_flags |= EF_CLRQUEUE;

		/* and don't try to deliver the partial message either */
		if (InChild)
			ExitStat = EX_QUIT;
		finis();
	}

	/*
	**  Find out some information from the headers.
	**	Examples are who is the from person & the date.
	*/

	eatheader(e, !requeueflag);

	/* collect statistics */
	if (OpMode != MD_VERIFY)
		markstats(e, (ADDRESS *) NULL);

	/*
	**  Add an Apparently-To: line if we have no recipient lines.
	*/

	if (hvalue("to", e) == NULL && hvalue("cc", e) == NULL &&
	    hvalue("bcc", e) == NULL && hvalue("apparently-to", e) == NULL)
	{
		register ADDRESS *q;

		/* create an Apparently-To: field */
		/*    that or reject the message.... */
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (q->q_alias != NULL)
				continue;
			if (tTd(30, 3))
				printf("Adding Apparently-To: %s\n", q->q_paddr);
			addheader("Apparently-To", q->q_paddr, e);
		}
	}

	/* check for message too large */
	if (MaxMessageSize > 0 && e->e_msgsize > MaxMessageSize)
	{
		usrerr("552 Message exceeds maximum fixed size (%ld)",
			MaxMessageSize);
	}

	if ((e->e_dfp = fopen(e->e_df, "r")) == NULL)
	{
		/* we haven't acked receipt yet, so just chuck this */
		syserr("Cannot reopen %s", e->e_df);
		finis();
	}
}
/*
**  FLUSHEOL -- if not at EOL, throw away rest of input line.
**
**	Parameters:
**		buf -- last line read in (checked for '\n'),
**		fp -- file to be read from.
**
**	Returns:
**		FALSE on error from sfgets(), TRUE otherwise.
**
**	Side Effects:
**		none.
*/

bool
flusheol(buf, fp)
	char *buf;
	FILE *fp;
{
	register char *p = buf;
	char junkbuf[MAXLINE];

	while (strchr(p, '\n') == NULL)
	{
		CollectErrorMessage = "553 header line too long";
		CollectErrno = 0;
		if (sfgets(junkbuf, MAXLINE, fp, TimeOuts.to_datablock,
				"long line flush") == NULL)
			return (FALSE);
		p = junkbuf;
	}

	return (TRUE);
}
/*
**  TFERROR -- signal error on writing the temporary file.
**
**	Parameters:
**		tf -- the file pointer for the temporary file.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Gives an error message.
**		Arranges for following output to go elsewhere.
*/

tferror(tf, e)
	FILE *tf;
	register ENVELOPE *e;
{
	CollectErrno = errno;
	if (errno == ENOSPC)
	{
		struct stat st;
		long avail;
		long bsize;

		NoReturn = TRUE;
		if (fstat(fileno(tf), &st) < 0)
			st.st_size = 0;
		(void) freopen(e->e_df, "w", tf);
		if (st.st_size <= 0)
			fprintf(tf, "\n*** Mail could not be accepted");
		else if (sizeof st.st_size > sizeof (long))
			fprintf(tf, "\n*** Mail of at least %qd bytes could not be accepted\n",
				st.st_size);
		else
			fprintf(tf, "\n*** Mail of at least %ld bytes could not be accepted\n",
				st.st_size);
		fprintf(tf, "*** at %s due to lack of disk space for temp file.\n",
			MyHostName);
		avail = freespace(QueueDir, &bsize);
		if (avail > 0)
		{
			if (bsize > 1024)
				avail *= bsize / 1024;
			else if (bsize < 1024)
				avail /= 1024 / bsize;
			fprintf(tf, "*** Currently, %ld kilobytes are available for mail temp files.\n",
				avail);
		}
		CollectErrorMessage = "452 Out of disk space for temp file";
	}
	else
	{
		CollectErrorMessage = "cannot write message body to disk (%s)";
	}
	(void) freopen("/dev/null", "w", tf);
}
/*
**  EATFROM -- chew up a UNIX style from line and process
**
**	This does indeed make some assumptions about the format
**	of UNIX messages.
**
**	Parameters:
**		fm -- the from line.
**
**	Returns:
**		none.
**
**	Side Effects:
**		extracts what information it can from the header,
**		such as the date.
*/

# ifndef NOTUNIX

char	*DowList[] =
{
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL
};

char	*MonthList[] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	NULL
};

eatfrom(fm, e)
	char *fm;
	register ENVELOPE *e;
{
	register char *p;
	register char **dt;

	if (tTd(30, 2))
		printf("eatfrom(%s)\n", fm);

	/* find the date part */
	p = fm;
	while (*p != '\0')
	{
		/* skip a word */
		while (*p != '\0' && *p != ' ')
			p++;
		while (*p == ' ')
			p++;
		if (!(isascii(*p) && isupper(*p)) ||
		    p[3] != ' ' || p[13] != ':' || p[16] != ':')
			continue;

		/* we have a possible date */
		for (dt = DowList; *dt != NULL; dt++)
			if (strncmp(*dt, p, 3) == 0)
				break;
		if (*dt == NULL)
			continue;

		for (dt = MonthList; *dt != NULL; dt++)
			if (strncmp(*dt, &p[4], 3) == 0)
				break;
		if (*dt != NULL)
			break;
	}

	if (*p != '\0')
	{
		char *q;
		extern char *arpadate();

		/* we have found a date */
		q = xalloc(25);
		(void) strncpy(q, p, 25);
		q[24] = '\0';
		q = arpadate(q);
		define('a', newstr(q), e);
	}
}

# endif /* NOTUNIX */
