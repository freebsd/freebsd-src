/*-
 * Copyright (c) 1999, 2000 Andrew J. Korty
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
#include <sys/queue.h>

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

#include <openssl/dsa.h>

#include "includes.h"
#include "rsa.h"
#include "key.h"
#include "ssh.h"
#include "authfd.h"
#include "authfile.h"

#define	MODULE_NAME	"pam_ssh"
#define	NEED_PASSPHRASE	"Need passphrase for %s (%s).\nEnter passphrase: "
#define	PATH_SSH_AGENT	"/usr/bin/ssh-agent"


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


/*
 * The following set of functions allow the module to manipulate the
 * environment without calling the putenv() or setenv() stdlib functions.
 * At least one version of these functions, on the first call, copies
 * the environment into dynamically-allocated memory and then augments
 * it.  On subsequent calls, the realloc() call is used to grow the
 * previously allocated buffer.  Problems arise when the "environ"
 * variable is changed to point to static memory after putenv()/setenv()
 * have been called.
 * 
 * We don't use putenv() or setenv() in case the application subsequently
 * manipulates environ, (e.g., to clear the environment by pointing
 * environ at an array of one element equal to NULL).
 */

SLIST_HEAD(env_head, env_entry);

struct env_entry {
	char			*ee_env;
	SLIST_ENTRY(env_entry)	 ee_entries;
};

typedef struct env {
	char		**e_environ_orig;
	char		**e_environ_new;
	int		  e_count;
	struct env_head	  e_head;
	int		  e_committed;
} ENV;

extern char **environ;


static ENV *
env_new(void)
{
	ENV	*self;

	if (!(self = malloc(sizeof (ENV)))) {
		syslog(LOG_CRIT, "%m");
		return NULL;
	}
	SLIST_INIT(&self->e_head);
	self->e_count = 0;
	self->e_committed = 0;
	return self;
}


static int
env_put(ENV *self, char *s)
{
	struct env_entry	*env;

	if (!(env = malloc(sizeof (struct env_entry))) ||
	    !(env->ee_env = strdup(s))) {
		syslog(LOG_CRIT, "%m");
		return PAM_SERVICE_ERR;
	}
	SLIST_INSERT_HEAD(&self->e_head, env, ee_entries);
	++self->e_count;
	return PAM_SUCCESS;
}


static void
env_swap(ENV *self, int which)
{
	environ = which ? self->e_environ_new : self->e_environ_orig;
}


static int
env_commit(ENV *self)
{
	int			  n;
	struct env_entry	 *p;
	char 			**v;

	for (v = environ, n = 0; v && *v; v++, n++)
		;
	if (!(v = malloc((n + self->e_count + 1) * sizeof (char *)))) {
		syslog(LOG_CRIT, "%m");
		return PAM_SERVICE_ERR;
	}
	self->e_committed = 1;
	(void)memcpy(v, environ, n * sizeof (char *));
	SLIST_FOREACH(p, &self->e_head, ee_entries)
		v[n++] = p->ee_env;
	v[n] = NULL;
	self->e_environ_orig = environ;
	self->e_environ_new = v;
	env_swap(self, 1);
	return PAM_SUCCESS;
}


static void
env_destroy(ENV *self)
{
	struct env_entry	 *p;

	if (self->e_committed)
		env_swap(self, 0);
	env_swap(self, 0);
	SLIST_FOREACH(p, &self->e_head, ee_entries) {
		free(p->ee_env);
		free(p);
	}
	if (self->e_committed)
		free(self->e_environ_new);
	free(self);
}


void
env_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
	if (data)
		env_destroy(data);
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
	Key		 key;			/* user's private key */
	int		 options;		/* module options */
	const char	*pass;			/* passphrase */
	char		*prompt;		/* passphrase prompt */
	Key		 public_key;		/* user's public key */
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
	key.type = KEY_RSA;
	key.rsa = RSA_new();
	public_key.type = KEY_RSA;
	public_key.rsa = RSA_new();
	saved_uid = getuid();
	(void)setreuid(pwent->pw_uid, saved_uid);
	retval = load_public_key(identity, &public_key, &comment_pub);
	(void)setuid(saved_uid);
	if (!retval) {
		free(identity);
		return PAM_AUTH_ERR;
	}
	RSA_free(public_key.rsa);
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
	retval = load_private_key(identity, pass, &key, &comment_priv);
	free(identity);
	(void)setuid(saved_uid);
	if (!retval)
		return PAM_AUTH_ERR;
	/*
	 * Save the key and comment to pass to ssh-agent in the session
	 * phase.
	 */
	if ((retval = pam_set_data(pamh, "ssh_private_key", key.rsa,
	    rsa_cleanup)) != PAM_SUCCESS) {
		RSA_free(key.rsa);
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
	Key		 key;			/* user's private key */
	FILE		*pipe;			/* ssh-agent handle */
	const PASSWD	*pwent;			/* user's passwd entry */
	int		 retval;		/* from calls */
	uid_t		 saved_uid;		/* caller's uid */
	ENV		*ssh_env;		/* env handle */
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
	if (!(ssh_env = env_new()))
		return PAM_SESSION_ERR;
	if ((retval = pam_set_data(pamh, "ssh_env_handle", ssh_env,
	    env_cleanup)) != PAM_SUCCESS)
		return retval;
	while (fgets(parse, sizeof parse, pipe)) {
		if (env_fp)
			(void)fputs(parse, env_fp);
		/*
		 * Save environment for application with pam_putenv()
		 * but also with env_* functions for our own call to
		 * ssh_get_authentication_connection().
		 */
		if (strchr(parse, '=') && (env_end = strchr(parse, ';'))) {
			*env_end = '\0';
			/* pass to the application ... */
			if (!((retval = pam_putenv(pamh, parse)) ==
			    PAM_SUCCESS)) {
				(void)pclose(pipe);
				if (env_fp)
					(void)fclose(env_fp);
				env_destroy(ssh_env);
				return PAM_SERVICE_ERR;
			}
			env_put(ssh_env, parse);
		}
	}
	if (env_fp)
		(void)fclose(env_fp);
	switch (retval = pclose(pipe)) {
	case -1:
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, PATH_SSH_AGENT);
		env_destroy(ssh_env);
		return PAM_SESSION_ERR;
	case 0:
		break;
	case 127:
		syslog(LOG_ERR, "%s: cannot execute %s", MODULE_NAME,
		    PATH_SSH_AGENT);
		env_destroy(ssh_env);
		return PAM_SESSION_ERR;
	default:
		syslog(LOG_ERR, "%s: %s exited with status %d",
		    MODULE_NAME, PATH_SSH_AGENT, WEXITSTATUS(retval));
		env_destroy(ssh_env);
		return PAM_SESSION_ERR;
	}
	key.type = KEY_RSA;
	/* connect to the agent and hand off the private key */
	if ((retval = pam_get_data(pamh, "ssh_private_key",
	    (const void **)&key.rsa)) != PAM_SUCCESS ||
	    (retval = pam_get_data(pamh, "ssh_key_comment",
	    (const void **)&comment)) != PAM_SUCCESS ||
	    (retval = env_commit(ssh_env)) != PAM_SUCCESS) {
		env_destroy(ssh_env);
		return retval;
	}
	if (!(ac = ssh_get_authentication_connection())) {
		syslog(LOG_ERR, "%s: could not connect to agent",
		    MODULE_NAME);
		env_destroy(ssh_env);
		return PAM_SESSION_ERR;
	}
	retval = ssh_add_identity(ac, &key, comment);
	ssh_close_authentication_connection(ac);
	env_swap(ssh_env, 0);
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
	ENV		*ssh_env;	/* env handle */

	if ((retval = pam_get_data(pamh, "ssh_env_handle",
	    (const void **)&ssh_env)) != PAM_SUCCESS)
		return retval;
	env_swap(ssh_env, 1);
	/* kill the agent */
	retval = system(PATH_SSH_AGENT " -k");
	env_destroy(ssh_env);
	switch (retval) {
	case -1:
		syslog(LOG_ERR, "%s: %s -k: %m", MODULE_NAME,
		    PATH_SSH_AGENT);
		return PAM_SESSION_ERR;
	case 0:
		break;
	case 127:
		syslog(LOG_ERR, "%s: cannot execute %s -k", MODULE_NAME,
		    PATH_SSH_AGENT);
		return PAM_SESSION_ERR;
	default:
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
