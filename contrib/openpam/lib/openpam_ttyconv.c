/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
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
 * $Id$
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

/*
 * Simple tty-based conversation function.
 */

int
openpam_ttyconv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{
	char buf[PAM_MAX_RESP_SIZE];
	struct termios tattr;
	tcflag_t lflag;
	int fd, err, i;
	size_t len;

	data = data;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		return (PAM_CONV_ERR);
	if ((*resp = calloc(n, sizeof **resp)) == NULL)
		return (PAM_BUF_ERR);
	fd = fileno(stdin);
	for (i = 0; i < n; ++i) {
		resp[i]->resp_retcode = 0;
		resp[i]->resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
				if (tcgetattr(fd, &tattr) != 0) {
					openpam_log(PAM_LOG_ERROR,
					    "tcgetattr(): %m");
					err = PAM_CONV_ERR;
					goto fail;
				}
				lflag = tattr.c_lflag;
				tattr.c_lflag &= ~ECHO;
				if (tcsetattr(fd, TCSAFLUSH, &tattr) != 0) {
					openpam_log(PAM_LOG_ERROR,
					    "tcsetattr(): %m");
					err = PAM_CONV_ERR;
					goto fail;
				}
			}
			fputs(msg[i]->msg, stderr);
			buf[0] = '\0';
			fgets(buf, sizeof buf, stdin);
			if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
				tattr.c_lflag = lflag;
				(void)tcsetattr(fd, TCSANOW, &tattr);
				fputs("\n", stderr);
			}
			if (ferror(stdin)) {
				err = PAM_CONV_ERR;
				goto fail;
			}
			for (len = strlen(buf); len > 0; --len)
				if (!isspace(buf[len - 1]))
					break;
			buf[len] = '\0';
			if ((resp[i]->resp = strdup(buf)) == NULL) {
				err = PAM_BUF_ERR;
				goto fail;
			}
			break;
		case PAM_ERROR_MSG:
			fputs(msg[i]->msg, stderr);
			break;
		case PAM_TEXT_INFO:
			fputs(msg[i]->msg, stdout);
			break;
		default:
			err = PAM_BUF_ERR;
			goto fail;
		}
	}
	return (PAM_SUCCESS);
 fail:
	while (i)
		free(resp[--i]);
	free(*resp);
	*resp = NULL;
	return (err);
}
