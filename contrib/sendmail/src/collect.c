/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
static char id[] = "@(#)$Id: collect.c,v 8.136.4.21 2001/05/17 18:10:14 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


static void	collecttimeout __P((time_t));
static void	dferror __P((FILE *volatile, char *, ENVELOPE *));
static void	eatfrom __P((char *volatile, ENVELOPE *));

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
static bool	volatile CollectProgress;
static EVENT	*volatile CollectTimeout = NULL;

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
	register FILE *volatile df;
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
	volatile int hdrslen = 0;
	volatile int numhdrs = 0;
	volatile int dfd;
	volatile int rstat = EX_OK;
	u_char *volatile pbp;
	u_char peekbuf[8];
	char hsize[16];
	char hnum[16];
	char dfname[MAXPATHLEN];
	char bufbuf[MAXLINE];

	headeronly = hdrp != NULL;

	/*
	**  Create the temp file name and create the file.
	*/

	if (!headeronly)
	{
		struct stat stbuf;
		long sff = SFF_OPENASROOT;


		(void) strlcpy(dfname, queuename(e, 'd'), sizeof dfname);
#if _FFR_QUEUE_FILE_MODE
		{
			MODE_T oldumask;

			if (bitset(S_IWGRP, QueueFileMode))
				oldumask = umask(002);
			df = bfopen(dfname, QueueFileMode,
				    DataFileBufferSize, sff);
			if (bitset(S_IWGRP, QueueFileMode))
				(void) umask(oldumask);
		}
#else /* _FFR_QUEUE_FILE_MODE */
		df = bfopen(dfname, FileMode, DataFileBufferSize, sff);
#endif /* _FFR_QUEUE_FILE_MODE */
		if (df == NULL)
		{
			HoldErrs = FALSE;
			if (smtpmode)
				syserr("421 4.3.5 Unable to create data file");
			else
				syserr("Cannot create %s", dfname);
			e->e_flags |= EF_NO_BODY_RETN;
			finis(TRUE, ExitStat);
			/* NOTREACHED */
		}
		dfd = fileno(df);
		if (dfd < 0 || fstat(dfd, &stbuf) < 0)
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
		dprintf("collect\n");

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
			usrerr("451 4.4.1 timeout waiting for input during message collect");
			goto readerr;
		}
		CollectTimeout = setevent(dbto, collecttimeout, dbto);
	}

	for (;;)
	{
		if (tTd(30, 35))
			dprintf("top, istate=%d, mstate=%d\n", istate, mstate);
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

					if (c == EOF && errno == EINTR)
					{
						/* Interrupted, retry */
						clearerr(fp);
						continue;
					}
					break;
				}
				CollectProgress = TRUE;
				if (TrafficLogFile != NULL && !headeronly)
				{
					if (istate == IS_BOL)
						(void) fprintf(TrafficLogFile,
							       "%05d <<< ",
							       (int) getpid());
					if (c == EOF)
						(void) fprintf(TrafficLogFile,
							       "[EOF]\n");
					else
						(void) putc(c, TrafficLogFile);
				}
				if (c == EOF)
					goto readerr;
				if (SevenBitInput)
					c &= 0x7f;
				else
					HasEightBits |= bitset(0x80, c);
			}
			if (tTd(30, 94))
				dprintf("istate=%d, c=%c (0x%x)\n",
					istate, (char) c, c);
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
					(void) ungetc(c, fp);
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
			{
				/* no overflow? */
				if (e->e_msgsize >= 0)
				{
					e->e_msgsize++;
					if (MaxMessageSize > 0 &&
					    !bitset(EF_TOOBIG, e->e_flags) &&
					    e->e_msgsize > MaxMessageSize)
						 e->e_flags |= EF_TOOBIG;
				}
			}
			switch (mstate)
			{
			  case MS_BODY:
				/* just put the character out */
				if (!bitset(EF_TOOBIG, e->e_flags))
					(void) putc(c, df);
				/* FALLTHROUGH */

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
				memmove(buf, obuf, bp - obuf);
				bp = &buf[bp - obuf];
				if (obuf != bufbuf)
					sm_free(obuf);
			}
			if (c >= 0200 && c <= 0237)
			{
#if 0	/* causes complaints -- figure out something for 8.11 */
				usrerr("Illegal character 0x%x in header", c);
#else /* 0 */
				/* EMPTY */
#endif /* 0 */
			}
			else if (c != '\0')
			{
				*bp++ = c;
				hdrslen++;
				if (MaxHeadersLength > 0 &&
				    hdrslen > MaxHeadersLength)
				{
					sm_syslog(LOG_NOTICE, e->e_id,
						  "headers too large (%d max) from %s during message collect",
						  MaxHeadersLength,
						  CurHostName != NULL ? CurHostName : "localhost");
					errno = 0;
					e->e_flags |= EF_CLRQUEUE;
					e->e_status = "5.6.0";
					usrerrenh(e->e_status,
						  "552 Headers too large (%d max)",
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
			dprintf("nextstate, istate=%d, mstate=%d, line = \"%s\"\n",
				istate, mstate, buf);
		switch (mstate)
		{
		  case MS_UFROM:
			mstate = MS_HEADER;
#ifndef NOTUNIX
			if (strncmp(buf, "From ", 5) == 0)
			{
				bp = buf;
				eatfrom(buf, e);
				continue;
			}
#endif /* ! NOTUNIX */
			/* FALLTHROUGH */

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
			} while (c == EOF && errno == EINTR);
			if (c != EOF)
				(void) ungetc(c, fp);
			if (c == ' ' || c == '\t')
			{
				/* yep -- defer this */
				continue;
			}

			/* trim off trailing CRLF or NL */
			if (*--bp != '\n' || *--bp != '\r')
				bp++;
			*bp = '\0';

			if (bitset(H_EOH, chompheader(buf,
						      CHHDR_CHECK | CHHDR_USER,
						      hdrp, e)))
			{
				mstate = MS_BODY;
				goto nextstate;
			}
			numhdrs++;
			break;

		  case MS_BODY:
			if (tTd(30, 1))
				dprintf("EOH\n");

			if (headeronly)
				goto readerr;

			/* call the end-of-header check ruleset */
			snprintf(hnum, sizeof hnum, "%d", numhdrs);
			snprintf(hsize, sizeof hsize, "%d", hdrslen);
			if (tTd(30, 10))
				dprintf("collect: rscheck(\"check_eoh\", \"%s $| %s\")\n",
					hnum, hsize);
			rstat = rscheck("check_eoh", hnum, hsize, e, FALSE,
					TRUE, 4, NULL);

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
			if (!bitset(EF_TOOBIG, e->e_flags))
			{
				while (*bp != '\0')
					(void) putc(*bp++, df);
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
			dprintf("collect: premature EOM: %s\n", errmsg);
		if (LogLevel >= 2)
			sm_syslog(LOG_WARNING, e->e_id,
				"collect: premature EOM: %s", errmsg);
		inputerr = TRUE;
	}

	/* reset global timer */
	if (CollectTimeout != NULL)
		clrevent(CollectTimeout);

	if (headeronly)
		return;

	if (df == NULL)
	{
		/* skip next few clauses */
		/* EMPTY */
	}
	else if (fflush(df) != 0 || ferror(df))
	{
		dferror(df, "fflush||ferror", e);
		flush_errors(TRUE);
		finis(TRUE, ExitStat);
		/* NOTREACHED */
	}
	else if (!SuperSafe)
	{
		/* skip next few clauses */
		/* EMPTY */
	}
	else if (bfcommit(df) < 0)
	{
		int save_errno = errno;

		if (save_errno == EEXIST)
		{
			char *dfile;
			struct stat st;

			dfile = queuename(e, 'd');
			if (stat(dfile, &st) < 0)
				st.st_size = -1;
			errno = EEXIST;
			syserr("collect: bfcommit(%s): already on disk, size = %ld",
			       dfile, (long) st.st_size);
			dfd = fileno(df);
			if (dfd >= 0)
				dumpfd(dfd, TRUE, TRUE);
		}
		errno = save_errno;
		dferror(df, "bfcommit", e);
		flush_errors(TRUE);
		finis(save_errno != EEXIST, ExitStat);
	}
	else if (bffsync(df) < 0)
	{
		dferror(df, "bffsync", e);
		flush_errors(TRUE);
		finis(TRUE, ExitStat);
		/* NOTREACHED */
	}
	else if (bfclose(df) < 0)
	{
		dferror(df, "bfclose", e);
		flush_errors(TRUE);
		finis(TRUE, ExitStat);
		/* NOTREACHED */
	}
	else
	{
		/* everything is happily flushed to disk */
		df = NULL;
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
			usrerr("451 4.4.1 collect: %s on connection from %s, from=%s",
				problem, host,
				shortenstring(e->e_from.q_paddr, MAXSHORTSTR));
		else
			syserr("451 4.4.1 collect: %s on connection from %s, from=%s",
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
		/* NOTREACHED */
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
			addheader("Bcc", " ", 0, &e->e_header);
			break;

		  case NRA_ADD_TO_UNDISCLOSED:
			addheader("To", "undisclosed-recipients:;", 0, &e->e_header);
			break;
		}

		if (hdr != NULL)
		{
			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (q->q_alias != NULL)
					continue;
				if (tTd(30, 3))
					dprintf("Adding %s: %s\n",
						hdr, q->q_paddr);
				addheader(hdr, q->q_paddr, 0, &e->e_header);
			}
		}
	}

	/* check for message too large */
	if (bitset(EF_TOOBIG, e->e_flags))
	{
		e->e_flags |= EF_NO_BODY_RETN|EF_CLRQUEUE;
		e->e_status = "5.2.3";
		usrerrenh(e->e_status,
			  "552 Message exceeds maximum fixed size (%ld)",
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
			usrerrenh(e->e_status, "554 Eight bit data not allowed");
		}
	}
	else
	{
		/* if it claimed to be 8 bits, well, it lied.... */
		if (e->e_bodytype != NULL &&
		    strcasecmp(e->e_bodytype, "8BITMIME") == 0)
			e->e_bodytype = "7BIT";
	}

	if (SuperSafe)
	{
		if ((e->e_dfp = fopen(dfname, "r")) == NULL)
		{
			/* we haven't acked receipt yet, so just chuck this */
			syserr("Cannot reopen %s", dfname);
			finis(TRUE, ExitStat);
			/* NOTREACHED */
		}
	}
	else
		e->e_dfp = df;
	if (e->e_dfp == NULL)
		syserr("!collect: no e_dfp");
}


static void
collecttimeout(timeout)
	time_t timeout;
{
	int save_errno = errno;

	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	if (CollectProgress)
	{
		/* reset the timeout */
		CollectTimeout = sigsafe_setevent(timeout, collecttimeout,
						  timeout);
		CollectProgress = FALSE;
	}
	else
	{
		/* event is done */
		CollectTimeout = NULL;
	}

	/* if no progress was made or problem resetting event, die now */
	if (CollectTimeout == NULL)
	{
		errno = ETIMEDOUT;
		longjmp(CtxCollectTimeout, 1);
	}

	errno = save_errno;
}
/*
**  DFERROR -- signal error on writing the data file.
**
**	Parameters:
**		df -- the file pointer for the data file.
**		msg -- detailed message.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Gives an error message.
**		Arranges for following output to go elsewhere.
*/

static void
dferror(df, msg, e)
	FILE *volatile df;
	char *msg;
	register ENVELOPE *e;
{
	char *dfname;

	dfname = queuename(e, 'd');
	setstat(EX_IOERR);
	if (errno == ENOSPC)
	{
#if STAT64 > 0
		struct stat64 st;
#else /* STAT64 > 0 */
		struct stat st;
#endif /* STAT64 > 0 */
		long avail;
		long bsize;

		e->e_flags |= EF_NO_BODY_RETN;

		if (
#if STAT64 > 0
		    fstat64(fileno(df), &st)
#else /* STAT64 > 0 */
		    fstat(fileno(df), &st)
#endif /* STAT64 > 0 */
		    < 0)
		  st.st_size = 0;
		(void) freopen(dfname, "w", df);
		if (st.st_size <= 0)
			fprintf(df, "\n*** Mail could not be accepted");
		/*CONSTCOND*/
		else if (sizeof st.st_size > sizeof (long))
			fprintf(df, "\n*** Mail of at least %s bytes could not be accepted\n",
				quad_to_string(st.st_size));
		else
			fprintf(df, "\n*** Mail of at least %lu bytes could not be accepted\n",
				(unsigned long) st.st_size);
		fprintf(df, "*** at %s due to lack of disk space for temp file.\n",
			MyHostName);
		avail = freediskspace(qid_printqueue(e->e_queuedir), &bsize);
		if (avail > 0)
		{
			if (bsize > 1024)
				avail *= bsize / 1024;
			else if (bsize < 1024)
				avail /= 1024 / bsize;
			fprintf(df, "*** Currently, %ld kilobytes are available for mail temp files.\n",
				avail);
		}
		e->e_status = "4.3.1";
		usrerrenh(e->e_status, "452 Out of disk space for temp file");
	}
	else
		syserr("collect: Cannot write %s (%s, uid=%d)",
			dfname, msg, geteuid());
	if (freopen("/dev/null", "w", df) == NULL)
		sm_syslog(LOG_ERR, e->e_id,
			  "dferror: freopen(\"/dev/null\") failed: %s",
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

#ifndef NOTUNIX

static char	*DowList[] =
{
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL
};

static char	*MonthList[] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	NULL
};

static void
eatfrom(fm, e)
	char *volatile fm;
	register ENVELOPE *e;
{
	register char *p;
	register char **dt;

	if (tTd(30, 2))
		dprintf("eatfrom(%s)\n", fm);

	/* find the date part */
	p = fm;
	while (*p != '\0')
	{
		/* skip a word */
		while (*p != '\0' && *p != ' ')
			p++;
		while (*p == ' ')
			p++;
		if (strlen(p) < 17)
		{
			/* no room for the date */
			return;
		}
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
		{
			if (strncmp(*dt, &p[4], 3) == 0)
				break;
		}
		if (*dt != NULL)
			break;
	}

	if (*p != '\0')
	{
		char *q;

		/* we have found a date */
		q = xalloc(25);
		(void) strlcpy(q, p, 25);
		q = arpadate(q);
		define('a', newstr(q), e);
	}
}
#endif /* ! NOTUNIX */
