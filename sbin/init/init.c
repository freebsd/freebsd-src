/*
 * Copyright (c) 1986, 1987, 1992 Daniel D. Lanciani.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Daniel D. Lanciani.
 * 4. The name of the author may not
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Daniel D. Lanciani ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Daniel D. Lanciani BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         5       00076
 * --------------------         -----   ----------------------
 *
 * 15 Aug 92	David Dawes		SIGTERM + 10 seconds before SIGKILL
 * 24 Jul 92	Nate Williams		Fixed utmp removal, wtmp info
 * 31 Jul 92	Christoph Robitschko	Fixed run level change code
 * 04 Sep 92	Paul Kranenburg		Fixed kill -1 and kill -15 for
 *					daemons started from /etc/rc.
 * 26 Jan 93	Nate Williams		Fixed patchkit error
 */


#include <sys/types.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <ttyent.h>
#include <unistd.h>

#define NTTY 32			/* max ttys */
#define NARG 16			/* max args to login/getty */

/* internal flags */
#define TTY_SEEN 0x8000
#define TTY_DIFF 0x4000
#define TTY_LOGIN 0x2000

/* non-standard tty_logout: rerun login/getty with -o switch to clean line */
#ifndef	TTY_LOGOUT
#define TTY_LOGOUT 0x1000
#endif

/* non-standard tty_open: open device for login/getty */
#ifndef	TTY_OPEN
#define TTY_OPEN 0x0800
#endif

#define isspace(c) ((c) == ' ' || (c) == '\t')

struct ttytab {
	char *tt_name;
	char *tt_getty;
	char *tt_type;
	int tt_status;
	int tt_pid;
} ttytab[NTTY], *ttytabend = ttytab;
int drain, sflag;
char arg[128], nam[64], term[64], *env[] = { term, 0 };
jmp_buf single, reread;
char *Reboot = "autoboot";

char *newstring(), *malloc();
extern int errno;

/* signal state of child process */
#define	SIGNALSFORCHILD	 \
	signal(SIGHUP, SIG_DFL); signal(SIGINT, SIG_DFL); \
	signal(SIGTERM, SIG_DFL); signal(SIGALRM, SIG_DFL); \
	signal(SIGTSTP, SIG_DFL); signal(SIGCHLD, SIG_DFL); \
	signal(SIGTTIN, SIG_DFL); signal(SIGTTOU, SIG_DFL); \
	sigsetmask( 0);				/* 04 Sep 92*/

/* SIGHUP: reread /etc/ttys */
void
shup(sig)
{
	longjmp(reread, 1);
}

/* SIGALRM: abort wait and go single user */
void
salrm(sig)
{
	signal(SIGALRM, SIG_DFL);
	warn("process hung");
	longjmp(single, 1);
}

/* SIGTERM: go single user */
void
sterm(sig)
{
	register struct ttytab *tt;

	if (!Reboot) {
		for(tt = ttytab; tt < ttytabend; tt++) {
			free(tt->tt_name);
			free(tt->tt_getty);
			free(tt->tt_type);
		}
		ttytabend = ttytab;
		/* give processes time to exit cleanly */	/* 15 Aug 92*/
		kill(-1, SIGTERM);
		sleep(10);
		/* Now murder them */
		kill(-1, SIGKILL);
		kill(-1, SIGCONT);
		signal(SIGALRM, salrm);
		alarm(30);
		while(wait((int *)0) > 0);
		alarm(0);
		signal(SIGALRM, SIG_DFL);
		longjmp(single, 1);
	}
}

/* SIGTSTP: drain system */
void
ststp(sig)
{
	drain = 1;
}

/* init [-s] [-f] */

main(argc, argv)
char **argv;
{
	register int pid;
	register struct ttytab *tt;
	struct ttyent *ty;
	int status;
	long mask = sigblock(sigmask(SIGHUP) | sigmask(SIGTERM));

	/* did some idiot try to run us? */
	if(getpid() != 1) {
		writes(2,"init: sorry, system daemon, runnable only by system\n");
		exit(0xff);
	}

	/* allocate a session for init */
	(void) setsid();

	/* protect against signals, listen for outside requests */
	signal(SIGHUP, shup);
	signal(SIGTSTP, ststp);

	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	signal (SIGCHLD, SIG_IGN);
	signal (SIGINT, SIG_IGN);

	/* handle arguments, if any */
	if(argc > 1)
		if(!strcmp(argv[1], "-s"))
			sflag++;
		else if(!strcmp(argv[1], "-f"))
			Reboot = 0;
top:
	/* Single user mode? */
	if(sflag) {
		sflag = 0;
		status = 1;
	} else {
		/* otherwise, execute /etc/rc */
		if (access("/etc/rc", F_OK)  == 0) {
			
			signal(SIGTERM, SIG_IGN);	/* XXX */
			if((pid = fork()) < 0)
				fatal("fork");
			else if(!pid) {
				/* signals, to default state */
				SIGNALSFORCHILD;

				/* clean off console */
				revoke("/dev/console");

				/* create a shell */
				login_tty(open("/dev/console", 2));
				execl("/bin/sh", "sh", "/etc/rc", Reboot, (char *)0);
				_exit(127);
			}
			Reboot = 0;			/* 31 Jul 92*/
			while(wait(&status) != pid);

			/* if we are about to be rebooted, then wait for it */
			if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
				pause();
			logwtmp("~", "reboot", "");
		} else	{ status = 1;  sflag = 1; goto top; }
	}
	signal(SIGTERM, sterm);
	Reboot = 0;

	/* do single user shell on console */
	if (setjmp(single) || status) {
		if((pid = fork()) < 0)
			fatal("fork");
		else if(!pid) {
			/* signals, to default state */
			SIGNALSFORCHILD;

			/* clean off console */
			revoke("/dev/console");

			/* do open and configuration of console */
			login_tty(open("/dev/console", 2));
			execl("/bin/sh", "-", (char *)0);
			_exit(127);
		}
		while(wait(&status) != pid);
		while(drain)				/* 31 Jul 92*/
			pause();
		goto top;
	}

	/* multiuser mode, traipse through table */
	setttyent();
	for(tt = ttytab; (ty = getttyent()) && tt < &ttytab[NTTY]; tt++) {
		tt->tt_name = newstring(ty->ty_name);
		tt->tt_getty = newstring(ty->ty_getty);
		tt->tt_type = newstring(ty->ty_type);
		tt->tt_status = ty->ty_status;
	}
	ttytabend = tt;
	endttyent();
	for(tt = ttytab; tt < ttytabend; getty(tt++));

	/* if we receive a request to reread the table, come here */
	if(setjmp(reread)) {

		/* first pass. find and clean the entries that have changed */
		setttyent();
		while(ty = getttyent()) {
			for(tt = ttytab; tt < ttytabend; tt++)
			if(!strcmp(tt->tt_name, ty->ty_name)) {
				/* if a process present, mark */
				if((tt->tt_status & ~TTY_LOGIN) !=ty->ty_status)
					tt->tt_status = ty->ty_status |TTY_DIFF;
				if(strcmp(tt->tt_getty, ty->ty_getty)) {
					free(tt->tt_getty);
					tt->tt_getty = newstring(ty->ty_getty);
					tt->tt_status |= TTY_DIFF;
				}
				if(strcmp(tt->tt_type, ty->ty_type)) {
					free(tt->tt_type);
					tt->tt_type = newstring(ty->ty_type);
					tt->tt_status |= TTY_DIFF;
				}
				if(((tt->tt_status |= TTY_SEEN) & TTY_DIFF)
					&& tt->tt_pid > 1)
					kill(tt->tt_pid, 9);
				break;
			}
			if(tt == ttytabend && tt < &ttytab[NTTY]) {
				tt->tt_name = newstring(ty->ty_name);
				tt->tt_getty = newstring(ty->ty_getty);
				tt->tt_type = newstring(ty->ty_type);
				tt->tt_status = ty->ty_status |
					TTY_SEEN | TTY_DIFF;
				ttytabend++;
			}
		}
		endttyent();
		/* second pass. offer gettys on previously cleaned entries,
		   and garbage collect "dead" entries */
		for(tt = ttytab; tt < ttytabend; tt++)
			if(tt->tt_status & TTY_SEEN) {
				tt->tt_status &= ~TTY_SEEN;
				if(tt->tt_status & TTY_DIFF) {
					tt->tt_status &= ~TTY_DIFF;
					getty(tt);
				}
			}
			else {
				if(tt->tt_pid > 1)
					kill(tt->tt_pid, 9);
				free(tt->tt_name);
				free(tt->tt_getty);
				free(tt->tt_type);
				pid = tt - ttytab;
				for(tt++; tt < ttytabend; tt++)
					tt[-1] = *tt;
				ttytabend--;
				tt = &ttytab[pid];
			}
	}
	drain = 0;

	/* listen for terminating gettys and sessions, and process them */
	while(1) {
		sigsetmask(mask);
		pid = wait(&status);
		sigblock(sigmask(SIGHUP) | sigmask(SIGTERM));
		if(pid < 0) {
			sleep(5);
			continue;
		}
		for(tt = ttytab; tt < ttytabend; tt++)
			if(pid == tt->tt_pid) {
/* 24 Jul 92*/			if (logout(tt->tt_name)) logwtmp(tt->tt_name,"","");
				if(drain && !(tt->tt_status & TTY_LOGIN)) {
					free(tt->tt_name);
					free(tt->tt_getty);
					free(tt->tt_type);
					for(tt++; tt < ttytabend; tt++)
						tt[-1] = *tt;
					ttytabend--;
				}
				else
					getty(tt);
				break;
			}
	}
}

/* process a getty for a "line". N.B. by having getty do open, init
   is not limited by filedescriptors for number of possible users */
getty(tt)
struct ttytab *tt;
{
	char *sargv[NARG];
	register char *p = arg, **sp = sargv;

	if(!(tt->tt_status & TTY_ON)) {
		tt->tt_pid = -1;
		return;
	}
	if((tt->tt_pid = fork()) < 0)
		fatal("getty fork");
	else if(tt->tt_pid) {
		if(tt->tt_status & TTY_LOGOUT)
			tt->tt_status ^= TTY_LOGIN;
		return;
	}
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	sigsetmask(0);
	strcpy(p, tt->tt_getty);
	while(sp < &sargv[NARG - 2]) {
		while(isspace(*p))
			p++;
		if(!*p)
			break;
		*sp++ = p;
		while(!isspace(*p) && *p)
			p++;
		if(!*p)
			break;
		*p++ = 0;
	}
	strcpy(nam, tt->tt_name);
	*sp++ = nam;
	*sp = 0;
	p = *sargv;
	strcpy(term, "TERM=");
	strcat(term, tt->tt_type);
	execve(p, sargv, env);
bad:
	sleep(30);
	fatal(tt->tt_name);
}

char *
newstring(s)
register char *s;
{
	register char *n;

	if(!(n = malloc(strlen(s) + 1)))
		fatal("out of memory");
	strcpy(n, s);
	return(n);
}

warn(s)
char *s;
{
	register int pid;
	int fd;

	fd = open("/dev/console", 2);
	writes(fd, "init WARNING: ");
	writes(fd, s);
	write(fd, "\n", 1);
	close(fd);
}

fatal(s)
char *s;
{
	login_tty(open("/dev/console", 2));
	writes(2, "init FATAL error: ");
	perror(s);
	_exit(1);				/* 04 Sep 92*/
	/* panic: init died */
}

writes(n, s)
char *s;
{
	write(n, s, strlen(s));
}
