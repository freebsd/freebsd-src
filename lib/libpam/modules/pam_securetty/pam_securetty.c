/*-
 * Copyright (c) 2001 Mark R V Murray
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ttyent.h>
#include <string.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include <pam_mod_misc.h>

#define TTY_PREFIX	"/dev/"

PAM_EXTERN int 
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	struct ttyent  *ttyfileinfo;
	struct passwd  *user_pwd;
	int             i, options, retval;
	const char     *username, *ttyname;

	options = 0;
	for (i = 0; i < argc; i++)
		pam_std_option(&options, argv[i]);

	retval = pam_get_user(pamh, &username, NULL);
	if (retval != PAM_SUCCESS)
		return retval;

	retval = pam_get_item(pamh, PAM_TTY, (const void **)&ttyname);
	if (retval != PAM_SUCCESS)
		return retval;

	/* Ignore any "/dev/" on the PAM_TTY item */
	if (strncmp(TTY_PREFIX, ttyname, sizeof(TTY_PREFIX) - 1) == 0)
		ttyname += sizeof(TTY_PREFIX) - 1;

	/* If the user is not root, secure ttys do not apply */
	user_pwd = getpwnam(username);
	if (user_pwd == NULL)
		return PAM_IGNORE;
	else if (user_pwd->pw_uid != 0)
		return PAM_SUCCESS;

	ttyfileinfo = getttynam(ttyname);
	if (ttyfileinfo == NULL)
		return PAM_SERVICE_ERR;

	if (ttyfileinfo->ty_status & TTY_SECURE)
		return PAM_SUCCESS;
	else
		return PAM_PERM_DENIED;
}

PAM_EXTERN
int 
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

/* end of module definition */

PAM_MODULE_ENTRY("pam_securetty");
