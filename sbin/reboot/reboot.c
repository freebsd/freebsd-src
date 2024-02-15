/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/boottrace.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmpx.h>

#define PATH_NEXTBOOT "/boot/nextboot.conf"

static void usage(void) __dead2;
static uint64_t get_pageins(void);

static bool dohalt;
static bool donextboot;

#define E(...) do {				\
		if (force) {			\
			warn( __VA_ARGS__ );	\
			return;			\
		}				\
		err(1, __VA_ARGS__);		\
	} while (0)				\

static void
zfsbootcfg(const char *pool, bool force)
{
	char *k;
	int rv;

	asprintf(&k,
	    "zfsbootcfg -z %s -n freebsd:nvstore -k nextboot_enable -v YES",
	    pool);
	if (k == NULL)
		E("No memory for zfsbootcfg");

	rv = system(k);
	if (rv == 0)
		return;
	if (rv == -1)
		E("system zfsbootcfg");
	if (rv == 127)
		E("zfsbootcfg not found in path");
	E("zfsbootcfg returned %d", rv);
}

static void
write_nextboot(const char *fn, const char *env, bool force)
{
	FILE *fp;
	struct statfs sfs;
	bool supported = false;
	bool zfs = false;

	if (statfs("/boot", &sfs) != 0)
		err(1, "statfs /boot");
	if (strcmp(sfs.f_fstypename, "ufs") == 0) {
		/*
		 * Only UFS supports the full nextboot protocol.
		 */
		supported = true;
	} else if (strcmp(sfs.f_fstypename, "zfs") == 0) {
		zfs = true;
	}

	if (zfs) {
		zfsbootcfg(sfs.f_mntfromname, force);
	}

	fp = fopen(fn, "w");
	if (fp == NULL)
		E("Can't create %s", fn);

	if (fprintf(fp,"%s%s",
	    supported ? "nextboot_enable=\"YES\"\n" : "",
	    env != NULL ? env : "") < 0) {
		int e;

		e = errno;
		fclose(fp);
		if (unlink(fn))
			warn("unlink %s", fn);
		errno = e;
		E("Can't write %s", fn);
	}
	fclose(fp);
}

static char *
split_kv(char *raw)
{
	char *eq;
	int len;

	eq = strchr(raw, '=');
	if (eq == NULL)
		errx(1, "No = in environment string %s", raw);
	*eq++ = '\0';
	len = strlen(eq);
	if (len == 0)
		errx(1, "Invalid null value %s=", raw);
	if (eq[0] == '"') {
		if (len < 2 || eq[len - 1] != '"')
			errx(1, "Invalid string '%s'", eq);
		eq[len - 1] = '\0';
		return (eq + 1);
	}
	return (eq);
}

static void
add_env(char **env, const char *key, const char *value)
{
	char *oldenv;

	oldenv = *env;
	asprintf(env, "%s%s=\"%s\"\n", oldenv != NULL ? oldenv : "", key, value);
	if (env == NULL)
		errx(1, "No memory to build env array");
	free(oldenv);
}

/*
 * Different options are valid for different programs.
 */
#define GETOPT_REBOOT "cDde:k:lNno:pqr"
#define GETOPT_NEXTBOOT "De:k:o:"

int
main(int argc, char *argv[])
{
	struct utmpx utx;
	const struct passwd *pw;
	int ch, howto = 0, i, sverrno;
	bool Dflag, fflag, lflag, Nflag, nflag, qflag;
	uint64_t pageins;
	const char *user, *kernel = NULL, *getopts = GETOPT_REBOOT;
	char *env = NULL, *v;

	if (strstr(getprogname(), "halt") != NULL) {
		dohalt = true;
		howto = RB_HALT;
	} else if (strcmp(getprogname(), "nextboot") == 0) {
		donextboot = true;
		getopts = GETOPT_NEXTBOOT; /* Note: reboot's extra opts return '?' */
	} else {
		/* reboot */
		howto = 0;
	}
	Dflag = fflag = lflag = Nflag = nflag = qflag = false;
	while ((ch = getopt(argc, argv, getopts)) != -1) {
		switch(ch) {
		case 'c':
			howto |= RB_POWERCYCLE;
			break;
		case 'D':
			Dflag = true;
			break;
		case 'd':
			howto |= RB_DUMP;
			break;
		case 'e':
			v = split_kv(optarg);
			add_env(&env, optarg, v);
			break;
		case 'f':
			fflag = true;
			break;
		case 'k':
			kernel = optarg;
			break;
		case 'l':
			lflag = true;
			break;
		case 'n':
			nflag = true;
			howto |= RB_NOSYNC;
			break;
		case 'N':
			nflag = true;
			Nflag = true;
			break;
		case 'o':
			add_env(&env, "kernel_options", optarg);
			break;
		case 'p':
			howto |= RB_POWEROFF;
			break;
		case 'q':
			qflag = true;
			break;
		case 'r':
			howto |= RB_REROOT;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	if (Dflag && ((howto & ~RB_HALT) != 0  || kernel != NULL))
		errx(1, "cannot delete existing nextboot config and do anything else");
	if ((howto & (RB_DUMP | RB_HALT)) == (RB_DUMP | RB_HALT))
		errx(1, "cannot dump (-d) when halting; must reboot instead");
	if (Nflag && (howto & RB_NOSYNC) != 0)
		errx(1, "-N cannot be used with -n");
	if ((howto & RB_POWEROFF) && (howto & RB_POWERCYCLE))
		errx(1, "-c and -p cannot be used together");
	if ((howto & RB_REROOT) != 0 && howto != RB_REROOT)
		errx(1, "-r cannot be used with -c, -d, -n, or -p");
	if ((howto & RB_REROOT) != 0 && kernel != NULL)
		errx(1, "-r and -k cannot be used together, there is no next kernel");

	if (Dflag) {
		if (unlink(PATH_NEXTBOOT) != 0)
			err(1, "unlink %s", PATH_NEXTBOOT);
		exit(0);
	}

	if (!donextboot && geteuid() != 0) {
		errno = EPERM;
		err(1, NULL);
	}

	if (qflag) {
		reboot(howto);
		err(1, NULL);
	}

	if (kernel != NULL) {
		if (!fflag) {
			char *k;
			struct stat sb;

			asprintf(&k, "/boot/%s/kernel", kernel);
			if (k == NULL)
				errx(1, "No memory to check %s", kernel);
			if (stat(k, &sb) != 0)
				err(1, "stat %s", k);
			if (!S_ISREG(sb.st_mode))
				errx(1, "%s is not a file", k);
			free(k);
		}
		add_env(&env, "kernel", kernel);
	}

	if (env != NULL)
		write_nextboot(PATH_NEXTBOOT, env, fflag);
	if (donextboot)
		exit (0);

	/* Log the reboot. */
	if (!lflag)  {
		if ((user = getlogin()) == NULL)
			user = (pw = getpwuid(getuid())) ?
			    pw->pw_name : "???";
		if (dohalt) {
			openlog("halt", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "halted by %s", user);
		} else if (howto & RB_REROOT) {
			openlog("reroot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "rerooted by %s", user);
		} else if (howto & RB_POWEROFF) {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "powered off by %s", user);
		} else if (howto & RB_POWERCYCLE) {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "power cycled by %s", user);
		} else {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "rebooted by %s", user);
		}
	}
	utx.ut_type = SHUTDOWN_TIME;
	gettimeofday(&utx.ut_tv, NULL);
	pututxline(&utx);

	/*
	 * Do a sync early on, so disks start transfers while we're off
	 * killing processes.  Don't worry about writes done before the
	 * processes die, the reboot system call syncs the disks.
	 */
	if (!nflag)
		sync();

	/*
	 * Ignore signals that we can get as a result of killing
	 * parents, group leaders, etc.
	 */
	(void)signal(SIGHUP,  SIG_IGN);
	(void)signal(SIGINT,  SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);

	/*
	 * If we're running in a pipeline, we don't want to die
	 * after killing whatever we're writing to.
	 */
	(void)signal(SIGPIPE, SIG_IGN);

	/*
	 * Only init(8) can perform rerooting.
	 */
	if (howto & RB_REROOT) {
		if (kill(1, SIGEMT) == -1)
			err(1, "SIGEMT init");

		return (0);
	}

	/* Just stop init -- if we fail, we'll restart it. */
	BOOTTRACE("SIGTSTP to init(8)...");
	if (kill(1, SIGTSTP) == -1)
		err(1, "SIGTSTP init");

	/* Send a SIGTERM first, a chance to save the buffers. */
	BOOTTRACE("SIGTERM to all other processes...");
	if (kill(-1, SIGTERM) == -1 && errno != ESRCH)
		err(1, "SIGTERM processes");

	/*
	 * After the processes receive the signal, start the rest of the
	 * buffers on their way.  Wait 5 seconds between the SIGTERM and
	 * the SIGKILL to give everybody a chance. If there is a lot of
	 * paging activity then wait longer, up to a maximum of approx
	 * 60 seconds.
	 */
	sleep(2);
	for (i = 0; i < 20; i++) {
		pageins = get_pageins();
		if (!nflag)
			sync();
		sleep(3);
		if (get_pageins() == pageins)
			break;
	}

	for (i = 1;; ++i) {
		BOOTTRACE("SIGKILL to all other processes(%d)...", i);
		if (kill(-1, SIGKILL) == -1) {
			if (errno == ESRCH)
				break;
			goto restart;
		}
		if (i > 5) {
			(void)fprintf(stderr,
			    "WARNING: some process(es) wouldn't die\n");
			break;
		}
		(void)sleep(2 * i);
	}

	reboot(howto);
	/* FALLTHROUGH */

restart:
	BOOTTRACE("SIGHUP to init(8)...");
	sverrno = errno;
	errx(1, "%s%s", kill(1, SIGHUP) == -1 ? "(can't restart init): " : "",
	    strerror(sverrno));
	/* NOTREACHED */
}

static void
usage(void)
{

	(void)fprintf(stderr, dohalt ?
	    "usage: halt [-clNnpq] [-k kernel]\n" :
	    "usage: reboot [-cdlNnpqr] [-k kernel]\n");
	exit(1);
}

static uint64_t
get_pageins(void)
{
	uint64_t pageins;
	size_t len;

	len = sizeof(pageins);
	if (sysctlbyname("vm.stats.vm.v_swappgsin", &pageins, &len, NULL, 0)
	    != 0) {
		warn("v_swappgsin");
		return (0);
	}
	return (pageins);
}
