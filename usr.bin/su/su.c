/*
 * Copyright (c) 1988, 1993, 1994
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)su.c	8.3 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/su/su.c,v 1.34 2000/02/24 21:06:21 markm Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <libutil.h>

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#ifdef	SKEY
#include <skey.h>
#endif

#ifdef KERBEROS
#include <openssl/des.h>
#include <krb.h>
#include <netdb.h>

#ifdef LOGIN_CAP
#define	ARGSTR	"-Kflmc:"
#else
#define	ARGSTR	"-Kflm"
#endif

static int kerberos(char *username, char *user, int uid, char *pword);
static int koktologin(char *name, char *toname);

int use_kerberos = 1;
#else /* !KERBEROS */
#ifdef LOGIN_CAP
#define	ARGSTR	"-flmc:"
#else
#define	ARGSTR	"-flm"
#endif
#endif /* KERBEROS */

char   *ontty __P((void));
int	chshell __P((char *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char **environ;
	struct passwd *pwd;
#ifdef WHEELSU
	char *targetpass;
	int iswheelsu;
#endif /* WHEELSU */
	char *p, **g, *user, *shell=NULL, *username, **cleanenv, **nargv, **np;
	struct group *gr;
	uid_t ruid;
	gid_t gid;
	int asme, ch, asthem, fastlogin, prio, i;
	enum { UNSET, YES, NO } iscsh = UNSET;
#ifdef LOGIN_CAP
	login_cap_t *lc;
	char *class=NULL;
	int setwhat;
#endif
#ifdef KERBEROS
	char *k;
#endif
	char shellbuf[MAXPATHLEN];

#ifdef WHEELSU
	iswheelsu =
#endif /* WHEELSU */
	asme = asthem = fastlogin = 0;
	user = "root";
	while((ch = getopt(argc, argv, ARGSTR)) != -1) 
		switch((char)ch) {
#ifdef KERBEROS
		case 'K':
			use_kerberos = 0;
			break;
#endif
		case 'f':
			fastlogin = 1;
			break;
		case '-':
		case 'l':
			asme = 0;
			asthem = 1;
			break;
		case 'm':
			asme = 1;
			asthem = 0;
			break;
#ifdef LOGIN_CAP
		case 'c':
			class = optarg;
			break;
#endif
		case '?':
		default:
			usage();
		}

	if (optind < argc)
		user = argv[optind++];

	if (strlen(user) > MAXLOGNAME - 1) {
		(void)fprintf(stderr, "su: username too long.\n");
		exit(1);
	}
		
	if (user == NULL)
		usage();

	if ((nargv = malloc (sizeof (char *) * (argc + 4))) == NULL) {
	    errx(1, "malloc failure");
	}

	nargv[argc + 3] = NULL;
	for (i = argc; i >= optind; i--)
	    nargv[i + 3] = argv[i];
	np = &nargv[i + 3];

	argv += optind;

#ifdef KERBEROS
	k = auth_getval("auth_list");
	if (k && !strstr(k, "kerberos"))
	    use_kerberos = 0;
#endif
	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
	if (errno)
		prio = 0;
	(void)setpriority(PRIO_PROCESS, 0, -2);
	openlog("su", LOG_CONS, 0);

	/* get current login name and shell */
	ruid = getuid();
	username = getlogin();
	if (username == NULL || (pwd = getpwnam(username)) == NULL ||
	    pwd->pw_uid != ruid)
		pwd = getpwuid(ruid);
	if (pwd == NULL)
		errx(1, "who are you?");
	username = strdup(pwd->pw_name);
	gid = pwd->pw_gid;
	if (username == NULL)
		err(1, NULL);
	if (asme) {
		if (pwd->pw_shell != NULL && *pwd->pw_shell != '\0') {
			/* copy: pwd memory is recycled */
			shell = strncpy(shellbuf,  pwd->pw_shell, sizeof shellbuf);
			shellbuf[sizeof shellbuf - 1] = '\0';
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}

	/* get target login information, default to root */
	if ((pwd = getpwnam(user)) == NULL) {
		errx(1, "unknown login: %s", user);
	}
#ifdef LOGIN_CAP
	if (class==NULL) {
		lc = login_getpwclass(pwd);
	} else {
		if (ruid)
			errx(1, "only root may use -c");
		lc = login_getclass(class);
		if (lc == NULL)
			errx(1, "unknown class: %s", class);
	}
#endif

#ifdef WHEELSU
	targetpass = strdup(pwd->pw_passwd);
#endif /* WHEELSU */

	if (ruid) {
#ifdef KERBEROS
		if (use_kerberos && koktologin(username, user)
		    && !pwd->pw_uid) {
			warnx("kerberos: not in %s's ACL.", user);
			use_kerberos = 0;
		}
#endif
		{
			/*
			 * Only allow those with pw_gid==0 or those listed in
			 * group zero to su to root.  If group zero entry is
			 * missing or empty, then allow anyone to su to root.
			 * iswheelsu will only be set if the user is EXPLICITLY
			 * listed in group zero.
			 */
			if (pwd->pw_uid == 0 && (gr = getgrgid((gid_t)0)) &&
			    gr->gr_mem && *(gr->gr_mem))
				for (g = gr->gr_mem;; ++g) {
					if (!*g) {
						if (gid == 0)
							break;
						else
							errx(1, "you are not in the correct group to su %s.", user);
					}
					if (strcmp(username, *g) == 0) {
#ifdef WHEELSU
						iswheelsu = 1;
#endif /* WHEELSU */
						break;
					}
				}
		}
		/* if target requires a password, verify it */
		if (*pwd->pw_passwd) {
#ifdef	SKEY
#ifdef WHEELSU
			if (iswheelsu) {
				pwd = getpwnam(username);
			}
#endif /* WHEELSU */
			p = skey_getpass("Password:", pwd, 1);
			if (!(!strcmp(pwd->pw_passwd, skey_crypt(p, pwd->pw_passwd, pwd, 1))
#ifdef WHEELSU
			      || (iswheelsu && !strcmp(targetpass, crypt(p,targetpass)))
#endif /* WHEELSU */
			      )) {
#else
			p = getpass("Password:");
			if (strcmp(pwd->pw_passwd, crypt(p, pwd->pw_passwd))) {
#endif
#ifdef KERBEROS
	    			if (!use_kerberos || (use_kerberos && kerberos(username, user, pwd->pw_uid, p)))
#endif
					{
					fprintf(stderr, "Sorry\n");
					syslog(LOG_AUTH|LOG_WARNING, "BAD SU %s to %s%s", username, user, ontty());
					exit(1);
				}
			}
#ifdef WHEELSU
			if (iswheelsu) {
				pwd = getpwnam(user);
			}
#endif /* WHEELSU */
		}
		if (pwd->pw_expire && time(NULL) >= pwd->pw_expire) {
			fprintf(stderr, "Sorry - account expired\n");
			syslog(LOG_AUTH|LOG_WARNING,
				"BAD SU %s to %s%s", username,
				user, ontty());
			exit(1);
		}
	}

	if (asme) {
		/* if asme and non-standard target shell, must be root */
		if (!chshell(pwd->pw_shell) && ruid)
			errx(1, "permission denied (shell).");
	} else if (pwd->pw_shell && *pwd->pw_shell) {
		shell = pwd->pw_shell;
		iscsh = UNSET;
	} else {
		shell = _PATH_BSHELL;
		iscsh = NO;
	}

	/* if we're forking a csh, we want to slightly muck the args */
	if (iscsh == UNSET) {
		p = strrchr(shell, '/');
		if (p)
			++p;
		else
			p = shell;
		if ((iscsh = strcmp(p, "csh") ? NO : YES) == NO)
		    iscsh = strcmp(p, "tcsh") ? NO : YES;
	}

	(void)setpriority(PRIO_PROCESS, 0, prio);

#ifdef LOGIN_CAP
	/* Set everything now except the environment & umask */
	setwhat = LOGIN_SETUSER|LOGIN_SETGROUP|LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
	/*
	 * Don't touch resource/priority settings if -m has been
	 * used or -l and -c hasn't, and we're not su'ing to root.
	 */
        if ((asme || (!asthem && class == NULL)) && pwd->pw_uid)
		setwhat &= ~(LOGIN_SETPRIORITY|LOGIN_SETRESOURCES);
	if (setusercontext(lc, pwd, pwd->pw_uid, setwhat) < 0)
		err(1, "setusercontext");
#else
	/* set permissions */
	if (setgid(pwd->pw_gid) < 0)
		err(1, "setgid");
	if (initgroups(user, pwd->pw_gid))
		errx(1, "initgroups failed");
	if (setuid(pwd->pw_uid) < 0)
		err(1, "setuid");
#endif

	if (!asme) {
		if (asthem) {
			p = getenv("TERM");
#ifdef KERBEROS
			k = getenv("KRBTKFILE");
#endif
			if ((cleanenv = calloc(20, sizeof(char*))) == NULL)
				errx(1, "calloc");
			cleanenv[0] = NULL;
			environ = cleanenv;
#ifdef LOGIN_CAP
			/* set the su'd user's environment & umask */
			setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH|LOGIN_SETUMASK|LOGIN_SETENV);
#else
			(void)setenv("PATH", _PATH_DEFPATH, 1);
#endif
			if (p)
				(void)setenv("TERM", p, 1);
#ifdef KERBEROS
			if (k)
				(void)setenv("KRBTKFILE", k, 1);
#endif
			if (chdir(pwd->pw_dir) < 0)
				errx(1, "no directory");
		}
		if (asthem || pwd->pw_uid)
			(void)setenv("USER", pwd->pw_name, 1);
		(void)setenv("HOME", pwd->pw_dir, 1);
		(void)setenv("SHELL", shell, 1);
	}
	if (iscsh == YES) {
		if (fastlogin)
			*np-- = "-f";
		if (asme)
			*np-- = "-m";
	}

	/* csh strips the first character... */
	*np = asthem ? "-su" : iscsh == YES ? "_su" : "su";

	if (ruid != 0)
		syslog(LOG_NOTICE|LOG_AUTH, "%s to %s%s",
		    username, user, ontty());

	login_close(lc);

	execv(shell, np);
	err(1, "%s", shell);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: su [%s] [login [args]]\n", ARGSTR);
	exit(1);
}

int
chshell(sh)
	char *sh;
{
	int  r = 0;
	char *cp;

	setusershell();
	while (!r && (cp = getusershell()) != NULL)
		r = strcmp(cp, sh) == 0;
	endusershell();
	return r;
}

char *
ontty()
{
	char *p;
	static char buf[MAXPATHLEN + 4];

	buf[0] = 0;
	p = ttyname(STDERR_FILENO);
	if (p)
		snprintf(buf, sizeof(buf), " on %s", p);
	return (buf);
}

#ifdef KERBEROS
int
kerberos(username, user, uid, pword)
	char *username, *user;
	int uid;
	char *pword;
{
	KTEXT_ST ticket;
	AUTH_DAT authdata;
	int kerno;
	u_long faddr;
	char lrealm[REALM_SZ], krbtkfile[MAXPATHLEN];
	char hostname[MAXHOSTNAMELEN], savehost[MAXHOSTNAMELEN];
	char *krb_get_phost();
	struct hostent *hp;

	if (krb_get_lrealm(lrealm, 1) != KSUCCESS)
		return (1);
	(void)sprintf(krbtkfile, "%s_%s_%lu", TKT_ROOT, user,
	    (unsigned long)getuid());

	(void)setenv("KRBTKFILE", krbtkfile, 1);
	(void)krb_set_tkt_string(krbtkfile);
	/*
	 * Set real as well as effective ID to 0 for the moment,
	 * to make the kerberos library do the right thing.
	 */
	if (setuid(0) < 0) {
		warn("setuid");
		return (1);
	}

	/*
	 * Little trick here -- if we are su'ing to root,
	 * we need to get a ticket for "xxx.root", where xxx represents
	 * the name of the person su'ing.  Otherwise (non-root case),
	 * we need to get a ticket for "yyy.", where yyy represents
	 * the name of the person being su'd to, and the instance is null
	 *
	 * We should have a way to set the ticket lifetime,
	 * with a system default for root.
	 */
	kerno = krb_get_pw_in_tkt((uid == 0 ? username : user),
		(uid == 0 ? "root" : ""), lrealm,
	    	"krbtgt", lrealm, DEFAULT_TKT_LIFE, pword);

	if (kerno != KSUCCESS) {
		if (kerno == KDC_PR_UNKNOWN) {
			warnx("kerberos: principal unknown: %s.%s@%s",
				(uid == 0 ? username : user),
				(uid == 0 ? "root" : ""), lrealm);
			return (1);
		}
		warnx("kerberos: unable to su: %s", krb_err_txt[kerno]);
		syslog(LOG_NOTICE|LOG_AUTH,
		    "BAD Kerberos SU: %s to %s%s: %s",
		    username, user, ontty(), krb_err_txt[kerno]);
		return (1);
	}

	if (chown(krbtkfile, uid, -1) < 0) {
		warn("chown");
		(void)unlink(krbtkfile);
		return (1);
	}

	(void)setpriority(PRIO_PROCESS, 0, -2);

	if (gethostname(hostname, sizeof(hostname)) == -1) {
		warn("gethostname");
		dest_tkt();
		return (1);
	}

	(void)strncpy(savehost, krb_get_phost(hostname), sizeof(savehost));
	savehost[sizeof(savehost) - 1] = '\0';

	kerno = krb_mk_req(&ticket, "rcmd", savehost, lrealm, 33);

	if (kerno == KDC_PR_UNKNOWN) {
		warnx("Warning: TGT not verified.");
		syslog(LOG_NOTICE|LOG_AUTH,
		    "%s to %s%s, TGT not verified (%s); %s.%s not registered?",
		    username, user, ontty(), krb_err_txt[kerno],
		    "rcmd", savehost);
	} else if (kerno != KSUCCESS) {
		warnx("Unable to use TGT: %s", krb_err_txt[kerno]);
		syslog(LOG_NOTICE|LOG_AUTH, "failed su: %s to %s%s: %s",
		    username, user, ontty(), krb_err_txt[kerno]);
		dest_tkt();
		return (1);
	} else {
		if (!(hp = gethostbyname(hostname))) {
			warnx("can't get addr of %s", hostname);
			dest_tkt();
			return (1);
		}
		memmove((char *)&faddr, (char *)hp->h_addr, sizeof(faddr));

		if ((kerno = krb_rd_req(&ticket, "rcmd", savehost, faddr,
		    &authdata, "")) != KSUCCESS) {
			warnx("kerberos: unable to verify rcmd ticket: %s\n",
			    krb_err_txt[kerno]);
			syslog(LOG_NOTICE|LOG_AUTH,
			    "failed su: %s to %s%s: %s", username,
			     user, ontty(), krb_err_txt[kerno]);
			dest_tkt();
			return (1);
		}
	}
	return (0);
}

int
koktologin(name, toname)
	char *name, *toname;
{
	AUTH_DAT *kdata;
	AUTH_DAT kdata_st;
	char realm[REALM_SZ];

	if (krb_get_lrealm(realm, 1) != KSUCCESS)
		return (1);
	kdata = &kdata_st;
	memset((char *)kdata, 0, sizeof(*kdata));
	(void)strncpy(kdata->pname, name, sizeof kdata->pname - 1);
	(void)strncpy(kdata->pinst,
	    ((strcmp(toname, "root") == 0) ? "root" : ""), sizeof kdata->pinst - 1);
	(void)strncpy(kdata->prealm, realm, sizeof kdata->prealm - 1);
	return (kuserok(kdata, toname));
}
#endif
