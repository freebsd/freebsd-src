/*-
 * Copyright 2001 Mark R V Murray
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

#define PAM_SM_AUTH

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include "pam_mod_misc.h"

#define	NOLOGIN	"/var/run/nologin"

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct pam_conv *conv;
	struct pam_message message, *pmessage;
	struct pam_response *resp;
	struct passwd *user_pwd;
	struct stat st;
	int retval, fd;
	const char *user;
	char *mtmp;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got user: %s", user);

	fd = open(NOLOGIN, O_RDONLY, 0);
	if (fd < 0)
		PAM_RETURN(PAM_SUCCESS);

	PAM_LOG("Opened %s file", NOLOGIN);

	user_pwd = getpwnam(user);
	if (user_pwd && user_pwd->pw_uid == 0) {
		message.msg_style = PAM_TEXT_INFO;
		retval = PAM_SUCCESS;
	}
	else {
		message.msg_style = PAM_ERROR_MSG;
		if (!user_pwd)
			retval = PAM_USER_UNKNOWN;
		else
			retval = PAM_AUTH_ERR;
	}
	
	if (fstat(fd, &st) < 0)
		PAM_RETURN(retval);
	message.msg = mtmp = malloc(st.st_size + 1);
	if (!message.msg)
		PAM_RETURN(retval);

	read(fd, mtmp, st.st_size);
	mtmp[st.st_size] = '\0';

	pmessage = &message;
	resp = NULL;
	pam_get_item(pamh, PAM_CONV, (const void **)&conv);
	conv->conv(1, (const struct pam_message **)&pmessage, &resp,
	    conv->appdata_ptr);

	free(mtmp);
	if (resp)
		   _pam_drop_reply(resp, 1);
	
	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_nologin");
