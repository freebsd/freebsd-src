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
 *
 * $FreeBSD$
 */

#ifdef USE_PAM
#include "includes.h"
RCSID("$FreeBSD$");

#include <security/pam_appl.h>

#include "auth.h"
#include "xmalloc.h"

struct pam_ctxt {
	char		*pam_user;
	pid_t		 pam_pid;
	int		 pam_sock;
	int		 pam_done;
};

static void pam_free_ctx(void *);

/*
 * Send message to parent or child.
 */
static int
pam_send(struct pam_ctxt *ctxt, char *fmt, ...)
{
	va_list ap;
	char *mstr;
	size_t len;
	int r;

	va_start(ap, fmt);
	len = vasprintf(&mstr, fmt, ap);
	va_end(ap);
	if (mstr == NULL)
		exit(1);
	if (ctxt->pam_pid != 0)
		debug2("to child: %s", mstr);
	r = send(ctxt->pam_sock, mstr, len + 1, MSG_EOR);
	free(mstr);
	return (r);
}

/*
 * Peek at first byte of next message.
 */
static int
pam_peek(struct pam_ctxt *ctxt)
{
	char ch;

	if (recv(ctxt->pam_sock, &ch, 1, MSG_PEEK) < 1)
		return (-1);
	return (ch);
}

/*
 * Receive a message from parent or child.
 */
static char *
pam_receive(struct pam_ctxt *ctxt)
{
	char *buf;
	size_t len;
	ssize_t rlen;

	len = 64;
	buf = NULL;
	do {
		len *= 2;
		buf = xrealloc(buf, len);
		rlen = recv(ctxt->pam_sock, buf, len, MSG_PEEK);
		if (rlen < 1) {
			xfree(buf);
			return (NULL);
		}
	} while (rlen == len);
	if (recv(ctxt->pam_sock, buf, len, 0) != rlen) {
		xfree(buf);
		return (NULL);
	}
	if (ctxt->pam_pid != 0)
		debug2("from child: %s", buf);
	return (buf);
}

/*
 * Conversation function for child process.
 */
static int
pam_child_conv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{
	struct pam_ctxt *ctxt;
	char *line;
	size_t len;
	int i;

	ctxt = data;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);
	if ((*resp = calloc(n, sizeof **resp)) == NULL)
		return (PAM_BUF_ERR);
	for (i = 0; i < n; ++i) {
		resp[i]->resp_retcode = 0;
		resp[i]->resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			pam_send(ctxt, "p%s", msg[i]->msg);
			resp[i]->resp = pam_receive(ctxt);
			break;
		case PAM_PROMPT_ECHO_ON:
			pam_send(ctxt, "P%s", msg[i]->msg);
			resp[i]->resp = pam_receive(ctxt);
			break;
		case PAM_ERROR_MSG:
			/*pam_send(ctxt, "e%s", msg[i]->msg);*/
			break;
		case PAM_TEXT_INFO:
			/*pam_send(ctxt, "i%s", msg[i]->msg);*/
			break;
		default:
			goto fail;
		}
	}
	return (PAM_SUCCESS);
 fail:
	while (i)
		free(resp[--i]);
	free(*resp);
	*resp = NULL;
	return (PAM_CONV_ERR);
}

/*
 * Child process.
 */
static void *
pam_child(struct pam_ctxt *ctxt)
{
	struct pam_conv pam_conv = { pam_child_conv, ctxt };
	pam_handle_t *pamh;
	char *msg;
	int pam_err;

	pam_err = pam_start("sshd", ctxt->pam_user, &pam_conv, &pamh);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_err = pam_authenticate(pamh, 0);
	if (pam_err != PAM_SUCCESS)
		goto auth_fail;
	pam_send(ctxt, "=OK");
	pam_end(pamh, pam_err);
	exit(0);
 auth_fail:
	pam_send(ctxt, "!%s", pam_strerror(pamh, pam_err));
	pam_end(pamh, pam_err);
	exit(0);
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
	if (socketpair(AF_UNIX, SOCK_DGRAM, PF_UNSPEC, socks) == -1) {
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
		for (i = 0; i < getdtablesize(); ++i)
			if (i != ctxt->pam_sock)
				close(i);
		pam_child(ctxt);
		/* not reached */
		exit(1);
	}
	ctxt->pam_sock = socks[0];
	close(socks[1]);
	return (ctxt);
}

static int
pam_query(void *ctx, char **name, char **info,
    u_int *num, char ***prompts, u_int **echo_on)
{
	struct pam_ctxt *ctxt = ctx;
	char *msg;

	if ((msg = pam_receive(ctxt)) == NULL)
		return (-1);
	*name = xstrdup("");
	*info = xstrdup("");
	*prompts = xmalloc(sizeof(char *));
	*echo_on = xmalloc(sizeof(u_int));
	switch (*msg) {
	case 'P':
		**echo_on = 1;
	case 'p':
		*num = 1;
		**prompts = xstrdup(msg + 1);
		**echo_on = (*msg == 'P');
		break;
	case '=':
		*num = 0;
		**echo_on = 0;
		ctxt->pam_done = 1;
		break;
	case '!':
		error("%s", msg + 1);
	default:
		*num = 0;
		**echo_on = 0;
		xfree(msg);
		ctxt->pam_done = -1;
		return (-1);
	}
	xfree(msg);
	return (0);
}

static int
pam_respond(void *ctx, u_int num, char **resp)
{
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
	pam_send(ctxt, "%s", *resp);
	switch (pam_peek(ctxt)) {
	case 'P':
	case 'p':
		return (1);
	case '=':
		msg = pam_receive(ctxt);
		xfree(msg);
		ctxt->pam_done = 1;
		return (0);
	default:
		msg = pam_receive(ctxt);
		if (*msg == '!')
			error("%s", msg + 1);
		xfree(msg);
		ctxt->pam_done = -1;
		return (-1);
	}
}

static void
pam_free_ctx(void *ctxtp)
{
	struct pam_ctxt *ctxt = ctxtp;
	int i;

	close(ctxt->pam_sock);
	kill(ctxt->pam_pid, SIGHUP);
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

#endif /* USE_PAM */
