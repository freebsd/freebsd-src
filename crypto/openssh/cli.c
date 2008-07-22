/*	$OpenBSD: cli.c,v 1.11 2001/03/06 00:33:04 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: cli.c,v 1.11 2001/03/06 00:33:04 deraadt Exp $");

#include "xmalloc.h"
#include "log.h"
#include "cli.h"

#include <vis.h>

static int cli_input = -1;
static int cli_output = -1;
static int cli_from_stdin = 0;

sigset_t oset;
sigset_t nset;
struct sigaction nsa;
struct sigaction osa;
struct termios ntio;
struct termios otio;
int echo_modified;

volatile int intr;

static int
cli_open(int from_stdin)
{
	if (cli_input >= 0 && cli_output >= 0 && cli_from_stdin == from_stdin)
		return 1;

	if (from_stdin) {
		if (!cli_from_stdin && cli_input >= 0) {
			(void)close(cli_input);
		}
		cli_input = STDIN_FILENO;
		cli_output = STDERR_FILENO;
	} else {
		cli_input = cli_output = open(_PATH_TTY, O_RDWR);
		if (cli_input < 0)
			fatal("You have no controlling tty.  Cannot read passphrase.");
	}

	cli_from_stdin = from_stdin;

	return cli_input >= 0 && cli_output >= 0 && cli_from_stdin == from_stdin;
}

static void
cli_close(void)
{
	if (!cli_from_stdin && cli_input >= 0)
		close(cli_input);
	cli_input = -1;
	cli_output = -1;
	cli_from_stdin = 0;
	return;
}

void
intrcatch(int sig)
{
	intr = 1;
}

static void
cli_echo_disable(void)
{
	sigemptyset(&nset);
	sigaddset(&nset, SIGTSTP);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	intr = 0;

	memset(&nsa, 0, sizeof(nsa));
	nsa.sa_handler = intrcatch;
	(void) sigaction(SIGINT, &nsa, &osa);

	echo_modified = 0;
	if (tcgetattr(cli_input, &otio) == 0 && (otio.c_lflag & ECHO)) {
		echo_modified = 1;
		ntio = otio;
		ntio.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		(void) tcsetattr(cli_input, TCSANOW, &ntio);
	}
	return;
}

static void
cli_echo_restore(void)
{
	if (echo_modified != 0) {
		tcsetattr(cli_input, TCSANOW, &otio);
		echo_modified = 0;
	}

	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) sigaction(SIGINT, &osa, NULL);

	if (intr != 0) {
		kill(getpid(), SIGINT);
		sigemptyset(&nset);
		/* XXX tty has not neccessarily drained by now? */
		sigsuspend(&nset);
		intr = 0;
	}
	return;
}

static int
cli_read(char* buf, int size, int echo)
{
	char ch = 0;
	int i = 0;
	int n;

	if (!echo)
		cli_echo_disable();

	while (ch != '\n') {
		n = read(cli_input, &ch, 1);
		if (n == -1 && (errno == EAGAIN || errno == EINTR))
			continue;
		if (n != 1)
			break;
		if (ch == '\n' || intr != 0)
			break;
		if (i < size)
			buf[i++] = ch;
	}
	buf[i] = '\0';

	if (!echo)
		cli_echo_restore();
	if (!intr && !echo)
		(void) write(cli_output, "\n", 1);
	return i;
}

static int
cli_write(char* buf, int size)
{
	int i, len, pos, ret = 0;
	char *output, *p;

	output = xmalloc(4*size);
	for (p = output, i = 0; i < size; i++) {
		if (buf[i] == '\n' || buf[i] == '\r')
			*p++ = buf[i];
		else
			p = vis(p, buf[i], 0, 0);
	}
	len = p - output;

	for (pos = 0; pos < len; pos += ret) {
		ret = write(cli_output, output + pos, len - pos);
		if (ret == -1) {
			xfree(output);
			return -1;
		}
	}
	xfree(output);
	return 0;
}

/*
 * Presents a prompt and returns the response allocated with xmalloc().
 * Uses /dev/tty or stdin/out depending on arg.  Optionally disables echo
 * of response depending on arg.  Tries to ensure that no other userland
 * buffer is storing the response.
 */
char*
cli_read_passphrase(char* prompt, int from_stdin, int echo_enable)
{
	char	buf[BUFSIZ];
	char*	p;

	if (!cli_open(from_stdin))
		fatal("Cannot read passphrase.");

	fflush(stdout);

	cli_write(prompt, strlen(prompt));
	cli_read(buf, sizeof buf, echo_enable);

	cli_close();

	p = xstrdup(buf);
	memset(buf, 0, sizeof(buf));
	return (p);
}

char*
cli_prompt(char* prompt, int echo_enable)
{
	return cli_read_passphrase(prompt, 0, echo_enable);
}

void
cli_mesg(char* mesg)
{
	cli_open(0);
	cli_write(mesg, strlen(mesg));
	cli_write("\n", strlen("\n"));
	cli_close();
	return;
}
