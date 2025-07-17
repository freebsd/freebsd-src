/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010, 2012  David E. O'Brien
 * Copyright (c) 1980, 1992, 1993
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

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/endian.h>
#include <dev/filemon/filemon.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DEF_BUF 65536

struct stamp {
	uint64_t scr_len;	/* amount of data */
	uint64_t scr_sec;	/* time it arrived in seconds... */
	uint32_t scr_usec;	/* ...and microseconds */
	uint32_t scr_direction; /* 'i', 'o', etc (also indicates endianness) */
};

struct buf_elm {
	TAILQ_ENTRY(buf_elm) link;
	size_t rpos;
	size_t len;
	char ibuf[];
};

static FILE *fscript;
static int master, slave;
static int child;
static const char *fname;
static char *fmfname;
static int fflg, qflg, ttyflg;
static int usesleep, rawout, showexit;
static TAILQ_HEAD(, buf_elm) obuf_list = TAILQ_HEAD_INITIALIZER(obuf_list);
static volatile sig_atomic_t doresize;

static struct termios tt;

#ifndef TSTAMP_FMT
/* useful for tool and human reading */
# define TSTAMP_FMT "%n@ %s [%Y-%m-%d %T]%n"
#endif
static const char *tstamp_fmt = TSTAMP_FMT;
static int tflg;

static void done(int) __dead2;
static void doshell(char **);
static void finish(void);
static void record(FILE *, char *, size_t, int);
static void consume(FILE *, off_t, char *, int);
static void playback(FILE *) __dead2;
static void usage(void) __dead2;
static void resizeit(int);

int
main(int argc, char *argv[])
{
	struct termios rtt, stt;
	struct winsize win;
	struct timespec tv, *tvp;
	time_t tvec, start;
	char obuf[BUFSIZ];
	char ibuf[BUFSIZ];
	sigset_t *pselmask, selmask;
	fd_set rfd, wfd;
	struct buf_elm *be;
	ssize_t cc;
	int aflg, Fflg, kflg, pflg, wflg, ch, k, n, fcm;
	int flushtime, readstdin;
	int fm_fd, fm_log;

	aflg = Fflg = kflg = pflg = wflg = 0;
	doresize = 0;
	usesleep = 1;
	rawout = 0;
	flushtime = 30;
	fm_fd = -1;
	showexit = 0;

	/*
	 * For normal operation, we'll leave pselmask == NULL so that pselect(2)
	 * leaves the signal mask alone.  If -w is specified, we'll restore the
	 * process signal mask upon entry with SIGWINCH unblocked so that we can
	 * forward resize events properly.
	 */
	sigemptyset(&selmask);
	pselmask = NULL;

	while ((ch = getopt(argc, argv, "adeFfkpqrT:t:w")) != -1)
		switch (ch) {
		case 'a':
			aflg = 1;
			break;
		case 'd':
			usesleep = 0;
			break;
		case 'e':
			/* Default behavior, accepted for linux compat. */
			break;
		case 'F':
			Fflg = 1;
			break;
		case 'f':
			fflg = 1;
			break;
		case 'k':
			kflg = 1;
			break;
		case 'p':
			pflg = 1;
			break;
		case 'q':
			qflg = 1;
			break;
		case 'r':
			rawout = 1;
			break;
		case 't':
			flushtime = atoi(optarg);
			if (flushtime < 0)
				err(1, "invalid flush time %d", flushtime);
			break;
		case 'T':
			tflg = pflg = 1;
			if (strchr(optarg, '%'))
				tstamp_fmt = optarg;
			break;
		case 'w':
			wflg = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		fname = argv[0];
		argv++;
		argc--;
	} else
		fname = "typescript";

	if ((fscript = fopen(fname, pflg ? "r" : aflg ? "a" : "w")) == NULL)
		err(1, "%s", fname);

	if (fflg) {
		asprintf(&fmfname, "%s.filemon", fname);
		if (!fmfname)
			err(1, "%s.filemon", fname);
		if ((fm_fd = open("/dev/filemon", O_RDWR | O_CLOEXEC)) == -1)
			err(1, "open(\"/dev/filemon\", O_RDWR)");
		if ((fm_log = open(fmfname,
		    O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
			err(1, "open(%s)", fmfname);
		if (ioctl(fm_fd, FILEMON_SET_FD, &fm_log) < 0)
			err(1, "Cannot set filemon log file descriptor");
	}

	if (pflg)
		playback(fscript);

	if (tcgetattr(STDIN_FILENO, &tt) == -1 ||
	    ioctl(STDIN_FILENO, TIOCGWINSZ, &win) == -1) {
		if (errno != ENOTTY) /* For debugger. */
			err(1, "tcgetattr/ioctl");
		if (openpty(&master, &slave, NULL, NULL, NULL) == -1)
			err(1, "openpty");
	} else {
		if (openpty(&master, &slave, NULL, &tt, &win) == -1)
			err(1, "openpty");
		ttyflg = 1;
	}
	fcm = fcntl(master, F_GETFL);
	if (fcm == -1)
		err(1, "master F_GETFL");
	fcm |= O_NONBLOCK;
	if (fcntl(master, F_SETFL, fcm) == -1)
		err(1, "master F_SETFL");

	if (rawout)
		record(fscript, NULL, 0, 's');

	if (!qflg) {
		tvec = time(NULL);
		(void)printf("Script started, output file is %s\n", fname);
		if (!rawout) {
			(void)fprintf(fscript, "Script started on %s",
			    ctime(&tvec));
			if (argv[0]) {
				showexit = 1;
				fprintf(fscript, "Command: ");
				for (k = 0 ; argv[k] ; ++k)
					fprintf(fscript, "%s%s", k ? " " : "",
						argv[k]);
				fprintf(fscript, "\n");
			}
		}
		fflush(fscript);
		if (fflg) {
			(void)printf("Filemon started, output file is %s\n",
			    fmfname);
		}
	}
	if (ttyflg) {
		rtt = tt;
		cfmakeraw(&rtt);
		rtt.c_lflag &= ~ECHO;
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);
	}

	assert(fflg ? fm_fd >= 0 : fm_fd < 0);

	child = fork();
	if (child < 0) {
		warn("fork");
		done(1);
	}
	if (child == 0) {
		if (fflg) {
			int pid;

			pid = getpid();
			if (ioctl(fm_fd, FILEMON_SET_PID, &pid) < 0)
				err(1, "Cannot set filemon PID");
		}

		doshell(argv);
	}
	close(slave);

	if (wflg) {
		struct sigaction sa = { .sa_handler = resizeit };
		sigset_t smask;

		sigaction(SIGWINCH, &sa, NULL);

		sigemptyset(&smask);
		sigaddset(&smask, SIGWINCH);

		if (sigprocmask(SIG_BLOCK, &smask, &selmask) != 0)
			err(1, "Failed to block SIGWINCH");

		/* Just in case SIGWINCH was blocked before we came in. */
		sigdelset(&selmask, SIGWINCH);
		pselmask = &selmask;
	}

	start = tvec = time(0);
	readstdin = 1;
	for (;;) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_SET(master, &rfd);
		if (readstdin)
			FD_SET(STDIN_FILENO, &rfd);
		if (!TAILQ_EMPTY(&obuf_list))
			FD_SET(master, &wfd);
		if (!readstdin && ttyflg) {
			tv.tv_sec = 1;
			tv.tv_nsec = 0;
			tvp = &tv;
			readstdin = 1;
		} else if (flushtime > 0) {
			tv.tv_sec = flushtime - (tvec - start);
			tv.tv_nsec = 0;
			tvp = &tv;
		} else {
			tvp = NULL;
		}
		n = pselect(master + 1, &rfd, &wfd, NULL, tvp, pselmask);
		if (n < 0 && errno != EINTR)
			break;

		if (doresize) {
			if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) != -1)
				ioctl(master, TIOCSWINSZ, &win);
			doresize = 0;
		}

		if (n > 0 && FD_ISSET(STDIN_FILENO, &rfd)) {
			cc = read(STDIN_FILENO, ibuf, BUFSIZ);
			if (cc < 0)
				break;
			if (cc == 0) {
				if (tcgetattr(master, &stt) == 0 &&
				    (stt.c_lflag & ICANON) != 0) {
					(void)write(master, &stt.c_cc[VEOF], 1);
				}
				readstdin = 0;
			}
			if (cc > 0) {
				if (rawout)
					record(fscript, ibuf, cc, 'i');
				be = malloc(sizeof(*be) + cc);
				be->rpos = 0;
				be->len = cc;
				memcpy(be->ibuf, ibuf, cc);
				TAILQ_INSERT_TAIL(&obuf_list, be, link);
			}
		}
		if (n > 0 && FD_ISSET(master, &wfd)) {
			while ((be = TAILQ_FIRST(&obuf_list)) != NULL) {
				cc = write(master, be->ibuf + be->rpos,
				    be->len);
				if (cc == -1) {
					if (errno == EWOULDBLOCK ||
					    errno == EINTR)
						break;
					warn("write master");
					done(1);
				}
				if (cc == 0)
					break;		/* retry later ? */
				if (kflg && tcgetattr(master, &stt) >= 0 &&
				    ((stt.c_lflag & ECHO) == 0)) {
					(void)fwrite(be->ibuf + be->rpos,
					    1, cc, fscript);
				}
				be->len -= cc;
				if (be->len == 0) {
					TAILQ_REMOVE(&obuf_list, be, link);
					free(be);
				} else {
					be->rpos += cc;
				}
			}
		}
		if (n > 0 && FD_ISSET(master, &rfd)) {
			cc = read(master, obuf, sizeof(obuf));
			if (cc <= 0)
				break;
			(void)write(STDOUT_FILENO, obuf, cc);
			if (rawout)
				record(fscript, obuf, cc, 'o');
			else
				(void)fwrite(obuf, 1, cc, fscript);
		}
		tvec = time(0);
		if (tvec - start >= flushtime) {
			fflush(fscript);
			start = tvec;
		}
		if (Fflg)
			fflush(fscript);
	}
	finish();
	done(0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: script [-aeFfkpqrw] [-t time] [file [command ...]]\n");
	(void)fprintf(stderr,
	    "       script -p [-deq] [-T fmt] [file]\n");
	exit(1);
}

static void
finish(void)
{
	int e, status;

	if (waitpid(child, &status, 0) == child) {
		if (WIFEXITED(status))
			e = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			e = WTERMSIG(status);
		else /* can't happen */
			e = 1;
		done(e);
	}
}

static void
doshell(char **av)
{
	const char *shell;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	(void)close(master);
	(void)fclose(fscript);
	free(fmfname);
	login_tty(slave);
	setenv("SCRIPT", fname, 1);
	if (av[0]) {
		execvp(av[0], av);
		warn("%s", av[0]);
	} else {
		execl(shell, shell, "-i", (char *)NULL);
		warn("%s", shell);
	}
	exit(1);
}

static void
done(int eno)
{
	time_t tvec;

	if (ttyflg)
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
	tvec = time(NULL);
	if (rawout)
		record(fscript, NULL, 0, 'e');
	if (!qflg) {
		if (!rawout) {
			if (showexit)
				(void)fprintf(fscript, "\nCommand exit status:"
				    " %d", eno);
			(void)fprintf(fscript, "\nScript done on %s",
			    ctime(&tvec));
		}
		(void)printf("\nScript done, output file is %s\n", fname);
		if (fflg) {
			(void)printf("Filemon done, output file is %s\n",
			    fmfname);
		}
	}
	(void)fclose(fscript);
	(void)close(master);
	exit(eno);
}

static void
record(FILE *fp, char *buf, size_t cc, int direction)
{
	struct iovec iov[2];
	struct stamp stamp;
	struct timeval tv;

	(void)gettimeofday(&tv, NULL);
	stamp.scr_len = cc;
	stamp.scr_sec = tv.tv_sec;
	stamp.scr_usec = tv.tv_usec;
	stamp.scr_direction = direction;
	iov[0].iov_len = sizeof(stamp);
	iov[0].iov_base = &stamp;
	iov[1].iov_len = cc;
	iov[1].iov_base = buf;
	if (writev(fileno(fp), &iov[0], 2) == -1)
		err(1, "writev");
}

static void
consume(FILE *fp, off_t len, char *buf, int reg)
{
	size_t l;

	if (reg) {
		if (fseeko(fp, len, SEEK_CUR) == -1)
			err(1, NULL);
	} else {
		while (len > 0) {
			l = MIN(DEF_BUF, len);
			if (fread(buf, sizeof(char), l, fp) != l)
				err(1, "cannot read buffer");
			len -= l;
		}
	}
}

#define swapstamp(stamp) do { \
	if (stamp.scr_direction > 0xff) { \
		stamp.scr_len = bswap64(stamp.scr_len); \
		stamp.scr_sec = bswap64(stamp.scr_sec); \
		stamp.scr_usec = bswap32(stamp.scr_usec); \
		stamp.scr_direction = bswap32(stamp.scr_direction); \
	} \
} while (0/*CONSTCOND*/)

static void
termset(void)
{
	struct termios traw;

	if (tcgetattr(STDOUT_FILENO, &tt) == -1) {
		if (errno != ENOTTY) /* For debugger. */
			err(1, "tcgetattr");
		return;
	}
	ttyflg = 1;
	traw = tt;
	cfmakeraw(&traw);
	traw.c_lflag |= ISIG;
	(void)tcsetattr(STDOUT_FILENO, TCSANOW, &traw);
}

static void
termreset(void)
{
	if (ttyflg) {
		tcsetattr(STDOUT_FILENO, TCSADRAIN, &tt);
		ttyflg = 0;
	}
}

static void
playback(FILE *fp)
{
	struct timespec tsi, tso;
	struct stamp stamp;
	struct stat pst;
	char buf[DEF_BUF];
	off_t nread, save_len;
	size_t l;
	time_t tclock;
	time_t lclock;
	int reg;

	if (fstat(fileno(fp), &pst) == -1)
		err(1, "fstat failed");

	reg = S_ISREG(pst.st_mode);
	lclock = 0;

	for (nread = 0; !reg || nread < pst.st_size; nread += save_len) {
		if (fread(&stamp, sizeof(stamp), 1, fp) != 1) {
			if (reg)
				err(1, "reading playback header");
			else
				break;
		}
		swapstamp(stamp);
		save_len = sizeof(stamp);

		if (reg && stamp.scr_len >
		    (uint64_t)(pst.st_size - save_len) - nread)
			errx(1, "invalid stamp");

		save_len += stamp.scr_len;
		tclock = stamp.scr_sec;
		tso.tv_sec = stamp.scr_sec;
		tso.tv_nsec = stamp.scr_usec * 1000;
		if (nread == 0)
			tsi = tso;

		switch (stamp.scr_direction) {
		case 's':
			if (!qflg)
			    (void)printf("Script started on %s",
				ctime(&tclock));
			tsi = tso;
			(void)consume(fp, stamp.scr_len, buf, reg);
			termset();
			atexit(termreset);
			break;
		case 'e':
			termreset();
			if (!qflg)
				(void)printf("\nScript done on %s",
				    ctime(&tclock));
			(void)consume(fp, stamp.scr_len, buf, reg);
			break;
		case 'i':
			/* throw input away */
			(void)consume(fp, stamp.scr_len, buf, reg);
			break;
		case 'o':
			if (tflg) {
				if (stamp.scr_len == 0)
					continue;
				if (tclock - lclock > 0) {
				    l = strftime(buf, sizeof buf, tstamp_fmt,
					localtime(&tclock));
				    (void)write(STDOUT_FILENO, buf, l);
				}
				lclock = tclock;
			} else {
				tsi.tv_sec = tso.tv_sec - tsi.tv_sec;
				tsi.tv_nsec = tso.tv_nsec - tsi.tv_nsec;
				if (tsi.tv_nsec < 0) {
					tsi.tv_sec -= 1;
					tsi.tv_nsec += 1000000000;
				}
				if (usesleep)
					(void)nanosleep(&tsi, NULL);
				tsi = tso;
			}
			while (stamp.scr_len > 0) {
				l = MIN(DEF_BUF, stamp.scr_len);
				if (fread(buf, sizeof(char), l, fp) != l)
					err(1, "cannot read buffer");

				(void)write(STDOUT_FILENO, buf, l);
				stamp.scr_len -= l;
			}
			break;
		default:
			errx(1, "invalid direction");
		}
	}
	(void)fclose(fp);
	exit(0);
}

static void
resizeit(int signo __unused)
{
	doresize = 1;
}
