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
static char sccsid[] = "@(#)err.c	8.2 (Berkeley) 7/11/93";
#endif /* not lint */

# include "sendmail.h"
# include <errno.h>
# include <netdb.h>

/*
**  SYSERR -- Print error message.
**
**	Prints an error message via printf to the diagnostic
**	output.  If LOG is defined, it logs it also.
**
**	If the first character of the syserr message is `!' it will
**	log this as an ALERT message and exit immediately.  This can
**	leave queue files in an indeterminate state, so it should not
**	be used lightly.
**
**	Parameters:
**		f -- the format string
**		a, b, c, d, e -- parameters
**
**	Returns:
**		none
**		Through TopFrame if QuickAbort is set.
**
**	Side Effects:
**		increments Errors.
**		sets ExitStat.
*/

# ifdef lint
int	sys_nerr;
char	*sys_errlist[];
# endif lint
char	MsgBuf[BUFSIZ*2];	/* text of most recent message */

static void fmtmsg();

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
	VA_LOCAL_DECL

	panic = *fmt == '!';
	if (panic)
		fmt++;

	/* format and output the error message */
	if (olderrno == 0)
		p = "554";
	else
		p = "451";
	VA_START(fmt);
	fmtmsg(MsgBuf, (char *) NULL, p, olderrno, fmt, ap);
	VA_END;
	puterrmsg(MsgBuf);

	/* determine exit status if not already set */
	if (ExitStat == EX_OK)
	{
		if (olderrno == 0)
			ExitStat = EX_SOFTWARE;
		else
			ExitStat = EX_OSERR;
	}

# ifdef LOG
	if (LogLevel > 0)
		syslog(panic ? LOG_ALERT : LOG_CRIT, "%s: SYSERR: %s",
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id,
			&MsgBuf[4]);
# endif /* LOG */
	if (panic)
	{
#ifdef XLA
		xla_all_end();
#endif
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
**		fmt, a, b, c, d -- printf strings
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
	extern char SuprErrs;
	extern int errno;

	if (SuprErrs)
		return;

	VA_START(fmt);
	fmtmsg(MsgBuf, CurEnv->e_to, "501", 0, fmt, ap);
	VA_END;
	puterrmsg(MsgBuf);

# ifdef LOG
	if (LogLevel > 3 && LogUsrErrs)
		syslog(LOG_NOTICE, "%s: %s",
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id,
			&MsgBuf[4]);
# endif /* LOG */

	if (QuickAbort)
		longjmp(TopFrame, 1);
}
/*
**  MESSAGE -- print message (not necessarily an error)
**
**	Parameters:
**		msg -- the message (printf fmt) -- it can begin with
**			an SMTP reply code.  If not, 050 is assumed.
**		a, b, c, d, e -- printf arguments
**
**	Returns:
**		none
**
**	Side Effects:
**		none.
*/

/*VARARGS2*/
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
	putoutmsg(MsgBuf, FALSE);
}
/*
**  NMESSAGE -- print message (not necessarily an error)
**
**	Just like "message" except it never puts the to... tag on.
**
**	Parameters:
**		num -- the default ARPANET error number (in ascii)
**		msg -- the message (printf fmt) -- if it begins
**			with three digits, this number overrides num.
**		a, b, c, d, e -- printf arguments
**
**	Returns:
**		none
**
**	Side Effects:
**		none.
*/

/*VARARGS2*/
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
	putoutmsg(MsgBuf, FALSE);
}
/*
**  PUTOUTMSG -- output error message to transcript and channel
**
**	Parameters:
**		msg -- message to output (in SMTP format).
**		holdmsg -- if TRUE, don't output a copy of the message to
**			our output channel.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Outputs msg to the transcript.
**		If appropriate, outputs it to the channel.
**		Deletes SMTP reply code number as appropriate.
*/

putoutmsg(msg, holdmsg)
	char *msg;
	bool holdmsg;
{
	/* output to transcript if serious */
	if (CurEnv->e_xfp != NULL && (msg[0] == '4' || msg[0] == '5'))
		fprintf(CurEnv->e_xfp, "%s\n", msg);

	/* output to channel if appropriate */
	if (holdmsg || (!Verbose && msg[0] == '0'))
		return;

	(void) fflush(stdout);
	if (OpMode == MD_SMTP)
		fprintf(OutChannel, "%s\r\n", msg);
	else
		fprintf(OutChannel, "%s\n", &msg[4]);
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d >>> %s\n", getpid(),
			OpMode == MD_SMTP ? msg : &msg[4]);
	if (msg[3] == ' ')
		(void) fflush(OutChannel);
	if (!ferror(OutChannel))
		return;

	/* error on output -- if reporting lost channel, just ignore it */
	if (feof(InChannel) || ferror(InChannel))
		return;

	/* can't call syserr, 'cause we are using MsgBuf */
	HoldErrs = TRUE;
#ifdef LOG
	if (LogLevel > 0)
		syslog(LOG_CRIT,
			"%s: SYSERR: putoutmsg (%s): error on output channel sending \"%s\"",
			CurEnv->e_id == NULL ? "NOQUEUE" : CurEnv->e_id,
			CurHostName, msg);
#endif
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

puterrmsg(msg)
	char *msg;
{
	/* output the message as usual */
	putoutmsg(msg, HoldErrs);

	/* signal the error */
	Errors++;
	if (msg[0] == '5')
		CurEnv->e_flags |= EF_FATALERRS;
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
	char *to;
	char *num;
	int eno;
	char *fmt;
	va_list ap;
{
	char del;
	char *meb;

	/* output the reply code */
	if (isdigit(fmt[0]) && isdigit(fmt[1]) && isdigit(fmt[2]))
	{
		num = fmt;
		fmt += 4;
	}
	if (num[3] == '-')
		del = '-';
	else
		del = ' ';
	(void) sprintf(eb, "%3.3s%c", num, del);
	eb += 4;

	/* output the file name and line number */
	if (FileName != NULL)
	{
		(void) sprintf(eb, "%s: line %d: ", FileName, LineNumber);
		eb += strlen(eb);
	}

	/* output the "to" person */
	if (to != NULL && to[0] != '\0')
	{
		(void) sprintf(eb, "%s... ", to);
		while (*eb != '\0')
			*eb++ &= 0177;
	}

	meb = eb;

	/* output the message */
	(void) vsprintf(eb, fmt, ap);
	while (*eb != '\0')
		*eb++ &= 0177;

	/* output the error code, if any */
	if (eno != 0)
	{
		(void) sprintf(eb, ": %s", errstring(eno));
		eb += strlen(eb);
	}

	if (CurEnv->e_message == NULL && strchr("45", num[0]) != NULL)
		CurEnv->e_message = newstr(meb);
}
/*
**  ERRSTRING -- return string description of error code
**
**	Parameters:
**		errno -- the error number to translate
**
**	Returns:
**		A string description of errno.
**
**	Side Effects:
**		none.
*/

const char *
errstring(errno)
	int errno;
{
	extern const char *const sys_errlist[];
	extern int sys_nerr;
	static char buf[MAXLINE];
# ifdef SMTP
	extern char *SmtpPhase;
# endif /* SMTP */

# ifdef DAEMON
# ifdef ETIMEDOUT
	/*
	**  Handle special network error codes.
	**
	**	These are 4.2/4.3bsd specific; they should be in daemon.c.
	*/

	switch (errno)
	{
	  case ETIMEDOUT:
	  case ECONNRESET:
		(void) strcpy(buf, sys_errlist[errno]);
		if (SmtpPhase != NULL)
		{
			(void) strcat(buf, " during ");
			(void) strcat(buf, SmtpPhase);
		}
		if (CurHostName != NULL)
		{
			(void) strcat(buf, " with ");
			(void) strcat(buf, CurHostName);
		}
		return (buf);

	  case EHOSTDOWN:
		if (CurHostName == NULL)
			break;
		(void) sprintf(buf, "Host %s is down", CurHostName);
		return (buf);

	  case ECONNREFUSED:
		if (CurHostName == NULL)
			break;
		(void) sprintf(buf, "Connection refused by %s", CurHostName);
		return (buf);

# ifdef NAMED_BIND
	  case HOST_NOT_FOUND + MAX_ERRNO:
		return ("Name server: host not found");

	  case TRY_AGAIN + MAX_ERRNO:
		return ("Name server: host name lookup failure");

	  case NO_RECOVERY + MAX_ERRNO:
		return ("Name server: non-recoverable error");

	  case NO_DATA + MAX_ERRNO:
		return ("Name server: no data known for name");
# endif
	}
# endif
# endif

	if (errno > 0 && errno < sys_nerr)
		return (sys_errlist[errno]);

	(void) sprintf(buf, "Error %d", errno);
	return (buf);
}
