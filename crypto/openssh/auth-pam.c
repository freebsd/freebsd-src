/*
 * Copyright (c) 2000 Damien Miller.  All rights reserved.
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

#ifdef USE_PAM
#include "xmalloc.h"
#include "log.h"
#include "auth.h"
#include "auth-options.h"
#include "auth-pam.h"
#include "servconf.h"
#include "canohost.h"
#include "readpass.h"

extern char *__progname;

extern int use_privsep;

RCSID("$Id: auth-pam.c,v 1.55 2003/01/22 04:42:26 djm Exp $");

#define NEW_AUTHTOK_MSG \
	"Warning: Your password has expired, please change it now."
#define NEW_AUTHTOK_MSG_PRIVSEP \
	"Your password has expired, the session cannot proceed."

static int do_pam_conversation(int num_msg, const struct pam_message **msg,
	struct pam_response **resp, void *appdata_ptr);

/* module-local variables */
static struct pam_conv conv = {
	(int (*)())do_pam_conversation,
	NULL
};
static char *__pam_msg = NULL;
static pam_handle_t *__pamh = NULL;
static const char *__pampasswd = NULL;

/* states for do_pam_conversation() */
enum { INITIAL_LOGIN, OTHER } pamstate = INITIAL_LOGIN;
/* remember whether pam_acct_mgmt() returned PAM_NEW_AUTHTOK_REQD */
static int password_change_required = 0;
/* remember whether the last pam_authenticate() succeeded or not */
static int was_authenticated = 0;

/* Remember what has been initialised */
static int session_opened = 0;
static int creds_set = 0;

/* accessor which allows us to switch conversation structs according to
 * the authentication method being used */
void do_pam_set_conv(struct pam_conv *conv)
{
	pam_set_item(__pamh, PAM_CONV, conv);
}

/* start an authentication run */
int do_pam_authenticate(int flags)
{
	int retval = pam_authenticate(__pamh, flags);
	was_authenticated = (retval == PAM_SUCCESS);
	return retval;
}

/*
 * PAM conversation function.
 * There are two states this can run in.
 *
 * INITIAL_LOGIN mode simply feeds the password from the client into
 * PAM in response to PAM_PROMPT_ECHO_OFF, and collects output
 * messages with into __pam_msg.  This is used during initial
 * authentication to bypass the normal PAM password prompt.
 *
 * OTHER mode handles PAM_PROMPT_ECHO_OFF with read_passphrase()
 * and outputs messages to stderr. This mode is used if pam_chauthtok()
 * is called to update expired passwords.
 */
static int do_pam_conversation(int num_msg, const struct pam_message **msg,
	struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *reply;
	int count;
	char buf[1024];

	/* PAM will free this later */
	reply = xmalloc(num_msg * sizeof(*reply));

	for (count = 0; count < num_msg; count++) {
		if (pamstate == INITIAL_LOGIN) {
			/*
			 * We can't use stdio yet, queue messages for 
			 * printing later
			 */
			switch(PAM_MSG_MEMBER(msg, count, msg_style)) {
			case PAM_PROMPT_ECHO_ON:
				xfree(reply);
				return PAM_CONV_ERR;
			case PAM_PROMPT_ECHO_OFF:
				if (__pampasswd == NULL) {
					xfree(reply);
					return PAM_CONV_ERR;
				}
				reply[count].resp = xstrdup(__pampasswd);
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				if (PAM_MSG_MEMBER(msg, count, msg) != NULL) {
					message_cat(&__pam_msg, 
					    PAM_MSG_MEMBER(msg, count, msg));
				}
				reply[count].resp = xstrdup("");
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			default:
				xfree(reply);
				return PAM_CONV_ERR;
			}
		} else {
			/*
			 * stdio is connected, so interact directly
			 */
			switch(PAM_MSG_MEMBER(msg, count, msg_style)) {
			case PAM_PROMPT_ECHO_ON:
				fputs(PAM_MSG_MEMBER(msg, count, msg), stderr);
				fgets(buf, sizeof(buf), stdin);
				reply[count].resp = xstrdup(buf);
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_PROMPT_ECHO_OFF:
				reply[count].resp = 
				    read_passphrase(PAM_MSG_MEMBER(msg, count,
					msg), RP_ALLOW_STDIN);
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				if (PAM_MSG_MEMBER(msg, count, msg) != NULL)
					fprintf(stderr, "%s\n", 
					    PAM_MSG_MEMBER(msg, count, msg));
				reply[count].resp = xstrdup("");
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			default:
				xfree(reply);
				return PAM_CONV_ERR;
			}
		}
	}

	*resp = reply;

	return PAM_SUCCESS;
}

/* Called at exit to cleanly shutdown PAM */
void do_pam_cleanup_proc(void *context)
{
	int pam_retval = PAM_SUCCESS;

	if (__pamh && session_opened) {
		pam_retval = pam_close_session(__pamh, 0);
		if (pam_retval != PAM_SUCCESS)
			log("Cannot close PAM session[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
	}

	if (__pamh && creds_set) {
		pam_retval = pam_setcred(__pamh, PAM_DELETE_CRED);
		if (pam_retval != PAM_SUCCESS)
			debug("Cannot delete credentials[%d]: %.200s", 
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
	}

	if (__pamh) {
		pam_retval = pam_end(__pamh, pam_retval);
		if (pam_retval != PAM_SUCCESS)
			log("Cannot release PAM authentication[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
	}
}

/* Attempt password authentation using PAM */
int auth_pam_password(Authctxt *authctxt, const char *password)
{
	extern ServerOptions options;
	int pam_retval;
	struct passwd *pw = authctxt->pw;

	do_pam_set_conv(&conv);

	__pampasswd = password;

	pamstate = INITIAL_LOGIN;
	pam_retval = do_pam_authenticate(
	    options.permit_empty_passwd == 0 ? PAM_DISALLOW_NULL_AUTHTOK : 0);
	if (pam_retval == PAM_SUCCESS) {
		debug("PAM Password authentication accepted for "
		    "user \"%.100s\"", pw->pw_name);
		return 1;
	} else {
		debug("PAM Password authentication for \"%.100s\" "
		    "failed[%d]: %s", pw->pw_name, pam_retval, 
		    PAM_STRERROR(__pamh, pam_retval));
		return 0;
	}
}

/* Do account management using PAM */
int do_pam_account(char *username, char *remote_user)
{
	int pam_retval;

	do_pam_set_conv(&conv);

	if (remote_user) {
		debug("PAM setting ruser to \"%.200s\"", remote_user);
		pam_retval = pam_set_item(__pamh, PAM_RUSER, remote_user);
		if (pam_retval != PAM_SUCCESS)
			fatal("PAM set ruser failed[%d]: %.200s", pam_retval, 
			    PAM_STRERROR(__pamh, pam_retval));
	}

	pam_retval = pam_acct_mgmt(__pamh, 0);
	debug2("pam_acct_mgmt() = %d", pam_retval);
	switch (pam_retval) {
		case PAM_SUCCESS:
			/* This is what we want */
			break;
#if 0
		case PAM_NEW_AUTHTOK_REQD:
			message_cat(&__pam_msg, use_privsep ?
			    NEW_AUTHTOK_MSG_PRIVSEP : NEW_AUTHTOK_MSG);
			/* flag that password change is necessary */
			password_change_required = 1;
			/* disallow other functionality for now */
			no_port_forwarding_flag |= 2;
			no_agent_forwarding_flag |= 2;
			no_x11_forwarding_flag |= 2;
			break;
#endif
		default:
			log("PAM rejected by account configuration[%d]: "
			    "%.200s", pam_retval, PAM_STRERROR(__pamh, 
			    pam_retval));
			return(0);
	}

	return(1);
}

/* Do PAM-specific session initialisation */
void do_pam_session(char *username, const char *ttyname)
{
	int pam_retval;

	do_pam_set_conv(&conv);

	if (ttyname != NULL) {
		debug("PAM setting tty to \"%.200s\"", ttyname);
		pam_retval = pam_set_item(__pamh, PAM_TTY, ttyname);
		if (pam_retval != PAM_SUCCESS)
			fatal("PAM set tty failed[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
	}

	pam_retval = pam_open_session(__pamh, 0);
	if (pam_retval != PAM_SUCCESS)
		fatal("PAM session setup failed[%d]: %.200s",
		    pam_retval, PAM_STRERROR(__pamh, pam_retval));

	session_opened = 1;
}

/* Set PAM credentials */
void do_pam_setcred(int init)
{
	int pam_retval;

	if (__pamh == NULL)
		return;

	do_pam_set_conv(&conv);

	debug("PAM establishing creds");
	pam_retval = pam_setcred(__pamh, 
	    init ? PAM_ESTABLISH_CRED : PAM_REINITIALIZE_CRED);
	if (pam_retval != PAM_SUCCESS) {
		if (was_authenticated)
			fatal("PAM setcred failed[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
		else
			debug("PAM setcred failed[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
	} else
		creds_set = 1;
}

/* accessor function for file scope static variable */
int is_pam_password_change_required(void)
{
	return password_change_required;
}

/*
 * Have user change authentication token if pam_acct_mgmt() indicated
 * it was expired.  This needs to be called after an interactive
 * session is established and the user's pty is connected to
 * stdin/stdout/stderr.
 */
void do_pam_chauthtok(void)
{
	int pam_retval;

	do_pam_set_conv(&conv);

	if (password_change_required) {
		if (use_privsep)
			fatal("Password changing is currently unsupported"
			    " with privilege separation");
		pamstate = OTHER;
		pam_retval = pam_chauthtok(__pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
		if (pam_retval != PAM_SUCCESS)
			fatal("PAM pam_chauthtok failed[%d]: %.200s",
			    pam_retval, PAM_STRERROR(__pamh, pam_retval));
#if 0
		/* XXX: This would need to be done in the parent process,
		 * but there's currently no way to pass such request. */
		no_port_forwarding_flag &= ~2;
		no_agent_forwarding_flag &= ~2;
		no_x11_forwarding_flag &= ~2;
		if (!no_port_forwarding_flag && options.allow_tcp_forwarding)
			channel_permit_all_opens();
#endif
	}
}

/* Cleanly shutdown PAM */
void finish_pam(void)
{
	do_pam_cleanup_proc(NULL);
	fatal_remove_cleanup(&do_pam_cleanup_proc, NULL);
}

/* Start PAM authentication for specified account */
void start_pam(const char *user)
{
	int pam_retval;
	extern ServerOptions options;
	extern u_int utmp_len;
	const char *rhost;

	debug("Starting up PAM with username \"%.200s\"", user);

	pam_retval = pam_start(SSHD_PAM_SERVICE, user, &conv, &__pamh);

	if (pam_retval != PAM_SUCCESS)
		fatal("PAM initialisation failed[%d]: %.200s",
		    pam_retval, PAM_STRERROR(__pamh, pam_retval));

	rhost = get_remote_name_or_ip(utmp_len, options.verify_reverse_mapping);
	debug("PAM setting rhost to \"%.200s\"", rhost);

	pam_retval = pam_set_item(__pamh, PAM_RHOST, rhost);
	if (pam_retval != PAM_SUCCESS)
		fatal("PAM set rhost failed[%d]: %.200s", pam_retval,
		    PAM_STRERROR(__pamh, pam_retval));
#ifdef PAM_TTY_KLUDGE
	/*
	 * Some PAM modules (e.g. pam_time) require a TTY to operate,
	 * and will fail in various stupid ways if they don't get one.
	 * sshd doesn't set the tty until too late in the auth process and may
	 * not even need one (for tty-less connections)
	 * Kludge: Set a fake PAM_TTY
	 */
	pam_retval = pam_set_item(__pamh, PAM_TTY, "NODEVssh");
	if (pam_retval != PAM_SUCCESS)
		fatal("PAM set tty failed[%d]: %.200s",
		    pam_retval, PAM_STRERROR(__pamh, pam_retval));
#endif /* PAM_TTY_KLUDGE */

	fatal_add_cleanup(&do_pam_cleanup_proc, NULL);
}

/* Return list of PAM environment strings */
char **fetch_pam_environment(void)
{
#ifdef HAVE_PAM_GETENVLIST
	return(pam_getenvlist(__pamh));
#else /* HAVE_PAM_GETENVLIST */
	return(NULL);
#endif /* HAVE_PAM_GETENVLIST */
}

void free_pam_environment(char **env)
{
	int i;

	if (env != NULL) {
		for (i = 0; env[i] != NULL; i++)
			xfree(env[i]);
	}
}

/* Print any messages that have been generated during authentication */
/* or account checking to stderr */
void print_pam_messages(void)
{
	if (__pam_msg != NULL)
		fputs(__pam_msg, stderr);
}

/* Append a message to buffer */
void message_cat(char **p, const char *a)
{
	char *cp;
	size_t new_len;

	new_len = strlen(a);

	if (*p) {
		size_t len = strlen(*p);

		*p = xrealloc(*p, new_len + len + 2);
		cp = *p + len;
	} else
		*p = cp = xmalloc(new_len + 2);

	memcpy(cp, a, new_len);
	cp[new_len] = '\n';
	cp[new_len + 1] = '\0';
}

#endif /* USE_PAM */
