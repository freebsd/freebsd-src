/*
 * Copyright (c) 1995, 1996, 1997, 1999, 2007 Kungliga Tekniska Högskolan
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
#include <rand.h>

#include <roken.h>

#include "randi.h"

#ifndef WIN32 /* don't bother with this on windows */

static volatile int counter;
static volatile unsigned char *gdata; /* Global data */
static volatile int igdata;	/* Index into global data */
static int gsize;

static
RETSIGTYPE
sigALRM(int sig)
{
    if (igdata < gsize)
	gdata[igdata++] ^= counter & 0xff;

#ifndef HAVE_SIGACTION
    signal(SIGALRM, sigALRM); /* Reinstall SysV signal handler */
#endif
    SIGRETURN(0);
}

#ifndef HAVE_SETITIMER
static void
pacemaker(struct timeval *tv)
{
    fd_set fds;
    pid_t pid;
    pid = getppid();
    while(1){
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(1, &fds, NULL, NULL, tv);
	kill(pid, SIGALRM);
    }
}
#endif

#ifdef HAVE_SIGACTION
/* XXX ugly hack, should perhaps use function from roken */
static RETSIGTYPE
(*fake_signal(int sig, RETSIGTYPE (*f)(int)))(int)
{
    struct sigaction sa, osa;
    sa.sa_handler = f;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, &osa);
    return osa.sa_handler;
}
#define signal(S, F) fake_signal((S), (F))
#endif

#endif /* WIN32*/

/*
 *
 */

static void
timer_seed(const void *indata, int size)
{
}

static int
timer_bytes(unsigned char *outdata, int size)
{
#ifdef WIN32
    return 0;
#else /* WIN32 */
    struct itimerval tv, otv;
    RETSIGTYPE (*osa)(int);
    int i, j;
#ifndef HAVE_SETITIMER
    RETSIGTYPE (*ochld)(int);
    pid_t pid;
#endif

    gdata = outdata;
    gsize = size;
    igdata = 0;

    osa = signal(SIGALRM, sigALRM);

    /* Start timer */
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 10 * 1000; /* 10 ms */
    tv.it_interval = tv.it_value;
#ifdef HAVE_SETITIMER
    setitimer(ITIMER_REAL, &tv, &otv);
#else
    ochld = signal(SIGCHLD, SIG_IGN);
    pid = fork();
    if(pid == -1){
	signal(SIGCHLD, ochld != SIG_ERR ? ochld : SIG_DFL);
	des_not_rand_data(data, size);
	return;
    }
    if(pid == 0)
	pacemaker(&tv.it_interval);
#endif

    for(i = 0; i < 4; i++) {
	for (igdata = 0; igdata < size;) /* igdata++ in sigALRM */
	    counter++;
	for (j = 0; j < size; j++) /* Only use 2 bits each lap */
	    gdata[j] = (gdata[j]>>2) | (gdata[j]<<6);
    }
#ifdef HAVE_SETITIMER
    setitimer(ITIMER_REAL, &otv, 0);
#else
    kill(pid, SIGKILL);
    while(waitpid(pid, NULL, 0) != pid);
    signal(SIGCHLD, ochld != SIG_ERR ? ochld : SIG_DFL);
#endif
    signal(SIGALRM, osa != SIG_ERR ? osa : SIG_DFL);

    return 1;
#endif
}

static void
timer_cleanup(void)
{
}

static void
timer_add(const void *indata, int size, double entropi)
{
}

static int
timer_pseudorand(unsigned char *outdata, int size)
{
    return timer_bytes(outdata, size);
}

static int
timer_status(void)
{
#ifdef WIN32
    return 0;
#else
    return 1;
#endif
}

const RAND_METHOD hc_rand_timer_method = {
    timer_seed,
    timer_bytes,
    timer_cleanup,
    timer_add,
    timer_pseudorand,
    timer_status
};

const RAND_METHOD *
RAND_timer_method(void)
{
    return &hc_rand_timer_method;
}
