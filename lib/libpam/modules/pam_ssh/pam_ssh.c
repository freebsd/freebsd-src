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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <pwd.h>
#include <signal.h>
#include <ssh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PAM_SM_AUTH
#define	PAM_SM_SESSION
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>

#include <openssl/dsa.h>
#include <openssl/evp.h>

#include "key.h"
#include "authfd.h"
#include "authfile.h"
#include "log.h"
#include "pam_ssh.h"

int IPv4or6 = AF_UNSPEC;

/*
 * Generic cleanup function for SSH "Key" type.
 */

void
key_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
	if (data)
		key_free(data);
}


/*
 * Generic PAM cleanup function for this module.
 */

void
ssh_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
	if (data)
		free(data);
}


/*
 * Authenticate a user's key by trying to decrypt it with the password
 * provided.  The key and its comment are then stored for later
 * retrieval by the session phase.  An increasing index is embedded in
 * the PAM variable names so this function may be called multiple times
 * for multiple keys.
 */

int
auth_via_key(pam_handle_t *pamh, int type, const char *file,
    const char *dir, const struct passwd *user, const char *pass)
{
	char		*comment;		/* private key comment */
	char		*data_name;		/* PAM state */
	static int	 index = 0;		/* for saved keys */
	Key		*key;			/* user's key */
	char		*path;			/* to key files */
	int		 retval;		/* from calls */
	uid_t		 saved_uid;		/* caller's uid */

	/* locate the user's private key file */
	if (!asprintf(&path, "%s/%s", dir, file)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		return PAM_SERVICE_ERR;
	}
	saved_uid = geteuid();
	/*
	 * Try to decrypt the private key with the passphrase provided.
	 * If success, the user is authenticated.
	 */
	seteuid(user->pw_uid);
	key = key_load_private_type(type, path, pass, &comment);
	free(path);
	seteuid(saved_uid);
	if (key == NULL)
		return PAM_AUTH_ERR;
	/*
	 * Save the key and comment to pass to ssh-agent in the session
	 * phase.
	 */
	if (!asprintf(&data_name, "ssh_private_key_%d", index)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		free(comment);
		return PAM_SERVICE_ERR;
	}
	retval = pam_set_data(pamh, data_name, key, key_cleanup);
	free(data_name);
	if (retval != PAM_SUCCESS) {
		key_free(key);
		free(comment);
		return retval;
	}
	if (!asprintf(&data_name, "ssh_key_comment_%d", index)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		free(comment);
		return PAM_SERVICE_ERR;
	}
	retval = pam_set_data(pamh, data_name, comment, ssh_cleanup);
	free(data_name);
	if (retval != PAM_SUCCESS) {
		free(comment);
		return retval;
	}
	++index;
	return PAM_SUCCESS;
}


PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options	 options;		/* module options */
	int		 authenticated;		/* user authenticated? */
	char		*dotdir;		/* .ssh2 dir name */
	struct dirent	*dotdir_ent;		/* .ssh2 dir entry */
	DIR		*dotdir_p;		/* .ssh2 dir pointer */
	const char	*pass;			/* passphrase */
	struct passwd	*pwd;			/* user's passwd entry */
	struct passwd	*pwd_keep;		/* our own copy */
	int		 retval;		/* from calls */
	int		 pam_auth_dsa;		/* Authorised via DSA */
	int		 pam_auth_rsa;		/* Authorised via RSA */
	const char	*user;			/* username */

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_user(pamh, &user, NULL);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);
	pwd = getpwnam(user);
	if (pwd == NULL || pwd->pw_dir == NULL)
		/* delay? */
		PAM_RETURN(PAM_AUTH_ERR);

	PAM_LOG("Got user: %s", user);

	/*
	 * Pass prompt message to application and receive
	 * passphrase.
	 */
	retval = pam_get_pass(pamh, &pass, NEED_PASSPHRASE, &options);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);
	OpenSSL_add_all_algorithms();	/* required for DSA */

	PAM_LOG("Got passphrase");

	/*
	 * Either the DSA or the RSA key will authenticate us, but if
	 * we can decrypt both, we'll do so here so we can cache them in
	 * the session phase.
	 */
	if (!asprintf(&dotdir, "%s/%s", pwd->pw_dir, SSH_CLIENT_DIR)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		PAM_RETURN(PAM_SERVICE_ERR);
	}
	pam_auth_dsa = auth_via_key(pamh, KEY_DSA, SSH_CLIENT_ID_DSA, dotdir,
	    pwd, pass);
	pam_auth_rsa = auth_via_key(pamh, KEY_RSA1, SSH_CLIENT_IDENTITY, dotdir,
	    pwd, pass);
	authenticated = 0;
	if (pam_auth_dsa == PAM_SUCCESS)
		authenticated++;
	if (pam_auth_rsa == PAM_SUCCESS)
		authenticated++;

	PAM_LOG("Done pre-authenticating; got %d", authenticated);

	/*
	 * Compatibility with SSH2 from SSH Communications Security.
	 */
	if (!asprintf(&dotdir, "%s/%s", pwd->pw_dir, SSH2_CLIENT_DIR)) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		PAM_RETURN(PAM_SERVICE_ERR);
	}
	/*
	 * Try to load anything that looks like a private key.  For
	 * now, we only support DSA and RSA keys.
	 */
	dotdir_p = opendir(dotdir);
	while (dotdir_p && (dotdir_ent = readdir(dotdir_p))) {
		/* skip public keys */
		if (strcmp(&dotdir_ent->d_name[dotdir_ent->d_namlen -
		    strlen(SSH2_PUB_SUFFIX)], SSH2_PUB_SUFFIX) == 0)
			continue;
		/* DSA keys */
		if (strncmp(dotdir_ent->d_name, SSH2_DSA_PREFIX,
		    strlen(SSH2_DSA_PREFIX)) == 0)
			retval = auth_via_key(pamh, KEY_DSA,
			    dotdir_ent->d_name, dotdir, pwd, pass);
		/* RSA keys */
		else if (strncmp(dotdir_ent->d_name, SSH2_RSA_PREFIX,
		    strlen(SSH2_RSA_PREFIX)) == 0)
			retval = auth_via_key(pamh, KEY_RSA,
			    dotdir_ent->d_name, dotdir, pwd, pass);
		/* skip other files */
		else
			continue;
		authenticated += (retval == PAM_SUCCESS);
	}
	if (!authenticated) {
		PAM_VERBOSE_ERROR("SSH authentication refused");
		PAM_RETURN(PAM_AUTH_ERR);
	}

	PAM_LOG("Done authenticating; got %d", authenticated);

	/*
	 * Copy the passwd entry (in case successive calls are made)
	 * and save it for the session phase.
	 */
	pwd_keep = malloc(sizeof *pwd);
	if (pwd_keep == NULL) {
		syslog(LOG_CRIT, "%m");
		PAM_RETURN(PAM_SERVICE_ERR);
	}
	memcpy(pwd_keep, pwd, sizeof *pwd_keep);
	retval = pam_set_data(pamh, "ssh_passwd_entry", pwd_keep, ssh_cleanup);
	if (retval != PAM_SUCCESS) {
		free(pwd_keep);
		PAM_RETURN(retval);
	}

	PAM_LOG("Saved ssh_passwd_entry");

	PAM_RETURN(PAM_SUCCESS);
}


PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options	 options;		/* module options */

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}


typedef AuthenticationConnection AC;

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options	 options;		/* module options */
	AC		*ac;			/* to ssh-agent */
	char		*agent_socket;		/* agent socket */
	char		*comment;		/* on private key */
	char		*env_end;		/* end of env */
	char		*env_file;		/* to store env */
	FILE		*env_fp;		/* env_file handle */
	char		*env_value;		/* envariable value */
	char		*data_name;		/* PAM state */
	int		 final;			/* final return value */
	int		 index;			/* for saved keys */
	Key		*key;			/* user's private key */
	FILE		*pipe;			/* ssh-agent handle */
	struct passwd	*pwd;			/* user's passwd entry */
	int		 retval;		/* from calls */
	uid_t		 saved_uid;		/* caller's uid */
	const char	*tty;			/* tty or display name */
	char		 hname[MAXHOSTNAMELEN];	/* local hostname */
	char		 env_string[BUFSIZ];   	/* environment string */

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	/* dump output of ssh-agent in ~/.ssh */
	retval = pam_get_data(pamh, "ssh_passwd_entry", (const void **)&pwd);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got ssh_passwd_entry");

	/* use the tty or X display name in the filename */
	retval = pam_get_item(pamh, PAM_TTY, (const void **)&tty);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got TTY");

	if (gethostname(hname, sizeof hname) == 0) {
		if (asprintf(&env_file, "%s/.ssh/agent-%s%s%s",
		    pwd->pw_dir, hname, *tty == ':' ? "" : ":", tty)
		    == -1) {
			syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
			PAM_RETURN(PAM_SERVICE_ERR);
		}
	}
	else if (asprintf(&env_file, "%s/.ssh/agent-%s", pwd->pw_dir,
	    tty) == -1) {
		syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
		PAM_RETURN(PAM_SERVICE_ERR);
	}

	PAM_LOG("Got env_file: %s", env_file);

	/* save the filename so we can delete the file on session close */
	retval = pam_set_data(pamh, "ssh_agent_env", env_file, ssh_cleanup);
	if (retval != PAM_SUCCESS) {
		free(env_file);
		PAM_RETURN(retval);
	}

	PAM_LOG("Saved env_file");

	/* start the agent as the user */
	saved_uid = geteuid();
	seteuid(pwd->pw_uid);
	env_fp = fopen(env_file, "w");
	if (env_fp != NULL)
		chmod(env_file, S_IRUSR);
	pipe = popen(SSH_AGENT, "r");
	seteuid(saved_uid);
	if (!pipe) {
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, SSH_AGENT);
		if (env_fp)
			fclose(env_fp);
		PAM_RETURN(PAM_SESSION_ERR);
	}

	PAM_LOG("Agent started as user");

	/*
	 * Save environment for application with pam_putenv().
	 */
	agent_socket = NULL;
	while (fgets(env_string, sizeof env_string, pipe)) {
		if (env_fp)
			fputs(env_string, env_fp);
		env_value = strchr(env_string, '=');
		if (env_value == NULL)
			continue;
		env_end = strchr(env_value, ';');
		if (env_end == NULL)
				continue;
		*env_end = '\0';
		/* pass to the application ... */
		retval = pam_putenv(pamh, env_string);
		if (retval != PAM_SUCCESS) {
			pclose(pipe);
			if (env_fp)
				fclose(env_fp);
			PAM_RETURN(PAM_SERVICE_ERR);
		}
		putenv(env_string);

		PAM_LOG("Put to environment: %s", env_string);

		*env_value++ = '\0';
		if (strcmp(&env_string[strlen(env_string) -
		    strlen(ENV_SOCKET_SUFFIX)], ENV_SOCKET_SUFFIX) == 0) {
			agent_socket = strdup(env_value);
			if (agent_socket == NULL) {
				syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
				PAM_RETURN(PAM_SERVICE_ERR);
			}
		}
		else if (strcmp(&env_string[strlen(env_string) -
		    strlen(ENV_PID_SUFFIX)], ENV_PID_SUFFIX) == 0) {
			env_value = strdup(env_value);
			if (env_value == NULL) {
				syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
				PAM_RETURN(PAM_SERVICE_ERR);
			}
			retval = pam_set_data(pamh, "ssh_agent_pid",
			    env_value, ssh_cleanup);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Environment write successful");
		}
	}
	if (env_fp)
		fclose(env_fp);
	retval = pclose(pipe);
	switch (retval) {
	case -1:
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, SSH_AGENT);
		PAM_RETURN(PAM_SESSION_ERR);
	case 0:
		break;
	case 127:
		syslog(LOG_ERR, "%s: cannot execute %s", MODULE_NAME,
		    SSH_AGENT);
		PAM_RETURN(PAM_SESSION_ERR);
	default:
		syslog(LOG_ERR, "%s: %s exited %s %d", MODULE_NAME,
		    SSH_AGENT, WIFSIGNALED(retval) ? "on signal" :
		    "with status", WIFSIGNALED(retval) ? WTERMSIG(retval) :
		    WEXITSTATUS(retval));
		PAM_RETURN(PAM_SESSION_ERR);
	}
	if (agent_socket == NULL)
		PAM_RETURN(PAM_SESSION_ERR);

	PAM_LOG("Environment saved");

	/* connect to the agent */
	ac = ssh_get_authentication_connection();
	if (!ac) {
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, agent_socket);
		PAM_RETURN(PAM_SESSION_ERR);
	}

	PAM_LOG("Connected to agent");

	/* hand off each private key to the agent */
	final = 0;
	for (index = 0; ; index++) {
		if (!asprintf(&data_name, "ssh_private_key_%d", index)) {
			syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
			ssh_close_authentication_connection(ac);
			PAM_RETURN(PAM_SERVICE_ERR);
		}
		retval = pam_get_data(pamh, data_name, (const void **)&key);
		free(data_name);
		if (retval != PAM_SUCCESS)
			break;
		if (!asprintf(&data_name, "ssh_key_comment_%d", index)) {
			syslog(LOG_CRIT, "%s: %m", MODULE_NAME);
			ssh_close_authentication_connection(ac);
			PAM_RETURN(PAM_SERVICE_ERR);
		}
		retval = pam_get_data(pamh, data_name, (const void **)&comment);
		free(data_name);
		if (retval != PAM_SUCCESS)
			break;
		retval = ssh_add_identity(ac, key, comment);
		if (!final)
			final = retval;
	}
	ssh_close_authentication_connection(ac);

	PAM_LOG("Keys handed off");

	PAM_RETURN(final ? PAM_SUCCESS : PAM_SESSION_ERR);
}


PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options	 options;	/* module options */
	const char	*env_file;	/* ssh-agent environment */
	pid_t		 pid;		/* ssh-agent process id */
	int	 	 retval;	/* from calls */
	const char	*ssh_agent_pid;	/* ssh-agent pid string */

	pam_std_option(&options, NULL, argc, argv);

	PAM_LOG("Options processed");

	/* retrieve environment filename, then remove the file */
	retval = pam_get_data(pamh, "ssh_agent_env", (const void **)&env_file);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);
	unlink(env_file);

	PAM_LOG("Got ssh_agent_env");

	/* retrieve the agent's process id */
	retval = pam_get_data(pamh, "ssh_agent_pid", (const void **)&ssh_agent_pid);
	if (retval != PAM_SUCCESS)
		PAM_RETURN(retval);

	PAM_LOG("Got ssh_agent_pid");

	/*
	 * Kill the agent.  SSH2 from SSH Communications Security does
	 * not have a -k option, so we just call kill().
	 */
	pid = atoi(ssh_agent_pid);
	if (pid <= 0)
		PAM_RETURN(PAM_SESSION_ERR);
	if (kill(pid, SIGTERM) != 0) {
		syslog(LOG_ERR, "%s: %s: %m", MODULE_NAME, ssh_agent_pid);
		PAM_RETURN(PAM_SESSION_ERR);
	}

	PAM_LOG("Agent killed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_ssh");
