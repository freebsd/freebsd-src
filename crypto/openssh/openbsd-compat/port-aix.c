#include "includes.h"

#ifdef _AIX

#include <uinfo.h>
#include <../xmalloc.h>

/*
 * AIX has a "usrinfo" area where logname and
 * other stuff is stored - a few applications
 * actually use this and die if it's not set
 */
void
aix_usrinfo(struct passwd *pw, char *tty, int ttyfd) 
{
	u_int i;
	char *cp=NULL;

	if (ttyfd == -1)
		tty[0] = '\0';
	cp = xmalloc(22 + strlen(tty) + 2 * strlen(pw->pw_name));
	i = sprintf(cp, "LOGNAME=%s%cNAME=%s%cTTY=%s%c%c", pw->pw_name, 0, 
	    pw->pw_name, 0, tty, 0, 0);
	if (usrinfo(SETUINFO, cp, i) == -1)
		fatal("Couldn't set usrinfo: %s", strerror(errno));
	debug3("AIX/UsrInfo: set len %d", i);
	xfree(cp);
}

#endif /* _AIX */

