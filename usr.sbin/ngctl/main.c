
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
 * $FreeBSD$
 */

#include "ngctl.h"

#define PROMPT		"+ "
#define MAX_ARGS	512
#define WHITESPACE	" \t\r\n\v\f"

/* Internal functions */
static int	ReadFile(FILE *fp);
static int	DoParseCommand(char *line);
static int	DoCommand(int ac, char **av);
static int	DoInteractive(void);
static const	struct ngcmd *FindCommand(const char *string);
static void	Usage(const char *msg);
static int	ReadCmd(int ac, char **av);
static int	HelpCmd(int ac, char **av);
static int	QuitCmd(int ac, char **av);

/* List of commands */
static const struct ngcmd *const cmds[] = {
	&connect_cmd,
	&debug_cmd,
	&help_cmd,
	&list_cmd,
	&mkpeer_cmd,
	&name_cmd,
	&read_cmd,
	&rmhook_cmd,
	&show_cmd,
	&shutdown_cmd,
	&status_cmd,
	&types_cmd,
	&quit_cmd,
	NULL
};

/* Commands defined in this file */
const struct ngcmd read_cmd = {
	ReadCmd,
	"read <filename>",
	"Read and execute commands from a file",
	NULL
};
const struct ngcmd help_cmd = {
	HelpCmd,
	"help [command]",
	"Show command summary or get more help on a specific command",
	NULL
};
const struct ngcmd quit_cmd = {
	QuitCmd,
	"quit",
	"Exit program",
	NULL
};

/* Our control and data sockets */
int	csock, dsock;

/*
 * main()
 */
int
main(int ac, char *av[])
{
	char	name[NG_NODELEN + 1];
	int	interactive = isatty(0) && isatty(1);
	FILE	*fp = NULL;
	int	ch, rtn = 0;

	/* Set default node name */
	snprintf(name, sizeof(name), "ngctl%d", getpid());

	/* Parse command line */
	while ((ch = getopt(ac, av, "df:n:")) != EOF) {
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
		case 'n':
			snprintf(name, sizeof(name), "%s", optarg);
			break;
		case '?':
		default:
			Usage((char *)NULL);
			break;
		}
	}
	ac -= optind;
	av += optind;

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
	return(rtn);
}

/*
 * Process commands from a file
 */
static int
ReadFile(FILE *fp)
{
	char line[LINE_MAX];
	int num, rtn;

	for (num = 1; fgets(line, sizeof(line), fp) != NULL; num++) {
		if (*line == '#')
			continue;
		if ((rtn = DoParseCommand(line)) != 0) {
			warnx("line %d: error in file", num);
			return(rtn);
		}
	}
	return(CMDRTN_OK);
}

/*
 * Interactive mode
 */
static int
DoInteractive(void)
{
	char buf[LINE_MAX];

	/* Read commands from stdin */
	(*help_cmd.func)(0, NULL);
	do {
		printf("%s", PROMPT);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		fflush(stdout);
	} while (DoParseCommand(buf) != CMDRTN_QUIT);
	return(CMDRTN_QUIT);
}

/*
 * Parse a command line and execute the command
 */
static int
DoParseCommand(char *line)
{
	char *av[MAX_ARGS];
	int ac;

	/* Parse line */
	for (ac = 0, av[0] = strtok(line, WHITESPACE);
	    ac < MAX_ARGS - 1 && av[ac];
	    av[++ac] = strtok(NULL, WHITESPACE));

	/* Do command */
	return(DoCommand(ac, av));
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
		return(CMDRTN_OK);
	if ((cmd = FindCommand(av[0])) == NULL)
		return(CMDRTN_ERROR);
	if ((rtn = (*cmd->func)(ac, av)) == CMDRTN_USAGE)
		warnx("usage: %s", cmd->cmd);
	return(rtn);
}

/*
 * Find a command
 */
static const struct ngcmd *
FindCommand(const char *string)
{
	const struct ngcmd *cmd;
	int k, len, found;

	if (strcmp(string, "?") == 0)
		string = "help";
	for (k = 0, found = -1; cmds[k]; k++) {
		cmd = cmds[k];
		len = strcspn(cmd->cmd, WHITESPACE);
		if (len > strlen(string))
			len = strlen(string);
		if (!strncasecmp(string, cmd->cmd, len)) {
			if (found != -1) {
				warnx("\"%s\": ambiguous command", string);
				return(NULL);
			}
			found = k;
		}
	}
	if (found == -1) {
		warnx("\"%s\": unknown command", string);
		return(NULL);
	}
	return(cmds[found]);
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
		if ((fp = fopen(av[1], "r")) == NULL)
			warn("%s", av[1]);
		return(CMDRTN_ERROR);
	default:
		return(CMDRTN_USAGE);
	}

	/* Process it */
	rtn = ReadFile(fp);
	fclose(fp);
	return(rtn);
}

/*
 * HelpCmd()
 */
static int
HelpCmd(int ac, char **av)
{
	const struct ngcmd *cmd;
	int k;

	switch (ac) {
	case 0:
	case 1:
		/* Show all commands */
		printf("Available commands:\n");
		for (k = 0; cmds[k] != NULL; k++) {
			char *s, buf[100];

			cmd = cmds[k];
			snprintf(buf, sizeof(buf), "%s", cmd->cmd);
			for (s = buf; *s != '\0' && !isspace(*s); s++);
			*s = '\0';
			printf("  %-10s %s\n", buf, cmd->desc);
		}
		return(CMDRTN_OK);
	default:
		/* Show help on a specific command */
		if ((cmd = FindCommand(av[1])) != NULL) {
			printf("Usage:    %s\n", cmd->cmd);
			printf("Summary:  %s\n", cmd->desc);
			if (cmd->help != NULL) {
				const char *s;
				char buf[65];
				int tot, len, done;

				printf("Description:\n");
				for (s = cmd->help; *s != '\0'; s += len) {
					while (isspace(*s))
						s++;
					tot = snprintf(buf,
					    sizeof(buf), "%s", s);
					len = strlen(buf);
					done = len == tot;
					if (!done) {
						while (len > 0
						    && !isspace(buf[len-1]))
							buf[--len] = '\0';
					}
					printf("  %s\n", buf);
				}
			}
		}
	}
	return(CMDRTN_OK);
}

/*
 * QuitCmd()
 */
static int
QuitCmd(int ac, char **av)
{
	return(CMDRTN_QUIT);
}

/*
 * Usage()
 */
static void
Usage(const char *msg)
{
	if (msg)
		warnx("%s", msg);
	errx(EX_USAGE, "usage: ngctl [-d] [-f file] [-n name] [command ...]");
}

