/*-
 * Copyright (c) 2001 Mark R V Murray
 * All rights reserved.
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
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

#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define	PAM_SM_SESSION
#define	PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <pam_mod_misc.h>

enum { PAM_OPT_DENY=PAM_OPT_STD_MAX, PAM_OPT_GROUP, PAM_OPT_TRUST,
	PAM_OPT_AUTH_AS_SELF, PAM_OPT_NOROOT_OK };

static struct opttab other_options[] = {
	{ "deny",		PAM_OPT_DENY },
	{ "group",		PAM_OPT_GROUP },
	{ "trust",		PAM_OPT_TRUST },
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ "noroot_ok",		PAM_OPT_NOROOT_OK },
	{ NULL, 0 }
};

/* Is member in list? */
static int
in_list(char *const *list, const char *member)
{
	for (; *list; list++)
		if (strcmp(*list, member) == 0)
			return 1;
	return 0;
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pwd;
	struct group *grp;
	int retval;
	uid_t tuid;
	const char *user, *targetuser;
	char *use_group;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &targetuser, NULL);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);
	pwd = getpwnam(targetuser);
	if (pwd != NULL)
		tuid = pwd->pw_uid;
	else
		PAM_RETURN(PAM_AUTH_ERR);

	PAM_LOG("Got target user: %s   uid: %d", targetuser, tuid);

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL)) {
		pwd = getpwnam(getlogin());
		user = strdup(pwd->pw_name);
	}
	else {
		user = targetuser;
		pwd = getpwnam(user);
	}
	if (pwd == NULL)
		PAM_RETURN(PAM_AUTH_ERR);

	PAM_LOG("Got user: %s", user);
	PAM_LOG("User's primary uid, gid: %d, %d", pwd->pw_uid, pwd->pw_gid);

	/* Ignore if already uid 0 */
	if (pwd->pw_uid == 0)
		PAM_RETURN(PAM_IGNORE);

	PAM_LOG("Not superuser");

	/* If authenticating as something non-superuser, return OK */
	if (pam_test_option(&options, PAM_OPT_NOROOT_OK, NULL))
		if (tuid != 0)
			PAM_RETURN(PAM_SUCCESS);

	PAM_LOG("Checking group");

	if (!pam_test_option(&options, PAM_OPT_GROUP, &use_group)) {
		if ((grp = getgrnam("wheel")) == NULL)
			grp = getgrgid(0);
	}
	else
		grp = getgrnam(use_group);

	if (grp == NULL || grp->gr_mem == NULL) {
		if (pam_test_option(&options, PAM_OPT_DENY, NULL))
			PAM_RETURN(PAM_IGNORE);
		else {
			PAM_VERBOSE_ERROR("Permission denied");
			PAM_RETURN(PAM_AUTH_ERR);
		}
	}

	PAM_LOG("Got group: %s", grp->gr_name);

	if (pwd->pw_gid == grp->gr_gid || in_list(grp->gr_mem, pwd->pw_name)) {
		if (pam_test_option(&options, PAM_OPT_DENY, NULL)) {
			PAM_VERBOSE_ERROR("Member of group %s; denied",
			    grp->gr_name);
			PAM_RETURN(PAM_PERM_DENIED);
		}
		if (pam_test_option(&options, PAM_OPT_TRUST, NULL))
			PAM_RETURN(PAM_SUCCESS);
		PAM_RETURN(PAM_IGNORE);
	}

	if (pam_test_option(&options, PAM_OPT_DENY, NULL))
		PAM_RETURN(PAM_SUCCESS);

	PAM_VERBOSE_ERROR("Not member of group %s; denied", grp->gr_name);

	PAM_RETURN(PAM_PERM_DENIED);
}

PAM_EXTERN int 
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc ,const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_IGNORE);
}

PAM_MODULE_ENTRY("pam_wheel");
