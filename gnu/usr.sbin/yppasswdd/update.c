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

char *tempname, *passfile;
extern int *allow_chfn, *allow_chsh;

#define xprt_addr(xprt)	(svc_getcaller(xprt)->sin_addr)
#define xprt_port(xprt)	ntohs(svc_getcaller(xprt)->sin_port)
void reaper( int sig );

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
    int		pfd, tfd, c;

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

    pw_init();
    pfd = pw_lock();
    tfd = pw_tmp();

    /* Check if the user exists
     */
    if (!(pw = getpwnam(yppw->newpw.pw_name))) {        
        syslog ( LOG_WARNING, "%s failed", logbuf );
        syslog ( LOG_WARNING, "User not in password file." );
        return (&res);
    }

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
    pw->pw_passwd = newpw->pw_passwd;

    if (allow_chsh) {
	chsh = (strcmp(pw->pw_shell, newpw->pw_shell) != 0);
	pw->pw_shell = newpw->pw_shell;
    }

    if (allow_chfn) {
	chfn = (strcmp(pw->pw_gecos, newpw->pw_gecos) != 0);
	pw->pw_gecos = newpw->pw_gecos;
    }

    pw->pw_change = 0;
    pw_copy(pfd, tfd, pw);
    if (strcmp(passfile, _PATH_MASTERPASSWD)) {
	    close(pfd);
	    close(tfd);
	    rename(tempname,passfile);
	}
	else
	if (pw_mkdb()) {
	    syslog ( LOG_WARNING, "%s failed to rebuild password database", logbuf );
	    return(&res);
    	    }

    /* Fork off process to rebuild NIS passwd.* maps. If the fork
     * fails, restore old passwd file and return an error.
     */
    if ((c = fork()) < 0) {
    	syslog( LOG_ERR, "%s failed", logbuf );
    	syslog( LOG_ERR, "Couldn't fork map update process: %m" );
    	return (&res);
    }
    if (c == 0) {
    	execlp(MAP_UPDATE_PATH, MAP_UPDATE, NULL);
    	syslog( LOG_ERR, "Error: couldn't exec map update process: %m" );
    	exit(1);
    }

    syslog ( LOG_INFO, "%s successful. Password changed.", logbuf );
    if (chsh || chfn) {
    	syslog ( LOG_INFO, "Shell %schanged (%s), GECOS %schanged (%s).",
    			chsh? "" : "un", newpw->pw_shell,
    			chfn? "" : "un", newpw->pw_gecos );
    }
    res = 0;

    return (&res);
}
