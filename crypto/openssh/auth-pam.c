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

/* Based on $xFreeBSD: src/crypto/openssh/auth2-pam-freebsd.c,v 1.11 2003/03/31 13:48:18 des Exp $ */
#include "includes.h"
RCSID("$Id: auth-pam.c,v 1.72.2.2 2003/09/23 09:24:21 djm Exp $");
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
#include "auth-options.h"

extern ServerOptions options;

#ifndef __unused
#define __unused
#endif

#ifdef USE_POSIX_THREADS
#include <pthread.h>
/*
 * Avoid namespace clash when *not* using pthreads for systems *with* 
 * pthreads, which unconditionally define pthread_t via sys/types.h 
 * (e.g. Linux)
 */
typedef pthread_t sp_pthread_t; 
#else
/*
 * Simulate threads with processes.
 */
typedef pid_t sp_pthread_t;

static void
pthread_exit(void *value __unused)
{
	_exit(0);
}

static int
pthread_create(sp_pthread_t *thread, const void *attr __unused,
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
pthread_cancel(sp_pthread_t thread)
{
	return (kill(thread, SIGTERM));
}

static int
pthread_join(sp_pthread_t thread, void **value __unused)
{
	int status;

	waitpid(thread, &status, 0);
	return (status);
}
#endif


static pam_handle_t *sshpam_handle = NULL;
static int sshpam_err = 0;
static int sshpam_authenticated = 0;
static int sshpam_new_authtok_reqd = 0;
static int sshpam_session_open = 0;
static int sshpam_cred_established = 0;

struct pam_ctxt {
	sp_pthread_t	 pam_thread;
	int		 pam_psock;
	int		 pam_csock;
	int		 pam_done;
};

static void sshpam_free_ctx(void *);

/*
 * Conversation function for authentication thread.
 */
static int
sshpam_thread_conv(int n, const struct pam_message **msg,
    struct pam_response **resp, void *data)
{
	Buffer buffer;
	struct pam_ctxt *ctxt;
	struct pam_response *reply;
	int i;

	*resp = NULL;

	ctxt = data;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);

	if ((reply = malloc(n * sizeof(*reply))) == NULL)
		return (PAM_CONV_ERR);
	memset(reply, 0, n * sizeof(*reply));

	buffer_init(&buffer);
	for (i = 0; i < n; ++i) {
		switch (PAM_MSG_MEMBER(msg, i, msg_style)) {
		case PAM_PROMPT_ECHO_OFF:
			buffer_put_cstring(&buffer, 
			    PAM_MSG_MEMBER(msg, i, msg));
			ssh_msg_send(ctxt->pam_csock, 
			    PAM_MSG_MEMBER(msg, i, msg_style), &buffer);
			ssh_msg_recv(ctxt->pam_csock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			reply[i].resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_PROMPT_ECHO_ON:
			buffer_put_cstring(&buffer, 
			    PAM_MSG_MEMBER(msg, i, msg));
			ssh_msg_send(ctxt->pam_csock, 
			    PAM_MSG_MEMBER(msg, i, msg_style), &buffer);
			ssh_msg_recv(ctxt->pam_csock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			reply[i].resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_ERROR_MSG:
			buffer_put_cstring(&buffer, 
			    PAM_MSG_MEMBER(msg, i, msg));
			ssh_msg_send(ctxt->pam_csock, 
			    PAM_MSG_MEMBER(msg, i, msg_style), &buffer);
			break;
		case PAM_TEXT_INFO:
			buffer_put_cstring(&buffer, 
			    PAM_MSG_MEMBER(msg, i, msg));
			ssh_msg_send(ctxt->pam_csock, 
			    PAM_MSG_MEMBER(msg, i, msg_style), &buffer);
			break;
		default:
			goto fail;
		}
		buffer_clear(&buffer);
	}
	buffer_free(&buffer);
	*resp = reply;
	return (PAM_SUCCESS);

 fail:
	for(i = 0; i < n; i++) {
		if (reply[i].resp != NULL)
			xfree(reply[i].resp);
	}
	xfree(reply);
	buffer_free(&buffer);
	return (PAM_CONV_ERR);
}

/*
 * Authentication thread.
 */
static void *
sshpam_thread(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;
	Buffer buffer;
	struct pam_conv sshpam_conv;
#ifndef USE_POSIX_THREADS
	const char *pam_user;

	pam_get_item(sshpam_handle, PAM_USER, (const void **)&pam_user);
	setproctitle("%s [pam]", pam_user);
#endif

	sshpam_conv.conv = sshpam_thread_conv;
	sshpam_conv.appdata_ptr = ctxt;

	buffer_init(&buffer);
	sshpam_err = pam_set_item(sshpam_handle, PAM_CONV,
	    (const void *)&sshpam_conv);
	if (sshpam_err != PAM_SUCCESS)
		goto auth_fail;
	sshpam_err = pam_authenticate(sshpam_handle, 0);
	if (sshpam_err != PAM_SUCCESS)
		goto auth_fail;
	buffer_put_cstring(&buffer, "OK");
	ssh_msg_send(ctxt->pam_csock, sshpam_err, &buffer);
	buffer_free(&buffer);
	pthread_exit(NULL);

 auth_fail:
	buffer_put_cstring(&buffer,
	    pam_strerror(sshpam_handle, sshpam_err));
	ssh_msg_send(ctxt->pam_csock, PAM_AUTH_ERR, &buffer);
	buffer_free(&buffer);
	pthread_exit(NULL);
	
	return (NULL); /* Avoid warning for non-pthread case */
}

static void
sshpam_thread_cleanup(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;

	pthread_cancel(ctxt->pam_thread);
	pthread_join(ctxt->pam_thread, NULL);
	close(ctxt->pam_psock);
	close(ctxt->pam_csock);
}

static int
sshpam_null_conv(int n, const struct pam_message **msg,
    struct pam_response **resp, void *data)
{
	return (PAM_CONV_ERR);
}

static struct pam_conv null_conv = { sshpam_null_conv, NULL };

static void
sshpam_cleanup(void *arg)
{
	(void)arg;
	debug("PAM: cleanup");
	if (sshpam_handle == NULL)
		return;
	pam_set_item(sshpam_handle, PAM_CONV, (const void *)&null_conv);
	if (sshpam_cred_established) {
		pam_setcred(sshpam_handle, PAM_DELETE_CRED);
		sshpam_cred_established = 0;
	}
	if (sshpam_session_open) {
		pam_close_session(sshpam_handle, PAM_SILENT);
		sshpam_session_open = 0;
	}
	sshpam_authenticated = sshpam_new_authtok_reqd = 0;
	pam_end(sshpam_handle, sshpam_err);
	sshpam_handle = NULL;
}

static int
sshpam_init(const char *user)
{
	extern u_int utmp_len;
	extern char *__progname;
	const char *pam_rhost, *pam_user;

	if (sshpam_handle != NULL) {
		/* We already have a PAM context; check if the user matches */
		sshpam_err = pam_get_item(sshpam_handle,
		    PAM_USER, (const void **)&pam_user);
		if (sshpam_err == PAM_SUCCESS && strcmp(user, pam_user) == 0)
			return (0);
		fatal_remove_cleanup(sshpam_cleanup, NULL);
		pam_end(sshpam_handle, sshpam_err);
		sshpam_handle = NULL;
	}
	debug("PAM: initializing for \"%s\"", user);
	sshpam_err =
	    pam_start(SSHD_PAM_SERVICE, user, &null_conv, &sshpam_handle);
	if (sshpam_err != PAM_SUCCESS) {
		pam_end(sshpam_handle, sshpam_err);
		sshpam_handle = NULL;
		return (-1);
	}
	pam_rhost = get_remote_name_or_ip(utmp_len, options.use_dns);
	debug("PAM: setting PAM_RHOST to \"%s\"", pam_rhost);
	sshpam_err = pam_set_item(sshpam_handle, PAM_RHOST, pam_rhost);
	if (sshpam_err != PAM_SUCCESS) {
		pam_end(sshpam_handle, sshpam_err);
		sshpam_handle = NULL;
		return (-1);
	}
#ifdef PAM_TTY_KLUDGE
        /*
         * Some silly PAM modules (e.g. pam_time) require a TTY to operate.
         * sshd doesn't set the tty until too late in the auth process and 
	 * may not even set one (for tty-less connections)
         */
	debug("PAM: setting PAM_TTY to \"ssh\"");
	sshpam_err = pam_set_item(sshpam_handle, PAM_TTY, "ssh");
	if (sshpam_err != PAM_SUCCESS) {
		pam_end(sshpam_handle, sshpam_err);
		sshpam_handle = NULL;
		return (-1);
	}
#endif
	fatal_add_cleanup(sshpam_cleanup, NULL);
	return (0);
}

static void *
sshpam_init_ctx(Authctxt *authctxt)
{
	struct pam_ctxt *ctxt;
	int socks[2];

	/* Refuse to start if we don't have PAM enabled */
	if (!options.use_pam)
		return NULL;

	/* Initialize PAM */
	if (sshpam_init(authctxt->user) == -1) {
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
	if (pthread_create(&ctxt->pam_thread, NULL, sshpam_thread, ctxt) == -1) {
		error("PAM: failed to start authentication thread: %s",
		    strerror(errno));
		close(socks[0]);
		close(socks[1]);
		xfree(ctxt);
		return (NULL);
	}
	fatal_add_cleanup(sshpam_thread_cleanup, ctxt);
	return (ctxt);
}

static int
sshpam_query(void *ctx, char **name, char **info,
    u_int *num, char ***prompts, u_int **echo_on)
{
	Buffer buffer;
	struct pam_ctxt *ctxt = ctx;
	size_t plen;
	u_char type;
	char *msg;
	size_t len;

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
			len = plen + strlen(msg) + 1;
			**prompts = xrealloc(**prompts, len);
			plen += snprintf(**prompts + plen, len, "%s", msg);
			**echo_on = (type == PAM_PROMPT_ECHO_ON);
			xfree(msg);
			return (0);
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			/* accumulate messages */
			len = plen + strlen(msg) + 1;
			**prompts = xrealloc(**prompts, len);
			plen += snprintf(**prompts + plen, len, "%s", msg);
			xfree(msg);
			break;
		case PAM_SUCCESS:
		case PAM_AUTH_ERR:
			if (**prompts != NULL) {
				/* drain any accumulated messages */
#if 0 /* XXX - not compatible with privsep */
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

/* XXX - see also comment in auth-chall.c:verify_response */
static int
sshpam_respond(void *ctx, u_int num, char **resp)
{
	Buffer buffer;
	struct pam_ctxt *ctxt = ctx;

	debug2("PAM: %s", __func__);
	switch (ctxt->pam_done) {
	case 1:
		sshpam_authenticated = 1;
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
sshpam_free_ctx(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;

	fatal_remove_cleanup(sshpam_thread_cleanup, ctxt);
	sshpam_thread_cleanup(ctxtp);
	xfree(ctxt);
	/*
	 * We don't call sshpam_cleanup() here because we may need the PAM
	 * handle at a later stage, e.g. when setting up a session.  It's
	 * still on the cleanup list, so pam_end() *will* be called before
	 * the server process terminates.
	 */
}

KbdintDevice sshpam_device = {
	"pam",
	sshpam_init_ctx,
	sshpam_query,
	sshpam_respond,
	sshpam_free_ctx
};

KbdintDevice mm_sshpam_device = {
	"pam",
	mm_sshpam_init_ctx,
	mm_sshpam_query,
	mm_sshpam_respond,
	mm_sshpam_free_ctx
};

/*
 * This replaces auth-pam.c
 */
void
start_pam(const char *user)
{
	if (!options.use_pam)
		fatal("PAM: initialisation requested when UsePAM=no");

	if (sshpam_init(user) == -1)
		fatal("PAM: initialisation failed");
}

void
finish_pam(void)
{
	fatal_remove_cleanup(sshpam_cleanup, NULL);
	sshpam_cleanup(NULL);
}

u_int
do_pam_account(void)
{
	sshpam_err = pam_acct_mgmt(sshpam_handle, 0);
	debug3("%s: pam_acct_mgmt = %d", __func__, sshpam_err);
	
	if (sshpam_err != PAM_SUCCESS && sshpam_err != PAM_NEW_AUTHTOK_REQD)
		return (0);

	if (sshpam_err == PAM_NEW_AUTHTOK_REQD) {
		sshpam_new_authtok_reqd = 1;

		/* Prevent forwardings until password changed */
		no_port_forwarding_flag |= 2;
		no_agent_forwarding_flag |= 2;
		no_x11_forwarding_flag |= 2;
	}

	return (1);
}

void
do_pam_session(void)
{
	sshpam_err = pam_set_item(sshpam_handle, PAM_CONV, 
	    (const void *)&null_conv);
	if (sshpam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(sshpam_handle, sshpam_err));
	sshpam_err = pam_open_session(sshpam_handle, 0);
	if (sshpam_err != PAM_SUCCESS)
		fatal("PAM: pam_open_session(): %s",
		    pam_strerror(sshpam_handle, sshpam_err));
	sshpam_session_open = 1;
}

void
do_pam_set_tty(const char *tty)
{
	if (tty != NULL) {
		debug("PAM: setting PAM_TTY to \"%s\"", tty);
		sshpam_err = pam_set_item(sshpam_handle, PAM_TTY, tty);
		if (sshpam_err != PAM_SUCCESS)
			fatal("PAM: failed to set PAM_TTY: %s",
			    pam_strerror(sshpam_handle, sshpam_err));
	}
}

void
do_pam_setcred(int init)
{
	sshpam_err = pam_set_item(sshpam_handle, PAM_CONV,
	    (const void *)&null_conv);
	if (sshpam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(sshpam_handle, sshpam_err));
	if (init) {
		debug("PAM: establishing credentials");
		sshpam_err = pam_setcred(sshpam_handle, PAM_ESTABLISH_CRED);
	} else {
		debug("PAM: reinitializing credentials");
		sshpam_err = pam_setcred(sshpam_handle, PAM_REINITIALIZE_CRED);
	}
	if (sshpam_err == PAM_SUCCESS) {
		sshpam_cred_established = 1;
		return;
	}
	if (sshpam_authenticated)
		fatal("PAM: pam_setcred(): %s",
		    pam_strerror(sshpam_handle, sshpam_err));
	else
		debug("PAM: pam_setcred(): %s",
		    pam_strerror(sshpam_handle, sshpam_err));
}

int
is_pam_password_change_required(void)
{
	return (sshpam_new_authtok_reqd);
}

static int
pam_chauthtok_conv(int n, const struct pam_message **msg,
    struct pam_response **resp, void *data)
{
	char input[PAM_MAX_MSG_SIZE];
	struct pam_response *reply;
	int i;

	*resp = NULL;

	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);

	if ((reply = malloc(n * sizeof(*reply))) == NULL)
		return (PAM_CONV_ERR);
	memset(reply, 0, n * sizeof(*reply));

	for (i = 0; i < n; ++i) {
		switch (PAM_MSG_MEMBER(msg, i, msg_style)) {
		case PAM_PROMPT_ECHO_OFF:
			reply[i].resp =
			    read_passphrase(PAM_MSG_MEMBER(msg, i, msg), 
			    RP_ALLOW_STDIN);
			reply[i].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_PROMPT_ECHO_ON:
			fputs(PAM_MSG_MEMBER(msg, i, msg), stderr);
			fgets(input, sizeof input, stdin);
			reply[i].resp = xstrdup(input);
			reply[i].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			fputs(PAM_MSG_MEMBER(msg, i, msg), stderr);
			reply[i].resp_retcode = PAM_SUCCESS;
			break;
		default:
			goto fail;
		}
	}
	*resp = reply;
	return (PAM_SUCCESS);

 fail:
	for(i = 0; i < n; i++) {
		if (reply[i].resp != NULL)
			xfree(reply[i].resp);
	}
	xfree(reply);
	return (PAM_CONV_ERR);
}

/*
 * XXX this should be done in the authentication phase, but ssh1 doesn't
 * support that
 */
void
do_pam_chauthtok(void)
{
	struct pam_conv pam_conv;

	pam_conv.conv = pam_chauthtok_conv;
	pam_conv.appdata_ptr = NULL;

	if (use_privsep)
		fatal("Password expired (unable to change with privsep)");
	sshpam_err = pam_set_item(sshpam_handle, PAM_CONV,
	    (const void *)&pam_conv);
	if (sshpam_err != PAM_SUCCESS)
		fatal("PAM: failed to set PAM_CONV: %s",
		    pam_strerror(sshpam_handle, sshpam_err));
	debug("PAM: changing password");
	sshpam_err = pam_chauthtok(sshpam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
	if (sshpam_err != PAM_SUCCESS)
		fatal("PAM: pam_chauthtok(): %s",
		    pam_strerror(sshpam_handle, sshpam_err));
}

/* 
 * Set a PAM environment string. We need to do this so that the session
 * modules can handle things like Kerberos/GSI credentials that appear
 * during the ssh authentication process.
 */

int
do_pam_putenv(char *name, char *value) 
{
	int ret = 1;
#ifdef HAVE_PAM_PUTENV	
	char *compound;
	size_t len;

	len = strlen(name) + strlen(value) + 2;
	compound = xmalloc(len);

	snprintf(compound, len, "%s=%s", name, value);
	ret = pam_putenv(sshpam_handle, compound);
	xfree(compound);
#endif

	return (ret);
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
	return (pam_getenvlist(sshpam_handle));
#else
	return (NULL);
#endif
}

void
free_pam_environment(char **env)
{
	char **envp;

	if (env == NULL)
		return;

	for (envp = env; *envp; envp++)
		xfree(*envp);
	xfree(env);
}

#endif /* USE_PAM */
