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
static char sccsid[] = "@(#)err.c	8.74 (Berkeley) 6/4/1998";
#endif /* not lint */

# include "sendmail.h"
# include <errno.h>

/*
**  SYSERR -- Print error message.
**
**	Prints an error message via printf to the diagnostic output.
**
**	If the first character of the syserr message is `!' it will
**	log this as an ALERT message and exit immediately.  This can
**	leave queue files in an indeterminate state, so it should not
**	be used lightly.
**
**	Parameters:
**		fmt -- the format string.  If it does not begin with
**			a three-digit SMTP reply code, either 554 or
**			451 is assumed depending on whether errno
**			is set.
**		(others) -- parameters
**
**	Returns:
**		none
**		Through TopFrame if QuickAbort is set.
**
**	Side Effects:
**		increments Errors.
**		sets ExitStat.
*/

char	MsgBuf[BUFSIZ*2];		/* text of most recent message */
char	HeldMessageBuf[sizeof MsgBuf];	/* for held messages */

extern void	putoutmsg __P((char *, bool, bool));
extern void	puterrmsg __P((char *));
static void	fmtmsg __P((char *, const char *, const char *, int, const char *, va_list));

#if NAMED_BIND && !defined(NO_DATA)
# define NO_DATA	NO_ADDRESS
#endif

void
/*VARARGS1*/
#ifdef __STDC__
syserr(const char *fmt, ...)
#else
syserr(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	register char *p;
	int olderrno = errno;
	bool panic;
	char *uname;
	struct passwd *pw;
	char ubuf[80];
	VA_LOCAL_DECL

	panic = *fmt == '!';
	if (panic)
	{
		fmt++;
		HoldErrs = FALSE;
	}

	/* format and output the error message */
	if (olderrno == 0)
		p = "554";
	else
		p = "451";
	VA_START(fmt);
	fmtmsg(MsgBuf, (char *) NULL, p, olderrno, fmt, ap);
	VA_END;
	puterrmsg(MsgBuf);

	/* save this message for mailq printing */
	if (!panic && CurEnv != NULL)
	{
		if (CurEnv->e_message != NULL)
			free(CurEnv->e_message);
		CurEnv->e_message = newstr(MsgBuf + 4);
	}

	/* determine exit status if not already set */
	if (ExitStat == EX_OK)
	{
		if (olderrno == 0)
			ExitStat = EX_SOFTWARE;
		else
			ExitStat = EX_OSERR;
		if (tTd(54, 1))
			printf("syserr: ExitStat = %d\n", ExitStat);
	}

	pw = sm_getpwuid(getuid());
	if (pw != NULL)
		uname = pw->pw_name;
	else
	{
		uname = ubuf;
		snprintf(ubuf, sizeof ubuf, "UID%d", getuid());
	}

	if (LogLevel > 0)
		sm_syslog(panic ? LOG_ALERT : LOG_CRIT,
			  CurEnv == NULL ? NOQID : CurEnv->e_id,
			  "SYSERR(%s): %.900s",
			  uname, &MsgBuf[4]);
	switch (olderrno)
	{
	  case EBADF:
	  case ENFILE:
	  case EMFILE:
	  case ENOTTY:
#ifdef EFBIG
	  case EFBIG:
#endif
#ifdef ESPIPE
	  case ESPIPE:
#endif
#ifdef EPIPE
	  case EPIPE:
#endif
#ifdef ENOBUFS
	  case ENOBUFS:
#endif
#ifdef ESTALE
	  case ESTALE:
#endif
		printopenfds(TRUE);
		mci_dump_all(TRUE);
		break;
	}
	if (panic)
	{
#ifdef XLA
		xla_all_end();
#endif
		if (tTd(0, 1))
			abort();
		exit(EX_OSERR);
	}
	errno = 0;
	if (QuickAbort)
		longjmp(TopFrame, 2);
}
/*
**  USRERR -- Signal user error.
**
**	This is much like syserr except it is for user errors.
**
**	Parameters:
**		fmt -- the format string.  If it does not begin with
**			a three-digit SMTP reply code, 501 is assumed.
**		(others) -- printf strings
**
**	Returns:
**		none
**		Through TopFrame if QuickAbort is set.
**
**	Side Effects:
**		increments Errors.
*/

/*VARARGS1*/
void
#ifdef __STDC__
usrerr(const char *fmt, ...)
#else
usrerr(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	VA_LOCAL_DECL

	if (SuprErrs)
		return;

	VA_START(fmt);
	fmtmsg(MsgBuf, CurEnv->e_to, "501", 0, fmt, ap);
	VA_END;

	/* save this message for mailq printing */
	switch (MsgBuf[0])
	{
	  case '4':
	  case '8':
		if (CurEnv->e_message != NULL)
			break;

		/* fall through.... */

	  case '5':
	  case '6':
		if (CurEnv->e_message != NULL)
			free(CurEnv->e_message);
		if (MsgBuf[0] == '6')
		{
			char buf[MAXLINE];

			snprintf(buf, sizeof buf, "Postmaster warning: %.*s",
				sizeof buf - 22, MsgBuf + 4);
			CurEnv->e_message = newstr(buf);
		}
		else
		{
			CurEnv->e_message = newstr(MsgBuf + 4);
		}
		break;
	}

	puterrmsg(MsgBuf);

	if (LogLevel > 3 && LogUsrErrs)
		sm_syslog(LOG_NOTICE, CurEnv->e_id,
			"%.900s",
			&MsgBuf[4]);

	if (QuickAbort)
		longjmp(TopFrame, 1);
}
/*
**  MESSAGE -- print message (not necessarily an error)
**
**	Parameters:
**		msg -- the message (printf fmt) -- it can begin with
**			an SMTP reply code.  If not, 050 is assumed.
**		(others) -- printf arguments
**
**	Returns:
**		none
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
void
#ifdef __STDC__
message(const char *msg, ...)
#else
message(msg, va_alist)
	const char *msg;
	va_dcl
#endif
{
	VA_LOCAL_DECL

	errno = 0;
	VA_START(msg);
	fmtmsg(MsgBuf, CurEnv->e_to, "050", 0, msg, ap);
	VA_END;
	putoutmsg(MsgBuf, FALSE, FALSE);

	/* save this message for mailq printing */
	switch (MsgBuf[0])
	{
	  case '4':
	  case '8':
		if (CurEnv->e_message != NULL)
			break;
		/* fall through.... */

	  case '5':
		if (CurEnv->e_message != NULL)
			free(CurEnv->e_message);
		CurEnv->e_message = newstr(MsgBuf + 4);
		break;
	}
}
/*
**  NMESSAGE -- print message (not necessarily an error)
**
**	Just like "message" except it never puts the to... tag on.
**
**	Parameters:
**		msg -- the message (printf fmt) -- if it begins
**			with a three digit SMTP reply code, that is used,
**			otherwise 050 is assumed.
**		(others) -- printf arguments
**
**	Returns:
**		none
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
void
#ifdef __STDC__
nmessage(const char *msg, ...)
#else
nmessage(msg, va_alist)
	const char *msg;
	va_dcl
#endif
{
	VA_LOCAL_DECL

	errno = 0;
	VA_START(msg);
	fmtmsg(MsgBuf, (char *) NULL, "050", 0, msg, ap);
	VA_END;
	putoutmsg(MsgBuf, FALSE, FALSE);

	/* save this message for mailq printing */
	switch (MsgBuf[0])
	{
	  case '4':
	  case '8':
		if (CurEnv->e_message != NULL)
			break;
		/* fall through.... */

	  case '5':
		if (CurEnv->e_message != NULL)
			free(CurEnv->e_message);
		CurEnv->e_message = newstr(MsgBuf + 4);
		break;
	}
}
/*
**  PUTOUTMSG -- output error message to transcript and channel
**
**	Parameters:
**		msg -- message to output (in SMTP format).
**		holdmsg -- if TRUE, don't output a copy of the message to
**			our output channel.
**		heldmsg -- if TRUE, this is a previously held message;
**			don't log it to the transcript file.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Outputs msg to the transcript.
**		If appropriate, outputs it to the channel.
**		Deletes SMTP reply code number as appropriate.
*/

void
putoutmsg(msg, holdmsg, heldmsg)
	char *msg;
	bool holdmsg;
	bool heldmsg;
{
	char msgcode = msg[0];

	/* display for debugging */
	if (tTd(54, 8))
		printf("--- %s%s%s\n", msg, holdmsg ? " (hold)" : "",
			heldmsg ? " (held)" : "");

	/* map warnings to something SMTP can handle */
	if (msgcode == '6')
		msg[0] = '5';
	else if (msgcode == '8')
		msg[0] = '4';

	/* output to transcript if serious */
	if (!heldmsg && CurEnv != NULL && CurEnv->e_xfp != NULL &&
	    strchr("45", msg[0]) != NULL)
		fprintf(CurEnv->e_xfp, "%s\n", msg);

	if (LogLevel >= 15 && (OpMode == MD_SMTP || OpMode == MD_DAEMON))
		sm_syslog(LOG_INFO, CurEnv->e_id,
			"--> %s%s",
			msg, holdmsg ? " (held)" : "");

	if (msgcode == '8')
		msg[0] = '0';

	/* output to channel if appropriate */
	if (!Verbose && msg[0] == '0')
		return;
	if (holdmsg)
	{
		/* save for possible future display */
		msg[0] = msgcode;
		snprintf(HeldMessageBuf, sizeof HeldMessageBuf, "%s", msg);
		return;
	}

	(void) fflush(stdout);

	if (OutChannel == NULL)
		return;
	
	/* if DisConnected, OutChannel now points to the transcript */
	if (!DisConnected &&
	    (OpMode == MD_SMTP || OpMode == MD_DAEMON || OpMode == MD_ARPAFTP))
		fprintf(OutChannel, "%s\r\n", msg);
	else
		fprintf(OutChannel, "%s\n", &msg[4]);
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d >>> %s\n", (int) getpid(),
			(OpMode == MD_SMTP || OpMode == MD_DAEMON) ? msg : &msg[4]);
	if (msg[3] == ' ')
		(void) fflush(OutChannel);
	if (!ferror(OutChannel) || DisConnected)
		return;

	/*
	**  Error on output -- if reporting lost channel, just ignore it.
	**  Also, ignore errors from QUIT response (221 message) -- some
	**	rude servers don't read result.
	*/

	if (InChannel == NULL || feof(InChannel) || ferror(InChannel) ||
	    strncmp(msg, "221", 3) == 0)
		return;

	/* can't call syserr, 'cause we are using MsgBuf */
	HoldErrs = TRUE;
	if (LogLevel > 0)
		sm_syslog(LOG_CRIT, CurEnv->e_id,
			"SYSERR: putoutmsg (%s): error on output channel sending \"%s\": %s",
			CurHostName == NULL ? "NO-HOST" : CurHostName,
			shortenstring(msg, MAXSHORTSTR), errstring(errno));
}
/*
**  PUTERRMSG -- like putoutmsg, but does special processing for error messages
**
**	Parameters:
**		msg -- the message to output.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets the fatal error bit in the envelope as appropriate.
*/

void
puterrmsg(msg)
	char *msg;
{
	char msgcode = msg[0];

	/* output the message as usual */
	putoutmsg(msg, HoldErrs, FALSE);

	/* be careful about multiple error messages */
	if (OnlyOneError)
		HoldErrs = TRUE;

	/* signal the error */
	Errors++;

	if (CurEnv == NULL)
		return;
	
	if (msgcode == '6')
	{
		/* notify the postmaster */
		CurEnv->e_flags |= EF_PM_NOTIFY;
	}
	else if (msgcode == '5' && bitset(EF_GLOBALERRS, CurEnv->e_flags))
	{
		/* mark long-term fatal errors */
		CurEnv->e_flags |= EF_FATALERRS;
	}
}
/*
**  FMTMSG -- format a message into buffer.
**
**	Parameters:
**		eb -- error buffer to get result.
**		to -- the recipient tag for this message.
**		num -- arpanet error number.
**		en -- the error number to display.
**		fmt -- format of string.
**		a, b, c, d, e -- arguments.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

static void
fmtmsg(eb, to, num, eno, fmt, ap)
	register char *eb;
	const char *to;
	const char *num;
	int eno;
	const char *fmt;
	va_list ap;
{
	char del;
	int l;
	int spaceleft = sizeof MsgBuf;

	/* output the reply code */
	if (isascii(fmt[0]) && isdigit(fmt[0]) &&
	    isascii(fmt[1]) && isdigit(fmt[1]) &&
	    isascii(fmt[2]) && isdigit(fmt[2]))
	{
		num = fmt;
		fmt += 4;
	}
	if (num[3] == '-')
		del = '-';
	else
		del = ' ';
	(void) snprintf(eb, spaceleft, "%3.3s%c", num, del);
	eb += 4;
	spaceleft -= 4;

	/* output the file name and line number */
	if (FileName != NULL)
	{
		(void) snprintf(eb, spaceleft, "%s: line %d: ",
			shortenstring(FileName, 83), LineNumber);
		eb += (l = strlen(eb));
		spaceleft -= l;
	}

	/* output the "to" person */
	if (to != NULL && to[0] != '\0' &&
	    strncmp(num, "551", 3) != 0 &&
	    strncmp(num, "251", 3) != 0)
	{
		(void) snprintf(eb, spaceleft, "%s... ",
			shortenstring(to, MAXSHORTSTR));
		spaceleft -= strlen(eb);
		while (*eb != '\0')
			*eb++ &= 0177;
	}

	/* output the message */
	(void) vsnprintf(eb, spaceleft, fmt, ap);
	spaceleft -= strlen(eb);
	while (*eb != '\0')
		*eb++ &= 0177;

	/* output the error code, if any */
	if (eno != 0)
		(void) snprintf(eb, spaceleft, ": %s", errstring(eno));
}
/*
**  BUFFER_ERRORS -- arrange to buffer future error messages
**
**	Parameters:
**		none
**
**	Returns:
**		none.
*/

void
buffer_errors()
{
	HeldMessageBuf[0] = '\0';
	HoldErrs = TRUE;
}
/*
**  FLUSH_ERRORS -- flush the held error message buffer
**
**	Parameters:
**		print -- if set, print the message, otherwise just
**			delete it.
**
**	Returns:
**		none.
*/

void
flush_errors(print)
	bool print;
{
	if (print && HeldMessageBuf[0] != '\0')
		putoutmsg(HeldMessageBuf, FALSE, TRUE);
	HeldMessageBuf[0] = '\0';
	HoldErrs = FALSE;
}
/*
**  ERRSTRING -- return string description of error code
**
**	Parameters:
**		errnum -- the error number to translate
**
**	Returns:
**		A string description of errnum.
**
**	Side Effects:
**		none.
*/

const char *
errstring(errnum)
	int errnum;
{
	char *dnsmsg;
	char *bp;
	static char buf[MAXLINE];
# if !HASSTRERROR && !defined(ERRLIST_PREDEFINED)
	extern char *sys_errlist[];
	extern int sys_nerr;
# endif
# if SMTP
	extern char *SmtpPhase;
# endif /* SMTP */

	/*
	**  Handle special network error codes.
	**
	**	These are 4.2/4.3bsd specific; they should be in daemon.c.
	*/

	dnsmsg = NULL;
	switch (errnum)
	{
# if defined(DAEMON) && defined(ETIMEDOUT)
	  case ETIMEDOUT:
	  case ECONNRESET:
		bp = buf;
#if HASSTRERROR
		snprintf(bp, SPACELEFT(buf, bp), "%s", strerror(errnum));
#else
		if (errnum >= 0 && errnum < sys_nerr)
			snprintf(bp, SPACELEFT(buf, bp), "%s", sys_errlist[errnum]);
		else
			snprintf(bp, SPACELEFT(buf, bp), "Error %d", errnum);
#endif
		bp += strlen(bp);
		if (CurHostName != NULL)
		{
			if (errnum == ETIMEDOUT)
			{
				snprintf(bp, SPACELEFT(buf, bp), " with ");
				bp += strlen(bp);
			}
			else
			{
				bp = buf;
				snprintf(bp, SPACELEFT(buf, bp),
					"Connection reset by ");
				bp += strlen(bp);
			}
			snprintf(bp, SPACELEFT(buf, bp), "%s",
				shortenstring(CurHostName, MAXSHORTSTR));
			bp += strlen(buf);
		}
		if (SmtpPhase != NULL)
		{
			snprintf(bp, SPACELEFT(buf, bp), " during %s",
				SmtpPhase);
		}
		return (buf);

	  case EHOSTDOWN:
		if (CurHostName == NULL)
			break;
		(void) snprintf(buf, sizeof buf, "Host %s is down",
			shortenstring(CurHostName, MAXSHORTSTR));
		return (buf);

	  case ECONNREFUSED:
		if (CurHostName == NULL)
			break;
		(void) snprintf(buf, sizeof buf, "Connection refused by %s",
			shortenstring(CurHostName, MAXSHORTSTR));
		return (buf);
# endif

# if NAMED_BIND
	  case HOST_NOT_FOUND + E_DNSBASE:
		dnsmsg = "host not found";
		break;

	  case TRY_AGAIN + E_DNSBASE:
		dnsmsg = "host name lookup failure";
		break;

	  case NO_RECOVERY + E_DNSBASE:
		dnsmsg = "non-recoverable error";
		break;

	  case NO_DATA + E_DNSBASE:
		dnsmsg = "no data known";
		break;
# endif

	  case EPERM:
		/* SunOS gives "Not owner" -- this is the POSIX message */
		return "Operation not permitted";

	/*
	**  Error messages used internally in sendmail.
	*/

	  case E_SM_OPENTIMEOUT:
		return "Timeout on file open";

	  case E_SM_NOSLINK:
		return "Symbolic links not allowed";

	  case E_SM_NOHLINK:
		return "Hard links not allowed";

	  case E_SM_REGONLY:
		return "Regular files only";

	  case E_SM_ISEXEC:
		return "Executable files not allowed";

	  case E_SM_WWDIR:
		return "World writable directory";

	  case E_SM_GWDIR:
		return "Group writable directory";

	  case E_SM_FILECHANGE:
		return "File changed after open";

	  case E_SM_WWFILE:
		return "World writable file";

	  case E_SM_GWFILE:
		return "Group writable file";
	}

	if (dnsmsg != NULL)
	{
		bp = buf;
		strcpy(bp, "Name server: ");
		bp += strlen(bp);
		if (CurHostName != NULL)
		{
			snprintf(bp, SPACELEFT(buf, bp), "%s: ",
				shortenstring(CurHostName, MAXSHORTSTR));
			bp += strlen(bp);
		}
		snprintf(bp, SPACELEFT(buf, bp), "%s", dnsmsg);
		return buf;
	}

#if HASSTRERROR
	return strerror(errnum);
#else
	if (errnum > 0 && errnum < sys_nerr)
		return (sys_errlist[errnum]);

	(void) snprintf(buf, sizeof buf, "Error %d", errnum);
	return (buf);
#endif
}
