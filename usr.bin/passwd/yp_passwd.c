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
#ifndef lint
/*static char sccsid[] = "from: @(#)yp_passwd.c	1.0 2/2/93";*/
static char rcsid[] = "$Id: yp_passwd.c,v 1.1 1994/01/11 19:01:16 nate Exp $";
#endif /* not lint */

#ifdef	YP

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd
        
#ifndef _PASSWORD_LEN
#define _PASSWORD_LEN PASS_MAX
#endif
        
extern char *progname;
static char *getnewpasswd();        
static struct passwd *ypgetpwnam();

static uid_t uid;
char *domain;

yp_passwd(uname)
        char *uname;
{
        char *master;
        char *pp;
        int r, rpcport, status;
        struct yppasswd yppasswd;
        struct passwd *pw;
	struct timeval tv;
	CLIENT *client;
        
        uid = getuid();

        /*
         * Get local domain
         */
        if (r = yp_get_default_domain(&domain)) {
                (void)fprintf(stderr, "%s: can't get local YP domain. Reason: %s\n", progname, yperr_string(r));
                exit(1);
        }

        /*
         * Find the host for the passwd map; it should be running
         * the daemon.
         */
        if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
                (void)fprintf(stderr, "%s: can't find the master YP server. Reason: %s\n", progname, yperr_string(r));
                exit(1);
        }

        /*
         * Ask the portmapper for the port of the daemon.
         */
        if ((rpcport = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE, IPPROTO_UDP)) == 0) {
                (void)fprintf(stderr, "%s: master YP server not running yppasswd daemon.\n\tCan't change password.\n", progname);
                exit(1);
        }

        /*
         * Be sure the port is priviledged
         */
        if (rpcport >= IPPORT_RESERVED) {
                (void)fprintf(stderr, "%s: yppasswd daemon running on an invalid port.\n", progname);
                exit(1);
        }

        /* Get user's login identity */
        if (!(pw = ypgetpwnam(uname))) {
		(void)fprintf(stderr, "%s: unknown user %s.\n", progname, uname);
		exit(1);
	}
                
	if (uid && uid != pw->pw_uid) {
		(void)fprintf(stderr, "%s: you are only allowed to change your own password: %s\n", progname, strerror(EACCES));
		exit(1);
	}

        /* prompt for new password */
        yppasswd.newpw.pw_passwd = getnewpasswd(pw, &yppasswd.oldpass);

        /* tell rpc.yppasswdd */
        yppasswd.newpw.pw_name	= pw->pw_name;
        yppasswd.newpw.pw_uid 	= pw->pw_uid;
        yppasswd.newpw.pw_gid	= pw->pw_gid;
        yppasswd.newpw.pw_gecos = pw->pw_gecos;
        yppasswd.newpw.pw_dir	= pw->pw_dir;
        yppasswd.newpw.pw_shell	= pw->pw_shell;
        
        client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
        if (client==NULL) {
                fprintf(stderr, "can't contact yppasswdd on %s: Reason: %s\n",
                        master, yperr_string(YPERR_YPBIND));
                return(YPERR_YPBIND);
        }
        client->cl_auth = authunix_create_default();
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        r = clnt_call(client, YPPASSWDPROC_UPDATE,
                      xdr_yppasswd, &yppasswd, xdr_int, &status, tv);
        if (r)
                fprintf(stderr, "%s: rpc to yppasswdd failed.\n");
        else if (status)
                printf("Couldn't change YP password.\n");
        else
                printf("The YP password has been changed on %s, the master YP passwd server.\n", master);

        exit(0);
}

static char *
getnewpasswd(pw, old_pass)
	register struct passwd *pw;
        char **old_pass;
{
        static char buf[_PASSWORD_LEN+1];
	register char *p, *t;
	int tries;
	char salt[9], *crypt(), *getpass();
        
	(void)printf("Changing YP password for %s.\n", pw->pw_name);

        if (uid && old_pass) {
                *old_pass = NULL;
        
                if (pw->pw_passwd &&
#ifdef DES
                    strcmp(crypt(p = getpass("Old password:"), pw->pw_passwd),
                           pw->pw_passwd)) {
#else
                    strcmp(p = getpass("Old password:"), pw->pw_passwd)) {
#endif
                                   errno = EACCES;
                                   pw_error(NULL, 1, 1);
                }
                *old_pass = strdup(p);
        }
	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (strlen(p) <= 5 && (uid != 0 || ++tries < 2)) {
			(void)printf("Please enter a longer password.\n");
			continue;
		}
		for (t = p; *t && islower(*t); ++t);
		if (!*t && (uid != 0 || ++tries < 2)) {
			(void)printf("Please don't use an all-lower case password.\nUnusual capitalization, control characters or digits are suggested.\n");
			continue;
		}
		(void)strcpy(buf, p);
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
	/* grab a random printable character that isn't a colon */
	(void)srandom((int)time((time_t *)NULL));
#ifdef NEWSALT
	salt[0] = _PASSWORD_EFMT1;
	to64(&salt[1], (long)(29 * 25), 4);
	to64(&salt[5], random(), 4);
#else
	to64(&salt[0], random(), 2);
#endif
#ifdef DES
	return(strdup(crypt(buf, salt)));
#else
	return(buf);
#endif
}

static char *
pwskip(register char *p)
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

struct passwd *
interpret(struct passwd *pwent, char *line)
{
	register char	*p = line;
	register int	c;

        pwent->pw_passwd = "*";
        pwent->pw_uid = 0;
        pwent->pw_gid = 0;
        pwent->pw_gecos = "";
        pwent->pw_dir = "";
        pwent->pw_shell = "";
	pwent->pw_change = 0;
	pwent->pw_expire = 0;
	pwent->pw_class = "";
        
        /* line without colon separators is no good, so ignore it */
        if(!strchr(p,':'))
                return(NULL);

	pwent->pw_name = p;
	p = pwskip(p);
	pwent->pw_passwd = p;
	p = pwskip(p);
	pwent->pw_uid = (uid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gid = (gid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gecos = p;
	p = pwskip(p);
	pwent->pw_dir = p;
	p = pwskip(p);
	pwent->pw_shell = p;
	while (*p && *p != '\n')
		p++;
	*p = '\0';
	return (pwent);
}

static struct passwd *
ypgetpwnam(nam)
        char *nam;
{
        static struct passwd pwent;
        static char line[1024];
        char *val;
        int reason, vallen;
        
        reason = yp_match(domain, "passwd.byname", nam, strlen(nam),
                          &val, &vallen);
        switch(reason) {
        case 0:
                break;
        default:
                return (NULL);
                break;
        }
        val[vallen] = '\0';
        strcpy(line, val);
        free(val);

        return(interpret(&pwent, line));
}

#endif	/* YP */
