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
#include "auth.h"
#include "ssh.h"
#include "log.h"
#include "servconf.h"
#include "canohost.h"
#include "xmalloc.h"
#include "buffer.h"

#ifdef _AIX

#include <uinfo.h>
#include "port-aix.h"

extern ServerOptions options;
extern Buffer loginmsg;

# ifdef HAVE_SETAUTHDB
static char old_registry[REGISTRY_SIZE] = "";
# endif

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

# ifdef WITH_AIXAUTHENTICATE
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

/*
 * Do authentication via AIX's authenticate routine.  We loop until the
 * reenter parameter is 0, but normally authenticate is called only once.
 *
 * Note: this function returns 1 on success, whereas AIX's authenticate()
 * returns 0.
 */
int
sys_auth_passwd(Authctxt *ctxt, const char *password)
{
	char *authmsg = NULL, *host, *msg, *name = ctxt->pw->pw_name;
	int authsuccess = 0, expired, reenter, result;

	do {
		result = authenticate((char *)name, (char *)password, &reenter,
		    &authmsg);
		aix_remove_embedded_newlines(authmsg);	
		debug3("AIX/authenticate result %d, msg %.100s", result,
		    authmsg);
	} while (reenter);

	if (result == 0) {
		authsuccess = 1;

		host = (char *)get_canonical_hostname(options.use_dns);

	       	/*
		 * Record successful login.  We don't have a pty yet, so just
		 * label the line as "ssh"
		 */
		aix_setauthdb(name);
	       	if (loginsuccess((char *)name, (char *)host, "ssh", &msg) == 0) {
			if (msg != NULL) {
				debug("%s: msg %s", __func__, msg);
				buffer_append(&loginmsg, msg, strlen(msg));
				xfree(msg);
			}
		}

		/*
		 * Check if the user's password is expired.
		 */
                expired = passwdexpired(name, &msg);
                if (msg && *msg) {
                        buffer_append(&loginmsg, msg, strlen(msg));
                        aix_remove_embedded_newlines(msg);
                }
                debug3("AIX/passwdexpired returned %d msg %.100s", expired, msg);

		switch (expired) {
		case 0: /* password not expired */
			break;
		case 1: /* expired, password change required */
			ctxt->force_pwchange = 1;
			disable_forwarding();
			break;
		default: /* user can't change(2) or other error (-1) */
			logit("Password can't be changed for user %s: %.100s",
			    name, msg);
			if (msg)
				xfree(msg);
			authsuccess = 0;
		}

		aix_restoreauthdb();
	}

	if (authmsg != NULL)
		xfree(authmsg);

	return authsuccess;
}
  
#  ifdef CUSTOM_FAILED_LOGIN
/*
 * record_failed_login: generic "login failed" interface function
 */
void
record_failed_login(const char *user, const char *ttyname)
{
	char *hostname = (char *)get_canonical_hostname(options.use_dns);

	if (geteuid() != 0)
		return;

	aix_setauthdb(user);
#   ifdef AIX_LOGINFAILED_4ARG
	loginfailed((char *)user, hostname, (char *)ttyname, AUDIT_FAIL_AUTH);
#   else
	loginfailed((char *)user, hostname, (char *)ttyname);
#   endif
	aix_restoreauthdb();
}
#  endif /* CUSTOM_FAILED_LOGIN */

/*
 * If we have setauthdb, retrieve the password registry for the user's
 * account then feed it to setauthdb.  This will mean that subsequent AIX auth
 * functions will only use the specified loadable module.  If we don't have
 * setauthdb this is a no-op.
 */
void
aix_setauthdb(const char *user)
{
#  ifdef HAVE_SETAUTHDB
	char *registry;

	if (setuserdb(S_READ) == -1) {
		debug3("%s: Could not open userdb to read", __func__);
		return;
	}
	
	if (getuserattr((char *)user, S_REGISTRY, &registry, SEC_CHAR) == 0) {
		if (setauthdb(registry, old_registry) == 0)
			debug3("AIX/setauthdb set registry '%s'", registry);
		else 
			debug3("AIX/setauthdb set registry '%s' failed: %s",
			    registry, strerror(errno));
	} else
		debug3("%s: Could not read S_REGISTRY for user: %s", __func__,
		    strerror(errno));
	enduserdb();
#  endif /* HAVE_SETAUTHDB */
}

/*
 * Restore the user's registry settings from old_registry.
 * Note that if the first aix_setauthdb fails, setauthdb("") is still safe
 * (it restores the system default behaviour).  If we don't have setauthdb,
 * this is a no-op.
 */
void
aix_restoreauthdb(void)
{
#  ifdef HAVE_SETAUTHDB
	if (setauthdb(old_registry, NULL) == 0)
		debug3("%s: restoring old registry '%s'", __func__,
		    old_registry);
	else
		debug3("%s: failed to restore old registry %s", __func__,
		    old_registry);
#  endif /* HAVE_SETAUTHDB */
}

# endif /* WITH_AIXAUTHENTICATE */

#endif /* _AIX */
