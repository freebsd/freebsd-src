/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Command line interface for IP firewall facility
 *
 * $FreeBSD$
 */

#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "ipfw2.h"

static void
help(void)
{
	fprintf(stderr,
"ipfw syntax summary (but please do read the ipfw(8) manpage):\n\n"
"\tipfw [-abcdefhnNqStTv] <command>\n\n"
"where <command> is one of the following:\n\n"
"add [num] [set N] [prob x] RULE-BODY\n"
"{pipe|queue} N config PIPE-BODY\n"
"[pipe|queue] {zero|delete|show} [N{,N}]\n"
"nat N config {ip IPADDR|if IFNAME|log|deny_in|same_ports|unreg_only|reset|\n"
"		reverse|proxy_only|redirect_addr linkspec|\n"
"		redirect_port linkspec|redirect_proto linkspec}\n"
"set [disable N... enable N...] | move [rule] X to Y | swap X Y | show\n"
"set N {show|list|zero|resetlog|delete} [N{,N}] | flush\n"
"table N {add ip[/bits] [value] | delete ip[/bits] | flush | list}\n"
"table all {flush | list}\n"
"\n"
"RULE-BODY:	check-state [PARAMS] | ACTION [PARAMS] ADDR [OPTION_LIST]\n"
"ACTION:	check-state | allow | count | deny | unreach{,6} CODE |\n"
"               skipto N | {divert|tee} PORT | forward ADDR |\n"
"               pipe N | queue N | nat N | setfib FIB\n"
"PARAMS: 	[log [logamount LOGLIMIT]] [altq QUEUE_NAME]\n"
"ADDR:		[ MAC dst src ether_type ] \n"
"		[ ip from IPADDR [ PORT ] to IPADDR [ PORTLIST ] ]\n"
"		[ ipv6|ip6 from IP6ADDR [ PORT ] to IP6ADDR [ PORTLIST ] ]\n"
"IPADDR:	[not] { any | me | ip/bits{x,y,z} | table(t[,v]) | IPLIST }\n"
"IP6ADDR:	[not] { any | me | me6 | ip6/bits | IP6LIST }\n"
"IP6LIST:	{ ip6 | ip6/bits }[,IP6LIST]\n"
"IPLIST:	{ ip | ip/bits | ip:mask }[,IPLIST]\n"
"OPTION_LIST:	OPTION [OPTION_LIST]\n"
"OPTION:	bridged | diverted | diverted-loopback | diverted-output |\n"
"	{dst-ip|src-ip} IPADDR | {dst-ip6|src-ip6|dst-ipv6|src-ipv6} IP6ADDR |\n"
"	{dst-port|src-port} LIST |\n"
"	estab | frag | {gid|uid} N | icmptypes LIST | in | out | ipid LIST |\n"
"	iplen LIST | ipoptions SPEC | ipprecedence | ipsec | iptos SPEC |\n"
"	ipttl LIST | ipversion VER | keep-state | layer2 | limit ... |\n"
"	icmp6types LIST | ext6hdr LIST | flow-id N[,N] | fib FIB |\n"
"	mac ... | mac-type LIST | proto LIST | {recv|xmit|via} {IF|IPADDR} |\n"
"	setup | {tcpack|tcpseq|tcpwin} NN | tcpflags SPEC | tcpoptions SPEC |\n"
"	tcpdatalen LIST | verrevpath | versrcreach | antispoof\n"
);

	exit(0);
}

/*
 * Free a the (locally allocated) copy of command line arguments.
 */
static void
free_args(int ac, char **av)
{
	int i;

	for (i=0; i < ac; i++)
		free(av[i]);
	free(av);
}

/*
 * Called with the arguments, including program name because getopt
 * wants it to be present.
 * Returns 0 if successful, 1 if empty command, errx() in case of errors.
 */
static int
ipfw_main(int oldac, char **oldav)
{
	int ch, ac, save_ac;
	const char *errstr;
	char **av, **save_av;
	int do_acct = 0;		/* Show packet/byte count */
	int try_next = 0;

#define WHITESP		" \t\f\v\n\r"
	if (oldac < 2)
		return 1;	/* need at least one argument */

	if (oldac == 2) {
		/*
		 * If we are called with a single string, try to split it into
		 * arguments for subsequent parsing.
		 * But first, remove spaces after a ',', by copying the string
		 * in-place.
		 */
		char *arg = oldav[1];	/* The string is the first arg. */
		int l = strlen(arg);
		int copy = 0;		/* 1 if we need to copy, 0 otherwise */
		int i, j;

		for (i = j = 0; i < l; i++) {
			if (arg[i] == '#')	/* comment marker */
				break;
			if (copy) {
				arg[j++] = arg[i];
				copy = !index("," WHITESP, arg[i]);
			} else {
				copy = !index(WHITESP, arg[i]);
				if (copy)
					arg[j++] = arg[i];
			}
		}
		if (!copy && j > 0)	/* last char was a 'blank', remove it */
			j--;
		l = j;			/* the new argument length */
		arg[j++] = '\0';
		if (l == 0)		/* empty string! */
			return 1;

		/*
		 * First, count number of arguments. Because of the previous
		 * processing, this is just the number of blanks plus 1.
		 */
		for (i = 0, ac = 1; i < l; i++)
			if (index(WHITESP, arg[i]) != NULL)
				ac++;

		/*
		 * Allocate the argument list, including one entry for
		 * the program name because getopt expects it.
		 */
		av = safe_calloc(ac + 1, sizeof(char *));

		/*
		 * Second, copy arguments from arg[] to av[]. For each one,
		 * j is the initial character, i is the one past the end.
		 */
		for (ac = 1, i = j = 0; i < l; i++)
			if (index(WHITESP, arg[i]) != NULL || i == l-1) {
				if (i == l-1)
					i++;
				av[ac] = safe_calloc(i-j+1, 1);
				bcopy(arg+j, av[ac], i-j);
				ac++;
				j = i + 1;
			}
	} else {
		/*
		 * If an argument ends with ',' join with the next one.
		 */
		int first, i, l;

		av = safe_calloc(oldac, sizeof(char *));
		for (first = i = ac = 1, l = 0; i < oldac; i++) {
			char *arg = oldav[i];
			int k = strlen(arg);

			l += k;
			if (arg[k-1] != ',' || i == oldac-1) {
				/* Time to copy. */
				av[ac] = safe_calloc(l+1, 1);
				for (l=0; first <= i; first++) {
					strcat(av[ac]+l, oldav[first]);
					l += strlen(oldav[first]);
				}
				ac++;
				l = 0;
				first = i+1;
			}
		}
	}

	av[0] = strdup(oldav[0]);	/* copy progname from the caller */
	/* Set the force flag for non-interactive processes */
	if (!co.do_force)
		co.do_force = !isatty(STDIN_FILENO);

	/* Save arguments for final freeing of memory. */
	save_ac = ac;
	save_av = av;

	optind = optreset = 1;	/* restart getopt() */
	while ((ch = getopt(ac, av, "abcdefhinNqs:STtv")) != -1)
		switch (ch) {
		case 'a':
			do_acct = 1;
			break;

		case 'b':
			co.comment_only = 1;
			co.do_compact = 1;
			break;

		case 'c':
			co.do_compact = 1;
			break;

		case 'd':
			co.do_dynamic = 1;
			break;

		case 'e':
			co.do_expired = 1;
			break;

		case 'f':
			co.do_force = 1;
			break;

		case 'h': /* help */
			free_args(save_ac, save_av);
			help();
			break;	/* NOTREACHED */

		case 'i':
			co.do_value_as_ip = 1;
			break;

		case 'n':
			co.test_only = 1;
			break;

		case 'N':
			co.do_resolv = 1;
			break;

		case 'q':
			co.do_quiet = 1;
			break;

		case 's': /* sort */
			co.do_sort = atoi(optarg);
			break;

		case 'S':
			co.show_sets = 1;
			break;

		case 't':
			co.do_time = 1;
			break;

		case 'T':
			co.do_time = 2;	/* numeric timestamp */
			break;

		case 'v': /* verbose */
			co.verbose = 1;
			break;

		default:
			free_args(save_ac, save_av);
			return 1;
		}

	ac -= optind;
	av += optind;
	NEED1("bad arguments, for usage summary ``ipfw''");

	/*
	 * An undocumented behaviour of ipfw1 was to allow rule numbers first,
	 * e.g. "100 add allow ..." instead of "add 100 allow ...".
	 * In case, swap first and second argument to get the normal form.
	 */
	if (ac > 1 && isdigit(*av[0])) {
		char *p = av[0];

		av[0] = av[1];
		av[1] = p;
	}

	/*
	 * Optional: pipe, queue or nat.
	 */
	co.do_nat = 0;
	co.do_pipe = 0;
	if (!strncmp(*av, "nat", strlen(*av)))
 	        co.do_nat = 1;
 	else if (!strncmp(*av, "pipe", strlen(*av)))
		co.do_pipe = 1;
	else if (_substrcmp(*av, "queue") == 0)
		co.do_pipe = 2;
	else if (!strncmp(*av, "set", strlen(*av))) {
		if (ac > 1 && isdigit(av[1][0])) {
			co.use_set = strtonum(av[1], 0, resvd_set_number,
					&errstr);
			if (errstr)
				errx(EX_DATAERR,
				    "invalid set number %s\n", av[1]);
			ac -= 2; av += 2; co.use_set++;
		}
	}

	if (co.do_pipe || co.do_nat) {
		ac--;
		av++;
	}
	NEED1("missing command");

	/*
	 * For pipes, queues and nats we normally say 'nat|pipe NN config'
	 * but the code is easier to parse as 'nat|pipe config NN'
	 * so we swap the two arguments.
	 */
	if ((co.do_pipe || co.do_nat) && ac > 1 && isdigit(*av[0])) {
		char *p = av[0];

		av[0] = av[1];
		av[1] = p;
	}

	if (co.use_set == 0) {
		if (_substrcmp(*av, "add") == 0)
			ipfw_add(ac, av);
		else if (co.do_nat && _substrcmp(*av, "show") == 0)
 			ipfw_show_nat(ac, av);
		else if (co.do_pipe && _substrcmp(*av, "config") == 0)
			ipfw_config_pipe(ac, av);
		else if (co.do_nat && _substrcmp(*av, "config") == 0)
 			ipfw_config_nat(ac, av);
		else if (_substrcmp(*av, "set") == 0)
			ipfw_sets_handler(ac, av);
		else if (_substrcmp(*av, "table") == 0)
			ipfw_table_handler(ac, av);
		else if (_substrcmp(*av, "enable") == 0)
			ipfw_sysctl_handler(ac, av, 1);
		else if (_substrcmp(*av, "disable") == 0)
			ipfw_sysctl_handler(ac, av, 0);
		else
			try_next = 1;
	}

	if (co.use_set || try_next) {
		if (_substrcmp(*av, "delete") == 0)
			ipfw_delete(ac, av);
		else if (_substrcmp(*av, "flush") == 0)
			ipfw_flush(co.do_force);
		else if (_substrcmp(*av, "zero") == 0)
			ipfw_zero(ac, av, 0 /* IP_FW_ZERO */);
		else if (_substrcmp(*av, "resetlog") == 0)
			ipfw_zero(ac, av, 1 /* IP_FW_RESETLOG */);
		else if (_substrcmp(*av, "print") == 0 ||
		         _substrcmp(*av, "list") == 0)
			ipfw_list(ac, av, do_acct);
		else if (_substrcmp(*av, "show") == 0)
			ipfw_list(ac, av, 1 /* show counters */);
		else
			errx(EX_USAGE, "bad command `%s'", *av);
	}

	/* Free memory allocated in the argument parsing. */
	free_args(save_ac, save_av);
	return 0;
}


static void
ipfw_readfile(int ac, char *av[])
{
#define MAX_ARGS	32
	char buf[4096];
	char *progname = av[0];		/* original program name */
	const char *cmd = NULL;		/* preprocessor name, if any */
	const char *filename = av[ac-1]; /* file to read */
	int	c, lineno=0;
	FILE	*f = NULL;
	pid_t	preproc = 0;

	while ((c = getopt(ac, av, "cfNnp:qS")) != -1) {
		switch(c) {
		case 'c':
			co.do_compact = 1;
			break;

		case 'f':
			co.do_force = 1;
			break;

		case 'N':
			co.do_resolv = 1;
			break;

		case 'n':
			co.test_only = 1;
			break;

		case 'p':
			/*
			 * ipfw -p cmd [args] filename
			 *
			 * We are done with getopt(). All arguments
			 * except the filename go to the preprocessor,
			 * so we need to do the following:
			 * - check that a filename is actually present;
			 * - advance av by optind-1 to skip arguments
			 *   already processed;
			 * - decrease ac by optind, to remove the args
			 *   already processed and the final filename;
			 * - set the last entry in av[] to NULL so
			 *   popen() can detect the end of the array;
			 * - set optind=ac to let getopt() terminate.
			 */
			if (optind == ac)
				errx(EX_USAGE, "no filename argument");
			cmd = optarg;
			av[ac-1] = NULL;
			av += optind - 1;
			ac -= optind;
			optind = ac;
			break;

		case 'q':
			co.do_quiet = 1;
			break;

		case 'S':
			co.show_sets = 1;
			break;

		default:
			errx(EX_USAGE, "bad arguments, for usage"
			     " summary ``ipfw''");
		}

	}

	if (cmd == NULL && ac != optind + 1)
		errx(EX_USAGE, "extraneous filename arguments %s", av[ac-1]);

	if ((f = fopen(filename, "r")) == NULL)
		err(EX_UNAVAILABLE, "fopen: %s", filename);

	if (cmd != NULL) {			/* pipe through preprocessor */
		int pipedes[2];

		if (pipe(pipedes) == -1)
			err(EX_OSERR, "cannot create pipe");

		preproc = fork();
		if (preproc == -1)
			err(EX_OSERR, "cannot fork");

		if (preproc == 0) {
			/*
			 * Child, will run the preprocessor with the
			 * file on stdin and the pipe on stdout.
			 */
			if (dup2(fileno(f), 0) == -1
			    || dup2(pipedes[1], 1) == -1)
				err(EX_OSERR, "dup2()");
			fclose(f);
			close(pipedes[1]);
			close(pipedes[0]);
			execvp(cmd, av);
			err(EX_OSERR, "execvp(%s) failed", cmd);
		} else { /* parent, will reopen f as the pipe */
			fclose(f);
			close(pipedes[1]);
			if ((f = fdopen(pipedes[0], "r")) == NULL) {
				int savederrno = errno;

				(void)kill(preproc, SIGTERM);
				errno = savederrno;
				err(EX_OSERR, "fdopen()");
			}
		}
	}

	while (fgets(buf, sizeof(buf), f)) {		/* read commands */
		char linename[20];
		char *args[2];

		lineno++;
		snprintf(linename, sizeof(linename), "Line %d", lineno);
		setprogname(linename); /* XXX */
		args[0] = progname;
		args[1] = buf;
		ipfw_main(2, args);
	}
	fclose(f);
	if (cmd != NULL) {
		int status;

		if (waitpid(preproc, &status, 0) == -1)
			errx(EX_OSERR, "waitpid()");
		if (WIFEXITED(status) && WEXITSTATUS(status) != EX_OK)
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with status %d",
			    WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with signal %d",
			    WTERMSIG(status));
	}
}

int
main(int ac, char *av[])
{
	/*
	 * If the last argument is an absolute pathname, interpret it
	 * as a file to be preprocessed.
	 */

	if (ac > 1 && av[ac - 1][0] == '/' && access(av[ac - 1], R_OK) == 0)
		ipfw_readfile(ac, av);
	else {
		if (ipfw_main(ac, av)) {
			errx(EX_USAGE,
			    "usage: ipfw [options]\n"
			    "do \"ipfw -h\" or \"man ipfw\" for details");
		}
	}
	return EX_OK;
}
