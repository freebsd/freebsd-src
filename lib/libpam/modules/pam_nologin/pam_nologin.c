/*-
 * Copyright 2001 Mark R V Murray
 * All rights reserved.
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAM_SM_AUTH

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#define	NOLOGIN	"/var/run/nologin"

static char nologin_def[] = NOLOGIN;

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	login_cap_t *lc;
	struct passwd *pwd;
	struct stat st;
	int retval, fd;
	const char *user, *nologin;
	char *mtmp;

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got user: %s", user);

	lc = login_getclass(NULL);
	nologin = login_getcapstr(lc, "nologin", nologin_def, nologin_def);
	login_close(lc);
	lc = NULL;

	fd = open(nologin, O_RDONLY, 0);
	if (fd < 0)
		return (PAM_SUCCESS);

	PAM_LOG("Opened %s file", NOLOGIN);

	pwd = getpwnam(user);
	if (pwd && pwd->pw_uid == 0)
		retval = PAM_SUCCESS;
	else {
		if (!pwd)
			retval = PAM_USER_UNKNOWN;
		else
			retval = PAM_AUTH_ERR;
	}

	if (fstat(fd, &st) < 0)
		return (retval);

	mtmp = malloc(st.st_size + 1);
	if (mtmp != NULL) {
		read(fd, mtmp, st.st_size);
		mtmp[st.st_size] = '\0';
		pam_error(pamh, "%s", mtmp, NULL);
		free(mtmp);
	}

	if (retval != PAM_SUCCESS)
		PAM_VERBOSE_ERROR("Administrator refusing you: %s", NOLOGIN);

	return (retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh __unused, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{

	return (PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_nologin");
