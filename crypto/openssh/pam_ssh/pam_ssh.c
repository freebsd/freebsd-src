/*-
 * Copyright (c) 1999 Andrew J. Korty
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
 *
 */


#include <sys/param.h>

#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PAM_SM_AUTH
#define	PAM_SM_SESSION
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#include "includes.h"
#include "rsa.h"
#include "ssh.h"
#include "authfd.h"

#define	MODULE_NAME	"pam_ssh"
#define	NEED_PASSPHRASE	"Need passphrase for %s (%s).\nEnter passphrase: "
#define	PATH_SSH_AGENT	"__PREFIX__/bin/ssh-agent"


void
rsa_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
	if (data)
		RSA_free(data);
}


void
ssh_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
	if (data)
		free(data);
}


typedef struct passwd PASSWD;

PAM_EXTERN int
pam_sm_authenticate(
	pam_handle_t	 *pamh,
	int		  flags,
	int		  argc,
	const char	**argv)
{
	char		*comment_priv;		/* on private key */
	char		*comment_pub;		/* on public key */
	char		*identity;		/* user's identity file */
	RSA		*key;			/* user's private key */
	int		 options;		/* module options */
	const char	*pass;			/* passphrase */
	char		*prompt;		/* passphrase prompt */
	RSA		*public_key;		/* user's public key */
	const PASSWD	*pwent;			/* user's passwd entry */
	PASSWD		*pwent_keep;		/* our own copy */
	int		 retval;		/* from calls */
	uid_t		 saved_uid;		/* caller's uid */
	const char	*user;			/* username */

	options = 0;
	while (argc--)
		pam_std_option(&options, *argv++);
	if ((retval = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return retval;
	if (!((pwent = getpwnam(user)) && pwent->pw_dir)) {
		/* delay? */
		return PAM_AUTH_ERR;
	}
	/* locate the user's private key file */
	if (!asprintf(&identity, "%s/%s", pwent->pw_dir,
	    SSH_CLIENT_IDENTITY)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		return PAM_SERVICE_ERR;
	}
	/*
	 * Fail unless we can load the public key.  Change to the
	 * owner's UID to appease load_public_key().
	 */
	key = RSA_new();
	public_key = RSA_new();
	saved_uid = getuid();
	(void)setreuid(pwent->pw_uid, saved_uid);
	retval = load_public_key(identity, public_key, &comment_pub);
	(void)setuid(saved_uid);
	if (!retval) {
		free(identity);
		return PAM_AUTH_ERR;
	}
	RSA_free(public_key);
	/* build the passphrase prompt */
	retval = asprintf(&prompt, NEED_PASSPHRASE, identity, comment_pub);
	free(comment_pub);
	if (!retval) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		free(identity);
		return PAM_SERVICE_ERR;
	}
	/* pass prompt message to application and receive passphrase */
	retval = pam_get_pass(pamh, &pass, prompt, options);
	free(prompt);
	if (retval != PAM_SUCCESS) {
		free(identity);
		return retval;
	}
	/*
	 * Try to decrypt the private key with the passphrase provided.
	 * If success, the user is authenticated.
	 */
	(void)setreuid(pwent->pw_uid, saved_uid);
	retval = load_private_key(identity, pass, key, &comment_priv);
	free(identity);
	(void)setuid(saved_uid);
	if (!retval)
		return PAM_AUTH_ERR;
	/*
	 * Save the key and comment to pass to ssh-agent in the session
	 * phase.
	 */
	if ((retval = pam_set_data(pamh, "ssh_private_key", key,
	    rsa_cleanup)) != PAM_SUCCESS) {
		RSA_free(key);
		free(comment_priv);
		return retval;
	}
	if ((retval = pam_set_data(pamh, "ssh_key_comment", comment_priv,
	    ssh_cleanup)) != PAM_SUCCESS) {
		free(comment_priv);
		return retval;
	}
	/*
	 * Copy the passwd entry (in case successive calls are made)
	 * and save it for the session phase.
	 */
	if (!(pwent_keep = malloc(sizeof *pwent))) {
		syslog(LOG_CRIT, "%m");
		return PAM_SERVICE_ERR;
	}
	(void)memcpy(pwent_keep, pwent, sizeof *pwent_keep);
	if ((retval = pam_set_data(pamh, "ssh_passwd_entry", pwent_keep,
	    ssh_cleanup)) != PAM_SUCCESS) {
		free(pwent_keep);
		return retval;
	}
	return PAM_SUCCESS;
}


PAM_EXTERN int
pam_sm_setcred(
	pam_handle_t	 *pamh,
	int		  flags,
	int		  argc,
	const char	**argv)
{
	return PAM_SUCCESS;
}


typedef AuthenticationConnection AC;

PAM_EXTERN int
pam_sm_open_session(
	pam_handle_t	 *pamh,
	int		  flags,
	int		  argc,
	const char	**argv)
{
	AC		*ac;			/* to ssh-agent */
	char		*comment;		/* on private key */
	char		*env_end;		/* end of env */
	char		*env_file;		/* to store env */
	FILE		*env_fp;		/* env_file handle */
	RSA		*key;			/* user's private key */
	FILE		*pipe;			/* ssh-agent handle */
	const PASSWD	*pwent;			/* user's passwd entry */
	int		 retval;		/* from calls */
	uid_t		 saved_uid;		/* caller's uid */
	const char	*tty;			/* tty or display name */
	char		 hname[MAXHOSTNAMELEN];	/* local hostname */
	char		 parse[BUFSIZ];		/* commands output */

	/* dump output of ssh-agent in ~/.ssh */
	if ((retval = pam_get_data(pamh, "ssh_passwd_entry",
	    (const void **)&pwent)) != PAM_SUCCESS)
		return retval;
	/* use the tty or X display name in the filename */
	if ((retval = pam_get_item(pamh, PAM_TTY, (const void **)&tty))
	    != PAM_SUCCESS)
		return retval;
	if (*tty == ':' && gethostname(hname, sizeof hname) == 0) {
		if (asprintf(&env_file, "%s/.ssh/agent-%s%s",
		    pwent->pw_dir, hname, tty) == -1) {
			syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
			return PAM_SERVICE_ERR;
		}
	} else if (asprintf(&env_file, "%s/.ssh/agent-%s", pwent->pw_dir,
	    tty) == -1) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		return PAM_SERVICE_ERR;
	}
	/* save the filename so we can delete the file on session close */
	if ((retval = pam_set_data(pamh, "ssh_agent_env", env_file,
	    ssh_cleanup)) != PAM_SUCCESS) {
		free(env_file);
		return retval;
	}
	/* start the agent as the user */
	saved_uid = geteuid();
	(void)seteuid(pwent->pw_uid);
	env_fp = fopen(env_file, "w");
	pipe = popen(PATH_SSH_AGENT, "r");
	(void)seteuid(saved_uid);
	if (!pipe) {
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, PATH_SSH_AGENT);
		if (env_fp)
			(void)fclose(env_fp);
		return PAM_SESSION_ERR;
	}
	while (fgets(parse, sizeof parse, pipe)) {
		if (env_fp)
			(void)fputs(parse, env_fp);
		/*
		 * Save environment for application with pam_putenv()
		 * but also with putenv() for our own call to
		 * ssh_get_authentication_connection().
		 */
		if (strchr(parse, '=') && (env_end = strchr(parse, ';'))) {
			*env_end = '\0';
			/* pass to the application ... */
			if (!((retval = pam_putenv(pamh, parse)) ==
			    PAM_SUCCESS && putenv(parse) == 0)) {
				(void)pclose(pipe);
				if (env_fp)
					(void)fclose(env_fp);
				return PAM_SERVICE_ERR;
			}
		}
	}
	if (env_fp)
		(void)fclose(env_fp);
	retval = pclose(pipe);
	if (retval > 0) {
		syslog(LOG_ERR, "%s: %s exited with status %d",
		    MODULE_NAME, PATH_SSH_AGENT, WEXITSTATUS(retval));
		return PAM_SESSION_ERR;
	} else if (retval < 0) {
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, PATH_SSH_AGENT);
		return PAM_SESSION_ERR;
	}
	/* connect to the agent and hand off the private key */
	if ((retval = pam_get_data(pamh, "ssh_private_key",
	    (const void **)&key)) != PAM_SUCCESS)
		return retval;
	if ((retval = pam_get_data(pamh, "ssh_key_comment",
	    (const void **)&comment)) != PAM_SUCCESS)
		return retval;
	if (!(ac = ssh_get_authentication_connection())) {
		syslog(LOG_ERR, "%s: could not connect to agent",
		    MODULE_NAME);
		return PAM_SESSION_ERR;
	}
	retval = ssh_add_identity(ac, key, comment);
	ssh_close_authentication_connection(ac);
	return retval ? PAM_SUCCESS : PAM_SESSION_ERR;
}


PAM_EXTERN int
pam_sm_close_session(
	pam_handle_t	 *pamh,
	int		  flags,
	int		  argc,
	const char	**argv)
{
	const char	*env_file;	/* ssh-agent environment */
	int	 	 retval;	/* from calls */

	/* kill the agent */
	if ((retval = system(PATH_SSH_AGENT " -k")) != 0) {
		syslog(LOG_ERR, "%s: %s -k exited with status %d",
		    MODULE_NAME, PATH_SSH_AGENT, WEXITSTATUS(retval));
		return PAM_SESSION_ERR;
	}
	/* retrieve environment filename, then remove the file */
	if ((retval = pam_get_data(pamh, "ssh_agent_env",
	    (const void **)&env_file)) != PAM_SUCCESS)
		return retval;
	(void)unlink(env_file);
	return PAM_SUCCESS;
}


PAM_MODULE_ENTRY(MODULE_NAME);
