/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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

#include "includes.h"
RCSID("$FreeBSD$");

#ifdef USE_PAM
#include <security/pam_appl.h>

#include "auth.h"
#include "auth-pam.h"
#include "buffer.h"
#include "bufaux.h"
#include "canohost.h"
#include "log.h"
#include "monitor_wrap.h"
#include "msg.h"
#include "packet.h"
#include "readpass.h"
#include "servconf.h"
#include "ssh2.h"
#include "xmalloc.h"

#ifdef USE_POSIX_THREADS
#include <pthread.h>
#else
/*
 * Simulate threads with processes.
 */
typedef pid_t pthread_t;

static void
pthread_exit(void *value __unused)
{
	_exit(0);
}

static int
pthread_create(pthread_t *thread, const void *attr __unused,
    void *(*thread_start)(void *), void *arg)
{
	pid_t pid;

	switch ((pid = fork())) {
	case -1:
		error("fork(): %s", strerror(errno));
		return (-1);
	case 0:
		thread_start(arg);
		_exit(1);
	default:
		*thread = pid;
		return (0);
	}
}

static int
pthread_cancel(pthread_t thread)
{
	return (kill(thread, SIGTERM));
}

static int
pthread_join(pthread_t thread, void **value __unused)
{
	int status;

	waitpid(thread, &status, 0);
	return (status);
}
#endif


static pam_handle_t *pam_handle;
static int pam_err;
static int pam_authenticated;
static int pam_new_authtok_reqd;
static int pam_session_open;
static int pam_cred_established;

struct pam_ctxt {
	pthread_t	 pam_thread;
	int		 pam_psock;
	int		 pam_csock;
	int		 pam_done;
};

static void pam_free_ctx(void *);

/*
 * Conversation function for authentication thread.
 */
static int
pam_thread_conv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{
	Buffer buffer;
	struct pam_ctxt *ctxt;
	int i;

	ctxt = data;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);
	*resp = xmalloc(n * sizeof **resp);
	buffer_init(&buffer);
	for (i = 0; i < n; ++i) {
		(*resp)[i].resp_retcode = 0;
		(*resp)[i].resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_csock, msg[i]->msg_style, &buffer);
			ssh_msg_recv(ctxt->pam_csock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			(*resp)[i].resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_PROMPT_ECHO_ON:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_csock, msg[i]->msg_style, &buffer);
			ssh_msg_recv(ctxt->pam_csock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			(*resp)[i].resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_ERROR_MSG:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_csock, msg[i]->msg_style, &buffer);
			break;
		case PAM_TEXT_INFO:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_csock, msg[i]->msg_style, &buffer);
			break;
		default:
			goto fail;
		}
		buffer_clear(&buffer);
	}
	buffer_free(&buffer);
	return (PAM_SUCCESS);
 fail:
	while (i)
		xfree(resp[--i]);
	xfree(*resp);
	*resp = NULL;
	buffer_free(&buffer);
	return (PAM_CONV_ERR);
}

/*
 * Authentication thread.
 */
static void *
pam_thread(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;
	Buffer buffer;
	struct pam_conv pam_conv = { pam_thread_conv, ctxt };

	buffer_init(&buffer);
	pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *)&pam_conv);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_err = pam_authenticate(pam_handle, 0);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_err = pam_acct_mgmt(pam_handle, 0);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	buffer_put_cstring(&buffer, "OK");
	ssh_msg_send(ctxt->pam_csock, PAM_SUCCESS, &buffer);
	buffer_free(&buffer);
	pthread_exit(NULL);
 auth_fail:
	buffer_put_cstring(&buffer,
	    pam_strerror(pam_handle, pam_err));
	ssh_msg_send(ctxt->pam_csock, PAM_AUTH_ERR, &buffer);
	buffer_free(&buffer);
	pthread_exit(NULL);
}

static void
pam_thread_cleanup(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;

	pthread_cancel(ctxt->pam_thread);
	pthread_join(ctxt->pam_thread, NULL);
	close(ctxt->pam_psock);
	close(ctxt->pam_csock);
}

static int
pam_null_conv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{

	return (PAM_CONV_ERR);
}

static struct pam_conv null_conv = { pam_null_conv, NULL };

static void
pam_cleanup(void *arg)
{
	(void)arg;
	debug("PAM: cleanup");
	pam_set_item(pam_handle, PAM_CONV, (const void *)&null_conv);
	if (pam_cred_established) {
		pam_setcred(pam_handle, PAM_DELETE_CRED);
		pam_cred_established = 0;
	}
	if (pam_session_open) {
		pam_close_session(pam_handle, PAM_SILENT);
		pam_session_open = 0;
	}
	pam_authenticated = pam_new_authtok_reqd = 0;
	pam_end(pam_handle, pam_err);
	pam_handle = NULL;
}

static int
pam_init(const char *user)
{
	extern ServerOptions options;
	extern u_int utmp_len;
	const char *pam_rhost, *pam_user;

	if (pam_handle != NULL) {
		/* We already have a PAM context; check if the user matches */
		pam_err = pam_get_item(pam_handle,
		    PAM_USER, (const void **)&pam_user);
		if (pam_err == PAM_SUCCESS && strcmp(user, pam_user) == 0)
			return (0);
		fatal_remove_cleanup(pam_cleanup, NULL);
		pam_end(pam_handle, pam_err);
		pam_handle = NULL;
	}
	debug("PAM: initializing for \"%s\"", user);
	pam_err = pam_start("sshd", user, &null_conv, &pam_handle);
	if (pam_err != PAM_SUCCESS)
		return (-1);
	pam_rhost = get_remote_name_or_ip(utmp_len,
	    options.verify_reverse_mapping);
	debug("PAM: setting PAM_RHOST to \"%s\"", pam_rhost);
	pam_err = pam_set_item(pam_handle, PAM_RHOST, pam_rhost);
	if (pam_err != PAM_SUCCESS) {
		pam_end(pam_handle, pam_err);
		pam_handle = NULL;
		return (-1);
	}
	fatal_add_cleanup(pam_cleanup, NULL);
	return (0);
}

static void *
pam_init_ctx(Authctxt *authctxt)
{
	struct pam_ctxt *ctxt;
	int socks[2];

	/* Initialize PAM */
	if (pam_init(authctxt->user) == -1) {
		error("PAM: initialization failed");
		return (NULL);
	}

	ctxt = xmalloc(sizeof *ctxt);
	ctxt->pam_done = 0;

	/* Start the authentication thread */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, socks) == -1) {
		error("PAM: failed create sockets: %s", strerror(errno));
		xfree(ctxt);
		return (NULL);
	}
	ctxt->pam_psock = socks[0];
	ctxt->pam_csock = socks[1];
	if (pthread_create(&ctxt->pam_thread, NULL, pam_thread, ctxt) == -1) {
		error("PAM: failed to start authentication thread: %s",
		    strerror(errno));
		close(socks[0]);
		close(socks[1]);
		xfree(ctxt);
		return (NULL);
	}
	fatal_add_cleanup(pam_thread_cleanup, ctxt);
	return (ctxt);
}

static int
pam_query(void *ctx, char **name, char **info,
    u_int *num, char ***prompts, u_int **echo_on)
{
	Buffer buffer;
	struct pam_ctxt *ctxt = ctx;
	size_t plen;
	u_char type;
	char *msg;

	buffer_init(&buffer);
	*name = xstrdup("");
	*info = xstrdup("");
	*prompts = xmalloc(sizeof(char *));
	**prompts = NULL;
	plen = 0;
	*echo_on = xmalloc(sizeof(u_int));
	while (ssh_msg_recv(ctxt->pam_psock, &buffer) == 0) {
		type = buffer_get_char(&buffer);
		msg = buffer_get_string(&buffer, NULL);
		switch (type) {
		case PAM_PROMPT_ECHO_ON:
		case PAM_PROMPT_ECHO_OFF:
			*num = 1;
			**prompts = xrealloc(**prompts, plen + strlen(msg) + 1);
			plen += sprintf(**prompts + plen, "%s", msg);
			**echo_on = (type == PAM_PROMPT_ECHO_ON);
			xfree(msg);
			return (0);
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			/* accumulate messages */
			**prompts = xrealloc(**prompts, plen + strlen(msg) + 1);
			plen += sprintf(**prompts + plen, "%s", msg);
			xfree(msg);
			break;
		case PAM_SUCCESS:
		case PAM_AUTH_ERR:
			if (**prompts != NULL) {
				/* drain any accumulated messages */
#if 0 /* not compatible with privsep */
				packet_start(SSH2_MSG_USERAUTH_BANNER);
				packet_put_cstring(**prompts);
				packet_put_cstring("");
				packet_send();
				packet_write_wait();
#endif
				xfree(**prompts);
				**prompts = NULL;
			}
			if (type == PAM_SUCCESS) {
				*num = 0;
				**echo_on = 0;
				ctxt->pam_done = 1;
				xfree(msg);
				return (0);
			}
			error("PAM: %s", msg);
		default:
			*num = 0;
			**echo_on = 0;
			xfree(msg);
			ctxt->pam_done = -1;
			return (-1);
		}
	}
	return (-1);
}

static int
pam_respond(void *ctx, u_int num, char **resp)
{
	Buffer buffer;
	struct pam_ctxt *ctxt = ctx;
	char *msg;

	debug2("PAM: %s", __func__);
	switch (ctxt->pam_done) {
	case 1:
		pam_authenticated = 1;
		return (0);
	case 0:
		break;
	default:
		return (-1);
	}
	if (num != 1) {
		error("PAM: expected one response, got %u", num);
		return (-1);
	}
	buffer_init(&buffer);
	buffer_put_cstring(&buffer, *resp);
	ssh_msg_send(ctxt->pam_psock, PAM_AUTHTOK, &buffer);
	buffer_free(&buffer);
	return (1);
}

static void
pam_free_ctx(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;

	fatal_remove_cleanup(pam_thread_cleanup, ctxt);
	pam_thread_cleanup(ctxtp);
	xfree(ctxt);
	/*
	 * We don't call pam_cleanup() here because we may need the PAM
	 * handle at a later stage, e.g. when setting up a session.  It's
	 * still on the cleanup list, so pam_end() *will* be called before
	 * the server process terminates.
	 */
}

KbdintDevice pam_device = {
	"pam",
	pam_init_ctx,
	pam_query,
	pam_respond,
	pam_free_ctx
};

KbdintDevice mm_pam_device = {
	"pam",
	mm_pam_init_ctx,
	mm_pam_query,
	mm_pam_respond,
	mm_pam_free_ctx
};

/*
 * This replaces auth-pam.c
 */
void
start_pam(const char *user)
{
	if (pam_init(user) == -1)
		fatal("PAM: initialisation failed");
}

void
finish_pam(void)
{
	fatal_remove_cleanup(pam_cleanup, NULL);
	pam_cleanup(NULL);
}

int
do_pam_account(const char *user, const char *ruser)
{
	/* XXX */
	return (1);
}

void
do_pam_session(const char *user, const char *tty)
{
	pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *)&null_conv);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(pam_handle, pam_err));
	debug("PAM: setting PAM_TTY to \"%s\"", tty);
	pam_err = pam_set_item(pam_handle, PAM_TTY, tty);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_TTY: %s",
		    pam_strerror(pam_handle, pam_err));
	pam_err = pam_open_session(pam_handle, 0);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: pam_open_session(): %s",
		    pam_strerror(pam_handle, pam_err));
	pam_session_open = 1;
}

void
do_pam_setcred(int init)
{
	pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *)&null_conv);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(pam_handle, pam_err));
	if (init) {
		debug("PAM: establishing credentials");
		pam_err = pam_setcred(pam_handle, PAM_ESTABLISH_CRED);
	} else {
		debug("PAM: reinitializing credentials");
		pam_err = pam_setcred(pam_handle, PAM_REINITIALIZE_CRED);
	}
	if (pam_err == PAM_SUCCESS) {
		pam_cred_established = 1;
		return;
	}
	if (pam_authenticated)
		fatal("PAM: pam_setcred(): %s",
		    pam_strerror(pam_handle, pam_err));
	else
		debug("PAM: pam_setcred(): %s",
		    pam_strerror(pam_handle, pam_err));
}

int
is_pam_password_change_required(void)
{
	return (pam_new_authtok_reqd);
}

static int
pam_chauthtok_conv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{
	char input[PAM_MAX_MSG_SIZE];
	int i;

	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);
	*resp = xmalloc(n * sizeof **resp);
	for (i = 0; i < n; ++i) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			(*resp)[i].resp =
			    read_passphrase(msg[i]->msg, RP_ALLOW_STDIN);
			(*resp)[i].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_PROMPT_ECHO_ON:
			fputs(msg[i]->msg, stderr);
			fgets(input, sizeof input, stdin);
			(*resp)[i].resp = xstrdup(input);
			(*resp)[i].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			fputs(msg[i]->msg, stderr);
			(*resp)[i].resp_retcode = PAM_SUCCESS;
			break;
		default:
			goto fail;
		}
	}
	return (PAM_SUCCESS);
 fail:
	while (i)
		xfree(resp[--i]);
	xfree(*resp);
	*resp = NULL;
	return (PAM_CONV_ERR);
}

/*
 * XXX this should be done in the authentication phase, but ssh1 doesn't
 * support that
 */
void
do_pam_chauthtok(void)
{
	struct pam_conv pam_conv = { pam_chauthtok_conv, NULL };

	if (use_privsep)
		fatal("PAM: chauthtok not supprted with privsep");
	pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *)&pam_conv);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(pam_handle, pam_err));
	debug("PAM: changing password");
	pam_err = pam_chauthtok(pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	if (pam_err != PAM_SUCCESS)
		fatal("PAM: pam_chauthtok(): %s",
		    pam_strerror(pam_handle, pam_err));
}

void
print_pam_messages(void)
{
	/* XXX */
}

char **
fetch_pam_environment(void)
{
#ifdef HAVE_PAM_GETENVLIST
	debug("PAM: retrieving environment");
	return (pam_getenvlist(pam_handle));
#else
	return (NULL);
#endif
}

void
free_pam_environment(char **env)
{
	char **envp;

	for (envp = env; *envp; envp++)
		xfree(*envp);
	xfree(env);
}

#endif /* USE_PAM */
