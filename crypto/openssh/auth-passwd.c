/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Sat Mar 18 05:11:38 1995 ylo
 * Password authentication.  This file contains the functions to check whether
 * the password is valid for the user.
 */

#include "includes.h"
RCSID("$Id: auth-passwd.c,v 1.14 1999/12/29 12:47:46 markus Exp $");

#include "packet.h"
#include "ssh.h"
#include "servconf.h"
#include "xmalloc.h"

/*
 * Tries to authenticate the user using password.  Returns true if
 * authentication succeeds.
 */
int 
auth_password(struct passwd * pw, const char *password)
{
	extern ServerOptions options;
	char *encrypted_password;

	/* deny if no user. */
	if (pw == NULL)
		return 0;
	if (pw->pw_uid == 0 && options.permit_root_login == 2)
		return 0;
	if (*password == '\0' && options.permit_empty_passwd == 0)
		return 0;

#ifdef SKEY
	if (options.skey_authentication == 1) {
		int ret = auth_skey_password(pw, password);
		if (ret == 1 || ret == 0)
			return ret;
		/* Fall back to ordinary passwd authentication. */
	}
#endif
#ifdef KRB4
	if (options.kerberos_authentication == 1) {
		int ret = auth_krb4_password(pw, password);
		if (ret == 1 || ret == 0)
			return ret;
		/* Fall back to ordinary passwd authentication. */
	}
#endif

	/* Check for users with no password. */
	if (strcmp(password, "") == 0 && strcmp(pw->pw_passwd, "") == 0)
		return 1;
	/* Encrypt the candidate password using the proper salt. */
	encrypted_password = crypt(password,
	    (pw->pw_passwd[0] && pw->pw_passwd[1]) ? pw->pw_passwd : "xx");

	/* Authentication is accepted if the encrypted passwords are identical. */
	return (strcmp(encrypted_password, pw->pw_passwd) == 0);
}
