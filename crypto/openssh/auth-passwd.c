/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Password authentication.  This file contains the functions to check whether
 * the password is valid for the user.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Dug Song.  All rights reserved.
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 */

#include "includes.h"
RCSID("$OpenBSD: auth-passwd.c,v 1.29 2003/08/26 09:58:43 markus Exp $");
RCSID("$FreeBSD$");

#include "packet.h"
#include "log.h"
#include "servconf.h"
#include "auth.h"
#ifdef WITH_AIXAUTHENTICATE
# include "buffer.h"
# include "canohost.h"
extern Buffer loginmsg;
#endif

extern ServerOptions options;

/*
 * Tries to authenticate the user using password.  Returns true if
 * authentication succeeds.
 */
int
auth_password(Authctxt *authctxt, const char *password)
{
	struct passwd * pw = authctxt->pw;
	int ok = authctxt->valid;

	/* deny if no user. */
	if (pw == NULL)
		return 0;
#ifndef HAVE_CYGWIN
	if (pw && pw->pw_uid == 0 && options.permit_root_login != PERMIT_YES)
		ok = 0;
#endif
	if (*password == '\0' && options.permit_empty_passwd == 0)
		return 0;

#if defined(HAVE_OSF_SIA)
	return auth_sia_password(authctxt, password) && ok;
#else
# ifdef KRB5
	if (options.kerberos_authentication == 1) {
		int ret = auth_krb5_password(authctxt, password);
		if (ret == 1 || ret == 0)
			return ret && ok;
		/* Fall back to ordinary passwd authentication. */
	}
# endif
# ifdef HAVE_CYGWIN
	if (is_winnt) {
		HANDLE hToken = cygwin_logon_user(pw, password);

		if (hToken == INVALID_HANDLE_VALUE)
			return 0;
		cygwin_set_impersonation_token(hToken);
		return ok;
	}
# endif
# ifdef WITH_AIXAUTHENTICATE
	{
		char *authmsg = NULL;
		int reenter = 1;
		int authsuccess = 0;

		if (authenticate(pw->pw_name, password, &reenter,
		    &authmsg) == 0 && ok) {
			char *msg;
			char *host = 
			    (char *)get_canonical_hostname(options.use_dns);

			authsuccess = 1;
			aix_remove_embedded_newlines(authmsg);	

			debug3("AIX/authenticate succeeded for user %s: %.100s",
				pw->pw_name, authmsg);

	        	/* No pty yet, so just label the line as "ssh" */
			aix_setauthdb(authctxt->user);
	        	if (loginsuccess(authctxt->user, host, "ssh", 
			    &msg) == 0) {
				if (msg != NULL) {
					debug("%s: msg %s", __func__, msg);
					buffer_append(&loginmsg, msg, 
					    strlen(msg));
					xfree(msg);
				}
			}
		} else {
			debug3("AIX/authenticate failed for user %s: %.100s",
			    pw->pw_name, authmsg);
		}

		if (authmsg != NULL)
			xfree(authmsg);

		return authsuccess;
	}
# endif
# ifdef BSD_AUTH
	if (auth_userokay(pw->pw_name, authctxt->style, "auth-ssh",
	    (char *)password) == 0)
		return 0;
	else
		return ok;
# else
	{
	/* Just use the supplied fake password if authctxt is invalid */
	char *pw_password = authctxt->valid ? shadow_pw(pw) : pw->pw_passwd;

	/* Check for users with no password. */
	if (strcmp(pw_password, "") == 0 && strcmp(password, "") == 0)
		return ok;
	else {
		/* Encrypt the candidate password using the proper salt. */
		char *encrypted_password = xcrypt(password,
		    (pw_password[0] && pw_password[1]) ? pw_password : "xx");

		/*
		 * Authentication is accepted if the encrypted passwords
		 * are identical.
		 */
		return (strcmp(encrypted_password, pw_password) == 0) && ok;
	}

	}
# endif
#endif /* !HAVE_OSF_SIA */
}
