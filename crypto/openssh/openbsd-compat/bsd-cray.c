/* 
 * $Id: bsd-cray.c,v 1.6 2002/05/15 16:39:51 mouring Exp $
 *
 * bsd-cray.c
 *
 * Copyright (c) 2002, Cray Inc.  (Wendy Palm <wendyp@cray.com>)
 * Significant portions provided by 
 *          Wayne Schroeder, SDSC <schroeder@sdsc.edu>
 *          William Jones, UTexas <jones@tacc.utexas.edu>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Created: Apr 22 16.34:00 2002 wp
 *
 * This file contains functions required for proper execution
 * on UNICOS systems.
 *
 */

#ifdef _CRAY
#include <udb.h>
#include <tmpdir.h>
#include <unistd.h>
#include <sys/category.h>
#include <utmp.h>
#include <sys/jtab.h>
#include <signal.h>
#include <sys/priv.h>
#include <sys/secparm.h>
#include <sys/usrv.h>
#include <sys/sysv.h>
#include <sys/sectab.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>

#include "bsd-cray.h"

char cray_tmpdir[TPATHSIZ+1];		    /* job TMPDIR path */

/*
 * Functions.
 */
void cray_retain_utmp(struct utmp *, int);
void cray_delete_tmpdir(char *, int, uid_t);
void cray_init_job(struct passwd *);
void cray_set_tmpdir(struct utmp *);


/*
 * Orignal written by:
 *     Wayne Schroeder
 *     San Diego Supercomputer Center
 *     schroeder@sdsc.edu
*/
void
cray_setup(uid_t uid, char *username)
{
	struct udb *p;
	extern char *setlimits();
	int i, j;
	int accts[MAXVIDS];
	int naccts;
	int err;
	char *sr;
	int pid;
	struct jtab jbuf;
	int jid;

	if ((jid = getjtab(&jbuf)) < 0)
		fatal("getjtab: no jid");

	err = setudb();	   /* open and rewind the Cray User DataBase */
	if (err != 0)
		fatal("UDB open failure");
	naccts = 0;
	p = getudbnam(username);
	if (p == NULL)
		fatal("No UDB entry for %.100s", username);
	if (uid != p->ue_uid)
		fatal("UDB entry %.100s uid(%d) does not match uid %d",
		    username, (int) p->ue_uid, (int) uid);
	for (j = 0; p->ue_acids[j] != -1 && j < MAXVIDS; j++) {
		accts[naccts] = p->ue_acids[j];
		naccts++;
	}
	endudb();	 /* close the udb */

	if (naccts != 0) {
		/* Perhaps someday we'll prompt users who have multiple accounts
		   to let them pick one (like CRI's login does), but for now just set
		   the account to the first entry. */
		if (acctid(0, accts[0]) < 0)
			fatal("System call acctid failed, accts[0]=%d", accts[0]);
	}

	/* Now set limits, including CPU time for the (interactive) job and process,
	   and set up permissions (for chown etc), etc.	 This is via an internal CRI
	   routine, setlimits, used by CRI's login. */

	pid = getpid();
	sr = setlimits(username, C_PROC, pid, UDBRC_INTER);
	if (sr != NULL)
		fatal("%.200s", sr);

	sr = setlimits(username, C_JOB, jid, UDBRC_INTER);
	if (sr != NULL)
		fatal("%.200s", sr);

}

/*
 * The rc.* and /etc/sdaemon methods of starting a program on unicos/unicosmk
 * can have pal privileges that sshd can inherit which
 * could allow a user to su to root with out a password.
 * This subroutine clears all privileges.
 */
void
drop_cray_privs()
{
#if defined(_SC_CRAY_PRIV_SU)
	priv_proc_t*		  privstate;
	int			  result;
	extern	    int		  priv_set_proc();
	extern	    priv_proc_t*  priv_init_proc();
	struct	    usrv	  usrv;

	/*
	 * If ether of theses two flags are not set
	 * then don't allow this version of ssh to run.
	 */
	if (!sysconf(_SC_CRAY_PRIV_SU))
		fatal("Not PRIV_SU system.");
	if (!sysconf(_SC_CRAY_POSIX_PRIV))
		fatal("Not POSIX_PRIV.");

	debug("Dropping privileges.");

	memset(&usrv, 0, sizeof(usrv));
	if (setusrv(&usrv) < 0)
		fatal("%s(%d): setusrv(): %s", __FILE__, __LINE__,
		    strerror(errno));

	if ((privstate = priv_init_proc()) != NULL) {
		result = priv_set_proc(privstate);
		if (result != 0 )
			fatal("%s(%d): priv_set_proc(): %s",
			    __FILE__, __LINE__, strerror(errno));
		priv_free_proc(privstate);
	}
	debug ("Privileges should be cleared...");
#else
	/* XXX: do this differently */
#	error Cray systems must be run with _SC_CRAY_PRIV_SU on!
#endif
}


/*
 *  Retain utmp/wtmp information - used by cray accounting.
 */
void
cray_retain_utmp(struct utmp *ut, int pid)
{
	int fd;
	struct utmp utmp;

	if ((fd = open(UTMP_FILE, O_RDONLY)) != -1) {
		while (read(fd, (char *)&utmp, sizeof(utmp)) == sizeof(utmp)) {
			if (pid == utmp.ut_pid) {
				ut->ut_jid = utmp.ut_jid;
				/* XXX: MIN_SIZEOF here? can this go in loginrec? */
				strncpy(ut->ut_tpath, utmp.ut_tpath, sizeof(utmp.ut_tpath));
				strncpy(ut->ut_host, utmp.ut_host, sizeof(utmp.ut_host));
				strncpy(ut->ut_name, utmp.ut_name, sizeof(utmp.ut_name));
				break;
			}
		}
		close(fd);
	}
	/* XXX: error message? */
}

/*
 * tmpdir support.
 */

/*
 * find and delete jobs tmpdir.
 */
void
cray_delete_tmpdir(char *login, int jid, uid_t uid)
{
	int child;
	static char jtmp[TPATHSIZ];
	struct stat statbuf;
	int c;
	int wstat;

	for (c = 'a'; c <= 'z'; c++) {
		snprintf(jtmp, TPATHSIZ, "%s/jtmp.%06d%c", JTMPDIR, jid, c);
		if (stat(jtmp, &statbuf) == 0 && statbuf.st_uid == uid)
			break;
	}

	if (c > 'z')
		return;

	if ((child = fork()) == 0) {
		execl(CLEANTMPCMD, CLEANTMPCMD, login, jtmp, (char *)NULL);
		fatal("cray_delete_tmpdir: execl of CLEANTMPCMD failed");
	}

	while (waitpid(child, &wstat, 0) == -1 && errno == EINTR)
		;
}

/*
 * Remove tmpdir on job termination.
 */
void
cray_job_termination_handler(int sig)
{
	int jid;
	char *login = NULL;
	struct jtab jtab;

	debug("Received SIG JOB.");

	if ((jid = waitjob(&jtab)) == -1 ||
	    (login = uid2nam(jtab.j_uid)) == NULL)
		return;

	cray_delete_tmpdir(login, jid, jtab.j_uid);
}

/*
 * Set job id and create tmpdir directory.
 */
void
cray_init_job(struct passwd *pw)
{
	int jid;
	int c;

	jid = setjob(pw->pw_uid, WJSIGNAL);
	if (jid < 0)
		fatal("System call setjob failure");

	for (c = 'a'; c <= 'z'; c++) {
		snprintf(cray_tmpdir, TPATHSIZ, "%s/jtmp.%06d%c", JTMPDIR, jid, c);
		if (mkdir(cray_tmpdir, JTMPMODE) != 0)
			continue;
		if (chown(cray_tmpdir,	pw->pw_uid, pw->pw_gid) != 0) {
			rmdir(cray_tmpdir);
			continue;
		}
		break;
	}

	if (c > 'z')
		cray_tmpdir[0] = '\0';
}

void
cray_set_tmpdir(struct utmp *ut)
{
	int jid;
	struct jtab jbuf;

	if ((jid = getjtab(&jbuf)) < 0)
		return;

	/*
	 * Set jid and tmpdir in utmp record.
	 */
	ut->ut_jid = jid;
	strncpy(ut->ut_tpath, cray_tmpdir, TPATHSIZ);
}
#endif
