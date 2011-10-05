/*
 * Copyright (c) 1997 - 2000, 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <roken.h>

#include <ui.h>
#ifdef HAVE_CONIO_H
#include <conio.h>
#endif

static sig_atomic_t intr_flag;

static void
intr(int sig)
{
    intr_flag++;
}

#ifdef HAVE_CONIO_H

/*
 * Windows does console slightly different then then unix case.
 */

static int
read_string(const char *preprompt, const char *prompt,
	    char *buf, size_t len, int echo)
{
    int of = 0;
    int c;
    char *p;
    void (*oldsigintr)(int);

    _cprintf("%s%s", preprompt, prompt);

    oldsigintr = signal(SIGINT, intr);

    p = buf;
    while(intr_flag == 0){
	c = ((echo)? _getche(): _getch());
	if(c == '\n' || c == '\r')
	    break;
	if(of == 0)
	    *p++ = c;
	of = (p == buf + len);
    }
    if(of)
	p--;
    *p = 0;

    if(echo == 0){
	printf("\n");
    }

    signal(SIGINT, oldsigintr);

    if(intr_flag)
	return -2;
    if(of)
	return -1;
    return 0;
}

#else /* !HAVE_CONIO_H */

#ifndef NSIG
#define NSIG 47
#endif

static int
read_string(const char *preprompt, const char *prompt,
	    char *buf, size_t len, int echo)
{
    struct sigaction sigs[NSIG];
    int oksigs[NSIG];
    struct sigaction sa;
    FILE *tty;
    int ret = 0;
    int of = 0;
    int i;
    int c;
    char *p;

    struct termios t_new, t_old;

    memset(&oksigs, 0, sizeof(oksigs));

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = intr;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    for(i = 1; i < sizeof(sigs) / sizeof(sigs[0]); i++)
	if (i != SIGALRM)
	    if (sigaction(i, &sa, &sigs[i]) == 0)
		oksigs[i] = 1;

    if((tty = fopen("/dev/tty", "r")) != NULL)
	rk_cloexec_file(tty);
    else
	tty = stdin;

    fprintf(stderr, "%s%s", preprompt, prompt);
    fflush(stderr);

    if(echo == 0){
	tcgetattr(fileno(tty), &t_old);
	memcpy(&t_new, &t_old, sizeof(t_new));
	t_new.c_lflag &= ~ECHO;
	tcsetattr(fileno(tty), TCSANOW, &t_new);
    }
    intr_flag = 0;
    p = buf;
    while(intr_flag == 0){
	c = getc(tty);
	if(c == EOF){
	    if(!ferror(tty))
		ret = 1;
	    break;
	}
	if(c == '\n')
	    break;
	if(of == 0)
	    *p++ = c;
	of = (p == buf + len);
    }
    if(of)
	p--;
    *p = 0;

    if(echo == 0){
	fprintf(stderr, "\n");
	tcsetattr(fileno(tty), TCSANOW, &t_old);
    }

    if(tty != stdin)
	fclose(tty);

    for(i = 1; i < sizeof(sigs) / sizeof(sigs[0]); i++)
	if (oksigs[i])
	    sigaction(i, &sigs[i], NULL);

    if(ret)
	return -3;
    if(intr_flag)
	return -2;
    if(of)
	return -1;
    return 0;
}

#endif /* HAVE_CONIO_H */

int
UI_UTIL_read_pw_string(char *buf, int length, const char *prompt, int verify)
{
    int ret;

    ret = read_string("", prompt, buf, length, 0);
    if (ret)
	return ret;

    if (verify) {
	char *buf2;
	buf2 = malloc(length);
	if (buf2 == NULL)
	    return 1;

	ret = read_string("Verify password - ", prompt, buf2, length, 0);
	if (ret) {
	    free(buf2);
	    return ret;
	}
	if (strcmp(buf2, buf) != 0)
	    ret = 1;
	free(buf2);
    }
    return ret;
}
