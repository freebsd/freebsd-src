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
#include "buffer.h"
#include "bufaux.h"
#include "log.h"
#include "monitor_wrap.h"
#include "msg.h"
#include "packet.h"
#include "ssh2.h"
#include "xmalloc.h"

struct pam_ctxt {
	char		*pam_user;
	pid_t		 pam_pid;
	int		 pam_sock;
	int		 pam_done;
};

static void pam_free_ctx(void *);

/*
 * Conversation function for child process.
 */
static int
pam_child_conv(int n,
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
		resp[i]->resp_retcode = 0;
		resp[i]->resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_sock, msg[i]->msg_style, &buffer);
			ssh_msg_recv(ctxt->pam_sock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			resp[i]->resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_PROMPT_ECHO_ON:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_sock, msg[i]->msg_style, &buffer);
			ssh_msg_recv(ctxt->pam_sock, &buffer);
			if (buffer_get_char(&buffer) != PAM_AUTHTOK)
				goto fail;
			resp[i]->resp = buffer_get_string(&buffer, NULL);
			break;
		case PAM_ERROR_MSG:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_sock, msg[i]->msg_style, &buffer);
			break;
		case PAM_TEXT_INFO:
			buffer_put_cstring(&buffer, msg[i]->msg);
			ssh_msg_send(ctxt->pam_sock, msg[i]->msg_style, &buffer);
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
 * Child process.
 */
static void *
pam_child(struct pam_ctxt *ctxt)
{
	Buffer buffer;
	struct pam_conv pam_conv = { pam_child_conv, ctxt };
	pam_handle_t *pamh;
	int pam_err;

	buffer_init(&buffer);
	setproctitle("%s [pam]", ctxt->pam_user);
	pam_err = pam_start("sshd", ctxt->pam_user, &pam_conv, &pamh);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_err = pam_authenticate(pamh, 0);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_err = pam_acct_mgmt(pamh, 0);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	buffer_put_cstring(&buffer, "OK");
	ssh_msg_send(ctxt->pam_sock, PAM_SUCCESS, &buffer);
	buffer_free(&buffer);
	pam_end(pamh, pam_err);
	exit(0);
 auth_fail:
	buffer_put_cstring(&buffer, pam_strerror(pamh, pam_err));
	ssh_msg_send(ctxt->pam_sock, PAM_AUTH_ERR, &buffer);
	buffer_free(&buffer);
	pam_end(pamh, pam_err);
	exit(0);
}

static void
pam_cleanup(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;
	int status;

	close(ctxt->pam_sock);
	kill(ctxt->pam_pid, SIGHUP);
	waitpid(ctxt->pam_pid, &status, 0);
}

static void *
pam_init_ctx(Authctxt *authctxt)
{
	struct pam_ctxt *ctxt;
	int socks[2];
	int i;

	ctxt = xmalloc(sizeof *ctxt);
	ctxt->pam_user = xstrdup(authctxt->user);
	ctxt->pam_done = 0;
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, socks) == -1) {
		error("%s: failed create sockets: %s",
		    __func__, strerror(errno));
		xfree(ctxt);
		return (NULL);
	}
	if ((ctxt->pam_pid = fork()) == -1) {
		error("%s: failed to fork auth-pam child: %s",
		    __func__, strerror(errno));
		close(socks[0]);
		close(socks[1]);
		xfree(ctxt);
		return (NULL);
	}
	if (ctxt->pam_pid == 0) {
		/* close everything except our end of the pipe */
		ctxt->pam_sock = socks[1];
		for (i = 3; i < getdtablesize(); ++i)
			if (i != ctxt->pam_sock)
				close(i);
		pam_child(ctxt);
		/* not reached */
		exit(1);
	}
	ctxt->pam_sock = socks[0];
	close(socks[1]);
	fatal_add_cleanup(pam_cleanup, ctxt);
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
	while (ssh_msg_recv(ctxt->pam_sock, &buffer) == 0) {
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
			error("%s", msg);
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

	debug2(__func__);
	switch (ctxt->pam_done) {
	case 1:
		return (0);
	case 0:
		break;
	default:
		return (-1);
	}
	if (num != 1) {
		error("expected one response, got %u", num);
		return (-1);
	}
	buffer_init(&buffer);
	buffer_put_cstring(&buffer, *resp);
	ssh_msg_send(ctxt->pam_sock, PAM_AUTHTOK, &buffer);
	buffer_free(&buffer);
	return (1);
}

static void
pam_free_ctx(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;
	int status;

	fatal_remove_cleanup(pam_cleanup, ctxt);
	close(ctxt->pam_sock);
	kill(ctxt->pam_pid, SIGHUP);
	waitpid(ctxt->pam_pid, &status, 0);
	xfree(ctxt->pam_user);
	xfree(ctxt);
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

#endif /* USE_PAM */
