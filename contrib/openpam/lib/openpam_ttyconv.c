/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
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
 *
 * $P4: //depot/projects/openpam/lib/openpam_ttyconv.c#14 $
 */

#include <sys/types.h>

#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

int openpam_ttyconv_timeout = 0;
static jmp_buf jmpenv;
static int timed_out;

static void
timeout(int sig)
{
	timed_out = 1;
	longjmp(jmpenv, sig);
}

static char *
prompt(const char *msg)
{
	char buf[PAM_MAX_RESP_SIZE];
	struct sigaction action, saved_action;
	sigset_t saved_sigset, sigset;
	unsigned int saved_alarm;
	size_t len;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTSTP);
	sigprocmask(SIG_SETMASK, &sigset, &saved_sigset);
	action.sa_handler = &timeout;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, &saved_action);
	fputs(msg, stdout);
	buf[0] = '\0';
	timed_out = 0;
	saved_alarm = alarm(openpam_ttyconv_timeout);
	if (setjmp(jmpenv) == 0)
		fgets(buf, sizeof buf, stdin);
	else
		fputs(" timeout!\n", stderr);
	alarm(0);
	sigaction(SIGALRM, &saved_action, NULL);
	sigprocmask(SIG_SETMASK, &saved_sigset, NULL);
	alarm(saved_alarm);
	if (timed_out || ferror(stdin))
		return (NULL);
	/* trim trailing whitespace */
	for (len = strlen(buf); len > 0; --len)
		if (!isspace(buf[len - 1]))
			break;
	buf[len] = '\0';
	return (strdup(buf));
}

static char *
prompt_echo_off(const char *msg)
{
	struct termios tattr;
	tcflag_t lflag;
	char *ret;
	int fd;

	fd = fileno(stdin);
	if (tcgetattr(fd, &tattr) != 0) {
		openpam_log(PAM_LOG_ERROR, "tcgetattr(): %m");
		return (NULL);
	}
	lflag = tattr.c_lflag;
	tattr.c_lflag &= ~ECHO;
	if (tcsetattr(fd, TCSAFLUSH, &tattr) != 0) {
		openpam_log(PAM_LOG_ERROR, "tcsetattr(): %m");
		return (NULL);
	}
	ret = prompt(msg);
	tattr.c_lflag = lflag;
	(void)tcsetattr(fd, TCSANOW, &tattr);
	if (ret != NULL)
		fputs("\n", stdout);
	return (ret);
}

/*
 * OpenPAM extension
 *
 * Simple tty-based conversation function
 */

int
openpam_ttyconv(int n,
	 const struct pam_message **msg,
	 struct pam_response **resp,
	 void *data)
{
	int i;

	ENTER();
	(void)data;
	if (n <= 0 || n > PAM_MAX_NUM_MSG)
		RETURNC(PAM_CONV_ERR);
	if ((*resp = calloc(n, sizeof **resp)) == NULL)
		RETURNC(PAM_BUF_ERR);
	for (i = 0; i < n; ++i) {
		resp[i]->resp_retcode = 0;
		resp[i]->resp = NULL;
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			resp[i]->resp = prompt_echo_off(msg[i]->msg);
			if (resp[i]->resp == NULL)
				goto fail;
			break;
		case PAM_PROMPT_ECHO_ON:
			resp[i]->resp = prompt(msg[i]->msg);
			if (resp[i]->resp == NULL)
				goto fail;
			break;
		case PAM_ERROR_MSG:
			fputs(msg[i]->msg, stderr);
			if (strlen(msg[i]->msg) > 0 &&
			    msg[i]->msg[strlen(msg[i]->msg) - 1] != '\n')
				fputc('\n', stderr);
			break;
		case PAM_TEXT_INFO:
			fputs(msg[i]->msg, stdout);
			if (strlen(msg[i]->msg) > 0 &&
			    msg[i]->msg[strlen(msg[i]->msg) - 1] != '\n')
				fputc('\n', stdout);
			break;
		default:
			goto fail;
		}
	}
	RETURNC(PAM_SUCCESS);
 fail:
	while (i)
		free(resp[--i]);
	free(*resp);
	*resp = NULL;
	RETURNC(PAM_CONV_ERR);
}

/*
 * NOLIST
 *
 * Error codes:
 *
 *	PAM_SYSTEM_ERR
 *	PAM_BUF_ERR
 *	PAM_CONV_ERR
 */

/**
 * The =openpam_ttyconv function is a standard conversation function
 * suitable for use on TTY devices.  It should be adequate for the needs
 * of most text-based interactive programs.
 *
 * The =openpam_ttyconv function allows the application to specify a
 * timeout for user input by setting the global variable
 * :openpam_ttyconv_timeout to the length of the timeout in seconds.
 *
 * >openpam_nullconv
 * >pam_prompt
 * >pam_vprompt
 */
