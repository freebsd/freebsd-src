/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)collect.c	8.93 (Berkeley) 1/26/1999";
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
**		fp -- file to read.
**		smtpmode -- if set, we are running SMTP: give an RFC821
**			style message to say we are ready to collect
**			input, and never ignore a single dot to mean
**			end of message.
**		hdrp -- the location to stash the header.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Temp file is created and filled.
**		The from person may be set.
*/

static jmp_buf	CtxCollectTimeout;
static void	collecttimeout __P((time_t));
static bool	CollectProgress;
static EVENT	*CollectTimeout;

/* values for input state machine */
#define IS_NORM		0	/* middle of line */
#define IS_BOL		1	/* beginning of line */
#define IS_DOT		2	/* read a dot at beginning of line */
#define IS_DOTCR	3	/* read ".\r" at beginning of line */
#define IS_CR		4	/* read a carriage return */

/* values for message state machine */
#define MS_UFROM	0	/* reading Unix from line */
#define MS_HEADER	1	/* reading message header */
#define MS_BODY		2	/* reading message body */
#define MS_DISCARD	3	/* discarding rest of message */

void
collect(fp, smtpmode, hdrp, e)
	FILE *fp;
	bool smtpmode;
	HDR **hdrp;
	register ENVELOPE *e;
{
	register FILE *volatile tf;
	volatile bool ignrdot = smtpmode ? FALSE : IgnrDot;
	volatile time_t dbto = smtpmode ? TimeOuts.to_datablock : 0;
	register char *volatile bp;
	volatile int c = EOF;
	volatile bool inputerr = FALSE;
	bool headeronly;
	char *volatile buf;
	volatile int buflen;
	volatile int istate;
	volatile int mstate;
	u_char *volatile pbp;
	int hdrslen = 0;
	u_char peekbuf[8];
	char dfname[MAXQFNAME];
	char bufbuf[MAXLINE];
	extern bool isheader __P((char *));
	extern void tferror __P((FILE *volatile, ENVELOPE *));

	headeronly = hdrp != NULL;

	/*
	**  Create the temp file name and create the file.
	*/

	if (!headeronly)
	{
		int tfd;
		struct stat stbuf;

		strcpy(dfname, queuename(e, 'd'));
		tfd = dfopen(dfname, O_WRONLY|O_CREAT|O_TRUNC, FileMode, SFF_ANYFILE);
		if (tfd < 0 || (tf = fdopen(tfd, "w")) == NULL)
		{
			syserr("Cannot create %s", dfname);
			e->e_flags |= EF_NO_BODY_RETN;
			finis(TRUE, ExitStat);
		}
		if (fstat(fileno(tf), &stbuf) < 0)
			e->e_dfino = -1;
		else
		{
			e->e_dfdev = stbuf.st_dev;
			e->e_dfino = stbuf.st_ino;
		}
		HasEightBits = FALSE;
		e->e_msgsize = 0;
		e->e_flags |= EF_HAS_DF;
	}

	/*
	**  Tell ARPANET to go ahead.
	*/

	if (smtpmode)
		message("354 Enter mail, end with \".\" on a line by itself");

	if (tTd(30, 2))
		printf("collect\n");

	/*
	**  Read the message.
	**
	**	This is done using two interleaved state machines.
	**	The input state machine is looking for things like
	**	hidden dots; the message state machine is handling
	**	the larger picture (e.g., header versus body).
	*/

	buf = bp = bufbuf;
	buflen = sizeof bufbuf;
	pbp = peekbuf;
	istate = IS_BOL;
	mstate = SaveFrom ? MS_HEADER : MS_UFROM;
	CollectProgress = FALSE;

	if (dbto != 0)
	{
		/* handle possible input timeout */
		if (setjmp(CtxCollectTimeout) != 0)
		{
			if (LogLevel > 2)
				sm_syslog(LOG_NOTICE, e->e_id,
				    "timeout waiting for input from %s during message collect",
				    CurHostName ? CurHostName : "<local machine>");
			errno = 0;
			usrerr("451 timeout waiting for input during message collect");
			goto readerr;
		}
		CollectTimeout = setevent(dbto, collecttimeout, dbto);
	}

	for (;;)
	{
		if (tTd(30, 35))
			printf("top, istate=%d, mstate=%d\n", istate, mstate);
		for (;;)
		{
			if (pbp > peekbuf)
				c = *--pbp;
			else
			{
				while (!feof(fp) && !ferror(fp))
				{
					errno = 0;
					c = getc(fp);
					if (errno != EINTR)
						break;
					clearerr(fp);
				}
				CollectProgress = TRUE;
				if (TrafficLogFile != NULL && !headeronly)
				{
					if (istate == IS_BOL)
						fprintf(TrafficLogFile, "%05d <<< ",
							(int) getpid());
					if (c == EOF)
						fprintf(TrafficLogFile, "[EOF]\n");
					else
						putc(c, TrafficLogFile);
				}
				if (c == EOF)
					goto readerr;
				if (SevenBitInput)
					c &= 0x7f;
				else
					HasEightBits |= bitset(0x80, c);
			}
			if (tTd(30, 94))
				printf("istate=%d, c=%c (0x%x)\n",
					istate, c, c);
			switch (istate)
			{
			  case IS_BOL:
				if (c == '.')
				{
					istate = IS_DOT;
					continue;
				}
				break;

			  case IS_DOT:
				if (c == '\n' && !ignrdot &&
				    !bitset(EF_NL_NOT_EOL, e->e_flags))
					goto readerr;
				else if (c == '\r' &&
					 !bitset(EF_CRLF_NOT_EOL, e->e_flags))
				{
					istate = IS_DOTCR;
					continue;
				}
				else if (c != '.' ||
					 (OpMode != MD_SMTP &&
					  OpMode != MD_DAEMON &&
					  OpMode != MD_ARPAFTP))
				{
					*pbp++ = c;
					c = '.';
				}
				break;

			  case IS_DOTCR:
				if (c == '\n' && !ignrdot)
					goto readerr;
				else
				{
					/* push back the ".\rx" */
					*pbp++ = c;
					*pbp++ = '\r';
					c = '.';
				}
				break;

			  case IS_CR:
				if (c == '\n')
					istate = IS_BOL;
				else
				{
					ungetc(c, fp);
					c = '\r';
					istate = IS_NORM;
				}
				goto bufferchar;
			}

			if (c == '\r' && !bitset(EF_CRLF_NOT_EOL, e->e_flags))
			{
				istate = IS_CR;
				continue;
			}
			else if (c == '\n' && !bitset(EF_NL_NOT_EOL, e->e_flags))
				istate = IS_BOL;
			else
				istate = IS_NORM;

bufferchar:
			if (!headeronly)
				e->e_msgsize++;
			switch (mstate)
			{
			  case MS_BODY:
				/* just put the character out */
				if (MaxMessageSize <= 0 ||
				    e->e_msgsize <= MaxMessageSize)
					putc(c, tf);

				/* fall through */

			  case MS_DISCARD:
				continue;
			}

			/* header -- buffer up */
			if (bp >= &buf[buflen - 2])
			{
				char *obuf;

				if (mstate != MS_HEADER)
					break;

				/* out of space for header */
				obuf = buf;
				if (buflen < MEMCHUNKSIZE)
					buflen *= 2;
				else
					buflen += MEMCHUNKSIZE;
				buf = xalloc(buflen);
				bcopy(obuf, buf, bp - obuf);
				bp = &buf[bp - obuf];
				if (obuf != bufbuf)
					free(obuf);
			}
			if (c >= 0200 && c <= 0237)
			{
#if 0	/* causes complaints -- figure out something for 8.9 */
				usrerr("Illegal character 0x%x in header", c);
#endif
			}
			else if (c != '\0')
			{
				*bp++ = c;
				if (MaxHeadersLength > 0 &&
				    ++hdrslen > MaxHeadersLength)
				{
					sm_syslog(LOG_NOTICE, e->e_id,
						  "headers too large (%d max) from %s during message collect",
						  MaxHeadersLength,
						  CurHostName != NULL ? CurHostName : "localhost");
					errno = 0;
					e->e_flags |= EF_CLRQUEUE;
					e->e_status = "5.6.0";
					usrerr("552 Headers too large (%d max)",
						MaxHeadersLength);
					mstate = MS_DISCARD;
				}
			}
			if (istate == IS_BOL)
				break;
		}
		*bp = '\0';

nextstate:
		if (tTd(30, 35))
			printf("nextstate, istate=%d, mstate=%d, line = \"%s\"\n",
				istate, mstate, buf);
		switch (mstate)
		{
		  case MS_UFROM:
			mstate = MS_HEADER;
#ifndef NOTUNIX
			if (strncmp(buf, "From ", 5) == 0)
			{
				extern void eatfrom __P((char *volatile, ENVELOPE *));

				bp = buf;
				eatfrom(buf, e);
				continue;
			}
#endif
			/* fall through */

		  case MS_HEADER:
			if (!isheader(buf))
			{
				mstate = MS_BODY;
				goto nextstate;
			}

			/* check for possible continuation line */
			do
			{
				clearerr(fp);
				errno = 0;
				c = getc(fp);
			} while (errno == EINTR);
			if (c != EOF)
				ungetc(c, fp);
			if (c == ' ' || c == '\t')
			{
				/* yep -- defer this */
				continue;
			}

			/* trim off trailing CRLF or NL */
			if (*--bp != '\n' || *--bp != '\r')
				bp++;
			*bp = '\0';

			if (bitset(H_EOH, chompheader(buf, FALSE, hdrp, e)))
			{
				mstate = MS_BODY;
				goto nextstate;
			}
			break;

		  case MS_BODY:
			if (tTd(30, 1))
				printf("EOH\n");
			if (headeronly)
				goto readerr;
			bp = buf;

			/* toss blank line */
			if ((!bitset(EF_CRLF_NOT_EOL, e->e_flags) &&
				bp[0] == '\r' && bp[1] == '\n') ||
			    (!bitset(EF_NL_NOT_EOL, e->e_flags) &&
				bp[0] == '\n'))
			{
				break;
			}

			/* if not a blank separator, write it out */
			if (MaxMessageSize <= 0 ||
			    e->e_msgsize <= MaxMessageSize)
			{
				while (*bp != '\0')
					putc(*bp++, tf);
			}
			break;
		}
		bp = buf;
	}

readerr:
	if ((feof(fp) && smtpmode) || ferror(fp))
	{
		const char *errmsg = errstring(errno);

		if (tTd(30, 1))
			printf("collect: premature EOM: %s\n", errmsg);
		if (LogLevel >= 2)
			sm_syslog(LOG_WARNING, e->e_id,
				"collect: premature EOM: %s", errmsg);
		inputerr = TRUE;
	}

	/* reset global timer */
	clrevent(CollectTimeout);

	if (headeronly)
		return;

	if (tf != NULL &&
	    (fflush(tf) != 0 || ferror(tf) ||
	     (SuperSafe && fsync(fileno(tf)) < 0) ||
	     fclose(tf) < 0))
	{
		tferror(tf, e);
		flush_errors(TRUE);
		finis(TRUE, ExitStat);
	}

	/* An EOF when running SMTP is an error */
	if (inputerr && (OpMode == MD_SMTP || OpMode == MD_DAEMON))
	{
		char *host;
		char *problem;

		host = RealHostName;
		if (host == NULL)
			host = "localhost";

		if (feof(fp))
			problem = "unexpected close";
		else if (ferror(fp))
			problem = "I/O error";
		else
			problem = "read timeout";
		if (LogLevel > 0 && feof(fp))
			sm_syslog(LOG_NOTICE, e->e_id,
			    "collect: %s on connection from %.100s, sender=%s: %s",
			    problem, host,
			    shortenstring(e->e_from.q_paddr, MAXSHORTSTR),
			    errstring(errno));
		if (feof(fp))
			usrerr("451 collect: %s on connection from %s, from=%s",
				problem, host,
				shortenstring(e->e_from.q_paddr, MAXSHORTSTR));
		else
			syserr("451 collect: %s on connection from %s, from=%s",
				problem, host,
				shortenstring(e->e_from.q_paddr, MAXSHORTSTR));

		/* don't return an error indication */
		e->e_to = NULL;
		e->e_flags &= ~EF_FATALERRS;
		e->e_flags |= EF_CLRQUEUE;

		/* and don't try to deliver the partial message either */
		if (InChild)
			ExitStat = EX_QUIT;
		finis(TRUE, ExitStat);
	}

	/*
	**  Find out some information from the headers.
	**	Examples are who is the from person & the date.
	*/

	eatheader(e, TRUE);

	if (GrabTo && e->e_sendqueue == NULL)
		usrerr("No recipient addresses found in header");

	/* collect statistics */
	if (OpMode != MD_VERIFY)
		markstats(e, (ADDRESS *) NULL, FALSE);

#if _FFR_DSN_RRT_OPTION
	/*
	**  If we have a Return-Receipt-To:, turn it into a DSN.
	*/

	if (RrtImpliesDsn && hvalue("return-receipt-to", e->e_header) != NULL)
	{
		ADDRESS *q;

		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			if (!bitset(QHASNOTIFY, q->q_flags))
				q->q_flags |= QHASNOTIFY|QPINGONSUCCESS;
	}
#endif

	/*
	**  Add an Apparently-To: line if we have no recipient lines.
	*/

	if (hvalue("to", e->e_header) != NULL ||
	    hvalue("cc", e->e_header) != NULL ||
	    hvalue("apparently-to", e->e_header) != NULL)
	{
		/* have a valid recipient header -- delete Bcc: headers */
		e->e_flags |= EF_DELETE_BCC;
	}
	else if (hvalue("bcc", e->e_header) == NULL)
	{
		/* no valid recipient headers */
		register ADDRESS *q;
		char *hdr = NULL;

		/* create an Apparently-To: field */
		/*    that or reject the message.... */
		switch (NoRecipientAction)
		{
		  case NRA_ADD_APPARENTLY_TO:
			hdr = "Apparently-To";
			break;

		  case NRA_ADD_TO:
			hdr = "To";
			break;

		  case NRA_ADD_BCC:
			addheader("Bcc", " ", &e->e_header);
			break;

		  case NRA_ADD_TO_UNDISCLOSED:
			addheader("To", "undisclosed-recipients:;", &e->e_header);
			break;
		}

		if (hdr != NULL)
		{
			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_alias != NULL)
					continue;
				if (tTd(30, 3))
					printf("Adding %s: %s\n",
						hdr, q->q_paddr);
				addheader(hdr, q->q_paddr, &e->e_header);
			}
		}
	}

	/* check for message too large */
	if (MaxMessageSize > 0 && e->e_msgsize > MaxMessageSize)
	{
		e->e_flags |= EF_NO_BODY_RETN|EF_CLRQUEUE;
		e->e_status = "5.2.3";
		usrerr("552 Message exceeds maximum fixed size (%ld)",
			MaxMessageSize);
		if (LogLevel > 6)
			sm_syslog(LOG_NOTICE, e->e_id,
				"message size (%ld) exceeds maximum (%ld)",
				e->e_msgsize, MaxMessageSize);
	}

	/* check for illegal 8-bit data */
	if (HasEightBits)
	{
		e->e_flags |= EF_HAS8BIT;
		if (!bitset(MM_PASS8BIT|MM_MIME8BIT, MimeMode) &&
		    !bitset(EF_IS_MIME, e->e_flags))
		{
			e->e_status = "5.6.1";
			usrerr("554 Eight bit data not allowed");
		}
	}
	else
	{
		/* if it claimed to be 8 bits, well, it lied.... */
		if (e->e_bodytype != NULL &&
		    strcasecmp(e->e_bodytype, "8BITMIME") == 0)
			e->e_bodytype = "7BIT";
	}

	if ((e->e_dfp = fopen(dfname, "r")) == NULL)
	{
		/* we haven't acked receipt yet, so just chuck this */
		syserr("Cannot reopen %s", dfname);
		finis(TRUE, ExitStat);
	}
}


static void
collecttimeout(timeout)
	time_t timeout;
{
	/* if no progress was made, die now */
	if (!CollectProgress)
		longjmp(CtxCollectTimeout, 1);

	/* otherwise reset the timeout */
	CollectTimeout = setevent(timeout, collecttimeout, timeout);
	CollectProgress = FALSE;
}
/*
**  TFERROR -- signal error on writing the temporary file.
**
**	Parameters:
**		tf -- the file pointer for the temporary file.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Gives an error message.
**		Arranges for following output to go elsewhere.
*/

void
tferror(tf, e)
	FILE *volatile tf;
	register ENVELOPE *e;
{
	setstat(EX_IOERR);
	if (errno == ENOSPC)
	{
#if STAT64 > 0
		struct stat64 st;
#else
		struct stat st;
#endif
		long avail;
		long bsize;
		extern long freediskspace __P((char *, long *));

		e->e_flags |= EF_NO_BODY_RETN;

		if (
#if STAT64 > 0
		    fstat64(fileno(tf), &st) 
#else
		    fstat(fileno(tf), &st) 
#endif
		    < 0)
		  st.st_size = 0;
		(void) freopen(queuename(e, 'd'), "w", tf);
		if (st.st_size <= 0)
			fprintf(tf, "\n*** Mail could not be accepted");
		else if (sizeof st.st_size > sizeof (long))
			fprintf(tf, "\n*** Mail of at least %s bytes could not be accepted\n",
				quad_to_string(st.st_size));
		else
			fprintf(tf, "\n*** Mail of at least %lu bytes could not be accepted\n",
				(unsigned long) st.st_size);
		fprintf(tf, "*** at %s due to lack of disk space for temp file.\n",
			MyHostName);
		avail = freediskspace(QueueDir, &bsize);
		if (avail > 0)
		{
			if (bsize > 1024)
				avail *= bsize / 1024;
			else if (bsize < 1024)
				avail /= 1024 / bsize;
			fprintf(tf, "*** Currently, %ld kilobytes are available for mail temp files.\n",
				avail);
		}
		e->e_status = "4.3.1";
		usrerr("452 Out of disk space for temp file");
	}
	else
		syserr("collect: Cannot write tf%s", e->e_id);
	if (freopen("/dev/null", "w", tf) == NULL)
		sm_syslog(LOG_ERR, e->e_id,
			  "tferror: freopen(\"/dev/null\") failed: %s",
			  errstring(errno));
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

void
eatfrom(fm, e)
	char *volatile fm;
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

		/* we have found a date */
		q = xalloc(25);
		(void) strncpy(q, p, 25);
		q[24] = '\0';
		q = arpadate(q);
		define('a', newstr(q), e);
	}
}

# endif /* NOTUNIX */
