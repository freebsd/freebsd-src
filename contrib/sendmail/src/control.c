/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: control.c,v 8.44.14.8 2000/09/17 17:04:26 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


int ControlSocket = -1;

/*
**  OPENCONTROLSOCKET -- create/open the daemon control named socket
**
**	Creates and opens a named socket for external control over
**	the sendmail daemon.
**
**	Parameters:
**		none.
**
**	Returns:
**		0 if successful, -1 otherwise
*/

int
opencontrolsocket()
{
#if NETUNIX
	int save_errno;
	int rval;
	long sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_CREAT|SFF_MUSTOWN;
	struct sockaddr_un controladdr;

	if (ControlSocketName == NULL)
		return 0;

	if (strlen(ControlSocketName) >= sizeof controladdr.sun_path)
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	rval = safefile(ControlSocketName, RunAsUid, RunAsGid, RunAsUserName,
			sff, S_IRUSR|S_IWUSR, NULL);

	/* if not safe, don't create */
	if (rval != 0)
	{
		errno = rval;
		return -1;
	}

	ControlSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ControlSocket < 0)
		return -1;

	(void) unlink(ControlSocketName);
	memset(&controladdr, '\0', sizeof controladdr);
	controladdr.sun_family = AF_UNIX;
	(void) strlcpy(controladdr.sun_path, ControlSocketName,
		       sizeof controladdr.sun_path);

	if (bind(ControlSocket, (struct sockaddr *) &controladdr,
		 sizeof controladdr) < 0)
	{
		save_errno = errno;
		clrcontrol();
		errno = save_errno;
		return -1;
	}

	if (geteuid() == 0 && TrustedUid != 0)
	{
		if (chown(ControlSocketName, TrustedUid, -1) < 0)
		{
			save_errno = errno;
			sm_syslog(LOG_ALERT, NOQID,
				  "ownership change on %s failed: %s",
				  ControlSocketName, errstring(save_errno));
			message("050 ownership change on %s failed: %s",
				ControlSocketName, errstring(save_errno));
			closecontrolsocket(TRUE);
			errno = save_errno;
			return -1;
		}
	}

	if (chmod(ControlSocketName, S_IRUSR|S_IWUSR) < 0)
	{
		save_errno = errno;
		closecontrolsocket(TRUE);
		errno = save_errno;
		return -1;
	}

	if (listen(ControlSocket, 8) < 0)
	{
		save_errno = errno;
		closecontrolsocket(TRUE);
		errno = save_errno;
		return -1;
	}
#endif /* NETUNIX */
	return 0;
}
/*
**  CLOSECONTROLSOCKET -- close the daemon control named socket
**
**	Close a named socket.
**
**	Parameters:
**		fullclose -- if set, close the socket and remove it;
**			     otherwise, just remove it
**
**	Returns:
**		none.
*/

void
closecontrolsocket(fullclose)
	bool fullclose;
{
#if NETUNIX
	long sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_CREAT|SFF_MUSTOWN;

	if (ControlSocket >= 0)
	{
		int rval;

		if (fullclose)
		{
			(void) close(ControlSocket);
			ControlSocket = -1;
		}

		rval = safefile(ControlSocketName, RunAsUid, RunAsGid, RunAsUserName,
				sff, S_IRUSR|S_IWUSR, NULL);

		/* if not safe, don't unlink */
		if (rval != 0)
			return;

		if (unlink(ControlSocketName) < 0)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "Could not remove control socket: %s",
				  errstring(errno));
			return;
		}
	}
#endif /* NETUNIX */
	return;
}
/*
**  CLRCONTROL -- reset the control connection
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		releases any resources used by the control interface.
*/

void
clrcontrol()
{
#if NETUNIX
	if (ControlSocket >= 0)
		(void) close(ControlSocket);
	ControlSocket = -1;
#endif /* NETUNIX */
}

#ifndef NOT_SENDMAIL

/*
**  CONTROL_COMMAND -- read and process command from named socket
**
**	Read and process the command from the opened socket.
**	Exits when done since it is running in a forked child.
**
**	Parameters:
**		sock -- the opened socket from getrequests()
**		e -- the current envelope
**
**	Returns:
**		none.
*/

struct cmd
{
	char	*cmd_name;	/* command name */
	int	cmd_code;	/* internal code, see below */
};

/* values for cmd_code */
# define CMDERROR	0	/* bad command */
# define CMDRESTART	1	/* restart daemon */
# define CMDSHUTDOWN	2	/* end daemon */
# define CMDHELP	3	/* help */
# define CMDSTATUS	4	/* daemon status */

static struct cmd	CmdTab[] =
{
	{ "help",	CMDHELP		},
	{ "restart",	CMDRESTART	},
	{ "shutdown",	CMDSHUTDOWN	},
	{ "status",	CMDSTATUS	},
	{ NULL,		CMDERROR	}
};

static jmp_buf	CtxControlTimeout;

static void
controltimeout(timeout)
	time_t timeout;
{
	longjmp(CtxControlTimeout, 1);
}

void
control_command(sock, e)
	int sock;
	ENVELOPE *e;
{
	volatile int exitstat = EX_OK;
	FILE *s = NULL;
	EVENT *ev = NULL;
	FILE *traffic;
	FILE *oldout;
	char *cmd;
	char *p;
	struct cmd *c;
	char cmdbuf[MAXLINE];
	char inp[MAXLINE];

	sm_setproctitle(FALSE, e, "control cmd read");

	if (TimeOuts.to_control > 0)
	{
		/* handle possible input timeout */
		if (setjmp(CtxControlTimeout) != 0)
		{
			if (LogLevel > 2)
				sm_syslog(LOG_NOTICE, e->e_id,
					  "timeout waiting for input during control command");
			exit(EX_IOERR);
		}
		ev = setevent(TimeOuts.to_control, controltimeout,
			      TimeOuts.to_control);
	}

	s = fdopen(sock, "r+");
	if (s == NULL)
	{
		int save_errno = errno;

		(void) close(sock);
		errno = save_errno;
		exit(EX_IOERR);
	}
	setbuf(s, NULL);

	if (fgets(inp, sizeof inp, s) == NULL)
	{
		(void) fclose(s);
		exit(EX_IOERR);
	}
	(void) fflush(s);

	/* clean up end of line */
	fixcrlf(inp, TRUE);

	sm_setproctitle(FALSE, e, "control: %s", inp);

	/* break off command */
	for (p = inp; isascii(*p) && isspace(*p); p++)
		continue;
	cmd = cmdbuf;
	while (*p != '\0' &&
	       !(isascii(*p) && isspace(*p)) &&
	       cmd < &cmdbuf[sizeof cmdbuf - 2])
		*cmd++ = *p++;
	*cmd = '\0';

	/* throw away leading whitespace */
	while (isascii(*p) && isspace(*p))
		p++;

	/* decode command */
	for (c = CmdTab; c->cmd_name != NULL; c++)
	{
		if (strcasecmp(c->cmd_name, cmdbuf) == 0)
			break;
	}

	switch (c->cmd_code)
	{
	  case CMDHELP:		/* get help */
		traffic = TrafficLogFile;
		TrafficLogFile = NULL;
		oldout = OutChannel;
		OutChannel = s;
		help("control", e);
		TrafficLogFile = traffic;
		OutChannel = oldout;
		break;

	  case CMDRESTART:	/* restart the daemon */
		fprintf(s, "OK\r\n");
		exitstat = EX_RESTART;
		break;

	  case CMDSHUTDOWN:	/* kill the daemon */
		fprintf(s, "OK\r\n");
		exitstat = EX_SHUTDOWN;
		break;

	  case CMDSTATUS:	/* daemon status */
		proc_list_probe();
		{
			long bsize;
			long free;

			free = freediskspace(QueueDir, &bsize);

			/*
			**  Prevent overflow and don't lose
			**  precision (if bsize == 512)
			*/

			free = (long)((double)free * ((double)bsize / 1024));

			fprintf(s, "%d/%d/%ld/%d\r\n",
				CurChildren, MaxChildren,
				free, sm_getla(NULL));
		}
		proc_list_display(s);
		break;

	  case CMDERROR:	/* unknown command */
		fprintf(s, "Bad command (%s)\r\n", cmdbuf);
		break;
	}
	(void) fclose(s);
	if (ev != NULL)
		clrevent(ev);
	exit(exitstat);
}
#endif /* ! NOT_SENDMAIL */

