/*
 *
 * Copyright (c) 2001 Gert Doering.  All rights reserved.
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
 */
#include "includes.h"
#include "ssh.h"
#include "log.h"
#include "servconf.h"
#include "canohost.h"
#include "xmalloc.h"

#ifdef _AIX

#include <uinfo.h>
#include "port-aix.h"

extern ServerOptions options;

/*
 * AIX has a "usrinfo" area where logname and other stuff is stored - 
 * a few applications actually use this and die if it's not set
 *
 * NOTE: TTY= should be set, but since no one uses it and it's hard to
 * acquire due to privsep code.  We will just drop support.
 */
void
aix_usrinfo(struct passwd *pw)
{
	u_int i;
	size_t len;
	char *cp;

	len = sizeof("LOGNAME= NAME= ") + (2 * strlen(pw->pw_name));
	cp = xmalloc(len);

	i = snprintf(cp, len, "LOGNAME=%s%cNAME=%s%c", pw->pw_name, '\0', 
	    pw->pw_name, '\0');
	if (usrinfo(SETUINFO, cp, i) == -1)
		fatal("Couldn't set usrinfo: %s", strerror(errno));
	debug3("AIX/UsrInfo: set len %d", i);

	xfree(cp);
}

#ifdef WITH_AIXAUTHENTICATE
/*
 * Remove embedded newlines in string (if any).
 * Used before logging messages returned by AIX authentication functions
 * so the message is logged on one line.
 */
void
aix_remove_embedded_newlines(char *p)
{
	if (p == NULL)
		return;

	for (; *p; p++) {
		if (*p == '\n')
			*p = ' ';
	}
	/* Remove trailing whitespace */
	if (*--p == ' ')
		*p = '\0';
}
#endif /* WITH_AIXAUTHENTICATE */
  
# ifdef CUSTOM_FAILED_LOGIN
/*
 * record_failed_login: generic "login failed" interface function
 */
void
record_failed_login(const char *user, const char *ttyname)
{
	char *hostname = get_canonical_hostname(options.use_dns);

	if (geteuid() != 0)
		return;

	aix_setauthdb(user);
#  ifdef AIX_LOGINFAILED_4ARG
	loginfailed((char *)user, hostname, (char *)ttyname, AUDIT_FAIL_AUTH);
#  else
	loginfailed((char *)user, hostname, (char *)ttyname);
#  endif
}

/*
 * If we have setauthdb, retrieve the password registry for the user's
 * account then feed it to setauthdb.  This may load registry-specific method
 * code.  If we don't have setauthdb or have already called it this is a no-op.
 */
void
aix_setauthdb(const char *user)
{
#  ifdef HAVE_SETAUTHDB
	static char *registry = NULL;

	if (registry != NULL)	/* have already done setauthdb */
		return;

	if (setuserdb(S_READ) == -1) {
		debug3("%s: Could not open userdb to read", __func__);
		return;
	}
	
	if (getuserattr((char *)user, S_REGISTRY, &registry, SEC_CHAR) == 0) {
		if (setauthdb(registry, NULL) == 0)
			debug3("%s: AIX/setauthdb set registry %s", __func__,
			    registry);
		else 
			debug3("%s: AIX/setauthdb set registry %s failed: %s",
			    __func__, registry, strerror(errno));
	} else
		debug3("%s: Could not read S_REGISTRY for user: %s", __func__,
		    strerror(errno));
	enduserdb();
#  endif
}
# endif /* CUSTOM_FAILED_LOGIN */
#endif /* _AIX */

