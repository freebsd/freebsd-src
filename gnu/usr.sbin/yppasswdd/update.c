/*
 * yppasswdd
 * Copyright 1994 Olaf Kirch, <okir@monad.swb.de>
 *
 * This program is covered by the GNU General Public License, version 2.
 * It is provided in the hope that it is useful. However, the author
 * disclaims ALL WARRANTIES, expressed or implied. See the GPL for details.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>

#include <syslog.h>
#include <stdio.h>
#include <string.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "yppasswd.h"

#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

char *tempname, *passfile;
extern int *allow_chfn, *allow_chsh;
extern int pid;
extern int     pw_copy __P((int, int, struct passwd *));
extern int     pw_lock __P((void));
extern int     pw_mkdb __P((void));
extern int     pw_tmp __P((void));
extern char	*domain;
static struct passwd yp_password;

#define xprt_addr(xprt)	(svc_getcaller(xprt)->sin_addr)
#define xprt_port(xprt)	ntohs(svc_getcaller(xprt)->sin_port)
void reaper( int sig );

static void copy_yp_pass(p, x, m)
	char *p;
	int x, m;
{
	register char *t, *s = p;
	static char *buf;

	yp_password.pw_fields = 0;

	buf = (char *)realloc(buf, m + 10);
	bzero(buf, m + 10);

	/* Turn all colons into NULLs */
	while (strchr(s, ':')) {
		s = (strchr(s, ':') + 1);
		*(s - 1)= '\0';
	}

	t = buf;
#define EXPAND(e)       e = t; while ((*t++ = *p++));
        EXPAND(yp_password.pw_name);
	yp_password.pw_fields |= _PWF_NAME;
        EXPAND(yp_password.pw_passwd);
	yp_password.pw_fields |= _PWF_PASSWD;
	yp_password.pw_uid = atoi(p);
        p += (strlen(p) + 1);
	yp_password.pw_fields |= _PWF_UID;
	yp_password.pw_gid = atoi(p);
        p += (strlen(p) + 1);
	yp_password.pw_fields |= _PWF_GID;
	if (x) {
		EXPAND(yp_password.pw_class);
		yp_password.pw_fields |= _PWF_CLASS;
		yp_password.pw_change = atol(p);
		p += (strlen(p) + 1);
		yp_password.pw_fields |= _PWF_CHANGE;
		yp_password.pw_expire = atol(p);
		p += (strlen(p) + 1);
		yp_password.pw_fields |= _PWF_EXPIRE;
	}
        EXPAND(yp_password.pw_gecos);
	yp_password.pw_fields |= _PWF_GECOS;
        EXPAND(yp_password.pw_dir);
	yp_password.pw_fields |= _PWF_DIR;
        EXPAND(yp_password.pw_shell);
	yp_password.pw_fields |= _PWF_SHELL;

	return;
}

/*===============================================================*
 * Argument validation. Avoid \n... (ouch).
 * We can't use isprint, because people may use 8bit chars which
 * aren't recognized as printable in the default locale.
 *===============================================================*/
static int
validate_string(char *str)
{
    while (*str && !iscntrl(*str)) str++;
    return (*str == '\0');
}

static int
validate_args(struct xpasswd *pw)
{
    if (pw->pw_name[0] == '-' || pw->pw_name[0] == '+') {
	syslog(LOG_ALERT, "attempt to modify NIS passwd entry \"%s\"",
			pw->pw_name);
    }

    return validate_string(pw->pw_passwd)
       &&  validate_string(pw->pw_shell)
       &&  validate_string(pw->pw_gecos);
}

/*===============================================================*
 * The passwd update handler
 *===============================================================*/
int *
yppasswdproc_pwupdate_1(yppasswd *yppw, struct svc_req *rqstp)
{
    struct xpasswd *newpw;	/* passwd struct passed by the client */
    struct passwd *pw;		/* passwd struct obtained from getpwent() */
    int		chsh = 0, chfn = 0;
    static int	res;
    char	logbuf[255];
    int		pfd, tfd;
    char	*passfile_hold;
    char	template[] = "/tmp/yppwtmp.XXXXX";
    char	*result;
    int		resultlen;

    newpw = &yppw->newpw;
    res = 1;

    sprintf( logbuf, "update %.12s (uid=%d) from host %s",
			    yppw->newpw.pw_name,
			    yppw->newpw.pw_uid,
			    inet_ntoa(xprt_addr(rqstp->rq_xprt)));

    if (!validate_args(newpw)) {
        syslog ( LOG_ALERT, "%s failed", logbuf );
        syslog ( LOG_ALERT, "Invalid characters in argument. "
        		    "Possible spoof attempt?" );
        return &res;
    }

    /* Check if the user exists
     */
    if (yp_match(domain, "master.passwd.byname", yppw->newpw.pw_name,
	strlen(yppw->newpw.pw_name), &result, &resultlen)) {
	syslog ( LOG_WARNING, "%s failed", logbuf );
	syslog ( LOG_WARNING, "User not in password file." );
	return (&res);
    } else {
	copy_yp_pass(result, 1, resultlen);
	pw = (struct passwd *)&yp_password;
	free(result);
    }

#ifdef notdef
    if (!(pw = getpwnam(yppw->newpw.pw_name))) {
        syslog ( LOG_WARNING, "%s failed", logbuf );
        syslog ( LOG_WARNING, "User not in password file." );
        return (&res);
    }
#endif

   /* Check the password.
    */
   if (strcmp(crypt(yppw->oldpass, pw->pw_passwd), pw->pw_passwd)) {
       	syslog ( LOG_WARNING, "%s rejected", logbuf );
        syslog ( LOG_WARNING, "Invalid password." );
        sleep(1);
        return(&res);
    }

   /* set the new passwd, shell, and full name
    */
    pw->pw_change = 0;
    pw->pw_passwd = newpw->pw_passwd;

    if (allow_chsh) {
	chsh = (strcmp(pw->pw_shell, newpw->pw_shell) != 0);
	pw->pw_shell = newpw->pw_shell;
    }

    if (allow_chfn) {
	chfn = (strcmp(pw->pw_gecos, newpw->pw_gecos) != 0);
	pw->pw_gecos = newpw->pw_gecos;
    }

    /*
     * Bail if locking the password file or temp file creation fails.
     * (These operations should log their own failure messages if need be,
     * so we don't have to log their failures here.)
     */
    if ((pfd = pw_lock()) < 0)
		return &res;
    if ((tfd = pw_tmp()) < 0)
		return &res;

    /* Placeholder in case we need to put the old password file back. */
    passfile_hold = mktemp((char *)&template);

    /*
     * Copy the password file to the temp file,
     * inserting new passwd entry along the way.
     */
    if (pw_copy(pfd, tfd, pw) < 0) {
	syslog(LOG_ERR, "%s > %s: copy failed. Cleaning up.",
						tempname, passfile);
	unlink(tempname);
	return (&res);
    }

    rename(passfile, passfile_hold);
    if (strcmp(passfile, _PATH_MASTERPASSWD)) {
	    rename(tempname, passfile);
	}
	else
	if (pw_mkdb() < 0) {
	    syslog (LOG_WARNING, "%s failed to rebuild password database", logbuf );
	    return(&res);
    	    }

    /* Fork off process to rebuild NIS passwd.* maps. If the fork
     * fails, restore old passwd file and return an error.
     */
    if ((pid = fork()) < 0) {
    	syslog( LOG_ERR, "%s failed", logbuf );
    	syslog( LOG_ERR, "Couldn't fork map update process: %m" );
	unlink(passfile);
	rename(passfile_hold, passfile);
	if (!strcmp(passfile, _PATH_MASTERPASSWD))
	    if (pw_mkdb()) {
		syslog (LOG_WARNING, "%s failed to rebuild password database", logbuf );
		return(&res);
	    }

    	return (&res);
    }
    if (pid == 0) {
	unlink(passfile_hold);
    	execlp(MAP_UPDATE_PATH, MAP_UPDATE, passfile, NULL);
    	syslog( LOG_ERR, "Error: couldn't exec map update process: %m" );
    	exit(1);
    }

    syslog (LOG_INFO, "%s successful. Password changed.", logbuf );
    if (chsh || chfn) {
    	syslog ( LOG_INFO, "Shell %schanged (%s), GECOS %schanged (%s).",
    			chsh? "" : "un", newpw->pw_shell,
    			chfn? "" : "un", newpw->pw_gecos );
    }

    res = 0;
    return (&res);
}
