/*
 * Copyright (c) 2005 The SCO Group. All rights reserved.
 * Copyright (c) 2005 Tim Rice. All rights reserved.
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

#if defined(HAVE_LIBIAF)  &&  !defined(BROKEN_LIBIAF)
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include "packet.h"
#include "buffer.h"
#include "log.h"
#include "servconf.h"
#include "auth.h"
#include "auth-options.h"

int nischeck(char *);

int
sys_auth_passwd(Authctxt *authctxt, const char *password)
{
	struct passwd *pw = authctxt->pw;
	char *encrypted_password;
	char *salt;
	int result;

	/* Just use the supplied fake password if authctxt is invalid */
	char *pw_password = authctxt->valid ? shadow_pw(pw) : pw->pw_passwd;

	/* Check for users with no password. */
	if (strcmp(pw_password, "") == 0 && strcmp(password, "") == 0)
		return (1);

	/* Encrypt the candidate password using the proper salt. */
	salt = (pw_password[0] && pw_password[1]) ? pw_password : "xx";
#ifdef UNIXWARE_LONG_PASSWORDS
	if (!nischeck(pw->pw_name))
		encrypted_password = bigcrypt(password, salt);
	else
#endif /* UNIXWARE_LONG_PASSWORDS */
		encrypted_password = xcrypt(password, salt);

	/*
	 * Authentication is accepted if the encrypted passwords
	 * are identical.
	 */
	result = (strcmp(encrypted_password, pw_password) == 0);

	if (authctxt->valid)
		free(pw_password);
	return(result);
}

#ifdef UNIXWARE_LONG_PASSWORDS
int
nischeck(char *namep)
{
	char password_file[] = "/etc/passwd";
	FILE *fd;
	struct passwd *ent = NULL;

	if ((fd = fopen (password_file, "r")) == NULL) {
		/*
		 * If the passwd file has dissapeared we are in a bad state.
		 * However, returning 0 will send us back through the
		 * authentication scheme that has checked the ia database for
		 * passwords earlier.
		 */
		return(0);
	}

	/*
	 * fgetpwent() only reads from password file, so we know for certain
	 * that the user is local.
	 */
	while (ent = fgetpwent(fd)) {
		if (strcmp (ent->pw_name, namep) == 0) {
			/* Local user */
			fclose (fd);
			return(0);
		}
	}

	fclose (fd);
	return (1);
}

#endif /* UNIXWARE_LONG_PASSWORDS */

/*
	NOTE: ia_get_logpwd() allocates memory for arg 2
	functions that call shadow_pw() will need to free
 */

char *
get_iaf_password(struct passwd *pw)
{
	char *pw_password = NULL;

	uinfo_t uinfo;
	if (!ia_openinfo(pw->pw_name,&uinfo)) {
		ia_get_logpwd(uinfo, &pw_password);
		if (pw_password == NULL)
			fatal("ia_get_logpwd: Unable to get the shadow passwd");
		ia_closeinfo(uinfo);
	 	return pw_password;
	}
	else
		fatal("ia_openinfo: Unable to open the shadow passwd file");
}
#endif /* HAVE_LIBIAF  && !BROKEN_LIBIAF */

