/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)control.c	8.17 (Berkeley) 12/1/1998";
#endif /* not lint */

#include "sendmail.h"

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
#ifdef NETUNIX
# if _FFR_CONTROL_SOCKET
	int rval;
	int sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_CREAT|SFF_MUSTOWN;
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

	unlink(ControlSocketName);
	bzero(&controladdr, sizeof controladdr);
	controladdr.sun_family = AF_UNIX;
	strcpy(controladdr.sun_path, ControlSocketName);

	if (bind(ControlSocket, (struct sockaddr *) &controladdr,
		 sizeof controladdr) < 0)
	{
		int save_errno = errno;

		(void) close(ControlSocket);
		ControlSocket = -1;
		errno = save_errno;
		return -1;
	}

#  if _FFR_TRUSTED_USER
	if (geteuid() == 0 && TrustedUid != 0)
	{
		if (chown(ControlSocketName, TrustedUid, -1) < 0)
		{
			int save_errno = errno;

			sm_syslog(LOG_ALERT, NOQID,
				  "ownership change on %s failed: %s",
				  ControlSocketName, errstring(save_errno));
			message("050 ownership change on %s failed: %s",
				ControlSocketName, errstring(save_errno));
			errno = save_errno;
			return -1;
		}
	}
#  endif

	if (chmod(ControlSocketName, S_IRUSR|S_IWUSR) < 0)
	{
		int save_errno = errno;

		closecontrolsocket(TRUE);
		errno = save_errno;
		return -1;
	}

	if (listen(ControlSocket, 8) < 0)
	{
		int save_errno = errno;

		closecontrolsocket(TRUE);
		errno = save_errno;
		return -1;
	}
# endif
#endif
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
#ifdef NETUNIX
# if _FFR_CONTROL_SOCKET
	int sff = SFF_SAFEDIRPATH|SFF_OPENASROOT|SFF_NOLINK|SFF_CREAT|SFF_MUSTOWN;

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
# endif
#endif
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
#ifdef NETUNIX
# if _FFR_CONTROL_SOCKET
	if (ControlSocket >= 0)
		(void) close(ControlSocket);
	ControlSocket = -1;
# endif
#endif
}

#ifndef NOT_SENDMAIL

/*
**  CONTROL_COMMAND -- read and process command from named socket
**
**	Read and process the command from the opened socket.
**	Return the results down the same socket.
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
	char	*cmdname;	/* command name */
	int	cmdcode;	/* internal code, see below */
};

/* values for cmdcode */
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

void
control_command(sock, e)
	int sock;
	ENVELOPE *e;
{
	FILE *s;
	FILE *traffic;
	FILE *oldout;
	char *cmd;
	char *p;
	struct cmd *c;
	char cmdbuf[MAXLINE];
	char inp[MAXLINE];
	extern char **SaveArgv;
	extern void help __P((char *));

	sm_setproctitle(FALSE, "control cmd read");
		
	s = fdopen(sock, "r+");
	if (s == NULL)
	{
		int save_errno = errno;

		close(sock);
		errno = save_errno;
		return;
	}
	setbuf(s, NULL);

	if (fgets(inp, sizeof inp, s) == NULL)
	{
		fclose(s);
		return;
	}
	(void) fflush(s);

	/* clean up end of line */
	fixcrlf(inp, TRUE);

	sm_setproctitle(FALSE, "control: %s", inp);

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
	for (c = CmdTab; c->cmdname != NULL; c++)
	{
		if (!strcasecmp(c->cmdname, cmdbuf))
			break;
	}

	switch (c->cmdcode)
	{
	  case CMDHELP:		/* get help */
		traffic = TrafficLogFile;
		TrafficLogFile = NULL;
		oldout = OutChannel;
		OutChannel = s;
		help("control");
		TrafficLogFile = traffic;
		OutChannel = oldout;
		break;
		
	  case CMDRESTART:	/* restart the daemon */
		if (SaveArgv[0][0] != '/')
		{
			fprintf(s, "ERROR: could not restart: need full path\r\n");
			break;
		}
		if (LogLevel > 3)
			sm_syslog(LOG_INFO, NOQID,
				  "restarting %s on due to control command",
				  SaveArgv[0]);
		closecontrolsocket(FALSE);
		if (drop_privileges(TRUE) != EX_OK)
		{
			if (LogLevel > 0)
				sm_syslog(LOG_ALERT, NOQID,
					  "could not set[ug]id(%d, %d): %m",
					  RunAsUid, RunAsGid);

			fprintf(s, "ERROR: could not set[ug]id(%d, %d): %s, exiting...\r\n",
				(int)RunAsUid, (int)RunAsGid, errstring(errno));
			finis(FALSE, EX_OSERR);
		}
		fprintf(s, "OK\r\n");
		clrcontrol();
		(void) fcntl(sock, F_SETFD, 1);
		execve(SaveArgv[0], (ARGV_T) SaveArgv, (ARGV_T) ExternalEnviron);
		if (LogLevel > 0)
			sm_syslog(LOG_ALERT, NOQID, "could not exec %s: %m",
				  SaveArgv[0]);
		fprintf(s, "ERROR: could not exec %s: %s, exiting...\r\n",
			SaveArgv[0], errstring(errno));
		finis(FALSE, EX_OSFILE);
		break;

	  case CMDSHUTDOWN:	/* kill the daemon */
		fprintf(s, "OK\r\n");
		finis(FALSE, EX_OK);
		break;

	  case CMDSTATUS:	/* daemon status */
		proc_list_probe();
		fprintf(s, "%d/%d\r\n", CurChildren, MaxChildren);
		proc_list_display(s);
		break;

	  case CMDERROR:	/* unknown command */
		fprintf(s, "Bad command (%s)\r\n", cmdbuf);
		break;
	}
	fclose(s);
}
#endif
