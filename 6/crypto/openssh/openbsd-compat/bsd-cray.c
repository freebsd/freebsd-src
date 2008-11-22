/* 
 * $Id: bsd-cray.c,v 1.16 2006/09/01 05:38:41 djm Exp $
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
#ifdef _UNICOS

#include <udb.h>
#include <tmpdir.h>
#include <unistd.h>
#include <sys/category.h>
#include <utmp.h>
#include <sys/jtab.h>
#include <signal.h>
#include <sys/priv.h>
#include <sys/secparm.h>
#include <sys/tfm.h>
#include <sys/usrv.h>
#include <sys/sysv.h>
#include <sys/sectab.h>
#include <sys/secstat.h>
#include <sys/stat.h>
#include <sys/session.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <ia.h>
#include <urm.h>
#include "ssh.h"

#include "includes.h"
#include "sys/types.h"

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
# define      _SS_MAXSIZE     128     /* Implementation specific max size */
# define       _SS_PADSIZE     (_SS_MAXSIZE - sizeof (struct sockaddr))

# define ss_family ss_sa.sa_family
#endif /* !HAVE_STRUCT_SOCKADDR_STORAGE */

#ifndef IN6_IS_ADDR_LOOPBACK
# define IN6_IS_ADDR_LOOPBACK(a) \
	(((u_int32_t *) (a))[0] == 0 && ((u_int32_t *) (a))[1] == 0 && \
	 ((u_int32_t *) (a))[2] == 0 && ((u_int32_t *) (a))[3] == htonl (1))
#endif /* !IN6_IS_ADDR_LOOPBACK */

#ifndef AF_INET6
/* Define it to something that should never appear */
#define AF_INET6 AF_MAX
#endif

#include "log.h"
#include "servconf.h"
#include "bsd-cray.h"

#define MAXACID 80

extern ServerOptions options;

char cray_tmpdir[TPATHSIZ + 1];		    /* job TMPDIR path */

struct sysv sysv;	/* system security structure */
struct usrv usrv;	/* user security structure */

/*
 * Functions.
 */
void cray_retain_utmp(struct utmp *, int);
void cray_delete_tmpdir(char *, int, uid_t);
void cray_init_job(struct passwd *);
void cray_set_tmpdir(struct utmp *);
void cray_login_failure(char *, int);
int cray_setup(uid_t, char *, const char *);
int cray_access_denied(char *);

void
cray_login_failure(char *username, int errcode)
{
	struct udb *ueptr;		/* UDB pointer for username */
	ia_failure_t fsent;		/* ia_failure structure */
	ia_failure_ret_t fret;		/* ia_failure return stuff */
	struct jtab jtab;		/* job table structure */
	int jid = 0;			/* job id */

	if ((jid = getjtab(&jtab)) < 0)
		debug("cray_login_failure(): getjtab error");

	getsysudb();
	if ((ueptr = getudbnam(username)) == UDB_NULL)
		debug("cray_login_failure(): getudbname() returned NULL");
	endudb();

	memset(&fsent, '\0', sizeof(fsent));
	fsent.revision = 0;
	fsent.uname = username;
	fsent.host = (char *)get_canonical_hostname(options.use_dns);
	fsent.ttyn = "sshd";
	fsent.caller = IA_SSHD;
	fsent.flags = IA_INTERACTIVE;
	fsent.ueptr = ueptr;
	fsent.jid = jid;
	fsent.errcode = errcode;
	fsent.pwdp = NULL;
	fsent.exitcode = 0;	/* dont exit in ia_failure() */

	fret.revision = 0;
	fret.normal = 0;

	/*
	 * Call ia_failure because of an login failure.
	 */
	ia_failure(&fsent, &fret);
}

/*
 *  Cray access denied
 */
int
cray_access_denied(char *username)
{
	struct udb *ueptr;		/* UDB pointer for username */
	int errcode;			/* IA errorcode */

	errcode = 0;
	getsysudb();
	if ((ueptr = getudbnam(username)) == UDB_NULL)
		debug("cray_login_failure(): getudbname() returned NULL");
	endudb();

	if (ueptr != NULL && ueptr->ue_disabled)
		errcode = IA_DISABLED;
	if (errcode)
		cray_login_failure(username, errcode);

	return (errcode);
}

/*
 * record_failed_login: generic "login failed" interface function
 */
void
record_failed_login(const char *user, const char *hostname, const char *ttyname)
{
	cray_login_failure((char *)user, IA_UDBERR);
}

int
cray_setup (uid_t uid, char *username, const char *command)
{
	extern struct udb *getudb();
	extern char *setlimits();

	int err;			/* error return */
	time_t system_time;		/* current system clock */
	time_t expiration_time;		/* password expiration time */
	int maxattempts;		/* maximum no. of failed login attempts */
	int SecureSys;			/* unicos security flag */
	int minslevel = 0;		/* system minimum security level */
	int i, j;
	int valid_acct = -1;		/* flag for reading valid acct */
	char acct_name[MAXACID] = { "" }; /* used to read acct name */
	struct jtab jtab;		/* Job table struct */
	struct udb ue;			/* udb entry for logging-in user */
	struct udb *up;			/* pointer to UDB entry */
	struct secstat secinfo;		/* file  security attributes */
	struct servprov init_info;	/* used for sesscntl() call */
	int jid;			/* job ID */
	int pid;			/* process ID */
	char *sr;			/* status return from setlimits() */
	char *ttyn = NULL;		/* ttyname or command name*/
	char hostname[MAXHOSTNAMELEN];
	/* passwd stuff for ia_user */
	passwd_t pwdacm, pwddialup, pwdudb, pwdwal, pwddce;
	ia_user_ret_t uret;		/* stuff returned from ia_user */
	ia_user_t usent;		/* ia_user main structure */
	int ia_rcode;			/* ia_user return code */
	ia_failure_t fsent;		/* ia_failure structure */
	ia_failure_ret_t fret;		/* ia_failure return stuff */
	ia_success_t ssent;		/* ia_success structure */
	ia_success_ret_t sret;		/* ia_success return stuff */
	int ia_mlsrcode;		/* ia_mlsuser return code */
	int secstatrc;			/* [f]secstat return code */

	if (SecureSys = (int)sysconf(_SC_CRAY_SECURE_SYS)) {
		getsysv(&sysv, sizeof(struct sysv));
		minslevel = sysv.sy_minlvl;
		if (getusrv(&usrv) < 0)
			fatal("getusrv() failed, errno = %d", errno);
	}
	hostname[0] = '\0';
	strlcpy(hostname,
	   (char *)get_canonical_hostname(options.use_dns),
	   MAXHOSTNAMELEN);
	/*
	 *  Fetch user's UDB entry.
	 */
	getsysudb();
	if ((up = getudbnam(username)) == UDB_NULL)
		fatal("cannot fetch user's UDB entry");

	/*
	 *  Prevent any possible fudging so perform a data
	 *  safety check and compare the supplied uid against
	 *  the udb's uid.
	 */
	if (up->ue_uid != uid)
		fatal("IA uid missmatch");
	endudb();

	if ((jid = getjtab(&jtab)) < 0) {
		debug("getjtab");
		return(-1);
	}
	pid = getpid();
	ttyn = ttyname(0);
	if (SecureSys) {
		if (ttyn != NULL)
			secstatrc = secstat(ttyn, &secinfo);
		else
			secstatrc = fsecstat(1, &secinfo);

		if (secstatrc == 0)
			debug("[f]secstat() successful");
		else
			fatal("[f]secstat() error, rc = %d", secstatrc);
	}
	if ((ttyn == NULL) && ((char *)command != NULL))
		ttyn = (char *)command;
	/*
	 *  Initialize all structures to call ia_user
	 */
	usent.revision = 0;
	usent.uname = username;
	usent.host = hostname;
	usent.ttyn = ttyn;
	usent.caller = IA_SSHD; 
	usent.pswdlist = &pwdacm;
	usent.ueptr = &ue;
	usent.flags = IA_INTERACTIVE | IA_FFLAG;
	pwdacm.atype = IA_SECURID;
	pwdacm.pwdp = NULL;
	pwdacm.next = &pwdudb;

	pwdudb.atype = IA_UDB;
	pwdudb.pwdp = NULL;
	pwdudb.next = &pwddce;

	pwddce.atype = IA_DCE;
	pwddce.pwdp = NULL;
	pwddce.next = &pwddialup;

	pwddialup.atype = IA_DIALUP;
	pwddialup.pwdp = NULL;
	/* pwddialup.next = &pwdwal; */
	pwddialup.next = NULL;

	pwdwal.atype = IA_WAL;
	pwdwal.pwdp = NULL;
	pwdwal.next = NULL;

	uret.revision = 0;
	uret.pswd = NULL;
	uret.normal = 0;

	ia_rcode = ia_user(&usent, &uret);
	switch (ia_rcode) {
	/*
	 *  These are acceptable return codes from ia_user()
	 */
	case IA_UDBWEEK:        /* Password Expires in 1 week */
		expiration_time = ue.ue_pwage.time + ue.ue_pwage.maxage;
		printf ("WARNING - your current password will expire %s\n",
		ctime((const time_t *)&expiration_time));
		break;
	case IA_UDBEXPIRED:
		if (ttyname(0) != NULL) {
			/* Force a password change */
			printf("Your password has expired; Choose a new one.\n");
			execl("/bin/passwd", "passwd", username, 0);
			exit(9);
			}
		break;
	case IA_NORMAL:         /* Normal Return Code */
		break;
	case IA_BACKDOOR:
		/* XXX: can we memset it to zero here so save some of this */
		strlcpy(ue.ue_name, "root", sizeof(ue.ue_name));
		strlcpy(ue.ue_dir, "/", sizeof(ue.ue_dir));
		strlcpy(ue.ue_shell, "/bin/sh", sizeof(ue.ue_shell));

		ue.ue_passwd[0] = '\0';
		ue.ue_age[0] = '\0';
		ue.ue_comment[0] = '\0';
		ue.ue_loghost[0] = '\0';
		ue.ue_logline[0] = '\0';

		ue.ue_uid = -1;
		ue.ue_nice[UDBRC_INTER] = 0;

		for (i = 0; i < MAXVIDS; i++)
			ue.ue_gids[i] = 0;

		ue.ue_logfails = 0;
		ue.ue_minlvl = ue.ue_maxlvl = ue.ue_deflvl = minslevel;
		ue.ue_defcomps = 0;
		ue.ue_comparts = 0;
		ue.ue_permits = 0;
		ue.ue_trap = 0;
		ue.ue_disabled = 0;
		ue.ue_logtime = 0;
		break;
	case IA_CONSOLE:        /* Superuser not from Console */
	case IA_TRUSTED:	/* Trusted user */
		if (options.permit_root_login > PERMIT_NO)
			break;	/* Accept root login */
	default:
	/*
	 *  These are failed return codes from ia_user()
	 */
		switch (ia_rcode) 
		{
		case IA_BADAUTH:
			printf("Bad authorization, access denied.\n");
			break;
		case IA_DISABLED:
			printf("Your login has been disabled. Contact the system ");
			printf("administrator for assistance.\n");
			break;
		case IA_GETSYSV:
			printf("getsysv() failed - errno = %d\n", errno);
			break;
		case IA_MAXLOGS:
			printf("Maximum number of failed login attempts exceeded.\n");
			printf("Access denied.\n");
			break;
		case IA_UDBPWDNULL:
			if (SecureSys)
				printf("NULL Password not allowed on MLS systems.\n");
			break;
		default:
			break;
		}

		/*
		 *  Authentication failed.
		 */
		printf("sshd: Login incorrect, (0%o)\n",
		    ia_rcode-IA_ERRORCODE);

		/*
		 *  Initialize structure for ia_failure
		 *  which will exit.
		 */
		fsent.revision = 0;
		fsent.uname = username;
		fsent.host = hostname;
		fsent.ttyn = ttyn;
		fsent.caller = IA_SSHD;
		fsent.flags = IA_INTERACTIVE;
		fsent.ueptr = &ue;
		fsent.jid = jid;
		fsent.errcode = ia_rcode;
		fsent.pwdp = uret.pswd;
		fsent.exitcode = 1;

		fret.revision = 0;
		fret.normal = 0;

		/*
		*  Call ia_failure because of an IA failure.
		*  There is no return because ia_failure exits.
		*/
		ia_failure(&fsent, &fret);

		exit(1); 
	}

	ia_mlsrcode = IA_NORMAL;
	if (SecureSys) {
		debug("calling ia_mlsuser()");
		ia_mlsrcode = ia_mlsuser(&ue, &secinfo, &usrv, NULL, 0);
	}
	if (ia_mlsrcode != IA_NORMAL) {
		printf("sshd: Login incorrect, (0%o)\n",
		    ia_mlsrcode-IA_ERRORCODE);
		/*
		 *  Initialize structure for ia_failure
		 *  which will exit.
		 */
		fsent.revision = 0;
		fsent.uname = username;
		fsent.host = hostname;
		fsent.ttyn = ttyn;
		fsent.caller = IA_SSHD;
		fsent.flags = IA_INTERACTIVE;
		fsent.ueptr = &ue;
		fsent.jid  = jid;
		fsent.errcode = ia_mlsrcode;
		fsent.pwdp = uret.pswd;
		fsent.exitcode = 1;
		fret.revision = 0;
		fret.normal = 0;

		/*
		 *  Call ia_failure because of an IA failure.
		 *  There is no return because ia_failure exits.
		 */
		ia_failure(&fsent,&fret);
		exit(1); 
	}

	/* Provide login status information */
	if (options.print_lastlog && ue.ue_logtime != 0) {
		printf("Last successful login was : %.*s ", 19,
		    (char *)ctime(&ue.ue_logtime));

		if (*ue.ue_loghost != '\0') {
			printf("from %.*s\n", sizeof(ue.ue_loghost),
			    ue.ue_loghost);
		} else {
			printf("on %.*s\n", sizeof(ue.ue_logline),
			    ue.ue_logline);
		}

		if (SecureSys && (ue.ue_logfails != 0)) {
			printf("  followed by %d failed attempts\n",
			    ue.ue_logfails);
		}
	}

	/*
	 * Call ia_success to process successful I/A.
	 */
	ssent.revision = 0;
	ssent.uname = username;
	ssent.host = hostname;
	ssent.ttyn = ttyn;
	ssent.caller = IA_SSHD;
	ssent.flags = IA_INTERACTIVE;
	ssent.ueptr = &ue;
	ssent.jid = jid;
	ssent.errcode = ia_rcode;
	ssent.us = NULL;
	ssent.time = 1;	/* Set ue_logtime */

	sret.revision = 0;
	sret.normal = 0;

	ia_success(&ssent, &sret);

	/*
	 * Query for account, iff > 1 valid acid & askacid permbit
	 */
	if (((ue.ue_permbits & PERMBITS_ACCTID) ||
	    (ue.ue_acids[0] >= 0) && (ue.ue_acids[1] >= 0)) &&
	    ue.ue_permbits & PERMBITS_ASKACID) {
		if (ttyname(0) != NULL) {
			debug("cray_setup: ttyname true case, %.100s", ttyname);
			while (valid_acct == -1) {
				printf("Account (? for available accounts)"
				    " [%s]: ", acid2nam(ue.ue_acids[0]));
				fgets(acct_name, MAXACID, stdin);
				switch (acct_name[0]) {
				case EOF:
					exit(0);
					break;
				case '\0':
					valid_acct = ue.ue_acids[0];
					strlcpy(acct_name, acid2nam(valid_acct), MAXACID);
					break;
				case '?':
					/* Print the list 3 wide */
					for (i = 0, j = 0; i < MAXVIDS; i++) {
						if (ue.ue_acids[i] == -1) {
							printf("\n");
							break;
						}
						if (++j == 4) {
							j = 1;
							printf("\n");
						}
						printf(" %s",
						    acid2nam(ue.ue_acids[i]));
					}
					if (ue.ue_permbits & PERMBITS_ACCTID) {
						printf("\"acctid\" permbit also allows"
						    " you to select any valid "
						    "account name.\n");
					}
					printf("\n");
					break;
				default:
					valid_acct = nam2acid(acct_name);
					if (valid_acct == -1) 
						printf(
						    "Account id not found for"
						    " account name \"%s\"\n\n",
						    acct_name);
					break;
				}
				/*
				 * If an account was given, search the user's
				 * acids array to verify they can use this account.
				 */
				if ((valid_acct != -1) &&
				    !(ue.ue_permbits & PERMBITS_ACCTID)) {
					for (i = 0; i < MAXVIDS; i++) {
						if (ue.ue_acids[i] == -1)
							break;
						if (valid_acct == ue.ue_acids[i])
							break;
					}
					if (i == MAXVIDS ||
					    ue.ue_acids[i] == -1) {
						fprintf(stderr, "Cannot set"
						    " account name to "
						    "\"%s\", permission "
						    "denied\n\n", acct_name);
						valid_acct = -1;
					}
				}
			}
		} else {
			/*
			 * The client isn't connected to a terminal and can't
			 * respond to an acid prompt.  Use default acid.
			 */
			debug("cray_setup: ttyname false case, %.100s",
			    ttyname);
			valid_acct = ue.ue_acids[0];
		}
	} else {
		/*
		 * The user doesn't have the askacid permbit set or
		 * only has one valid account to use.
		 */
		valid_acct = ue.ue_acids[0];
	}
	if (acctid(0, valid_acct) < 0) {
		printf ("Bad account id: %d\n", valid_acct);
		exit(1);
	}

	/* 
	 * Now set shares, quotas, limits, including CPU time for the 
	 * (interactive) job and process, and set up permissions 
	 * (for chown etc), etc.
	 */
	if (setshares(ue.ue_uid, valid_acct, printf, 0, 0)) {
		printf("Unable to give %d shares to <%s>(%d/%d)\n",
		    ue.ue_shares, ue.ue_name, ue.ue_uid, valid_acct);
		exit(1);
	}

	sr = setlimits(username, C_PROC, pid, UDBRC_INTER);
	if (sr != NULL) {
		debug("%.200s", sr);
		exit(1);
	}
	sr = setlimits(username, C_JOB, jid, UDBRC_INTER);
	if (sr != NULL) {
		debug("%.200s", sr);
		exit(1);
	}
	/*
	 * Place the service provider information into
	 * the session table (Unicos) or job table (Unicos/mk).
	 * There exist double defines for the job/session table in
	 * unicos/mk (jtab.h) so no need for a compile time switch.
	 */
	memset(&init_info, '\0', sizeof(init_info));
	init_info.s_sessinit.si_id = URM_SPT_LOGIN;
	init_info.s_sessinit.si_pid = getpid();
	init_info.s_sessinit.si_sid = jid;
	sesscntl(0, S_SETSERVPO, (int)&init_info);

	/*
	 * Set user and controlling tty security attributes.
	 */
	if (SecureSys) {
		if (setusrv(&usrv) == -1) {
			debug("setusrv() failed, errno = %d",errno);
			exit(1);
		}
	}

	return (0);
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
	priv_proc_t *privstate;
	int result;
	extern int priv_set_proc();
	extern priv_proc_t *priv_init_proc();

	/*
	 * If ether of theses two flags are not set
	 * then don't allow this version of ssh to run.
	 */
	if (!sysconf(_SC_CRAY_PRIV_SU))
		fatal("Not PRIV_SU system.");
	if (!sysconf(_SC_CRAY_POSIX_PRIV))
		fatal("Not POSIX_PRIV.");

	debug("Setting MLS labels.");;

	if (sysconf(_SC_CRAY_SECURE_MAC)) {
		usrv.sv_minlvl = SYSLOW;
		usrv.sv_actlvl = SYSHIGH;
		usrv.sv_maxlvl = SYSHIGH;
	} else {
		usrv.sv_minlvl = sysv.sy_minlvl;
		usrv.sv_actlvl = sysv.sy_minlvl;
		usrv.sv_maxlvl = sysv.sy_maxlvl;
	}       
	usrv.sv_actcmp = 0;
	usrv.sv_valcmp = sysv.sy_valcmp;

	usrv.sv_intcat = TFM_SYSTEM;
	usrv.sv_valcat |= (TFM_SYSTEM | TFM_SYSFILE);

	if (setusrv(&usrv) < 0) {
		fatal("%s(%d): setusrv(): %s", __FILE__, __LINE__,
		    strerror(errno));
	}

	if ((privstate = priv_init_proc()) != NULL) {
		result = priv_set_proc(privstate);
		if (result != 0 ) {
			fatal("%s(%d): priv_set_proc(): %s",
			    __FILE__, __LINE__, strerror(errno));
		}
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
		/* XXX use atomicio */
		while (read(fd, (char *)&utmp, sizeof(utmp)) == sizeof(utmp)) {
			if (pid == utmp.ut_pid) {
				ut->ut_jid = utmp.ut_jid;
				strncpy(ut->ut_tpath, utmp.ut_tpath, sizeof(utmp.ut_tpath));
				strncpy(ut->ut_host, utmp.ut_host, sizeof(utmp.ut_host));
				strncpy(ut->ut_name, utmp.ut_name, sizeof(utmp.ut_name));
				break;
			}
		}
		close(fd);
	} else
		fatal("Unable to open utmp file");
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
	static char jtmp[TPATHSIZ];
	struct stat statbuf;
	int child, c, wstat;

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

	debug("received signal %d",sig);

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
#endif /* UNICOS */

#ifdef _UNICOSMP
#include <pwd.h>
/*
 * Set job id and create tmpdir directory.
 */
void
cray_init_job(struct passwd *pw)
{
	initrm_silent(pw->pw_uid);
	return;
}
#endif /* _UNICOSMP */
