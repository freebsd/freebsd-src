/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
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

#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define PAM_SM_AUTH

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>


PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	const char *group, *user, *ruser;
	char *const *list;
	struct passwd *pwd;
	struct group *grp;

	/* get target account */
	if (pam_get_item(pamh, PAM_USER, (const void **)&user) != PAM_SUCCESS
	    || user == NULL || (pwd = getpwnam(user)) == NULL)
		return (PAM_AUTH_ERR);
	if (pwd->pw_uid != 0 && openpam_get_option(pamh, "root_only"))
		return (PAM_IGNORE);

	/* get applicant */
	if (pam_get_item(pamh, PAM_RUSER, (const void **)&ruser) != PAM_SUCCESS
	    || ruser == NULL || (pwd = getpwnam(ruser)) == NULL)
		return (PAM_AUTH_ERR);

	/* get regulating group */
	if ((group = openpam_get_option(pamh, "group")) == NULL)
		group = "wheel";
	if ((grp = getgrnam(group)) == NULL || grp->gr_mem == NULL)
		goto failed;

	/* check if the group is empty */
	if (*grp->gr_mem == NULL)
		goto failed;

	/* check membership */
	if (pwd->pw_gid == grp->gr_gid)
		goto found;
	for (list = grp->gr_mem; *list != NULL; ++list)
		if (strcmp(*list, pwd->pw_name) == 0)
			goto found;

 not_found:
	if (openpam_get_option(pamh, "deny"))
		return (PAM_SUCCESS);
	return (PAM_AUTH_ERR);
 found:
	if (openpam_get_option(pamh, "deny"))
		return (PAM_AUTH_ERR);
	return (PAM_SUCCESS);
 failed:
	if (openpam_get_option(pamh, "fail_safe"))
		goto found;
	else
		goto not_found;
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh __unused, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{

	return (PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_group");
