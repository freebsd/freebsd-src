#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ndc.c,v 1.22 2002/06/24 07:28:55 marka Exp $";
#endif /* not lint */

/*
 * Portions Copyright (c) 1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <isc/eventlib.h>
#include <isc/ctl.h>

#include "port_after.h"
#include "pathnames.h"
#ifndef PATH_SEP
#define PATH_SEP '/'
#endif

typedef union {
	struct sockaddr_in in;
#ifndef NO_SOCKADDR_UN
	struct sockaddr_un un;
#endif
} sockaddr_t;

typedef void (*closure)(void *, const char *, int);

static const char *	program = "amnesia";
static enum { e_channel, e_signals } mode = e_channel;
static const char *	channel = _PATH_NDCSOCK;
static const char	helpfmt[] = "\t%-16s\t%s\n";
static const char *	pidfile = _PATH_PIDFILE;
static sockaddr_t	client, server;
static int		quiet = 0, tracing = 0, silent = 0, client_set = 0;
static int		debug = 0, errors = 0, doneflag, exitflag;
static int		logger_show = 1;
static evContext	ev;
static char		cmd[1000];
static const char *	named_path = _PATH_NAMED;

static int		slashcmd(void);
static void		slashhelp(void);
static int		builtincmd(void);
static void		command(void);
static int		running(int, pid_t *);
static void		command_channel(void);
static void		channel_loop(const char *, int, closure, void *);
static void		getpid_closure(void *, const char *, int);
static void		banner(struct ctl_cctx *, void *, const char *, u_int);
static void		done(struct ctl_cctx *, void *, const char *, u_int);
static void		logger(enum ctl_severity, const char *fmt, ...) ISC_FORMAT_PRINTF(2, 3);
static void		command_signals(void);
static void		stop_named(pid_t);
static void		start_named(const char *, int);
static int		fgetpid(const char *, pid_t *);
static int		get_sockaddr(const char *, sockaddr_t *);
static size_t		impute_addrlen(const struct sockaddr *);
static void		vtrace(const char *, va_list);
static void		trace(const char *, ...) ISC_FORMAT_PRINTF(1, 2);
static void		result(const char *, ...) ISC_FORMAT_PRINTF(1, 2);
static void		fatal(const char *, ...) ISC_FORMAT_PRINTF(1, 2);
static void		verror(const char *, va_list) ISC_FORMAT_PRINTF(1, 0);
static void		error(const char *, ...) ISC_FORMAT_PRINTF(1, 2);

static void
usage(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: usage error: ", program);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
	fatal("usage: %s \
[-l localsock] [-c channel] [-p pidfile] [-n namedpath] \
[-dqst] [command [args]]\n\
",
	      program);
}

/* Public. */

int
main(int argc, char *argv[]) {
	char *p;
	int ch;

	if ((program = strrchr(argv[0], PATH_SEP)) != NULL)
		program++;
	else
		program = argv[0];
	while ((ch = getopt(argc, argv, "c:p:l:n:dqst")) != -1) {
		switch (ch) {
		case 'c':
			channel = optarg;
			mode = e_channel;
			break;
		case 'p':
			pidfile = optarg;
			mode = e_signals;
			break;
		case 'l':
			if (!get_sockaddr(optarg, &client))
				usage("bad local socket (%s)", optarg);
			client_set++;
			break;
		case 'n':
			named_path = optarg;
			break;
		case 'd':
			tracing++;
			debug++;
			break;
		case 'q':
			quiet++;
			break;
		case 's':
			silent++;
			break;
		case 't':
			tracing++;
			break;
		default:
			usage("unrecognized command option (%c)", ch);
			/* NOTREACHED */
		}
	}
	if (mode != e_channel && client_set)
		usage("the -l flag is only valid for control channels");
	if (mode == e_channel) {
		if (!get_sockaddr(channel, &server))
			usage("bad channel name (%s)", channel);
		if (evCreate(&ev) < 0)
			fatal("evCreate - %s", strerror(errno));
	}
	*(p = cmd) = '\0';
	for (argc -= optind, argv += optind;
	     argc > 0;
	     argc--, argv++) {
		size_t t = strlen(*argv);

		if ((p - cmd) + t + 2 > sizeof cmd)
			usage("command too long");
		strcpy(p, *argv);
		p += t;
		if (argv[1] != NULL)
			*p++ = ' ';
		*p = '\0';
	}
	if (cmd[0] != '\0') {
		command();
	} else {
		if (!quiet)
			result("Type   help  -or-   /h   if you need help.");
		for (exitflag = 0; !exitflag; (void)NULL) {
			if (!quiet) {
				printf("%s> ", program);
				fflush(stdout);
			}
			if (!fgets(cmd, sizeof cmd, stdin)) {
				if (!quiet)
					result("EOF");
				exitflag++;
				continue;
			}
			if (cmd[strlen(cmd) - 1] == '\n')
				cmd[strlen(cmd) - 1] = '\0';
			if (cmd[0] == '\0')
				continue;
			if (slashcmd())
				continue;
			command();
		}
	}
	if (mode == e_channel)
		evDestroy(ev);
	exit(errors != 0);
}

/* Private. */

static int
slashcmd(void) {
	if (strncasecmp(cmd, "/help", strlen(cmd)) == 0)
		slashhelp();
	else if (strncasecmp(cmd, "/exit", strlen(cmd)) == 0)
		exitflag++;
	else if (strncasecmp(cmd, "/trace", strlen(cmd)) == 0)
		result("tracing now %s",
		       (tracing = !tracing) ? "on" : "off");
	else if (strncasecmp(cmd, "/debug", strlen(cmd)) == 0)
		result("debugging now %s",
		       (debug = !debug) ? "on" : "off");
	else if (strncasecmp(cmd, "/quiet", strlen(cmd)) == 0)
		result("%s is now %s", program,
		       (quiet = !quiet) ? "quiet" : "noisy");
	else if (strncasecmp(cmd, "/silent", strlen(cmd)) == 0)
		result("%s is now %s", program,
		       (silent = !silent)
			? "silent" : "gregarious");
	else
		return (0);
	return (1);
}

static void
slashhelp(void) {
	printf(helpfmt, "/h(elp)", "this text");
	printf(helpfmt, "/e(xit)", "leave this program");
	printf(helpfmt, "/t(race)",
	       "toggle tracing (protocol and system events)");
	printf(helpfmt, "/d(ebug)",
	       "toggle debugging (internal program events)");
	printf(helpfmt, "/q(uiet)",
	       "toggle quietude (prompts and results)");
	printf(helpfmt, "/s(ilent)",
	       "toggle silence (suppresses nonfatal errors)");
}

struct argv {
	int argc;
	char **argv;
	int error;
};

static char hexdigits[] = "0123456789abcdef";

static void
getargs_closure(void *arg, const char *msg, int flags) {
	struct argv *argv = arg;
	int len;
	int i;
	const char *cp, *cp2;
	char *tp, c;

	UNUSED(flags);

	if (argv->error)
		return;

	if (argv->argc == -1) {
		i = atoi(msg + 4);
		if (i < 1) {
			argv->error = 1;
			return;
		}
		argv->argc = i;
		argv->argv = calloc((i+1), sizeof(char*));
		return;
	}
	len = 0;
	cp = msg + 4;
	while (*cp != NULL) {
		c = *cp;
		if (c == '%') {
			cp2 = strchr(hexdigits, cp[1]);
			if (cp2 == NULL) {
				argv->error = 1;
				return;
			}
			c = (cp2-hexdigits) << 4;
			cp2 = strchr(hexdigits, cp[2]);
			if (cp2 == NULL) {
				argv->error = 1;
				return;
			}
			c += (cp2-hexdigits);
			cp += 2;
		}
		if (!isalnum((unsigned)c)) {
			switch (c) {
			case '+': case '-': case '=': case '/': case '.':
				break;
			default:
				len++;
			}
		}
		len++;
		cp++;
	}
	i = 0;
	while (argv->argv[i] != NULL)
		i++;
	if (i >= argv->argc) {
		argv->error = 1;
		return;
	}
	argv->argv[i] = malloc(len + 1);
	if (argv->argv[i] == NULL) {
		argv->error = 1;
		return;
	}
	cp = msg + 4;
	tp = argv->argv[i];
	while (*cp != NULL) {
		c = *cp;
		if (c == '%') {
			cp2 = strchr(hexdigits, cp[1]);
			if (cp2 == NULL) {
				argv->error = 1;
				return;
			}
			c = (cp2-hexdigits) << 4;
			cp2 = strchr(hexdigits, cp[2]);
			if (cp2 == NULL) {
				argv->error = 1;
				return;
			}
			c += (cp2-hexdigits);
			cp += 2;
		}
		if (!isalnum((unsigned)c)) {
			switch (c) {
			case '+': case '-': case '=': case '/': case '.':
				break;
			default:
				*tp = '\\';
			}
		}
		*tp++ = c;
		cp++;
	}
}

static int
get_args(char **restp) {
	struct argv argv;
	int len, i;
	char *rest, *p;
	int result = 1;

	argv.argc = -1;
	argv.argv = NULL;
	argv.error = 0;

	channel_loop("args", 1, getargs_closure, &argv);
	if (argv.error) {
		result = 0;
		goto err;
	}
	len = 0;
	for (i = 1 ; i < argv.argc && argv.argv[i] != NULL; i++)
		len += strlen(argv.argv[i]) + 1;
	rest = malloc(len);
	if (rest == NULL) {
		result = 0;
		goto err;
	}
	p = rest;
	for (i = 1 ; i < argv.argc && argv.argv[i] != NULL; i++) {
		strcpy(p, argv.argv[i]);
		p += strlen(argv.argv[i]);
		*p++ = ' ';
	}
	if (p != rest)
		p[-1] = '\0';
	*restp = rest;

 err:
	if (argv.argv) {
		for (i = 0 ; i < argv.argc && argv.argv[i] != NULL; i++)
			free(argv.argv[i]);
		free(argv.argv);
	}
	return (result);
}

static void
exec_closure(void *arg, const char *msg, int flags) {
	int *result = arg;
	UNUSED(flags);
	if (atoi(msg) == 250)
		*result = 1;
}

static int
try_exec(int local_quiet) {
	int good = 0;
	pid_t pid;

	channel_loop("exec", 1, exec_closure, &good);

	if (good) {
		sleep(3);
		if (!running(0, &pid))
			error("name server has not restarted (yet?)");
		else if (!local_quiet)
			result("new pid is %ld", (long)pid);
	}
	return (good);
}

static int
builtincmd(void) {
	static const char spaces[] = " \t";
	char *rest, *syscmd;
	pid_t pid;
	int save_quiet = quiet;
	int len;
	int freerest = 0;

	quiet = 1;

	len = strcspn(cmd, spaces);
	rest = cmd + len;
	if (*rest != '\0')
		rest += strspn(rest, spaces);
	if (*rest == '\0' && !strncasecmp(cmd, "restart", len)) {
		if (try_exec(save_quiet))
			return (1);
		freerest = get_args(&rest);
	}
	syscmd = malloc(strlen(named_path) + sizeof " " + strlen(rest));
	if (syscmd == NULL)
		fatal("malloc() failed - %s", strerror(errno));
	strcpy(syscmd, named_path);
	if (*rest != '\0') {
		strcat(syscmd, " ");
		strcat(syscmd, rest);
	}
	if (freerest)
		free(rest);
	if (strncasecmp(cmd, "start", len) == 0) {
		if (running(debug, &pid))
			error("name server already running? (pid %ld)",
			      (long)pid);
		else
			start_named(syscmd, save_quiet);
		quiet = save_quiet;
		free(syscmd);
		return (1);
	} else if (strncasecmp(cmd, "restart", len) == 0) {
		if (!running(debug, &pid))
			error("name server was not running (warning only)");
		else
			stop_named(pid);
		start_named(syscmd, save_quiet);
		quiet = save_quiet;
		free(syscmd);
		return (1);
	}
	quiet = save_quiet;
	free(syscmd);
	return (0);
}

static void
builtinhelp(void) {
	const char *fmt;

	switch (mode) {
	case e_channel:
		fmt = "(builtin) %s - %s\n";
		break;
	case e_signals:
		fmt = helpfmt;
		break;
	default:
		abort();
	}
	printf(fmt, "start", "start the server");
	printf(fmt, "restart", "stop server if any, start a new one");
}

static void
command(void) {
	if (builtincmd())
		return;
	switch (mode) {
	case e_channel:
		command_channel();
		break;
	case e_signals:
		command_signals();
		break;
	default:
		abort();
	}
}

static int
running(int show, pid_t *pidp) {
	pid_t pid;

	switch (mode) {
	case e_channel:
		pid = 0;
		channel_loop("getpid", show, getpid_closure, &pid);
		if (pid != 0) {
			if (tracing)
				result("pid %ld is running", (long)pid);
			*pidp = pid;
			return (1);
		}
		break;
	case e_signals:
		if (fgetpid(pidfile, pidp)) {
			if (tracing)
				result("pid %ld is running", (long)pid);
			return (1);
		}
		break;
	default:
		abort();
	}
	if (show)
		error("pid not valid or server not running");
	return (0);
}

static void
getpid_closure(void *uap, const char *text, int flags) {
	pid_t *pidp = uap;
	const char *cp;

	flags = flags;
	if ((cp = strchr(text, '<')) != NULL) {
		long l = 0;
		char ch;

		while ((ch = *++cp) != '\0' && ch != '>' && isdigit(ch))
			l *= 10, l += (ch - '0');
		if (ch == '>') {
			*pidp = (pid_t)l;
				return;
		}
	}
	error("response does not contain pid (%s)", text);
}

static void
command_channel(void) {
	int helping = (strcasecmp(cmd, "help") == 0);
	int save_quiet = quiet;

	if (helping) {
		quiet = 0;
		builtinhelp();
	}
	channel_loop(cmd, !quiet, NULL, NULL);
	quiet = save_quiet;
}

struct args {
	const char *cmd;
	closure cl;
	void *ua;
};

static void
channel_loop(const char *cmdtext, int show, closure cl, void *ua) {
	struct ctl_cctx *ctl;
	struct sockaddr *client_addr;
	struct args a;
	evEvent e;
	int save_logger_show = logger_show;

	if (!client_set)
		client_addr = NULL;
	else
		client_addr = (struct sockaddr *)&client;
	a.cmd = cmdtext;
	a.cl = cl;
	a.ua = ua;
	logger_show = show;
	trace("command '%s'", cmdtext);
	ctl = ctl_client(ev, client_addr, impute_addrlen(client_addr),
			 (struct sockaddr *)&server,
			 impute_addrlen((struct sockaddr *)&server),
			 banner, &a, 15, logger);
	if (ctl == NULL) {
		if (show)
			error("cannot connect to command channel (%s)",
				channel);
	} else {
		doneflag = 0;
		while (evGetNext(ev, &e, EV_WAIT) == 0)
			if (evDispatch(ev, e) < 0 || doneflag)
				break;
		ctl_endclient(ctl);
	}
	logger_show = save_logger_show;
}

static void
banner(struct ctl_cctx *ctl, void *uap, const char *msg, u_int flags) {
	struct args *a = uap;

	if (msg == NULL) {
		trace("EOF");
		doneflag = 1;
		return;
	}
	trace("%s", msg);
	if ((flags & CTL_MORE) != 0)
		return;
	if (ctl_command(ctl, a->cmd, strlen(a->cmd), done, a) < 0) {
		error("ctl_command failed - %s", strerror(errno));
		doneflag = 1;
	}
}

static void
done(struct ctl_cctx *ctl, void *uap, const char *msg, u_int flags) {
	struct args *a = uap;

	UNUSED(ctl);

	if (msg == NULL) {
		trace("EOF");
		doneflag = 1;
		return;
	}
	if (!tracing && !quiet && strlen(msg) > 4)
		result("%s", msg + 4);
	trace("%s", msg);
	if (a->cl)
		(a->cl)(a->ua, msg, flags);
	if ((flags & CTL_MORE) == 0)
		doneflag = 1;
}

static void
logger(enum ctl_severity ctlsev, const char *format, ...) {
	va_list args;

	va_start(args, format);
	switch (ctlsev) {
	case ctl_debug:
		/* FALLTHROUGH */
	case ctl_warning:
		if (debug)
			vtrace(format, args);
		break;
	case ctl_error:
		if (logger_show)
			verror(format, args);
		break;
	default:
		va_end(args);
		abort();
	}
	va_end(args);
}

static struct cmdsig {
	const char *	cmd;
	int		sig;
	const char *	help;
} cmdsigs[] = {
	{ "dumpdb", SIGINT, "dump cache database to a file" },
	{ "reload", SIGHUP, "reload configuration file" },
	{ "stats", SIGILL, "dump statistics to a file" },
	{ "trace", SIGUSR1, "increment trace level" },
	{ "notrace", SIGUSR2, "turn off tracing" },
#ifdef SIGWINCH
	{ "querylog", SIGWINCH, "toggle query logging" },
	{ "qrylog", SIGWINCH, "alias for querylog" },
#endif
	{ NULL, 0, NULL }
};

static void
command_signals(void) {
	struct cmdsig *cmdsig;
	pid_t pid;

	if (strcasecmp(cmd, "help") == 0) {
		printf(helpfmt, "help", "this output");
		printf(helpfmt, "status", "check for running server");
		printf(helpfmt, "stop", "stop the server");
		builtinhelp();
		for (cmdsig = cmdsigs; cmdsig->cmd != NULL; cmdsig++)
			printf(helpfmt, cmdsig->cmd, cmdsig->help);
	} else if (strcasecmp(cmd, "status") == 0) {
		if (!fgetpid(pidfile, &pid))
			error("pid not valid or server not running");
		else
			result("pid %ld is running", (long)pid);
	} else if (strcasecmp(cmd, "stop") == 0) {
		if (!fgetpid(pidfile, &pid))
			error("name server not running");
		else
			stop_named(pid);
	} else {
		for (cmdsig = cmdsigs; cmdsig->cmd != NULL; cmdsig++)
			if (strcasecmp(cmd, cmdsig->cmd) == 0)
				break;
		if (cmdsig->cmd == NULL)
			error("unrecognized command (%s)", cmd);
		else if (!fgetpid(pidfile, &pid))
			error("can't get pid (%s)", pidfile);
		else if (kill(pid, cmdsig->sig) < 0)
			error("kill() failed - %s", strerror(errno));
		else
			trace("pid %ld sig %d OK", (long)pid, cmdsig->sig);
	}
}

static void
stop_named(pid_t pid) {
	int n;

	trace("stopping named (pid %ld)", (long)pid);
	switch (mode) {
	case e_signals:
		if (kill(pid, SIGTERM) < 0) {
			error("kill(%ld, SIGTERM) failed - %s",
			      (long)pid, strerror(errno));
			return;
		}
		trace("SIGTERM ok, waiting for death");
		break;
	case e_channel:
		channel_loop("stop", tracing, NULL, NULL);
		break;
	default:
		abort();
	}
	for (n = 0; n < 10; n++) {
		if (kill(pid, 0) != 0) {
			trace("named (pid %ld) is dead", (long)pid);
			return;
		}
		sleep(1);
	}
	error("named (pid %ld) didn't die", (long)pid);
}

static void
start_named(const char *syscmd, int local_quiet) {
	pid_t pid;

	if (system(syscmd) != 0)
		error("could not start new name server (%s)", syscmd);
	else {
		sleep(3);
		if (!running(0, &pid))
			error("name server has not started (yet?)");
		else if (!local_quiet)
			result("new pid is %ld", (long)pid);
	}
}

static int
fgetpid(const char *f, pid_t *pid) {
	FILE *fp;
	int try;
	long t;

	for (try = 0; try < 5; try++) {
		trace("pidfile is \"%s\" (try #%d)", f, try + 1);
		if ((fp = fopen(f, "r")) == NULL)
			trace("pid file (%s) unavailable - %s",
			      f, strerror(errno));
		else if (fscanf(fp, "%ld\n", &t) != 1)
			trace("pid file (%s) format is bad", f);
		else if (*pid = (pid_t)t, fclose(fp), kill(*pid, 0) < 0)
			trace("pid file (%s) contains unusable pid (%d) - %s",
			      f, *pid, strerror(errno));
		else {
			trace("pid is %ld", (long)*pid);
			return (1);
		}
		sleep(1);
	}
	trace("pid not found");
	return (0);
}

static int
get_sockaddr(const char *name, sockaddr_t *addr) {
	char *slash;

#ifndef NO_SOCKADDR_UN
	if (name[0] == '/') {
		memset(&addr->un, '\0', sizeof addr->un);
		addr->un.sun_family = AF_UNIX;
		strncpy(addr->un.sun_path, name, sizeof addr->un.sun_path - 1);
		addr->un.sun_path[sizeof addr->un.sun_path - 1] = '\0';
	} else
#endif
	if ((slash = strrchr(name, '/')) != NULL) {
		char *ibuf = malloc(slash - name + 1);
		if (!ibuf)
			usage("no memory for IP address (%s)", name);
		memcpy(ibuf, name, slash - name);
		ibuf[slash - name] = '\0';
		memset(&addr->in, '\0', sizeof addr->in);
		if (!inet_pton(AF_INET, ibuf, &addr->in.sin_addr))
			usage("bad ip address (%s)", name);
		if ((addr->in.sin_port = htons(atoi(slash+1))) == 0)
			usage("bad ip port (%s)", slash+1);
		addr->in.sin_family = AF_INET;
		free (ibuf);
	  } else {
		  return (0);
	}
	return (1);
}

static size_t
impute_addrlen(const struct sockaddr *sa) {
	if (sa == NULL)
		return (0);
	switch (sa->sa_family) {
	case AF_INET:
		return (sizeof(struct sockaddr_in));
#ifndef NO_SOCKADDR_UN
	case AF_UNIX:
		return (sizeof(struct sockaddr_un));
#endif
	default:
		abort();
	}
	/*NOTREACHED*/
}

static void
vtrace(const char *fmt, va_list ap) {
	if (tracing) {
		fprintf(stdout, "%s: [", program);
		vfprintf(stdout, fmt, ap);
		fputs("]\n", stdout);
	}
}

static void
trace(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vtrace(fmt, args);
	va_end(args);
}

static void
result(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fputc('\n', stdout);
	va_end(args);
}

static void
fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: fatal error: ", program);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
	exit(1);
}

static void
verror(const char *fmt, va_list ap) {
	fprintf(stderr, "%s: error: ", program);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	errors++;
}

static void
error(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	if (silent)
		vtrace(fmt, args);
	else
		verror(fmt, args);
	va_end(args);
}
