/*
 * Copyright (c) 1988 The Regents of the University of California.
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

#include "bsd_locl.h"

RCSID ("$Id: su.c,v 1.59 1997/05/26 17:45:54 bg Exp $");

#ifdef SYSV_SHADOW
#include "sysv_shadow.h"
#endif

static int kerberos (char *username, char *user, int uid);
static int chshell (char *sh);
static char *ontty (void);
static int koktologin (char *name, char *realm, char *toname);
static int chshell (char *sh);

/* Handle '-' option after all the getopt options */
#define	ARGSTR	"Kflmi:"

static int use_kerberos = 1;
static char *root_inst = "root";

int
main (int argc, char **argv)
{
    struct passwd *pwd;
    char *p, **g;
    struct group *gr;
    uid_t ruid;
    int asme, ch, asthem, fastlogin, prio;
    enum { UNSET, YES, NO } iscsh = UNSET;
    char *user, *shell, *avshell, *username, **np;
    char shellbuf[MaxPathLen], avshellbuf[MaxPathLen];

    set_progname (argv[0]);

    asme = asthem = fastlogin = 0;
    while ((ch = getopt (argc, argv, ARGSTR)) != EOF)
	switch ((char) ch) {
	case 'K':
	    use_kerberos = 0;
	    break;
	case 'f':
	    fastlogin = 1;
	    break;
	case 'l':
	    asme = 0;
	    asthem = 1;
	    break;
	case 'm':
	    asme = 1;
	    asthem = 0;
	    break;
	case 'i':
	    root_inst = optarg;
	    break;
	case '?':
	default:
	    fprintf (stderr,
		     "usage: su [-Kflm] [-i root-instance] [-] [login]\n");
	    exit (1);
	}
    /* Don't handle '-' option with getopt */
    if (optind < argc && strcmp (argv[optind], "-") == 0) {
	asme = 0;
	asthem = 1;
	optind++;
    }
    argv += optind;

    if (use_kerberos) {
	int fd = open (KEYFILE, O_RDONLY);

	if (fd >= 0)
	    close (fd);
	else
	    use_kerberos = 0;
    }
    errno = 0;
    prio = getpriority (PRIO_PROCESS, 0);
    if (errno)
	prio = 0;
    setpriority (PRIO_PROCESS, 0, -2);
    openlog ("su", LOG_CONS, 0);

    /* get current login name and shell */
    ruid = getuid ();
    username = getlogin ();
    if (username == NULL || (pwd = k_getpwnam (username)) == NULL ||
	pwd->pw_uid != ruid)
	pwd = k_getpwuid (ruid);
    if (pwd == NULL)
	errx (1, "who are you?");
    username = strdup (pwd->pw_name);
    if (asme)
	if (pwd->pw_shell && *pwd->pw_shell)
	    shell = strcpy (shellbuf, pwd->pw_shell);
	else {
	    shell = _PATH_BSHELL;
	    iscsh = NO;
	}

    /* get target login information, default to root */
    user = *argv ? *argv : "root";
    np = *argv ? argv : argv - 1;

    pwd = k_getpwnam (user);
    if (pwd == NULL)
	errx (1, "unknown login %s", user);
    if (pwd->pw_uid == 0 && strcmp ("root", user) != 0) {
	syslog (LOG_ALERT, "NIS attack, user %s has uid 0", user);
	errx (1, "unknown login %s", user);
    }
    if (!use_kerberos || kerberos (username, user, pwd->pw_uid)) {
#ifndef PASSWD_FALLBACK
	errx (1, "won't use /etc/passwd authentication");
#endif
	/* getpwnam() is not reentrant and kerberos might use it! */
	pwd = k_getpwnam (user);
	if (pwd == NULL)
	    errx (1, "unknown login %s", user);
	/* only allow those in group zero to su to root. */
	if (pwd->pw_uid == 0 && (gr = getgrgid ((gid_t) 0)))
	    for (g = gr->gr_mem;; ++g) {
		if (!*g) {
#if 1
		    /* if group 0 is empty or only 
		       contains root su is still ok. */
		    if (gr->gr_mem[0] == 0)
			break;	/* group 0 is empty */
		    if (gr->gr_mem[1] == 0 &&
			strcmp (gr->gr_mem[0], "root") == 0)
			break;	/* only root in group 0 */
#endif
		    errx (1, "you are not in the correct group to su %s.",
			  user);
		}
		if (!strcmp (username, *g))
		    break;
	    }
	/* if target requires a password, verify it */
	if (ruid && *pwd->pw_passwd) {
	    char prompt[128];
	    char passwd[256];

	    snprintf (prompt, sizeof(prompt), "%s's Password: ", pwd->pw_name);
	    if (des_read_pw_string (passwd, sizeof (passwd),
				    prompt, 0)) {
		memset (passwd, 0, sizeof (passwd));
		exit (1);
	    }
	    if (strcmp (pwd->pw_passwd,
			crypt (passwd, pwd->pw_passwd))) {
		memset (passwd, 0, sizeof (passwd));
		syslog (LOG_AUTH | LOG_WARNING,
			"BAD SU %s to %s%s", username,
			user, ontty ());
		errx (1, "Sorry");
	    }
	    memset (passwd, 0, sizeof (passwd));
	}
    }
    if (asme) {
	/* if asme and non-standard target shell, must be root */
	if (!chshell (pwd->pw_shell) && ruid)
	    errx (1, "permission denied (shell '%s' not in /etc/shells).",
		  pwd->pw_shell);
    } else if (pwd->pw_shell && *pwd->pw_shell) {
	shell = pwd->pw_shell;
	iscsh = UNSET;
    } else {
	shell = _PATH_BSHELL;
	iscsh = NO;
    }

    if ((p = strrchr (shell, '/')) != 0)
	avshell = p + 1;
    else
	avshell = shell;

    /* if we're forking a csh, we want to slightly muck the args */
    if (iscsh == UNSET)
	iscsh = strcmp (avshell, "csh") ? NO : YES;

    /* set permissions */

    if (setgid (pwd->pw_gid) < 0)
	err (1, "setgid");
    if (initgroups (user, pwd->pw_gid))
	errx (1, "initgroups failed.");

    if (setuid (pwd->pw_uid) < 0)
	err (1, "setuid");

    if (!asme) {
	if (asthem) {
	    char *k = getenv ("KRBTKFILE");
	    char *t = getenv ("TERM");

	    environ = malloc (10 * sizeof (char *));
	    environ[0] = NULL;
	    setenv ("PATH", _PATH_DEFPATH, 1);
	    if (t)
		setenv ("TERM", t, 1);
	    if (k)
		setenv ("KRBTKFILE", k, 1);
	    if (chdir (pwd->pw_dir) < 0)
		errx (1, "no directory");
	}
	if (asthem || pwd->pw_uid)
	    setenv ("USER", pwd->pw_name, 1);
	setenv ("HOME", pwd->pw_dir, 1);
	setenv ("SHELL", shell, 1);
    }
    if (iscsh == YES) {
	if (fastlogin)
	    *np-- = "-f";
	if (asme)
	    *np-- = "-m";
    }
    if (asthem) {
	avshellbuf[0] = '-';
	strcpy (avshellbuf + 1, avshell);
	avshell = avshellbuf;
    } else if (iscsh == YES) {
	/* csh strips the first character... */
	avshellbuf[0] = '_';
	strcpy (avshellbuf + 1, avshell);
	avshell = avshellbuf;
    }
    *np = avshell;

    if (ruid != 0)
	syslog (LOG_NOTICE | LOG_AUTH, "%s to %s%s",
		username, user, ontty ());

    setpriority (PRIO_PROCESS, 0, prio);

    if (k_hasafs ()) {
	int code;

	if (k_setpag () != 0)
	    warn ("setpag");
	code = k_afsklog (0, 0);
	if (code != KSUCCESS && code != KDC_PR_UNKNOWN)
	    warnx ("afsklog: %s", krb_get_err_text (code));
    }
    execv (shell, np);
    warn ("execv(%s)", shell);
    if (getuid () == 0) {
	execv (_PATH_BSHELL, np);
	warn ("execv(%s)", _PATH_BSHELL);
    }
    exit (1);
}

static int
chshell (char *sh)
{
    char *cp;

    while ((cp = getusershell ()) != NULL)
	if (!strcmp (cp, sh))
	    return (1);
    return (0);
}

static char *
ontty (void)
{
    char *p;
    static char buf[MaxPathLen + 4];

    buf[0] = 0;
    if ((p = ttyname (STDERR_FILENO)) != 0)
	snprintf (buf, sizeof(buf), " on %s", p);
    return (buf);
}

static int
kerberos (char *username, char *user, int uid)
{
    KTEXT_ST ticket;
    AUTH_DAT authdata;
    struct hostent *hp;
    int kerno;
    u_long faddr;
    char lrealm[REALM_SZ], krbtkfile[MaxPathLen];
    char hostname[MaxHostNameLen], savehost[MaxHostNameLen];

    if (krb_get_lrealm (lrealm, 1) != KSUCCESS)
	return (1);
    if (koktologin (username, lrealm, user) && !uid) {
#ifndef PASSWD_FALLBACK
	warnx ("not in %s's ACL.", user);
#endif
	return (1);
    }
    snprintf (krbtkfile, sizeof(krbtkfile),
	      "%s_%s_to_%s_%u", TKT_ROOT, username, user,
	     (unsigned) getpid ());

    setenv ("KRBTKFILE", krbtkfile, 1);
    krb_set_tkt_string (krbtkfile);
    /*
     * Little trick here -- if we are su'ing to root, we need to get a ticket
     * for "xxx.root", where xxx represents the name of the person su'ing.
     * Otherwise (non-root case), we need to get a ticket for "yyy.", where
     * yyy represents the name of the person being su'd to, and the instance
     * is null 
     *
     * We should have a way to set the ticket lifetime, with a system default
     * for root. 
     */
    {
	char prompt[128];
	char passw[256];

	snprintf (prompt, sizeof(prompt),
		  "%s's Password: ",
		  krb_unparse_name_long ((uid == 0 ? username : user),
					 (uid == 0 ? root_inst : ""),
					 lrealm));
	if (des_read_pw_string (passw, sizeof (passw), prompt, 0)) {
	    memset (passw, 0, sizeof (passw));
	    return (1);
	}
	if (strlen(passw) == 0)
	    return (1);		/* Empty passwords is not allowed */
	kerno = krb_get_pw_in_tkt ((uid == 0 ? username : user),
				   (uid == 0 ? root_inst : ""), lrealm,
				   KRB_TICKET_GRANTING_TICKET,
				   lrealm,
				   DEFAULT_TKT_LIFE,
				   passw);
	memset (passw, 0, strlen (passw));
    }

    if (kerno != KSUCCESS) {
	if (kerno == KDC_PR_UNKNOWN) {
	    warnx ("principal unknown: %s",
		   krb_unparse_name_long ((uid == 0 ? username : user),
					  (uid == 0 ? root_inst : ""),
					  lrealm));
	    return (1);
	}
	warnx ("unable to su: %s", krb_get_err_text (kerno));
	syslog (LOG_NOTICE | LOG_AUTH,
		"BAD SU: %s to %s%s: %s",
		username, user, ontty (), krb_get_err_text (kerno));
	return (1);
    }
    if (chown (krbtkfile, uid, -1) < 0) {
	warn ("chown");
	unlink (krbtkfile);
	return (1);
    }
    setpriority (PRIO_PROCESS, 0, -2);

    if (k_gethostname (hostname, sizeof (hostname)) == -1) {
	warn ("gethostname");
	dest_tkt ();
	return (1);
    }
    strncpy (savehost, krb_get_phost (hostname), sizeof (savehost));
    savehost[sizeof (savehost) - 1] = '\0';

    kerno = krb_mk_req (&ticket, "rcmd", savehost, lrealm, 33);

    if (kerno == KDC_PR_UNKNOWN) {
	warnx ("Warning: TGT not verified.");
	syslog (LOG_NOTICE | LOG_AUTH,
		"%s to %s%s, TGT not verified (%s); "
		"%s.%s not registered?",
		username, user, ontty (), krb_get_err_text (kerno),
		"rcmd", savehost);
#ifdef KLOGIN_PARANOID
	/*
	 * if the "VERIFY_SERVICE" doesn't exist in the KDC for this host, *
	 * don't allow kerberos login, also log the error condition. 
	 */
	warnx ("Trying local password!");
	return (1);
#endif
    } else if (kerno != KSUCCESS) {
	warnx ("Unable to use TGT: %s", krb_get_err_text (kerno));
	syslog (LOG_NOTICE | LOG_AUTH, "failed su: %s to %s%s: %s",
		username, user, ontty (), krb_get_err_text (kerno));
	dest_tkt ();
	return (1);
    } else {
	if (!(hp = gethostbyname (hostname))) {
	    warnx ("can't get addr of %s", hostname);
	    dest_tkt ();
	    return (1);
	}
	memcpy (&faddr, hp->h_addr, sizeof (faddr));

	if ((kerno = krb_rd_req (&ticket, "rcmd", savehost, faddr,
				 &authdata, "")) != KSUCCESS) {
	    warnx ("unable to verify rcmd ticket: %s",
		   krb_get_err_text (kerno));
	    syslog (LOG_NOTICE | LOG_AUTH,
		    "failed su: %s to %s%s: %s", username,
		    user, ontty (), krb_get_err_text (kerno));
	    dest_tkt ();
	    return (1);
	}
    }
    fprintf (stderr, "Don't forget to kdestroy before exiting the shell.\n");
    return (0);
}

static int
koktologin (char *name, char *realm, char *toname)
{
    return krb_kuserok (name,
			strcmp (toname, "root") == 0 ? root_inst : "",
			realm,
			toname);
}
