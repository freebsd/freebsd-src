/*
 * main.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 *
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 *
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * $Whistle: main.c,v 1.12 1999/11/29 19:17:46 archie Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#ifdef EDITLINE
#include <signal.h>
#include <histedit.h>
#include <pthread.h>
#endif
#ifdef JAIL
#include <sys/jail.h>
#include <jail.h>
#endif

#include <netgraph.h>

#include "ngctl.h"

#define PROMPT			"+ "
#define MAX_ARGS		512
#define WHITESPACE		" \t\r\n\v\f"
#define DUMP_BYTES_PER_LINE	16

/* Internal functions */
static int	ReadFile(FILE *fp);
static void	ReadCtrlSocket(void);
static void	ReadDataSocket(void);
static int	DoParseCommand(const char *line);
static int	DoCommand(int ac, char **av);
static int	DoInteractive(void);
static const	struct ngcmd *FindCommand(const char *string);
static int	MatchCommand(const struct ngcmd *cmd, const char *s);
static void	Usage(const char *msg);
static int	ReadCmd(int ac, char **av);
static int	HelpCmd(int ac, char **av);
static int	QuitCmd(int ac, char **av);
#ifdef EDITLINE
static volatile sig_atomic_t unblock;
static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	cond = PTHREAD_COND_INITIALIZER;
#endif

/* List of commands */
static const struct ngcmd *const cmds[] = {
	&config_cmd,
	&connect_cmd,
	&debug_cmd,
	&dot_cmd,
	&help_cmd,
	&list_cmd,
	&mkpeer_cmd,
	&msg_cmd,
	&name_cmd,
	&read_cmd,
	&rmhook_cmd,
	&show_cmd,
	&shutdown_cmd,
	&status_cmd,
	&types_cmd,
	&write_cmd,
	&quit_cmd,
	NULL
};

/* Commands defined in this file */
const struct ngcmd read_cmd = {
	ReadCmd,
	"read <filename>",
	"Read and execute commands from a file",
	NULL,
	{ "source", "." }
};
const struct ngcmd help_cmd = {
	HelpCmd,
	"help [command]",
	"Show command summary or get more help on a specific command",
	NULL,
	{ "?" }
};
const struct ngcmd quit_cmd = {
	QuitCmd,
	"quit",
	"Exit program",
	NULL,
	{ "exit" }
};

/* Our control and data sockets */
int	csock, dsock;

/*
 * main()
 */
int
main(int ac, char *av[])
{
	char		name[NG_NODESIZ];
	int		interactive = isatty(0) && isatty(1);
	FILE		*fp = NULL;
#ifdef JAIL
	const char	*jail_name = NULL;
	int		jid;
#endif
	int		ch, rtn = 0;

	/* Set default node name */
	snprintf(name, sizeof(name), "ngctl%d", getpid());

	/* Parse command line */
	while ((ch = getopt(ac, av, "df:j:n:")) != -1) {
		switch (ch) {
		case 'd':
			NgSetDebug(NgSetDebug(-1) + 1);
			break;
		case 'f':
			if (strcmp(optarg, "-") == 0)
				fp = stdin;
			else if ((fp = fopen(optarg, "r")) == NULL)
				err(EX_NOINPUT, "%s", optarg);
			break;
		case 'j':
#ifdef JAIL
			jail_name = optarg;
#else
			errx(EX_UNAVAILABLE, "not built with jail support");
#endif
			break;
		case 'n':
			snprintf(name, sizeof(name), "%s", optarg);
			break;
		default:
			Usage((char *)NULL);
			break;
		}
	}
	ac -= optind;
	av += optind;

#ifdef JAIL
	if (jail_name != NULL) {
		if (jail_name[0] == '\0')
			Usage("invalid jail name");

		jid = jail_getid(jail_name);

		if (jid == -1)
			errx((errno == EPERM) ? EX_NOPERM : EX_NOHOST,
			    "%s", jail_errmsg);
		if (jail_attach(jid) != 0)
			errx((errno == EPERM) ? EX_NOPERM : EX_OSERR,
			    "cannot attach to jail");
	}
#endif

	/* Create a new socket node */
	if (NgMkSockNode(name, &csock, &dsock) < 0)
		err(EX_OSERR, "can't create node");

	/* Do commands as requested */
	if (ac == 0) {
		if (fp != NULL) {
			rtn = ReadFile(fp);
		} else if (interactive) {
			rtn = DoInteractive();
		} else
			Usage("no command specified");
	} else {
		rtn = DoCommand(ac, av);
	}

	/* Convert command return code into system exit code */
	switch (rtn) {
	case CMDRTN_OK:
	case CMDRTN_QUIT:
		rtn = 0;
		break;
	case CMDRTN_USAGE:
		rtn = EX_USAGE;
		break;
	case CMDRTN_ERROR:
		rtn = EX_OSERR;
		break;
	}
	return (rtn);
}

/*
 * Process commands from a file
 */
static int
ReadFile(FILE *fp)
{
	char *line = NULL;
	ssize_t len;
	size_t sz = 0;
	unsigned int lineno = 0;
	int rtn = CMDRTN_OK;

	while ((len = getline(&line, &sz, fp)) >= 0) {
		lineno++;
		if (*line == '#')
			continue;
		if ((rtn = DoParseCommand(line)) != CMDRTN_OK) {
			warnx("line %d: error in file", lineno);
			break;
		}
	}
	if (ferror(fp))
		rtn = CMDRTN_ERROR;
	free(line);
	return (rtn);
}

#ifdef EDITLINE
/* Signal handler for Monitor() thread. */
static void
Unblock(int signal __unused)
{
	unblock = 1;
}

/*
 * Thread that monitors csock and dsock while main thread
 * can be blocked in el_gets().
 */
static void *
Monitor(void *v __unused)
{
	struct pollfd pfds[2] = {
		{ .fd = csock, .events = POLLIN },
		{ .fd = dsock, .events = POLLIN },
	};
	struct sigaction act;

	act.sa_handler = Unblock;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGUSR1, &act, NULL);

	pthread_mutex_lock(&mutex);
	for (;;) {
		unblock = 0;
		if (poll(pfds, 2, INFTIM) <= 0) {
			if (errno == EINTR) {
				if (unblock == 1)
					pthread_cond_wait(&cond, &mutex);
				continue;
			}
			err(EX_OSERR, "poll");
		}
		if (pfds[0].revents != 0)
			ReadCtrlSocket();
		if (pfds[1].revents != 0)
			ReadDataSocket();
	}

	return (NULL);
}

static char *
Prompt(EditLine *el __unused)
{
	return (PROMPT);
}

/*
 * Here we start a thread, that will monitor the netgraph
 * sockets and catch any unexpected messages or data on them,
 * that can arrive while user edits his/her commands.
 *
 * Whenever we expect data on netgraph sockets, we send signal
 * to monitoring thread. The signal forces it to exit select()
 * system call and sleep on condvar until we wake it. While
 * monitoring thread sleeps, we can do our work with netgraph
 * sockets.
 */
static int
DoInteractive(void)
{
	pthread_t monitor;
	EditLine *el;
	History *hist;
	HistEvent hev = { 0, "" };

	(*help_cmd.func)(0, NULL);
	pthread_create(&monitor, NULL, Monitor, NULL);
	el = el_init(getprogname(), stdin, stdout, stderr);
	if (el == NULL)
		return (CMDRTN_ERROR);
	el_set(el, EL_PROMPT, Prompt);
	el_set(el, EL_SIGNAL, 1);
	el_set(el, EL_EDITOR, "emacs");
	hist = history_init();
	if (hist == NULL)
		return (CMDRTN_ERROR);
	history(hist, &hev, H_SETSIZE, 100);
	history(hist, &hev, H_SETUNIQUE, 1);
	el_set(el, EL_HIST, history, (const char *)hist);
	el_source(el, NULL);

	for (;;) {
		const char *buf;
		int count;

		if ((buf = el_gets(el, &count)) == NULL) {
			printf("\n");
			break;
		}
		history(hist, &hev, H_ENTER, buf);
		pthread_kill(monitor, SIGUSR1);
		pthread_mutex_lock(&mutex);
		if (DoParseCommand(buf) == CMDRTN_QUIT) {
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}

	history_end(hist);
	el_end(el);
	pthread_cancel(monitor);

	return (CMDRTN_QUIT);
}

#else /* !EDITLINE */

/*
 * Interactive mode w/o libedit functionality.
 */
static int
DoInteractive(void)
{
	struct pollfd pfds[3] = {
		{ .fd = csock, .events = POLLIN },
		{ .fd = dsock, .events = POLLIN },
		{ .fd = STDIN_FILENO, .events = POLLIN },
	};
	char *line = NULL;
	ssize_t len;
	size_t sz = 0;

	(*help_cmd.func)(0, NULL);
	for (;;) {
		/* See if any data or control messages are arriving */
		if (poll(pfds, 2, 0) <= 0) {
			/* Issue prompt and wait for anything to happen */
			printf("%s", PROMPT);
			fflush(stdout);
			if (poll(pfds, 3, INFTIM) < 0 && errno != EINTR)
				err(EX_OSERR, "poll");
		} else {
			pfds[2].revents = 0;
		}

		/* If not user input, print a newline first */
		if (pfds[2].revents == 0)
			printf("\n");

		if (pfds[0].revents != 0)
			ReadCtrlSocket();
		if (pfds[1].revents != 0)
			ReadDataSocket();

		/* Get any user input */
		if (pfds[2].revents != 0) {
			if ((len = getline(&line, &sz, stdin)) <= 0) {
				printf("\n");
				break;
			}
			if (DoParseCommand(line) == CMDRTN_QUIT)
				break;
		}
	}
	free(line);
	return (CMDRTN_QUIT);
}
#endif /* !EDITLINE */

/*
 * Read and process data on netgraph control and data sockets.
 */
static void
ReadCtrlSocket(void)
{
	MsgRead();
}

static void
ReadDataSocket(void)
{
	char hook[NG_HOOKSIZ];
	u_char *buf;
	int rl;

	/* Read packet from socket. */
	if ((rl = NgAllocRecvData(dsock, &buf, hook)) < 0)
		err(EX_OSERR, "reading hook \"%s\"", hook);
	if (rl == 0)
		errx(EX_OSERR, "EOF from hook \"%s\"?", hook);

	/* Write packet to stdout. */
	printf("Rec'd data packet on hook \"%s\":\n", hook);
	DumpAscii(buf, rl);
	free(buf);
}

/*
 * Parse a command line and execute the command
 */
static int
DoParseCommand(const char *line)
{
	char *av[MAX_ARGS];
	int ac;

	/* Parse line */
	for (ac = 0, av[0] = strtok((char *)line, WHITESPACE);
	    ac < MAX_ARGS - 1 && av[ac];
	    av[++ac] = strtok(NULL, WHITESPACE));

	/* Do command */
	return (DoCommand(ac, av));
}

/*
 * Execute the command
 */
static int
DoCommand(int ac, char **av)
{
	const struct ngcmd *cmd;
	int rtn;

	if (ac == 0 || *av[0] == 0)
		return (CMDRTN_OK);
	if ((cmd = FindCommand(av[0])) == NULL)
		return (CMDRTN_ERROR);
	if ((rtn = (*cmd->func)(ac, av)) == CMDRTN_USAGE)
		warnx("usage: %s", cmd->cmd);
	return (rtn);
}

/*
 * Find a command
 */
static const struct ngcmd *
FindCommand(const char *string)
{
	int k, found = -1;

	for (k = 0; cmds[k] != NULL; k++) {
		if (MatchCommand(cmds[k], string)) {
			if (found != -1) {
				warnx("\"%s\": ambiguous command", string);
				return (NULL);
			}
			found = k;
		}
	}
	if (found == -1) {
		warnx("\"%s\": unknown command", string);
		return (NULL);
	}
	return (cmds[found]);
}

/*
 * See if string matches a prefix of "cmd" (or an alias) case insensitively
 */
static int
MatchCommand(const struct ngcmd *cmd, const char *s)
{
	int a;

	/* Try to match command, ignoring the usage stuff */
	if (strlen(s) <= strcspn(cmd->cmd, WHITESPACE)) {
		if (strncasecmp(s, cmd->cmd, strlen(s)) == 0)
			return (1);
	}

	/* Try to match aliases */
	for (a = 0; a < MAX_CMD_ALIAS && cmd->aliases[a] != NULL; a++) {
		if (strlen(cmd->aliases[a]) >= strlen(s)) {
			if (strncasecmp(s, cmd->aliases[a], strlen(s)) == 0)
				return (1);
		}
	}

	/* No match */
	return (0);
}

/*
 * ReadCmd()
 */
static int
ReadCmd(int ac, char **av)
{
	FILE *fp;
	int rtn;

	/* Open file */
	switch (ac) {
	case 2:
		if ((fp = fopen(av[1], "r")) == NULL) {
			warn("%s", av[1]);
			return (CMDRTN_ERROR);
		}
		break;
	default:
		return (CMDRTN_USAGE);
	}

	/* Process it */
	rtn = ReadFile(fp);
	if (ferror(fp))
		warn("%s", av[1]);
	fclose(fp);
	return (rtn);
}

/*
 * HelpCmd()
 */
static int
HelpCmd(int ac, char **av)
{
	const struct ngcmd *cmd;
	const char *s;
	const int maxcol = 63;
	int a, k, len;

	switch (ac) {
	case 0:
	case 1:
		/* Show all commands */
		printf("Available commands:\n");
		for (k = 0; cmds[k] != NULL; k++) {
			cmd = cmds[k];
			for (s = cmd->cmd; *s != '\0' && !isspace(*s); s++)
				/* nothing */;
			printf("  %.*s%*s %s\n", (int)(s - cmd->cmd), cmd->cmd,
			    (int)(10 - (s - cmd->cmd)), "", cmd->desc);
		}
		return (CMDRTN_OK);
	default:
		/* Show help on a specific command */
		if ((cmd = FindCommand(av[1])) != NULL) {
			printf("usage:    %s\n", cmd->cmd);
			if (cmd->aliases[0] != NULL) {
				printf("Aliases:  ");
				for (a = 0; a < MAX_CMD_ALIAS &&
				    cmd->aliases[a] != NULL; a++) {
					if (a > 0)
						printf(", ");
					printf("%s", cmd->aliases[a]);
				}
				printf("\n");
			}
			printf("Summary:  %s\n", cmd->desc);
			if (cmd->help == NULL)
				break;
			printf("Description:\n");
			for (s = cmd->help; *s != '\0'; s += len) {
				while (isspace(*s))
					s++;
				/* advance to the column limit */
				for (len = 0; s[len] && len < maxcol; len++)
					/* nothing */;
				/* back up to previous interword space */
				while (len > 0 && s[len] && !isblank(s[len]))
					len--;
				printf("  %.*s\n", len, s);
			}
		}
	}
	return (CMDRTN_OK);
}

/*
 * QuitCmd()
 */
static int
QuitCmd(int ac __unused, char **av __unused)
{
	return (CMDRTN_QUIT);
}

/*
 * Dump data in hex and ASCII form
 */
void
DumpAscii(const u_char *buf, int len)
{
	int k, count;

	for (count = 0; count < len; count += DUMP_BYTES_PER_LINE) {
		printf("%04x:  ", count);
		for (k = 0; k < DUMP_BYTES_PER_LINE; k++) {
			if (count + k < len) {
				printf("%02x ", buf[count + k]);
			} else {
				printf("   ");
			}
		}
		printf(" ");
		for (k = 0; k < DUMP_BYTES_PER_LINE; k++) {
			if (count + k < len) {
				printf("%c", isprint(buf[count + k]) ?
				    buf[count + k] : '.');
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
}

/*
 * Usage()
 */
static void
Usage(const char *msg)
{
	if (msg)
		warnx("%s", msg);
	fprintf(stderr,
		"usage: ngctl [-j jail] [-d] [-f filename] [-n nodename] "
		"[command [argument ...]]\n");
	exit(EX_USAGE);
}
