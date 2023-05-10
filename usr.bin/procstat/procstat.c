/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007, 2011 Robert N. M. Watson
 * Copyright (c) 2015 Allan Jude <allanjude@freebsd.org>
 * Copyright (c) 2017 Dell EMC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "procstat.h"

enum {
	PS_CMP_NORMAL = 0x00,
	PS_CMP_PLURAL = 0x01, 
	PS_CMP_SUBSTR = 0x02
};

struct procstat_cmd {
	const char *command;
	const char *xocontainer;
	const char *usage;
	void (*cmd)(struct procstat *, struct kinfo_proc *);
	void (*opt)(int, char * const *);
	int cmp;
};

int procstat_opts = 0;

static void cmdopt_none(int argc, char * const argv[]);
static void cmdopt_verbose(int argc, char * const argv[]);
static void cmdopt_signals(int argc, char * const argv[]);
static void cmdopt_rusage(int argc, char * const argv[]);
static void cmdopt_files(int argc, char * const argv[]);
static void cmdopt_cpuset(int argc, char * const argv[]);

static const char *progname;

/* aliased program parameters and arguments
 * - usage field is abused to hold the pointer to the function
 *   displaying program usage
 */
static const struct procstat_cmd pacmd_table[] = {
	/* arguments are the same as for pwdx: pid or core file */
	{ "pargs", "args", NULL, &procstat_pargs, &cmdopt_none,
	    PS_CMP_NORMAL | PS_MODE_COMPAT },
	{ "penv", "env", NULL, &procstat_penv, &cmdopt_none,
	    PS_CMP_NORMAL | PS_MODE_COMPAT },
	{ "pwdx", "pwd", NULL, &procstat_pwdx, &cmdopt_none,
	    PS_CMP_NORMAL | PS_MODE_COMPAT }
};

/* procstat parameters and arguments */
static const struct procstat_cmd cmd_table[] = {
	{ "advlock", "advisory_locks", NULL, &procstat_advlocks, &cmdopt_none,
	    PS_CMP_PLURAL | PS_CMP_SUBSTR | PS_MODE_NO_KINFO_PROC },
	{ "argument", "arguments", NULL, &procstat_args, &cmdopt_none,
	    PS_CMP_PLURAL | PS_CMP_SUBSTR },
	{ "auxv", "auxv", NULL, &procstat_auxv, &cmdopt_none, PS_CMP_NORMAL },
	{ "basic", "basic", NULL, &procstat_basic, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "binary", "binary", NULL, &procstat_bin, &cmdopt_none,
	    PS_CMP_SUBSTR },
	{ "cpuset", "cs", NULL, &procstat_cs, &cmdopt_cpuset, PS_CMP_NORMAL },
	{ "cs", "cs", NULL, &procstat_cs, &cmdopt_cpuset, PS_CMP_NORMAL },
	{ "credential", "credentials", NULL, &procstat_cred, &cmdopt_none,
	    PS_CMP_PLURAL | PS_CMP_SUBSTR },
	{ "environment", "environment", NULL, &procstat_env, &cmdopt_none,
	    PS_CMP_SUBSTR },
	{ "fd", "files", "[-C]", &procstat_files, &cmdopt_files,
	    PS_CMP_PLURAL },
	{ "file", "files", "[-C]", &procstat_files, &cmdopt_files,
	    PS_CMP_PLURAL },
	{ "kstack", "kstack", "[-v]", &procstat_kstack, &cmdopt_verbose,
	    PS_CMP_NORMAL },
	{ "pargs", "args", NULL, &procstat_pargs, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "penv", "env", NULL, &procstat_penv, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "ptlwpinfo", "ptlwpinfo", NULL, &procstat_ptlwpinfo, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "pwdx", "pwd", NULL, &procstat_pwdx, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "rlimit", "rlimit", NULL, &procstat_rlimit, &cmdopt_none,
	    PS_CMP_NORMAL },
	{ "rusage", "rusage", "[-Ht]", &procstat_rusage, &cmdopt_rusage,
	    PS_CMP_NORMAL },
	{ "sigfastblock", "sigfastblock", NULL, &procstat_sigfastblock,
	    &cmdopt_none, PS_CMP_NORMAL },
	{ "signal", "signals", "[-n]", &procstat_sigs, &cmdopt_signals,
	    PS_CMP_PLURAL | PS_CMP_SUBSTR },
	{ "thread", "threads", NULL, &procstat_threads, &cmdopt_none,
	    PS_CMP_PLURAL },
	{ "tsignal", "thread_signals", "[-n]", &procstat_threads_sigs,
	    &cmdopt_signals, PS_CMP_PLURAL | PS_CMP_SUBSTR },
	{ "vm", "vm", NULL, &procstat_vm, &cmdopt_none, PS_CMP_NORMAL }
};

static void
usage(const struct procstat_cmd *cmd)
{
	size_t i, l;
	int multi;

	if (cmd == NULL || (cmd->cmp & PS_MODE_COMPAT) == 0) {
		xo_error("usage: procstat [--libxo] [-h] [-M core] [-N system]"
		    " [-w interval] command\n"
		    "                [pid ... | core ...]\n"
		    "       procstat [--libxo] -a [-h] [-M core] [-N system] "
		    " [-w interval] command\n"
		    "       procstat [--libxo] [-h] [-M core] [-N system]"
		    " [-w interval]\n"
		    "                [-S | -b | -c | -e | -f [-C] | -i [-n] | "
		    "-j [-n] | -k [-k] |\n"
		    "                 -l | -r [-H] | -s | -t | -v | -x] "
		    "[pid ... | core ...]\n"
		    "       procstat [--libxo] -a [-h] [-M core] [-N system]"
		    " [-w interval]\n"
		    "                [-S | -b | -c | -e | -f [-C] | -i [-n] | "
		    "-j [-n] | -k [-k] |\n"
		    "                 -l | -r [-H] | -s | -t | -v | -x]\n"
		    "       procstat [--libxo] -L [-h] [-M core] [-N system] core ...\n"
		    "Available commands:\n");
		for (i = 0, l = nitems(cmd_table); i < l; i++) {
			multi = i + 1 < l && cmd_table[i].cmd ==
			    cmd_table[i + 1].cmd;
			xo_error("       %s%s%s", multi ? "[" : "",
			    cmd_table[i].command, (cmd_table[i].cmp &
			    PS_CMP_PLURAL) ? "(s)" : "");
			for (; i + 1 < l && cmd_table[i].cmd ==
			    cmd_table[i + 1].cmd; i++)
				xo_error(" | %s%s", cmd_table[i + 1].command,
				    (cmd_table[i].cmp & PS_CMP_PLURAL) ?
				    "(s)" : "");
			if (multi)
				xo_error("]");
			if (cmd_table[i].usage != NULL)
				xo_error(" %s", cmd_table[i].usage);
			xo_error("\n");
		}
	} else {
		xo_error("usage: %s [--libxo] pid ...\n", progname);
	}
	xo_finish();
	exit(EX_USAGE);
}

static void
procstat(const struct procstat_cmd *cmd, struct procstat *prstat,
    struct kinfo_proc *kipp)
{
	char *pidstr = NULL;

	asprintf(&pidstr, "%d", kipp->ki_pid);
	if (pidstr == NULL)
		xo_errc(1, ENOMEM, "Failed to allocate memory in procstat()");
	xo_open_container(pidstr);
	cmd->cmd(prstat, kipp);
	xo_close_container(pidstr);
	free(pidstr);
}

/*
 * Sort processes first by pid and then tid.
 */
static int
kinfo_proc_compare(const void *a, const void *b)
{
	int i;

	i = ((const struct kinfo_proc *)a)->ki_pid -
	    ((const struct kinfo_proc *)b)->ki_pid;
	if (i != 0)
		return (i);
	i = ((const struct kinfo_proc *)a)->ki_tid -
	    ((const struct kinfo_proc *)b)->ki_tid;
	return (i);
}

void
kinfo_proc_sort(struct kinfo_proc *kipp, int count)
{

	qsort(kipp, count, sizeof(*kipp), kinfo_proc_compare);
}

const char *
kinfo_proc_thread_name(const struct kinfo_proc *kipp)
{
	static char name[MAXCOMLEN+1];

	strlcpy(name, kipp->ki_tdname, sizeof(name));
	strlcat(name, kipp->ki_moretdname, sizeof(name));
	if (name[0] == '\0' || strcmp(kipp->ki_comm, name) == 0) {
		name[0] = '-';
		name[1] = '\0';
	}

	return (name);
}

static const struct procstat_cmd *
getcmdbyprogname(const char *pprogname)
{
	const char *ca;
	size_t i;

	if (pprogname == NULL)
		return (NULL);

	for (i = 0; i < nitems(pacmd_table); i++) {
		ca = pacmd_table[i].command;
		if (ca != NULL && strcmp(ca, pprogname) == 0)
			return (&pacmd_table[i]);
	}

	return (NULL);
}

static const struct procstat_cmd *
getcmd(const char *str)
{
	const struct procstat_cmd *cmd;
	size_t i, l;
	int cmp, s;

	if (str == NULL)
		return (NULL);
	cmd = NULL;
	if ((l = strlen(str)) == 0)
		return (getcmd("basic"));
	s = l > 1 && strcasecmp(str + l - 1, "s") == 0;
	for (i = 0; i < nitems(cmd_table); i++) {
		/*
		 * After the first match substring matches are disabled,
		 * allowing subsequent full matches to take precedence.
		 */
		if (cmd == NULL && (cmd_table[i].cmp & PS_CMP_SUBSTR))
			cmp = strncasecmp(str, cmd_table[i].command, l -
			    ((cmd_table[i].cmp & PS_CMP_PLURAL) && s ? 1 : 0));
		else if ((cmd_table[i].cmp & PS_CMP_PLURAL) && s &&
		    l == strlen(cmd_table[i].command) + 1)
			cmp = strncasecmp(str, cmd_table[i].command, l - 1);
		else
			cmp = strcasecmp(str, cmd_table[i].command);
		if (cmp == 0)
			cmd = &cmd_table[i];
	}
	return (cmd);
}

int
main(int argc, char *argv[])
{
	struct kinfo_proc *p;
	const struct procstat_cmd *cmd;
	struct procstat *prstat, *cprstat;
	char *dummy, *nlistf, *memf;
	const char *xocontainer;
	long l;
	pid_t pid;
	int aflag, ch, cnt, i, interval;

	interval = 0;
	cmd = NULL;
	memf = nlistf = NULL;
	aflag = 0;
	argc = xo_parse_args(argc, argv);

	progname = getprogname();
	cmd = getcmdbyprogname(progname);

	while ((ch = getopt(argc, argv, "abCcefHhijkLlM:N:nrSstvw:x")) != -1) {
		switch (ch) {
		case 'a':
			aflag++;
			break;
		case 'b':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("binary");
			break;
		case 'C':
			procstat_opts |= PS_OPT_CAPABILITIES;
			break;
		case 'c':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("arguments");
			break;
		case 'e':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("environment");
			break;
		case 'f':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("files");
			break;
		case 'H':
			procstat_opts |= PS_OPT_PERTHREAD;
			break;
		case 'h':
			procstat_opts |= PS_OPT_NOHEADER;
			break;
		case 'i':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("signals");
			break;
		case 'j':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("tsignals");
			break;
		case 'k':
			if (cmd != NULL && cmd->cmd == procstat_kstack) {
				if ((procstat_opts & PS_OPT_VERBOSE) != 0)
					usage(cmd);
				procstat_opts |= PS_OPT_VERBOSE;
			} else {
				if (cmd != NULL)
					usage(cmd);
				cmd = getcmd("kstack");
			}
			break;
		case 'L':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("ptlwpinfo");
			break;
		case 'l':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("rlimit");
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			procstat_opts |= PS_OPT_SIGNUM;
			break;
		case 'r':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("rusage");
			break;
		case 'S':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("cpuset");
			break;
		case 's':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("credentials");
			break;
		case 't':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("threads");
			break;
		case 'v':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("vm");
			break;
		case 'w':
			l = strtol(optarg, &dummy, 10);
			if (*dummy != '\0')
				usage(cmd);
			if (l < 1 || l > INT_MAX)
				usage(cmd);
			interval = l;
			break;
		case 'x':
			if (cmd != NULL)
				usage(cmd);
			cmd = getcmd("auxv");
			break;
		case '?':
		default:
			usage(cmd);
		}

	}
	argc -= optind;
	argv += optind;

	if (cmd == NULL && argv[0] != NULL && (cmd = getcmd(argv[0])) != NULL) {
		if ((procstat_opts & PS_SUBCOMMAND_OPTS) != 0)
			usage(cmd);
		if (cmd->opt != NULL) {
			optreset = 1;
			optind = 1;
			cmd->opt(argc, argv);
			if ((cmd->cmp & PS_MODE_COMPAT) == 0) {
				argc -= optind;
				argv += optind;
			}
		} else {
			argc -= 1;
			argv += 1;
		}
	} else {
		if (cmd == NULL)
			cmd = getcmd("basic");
		if (cmd->cmd != procstat_files &&
		    (procstat_opts & PS_OPT_CAPABILITIES) != 0 &&
		    (cmd->cmp & PS_MODE_COMPAT) == 0)
			usage(cmd);
	}

	/* Must specify either the -a flag or a list of pids. */
	if (!(aflag == 1 && argc == 0) && !(aflag == 0 && argc > 0) &&
	    (cmd->cmp & PS_MODE_NO_KINFO_PROC) == 0)
		usage(cmd);

	if (memf != NULL)
		prstat = procstat_open_kvm(nlistf, memf);
	else
		prstat = procstat_open_sysctl();
	if (prstat == NULL)
		xo_errx(1, "procstat_open()");
	do {
		xocontainer = cmd->xocontainer != NULL ? cmd->xocontainer :
		    cmd->command;
		xo_set_version(PROCSTAT_XO_VERSION);
		xo_open_container(progname);
		xo_open_container(xocontainer);

		if ((cmd->cmp & PS_MODE_NO_KINFO_PROC) != 0) {
			cmd->cmd(prstat, NULL);
			goto iter;
		}

		if (aflag) {
			p = procstat_getprocs(prstat, KERN_PROC_PROC, 0, &cnt);
			if (p == NULL)
				xo_errx(1, "procstat_getprocs()");
			kinfo_proc_sort(p, cnt);
			for (i = 0; i < cnt; i++) {
				procstat(cmd, prstat, &p[i]);

				/* Suppress header after first process. */
				procstat_opts |= PS_OPT_NOHEADER;
				xo_flush();
			}
			procstat_freeprocs(prstat, p);
		}
		for (i = 0; i < argc; i++) {
			l = strtol(argv[i], &dummy, 10);
			if (*dummy == '\0') {
				if (l < 0)
					usage(cmd);
				pid = l;

				p = procstat_getprocs(prstat, KERN_PROC_PID,
				    pid, &cnt);
				if (p == NULL)
					xo_errx(1, "procstat_getprocs()");
				if (cnt != 0)
					procstat(cmd, prstat, p);
				procstat_freeprocs(prstat, p);
			} else {
				if ((cmd->cmp & PS_MODE_COMPAT) == 0) {
					cprstat = procstat_open_core(argv[i]);
					if (cprstat == NULL) {
						warnx("procstat_open()");
						continue;
					}
					p = procstat_getprocs(cprstat,
					    KERN_PROC_PID, -1, &cnt);
					if (p == NULL) {
						xo_errx(1,
						    "procstat_getprocs()");
					}
					if (cnt != 0)
						procstat(cmd, cprstat, p);
					procstat_freeprocs(cprstat, p);
					procstat_close(cprstat);
				} else {
					usage(cmd);
				}
			}
			if ((cmd->cmp & PS_MODE_COMPAT) == 0) {
				/* Suppress header after first process. */
				procstat_opts |= PS_OPT_NOHEADER;
			}
		}

iter:
		xo_close_container(xocontainer);
		xo_close_container(progname);
		xo_finish();
		if (interval)
			sleep(interval);
	} while (interval);

	procstat_close(prstat);

	exit(0);
}

void
cmdopt_none(int argc, char * const argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		case '?':
		default:
			usage(NULL);
		}
	}
}

void
cmdopt_verbose(int argc, char * const argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			procstat_opts |= PS_OPT_VERBOSE;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}
}

void
cmdopt_signals(int argc, char * const argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "n")) != -1) {
		switch (ch) {
		case 'n':
			procstat_opts |= PS_OPT_SIGNUM;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}
}

void
cmdopt_rusage(int argc, char * const argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "Ht")) != -1) {
		switch (ch) {
		case 'H':
			/* FALLTHROUGH */
		case 't':
			procstat_opts |= PS_OPT_PERTHREAD;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}
}

void
cmdopt_files(int argc, char * const argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "C")) != -1) {
		switch (ch) {
		case 'C':
			procstat_opts |= PS_OPT_CAPABILITIES;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}
}

void
cmdopt_cpuset(int argc, char * const argv[])
{

	procstat_opts |= PS_OPT_PERTHREAD;
	cmdopt_none(argc, argv);
}
