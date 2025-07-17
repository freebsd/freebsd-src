#!/bin/sh

# The program is a test case by Eric which demonstrates the
# bug, unkillable spinning thread, owning a spinlock.

# "panic: spin lock held too long" seen.
# Fixed in r277970.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > kern_umtx_inf_loop.c
mycc -o kern_umtx_inf_loop -Wall -Wextra -O0 -g kern_umtx_inf_loop.c \
    -lpthread || exit 1
rm -f kern_umtx_inf_loop.c

/tmp/kern_umtx_inf_loop

rm -f /tmp/kern_umtx_inf_loop
exit 0
EOF
/*-
 * Copyright (c) 2015 Eric van Gyzen
 * All rights reserved.
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

#include <sys/types.h>

#include <machine/cpufunc.h>
#include <machine/cpu.h>

#include <err.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t the_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t contender;

static void *
contender_func(void *arg)
{
    int error;

    (void) arg;

    error = pthread_mutex_lock(&the_mutex);
    if (error) errc(1, error, "pthread_mutex_lock contender");

    fprintf(stderr, "contender lock succeeded\n");

    error = pthread_mutex_unlock(&the_mutex);
    if (error) errc(1, error, "pthread_mutex_unlock contender");

    fprintf(stderr, "contender unlock succeeded; exiting\n");

    return (NULL);
}

static void *
signaler_func(void *arg __unused)
{
    int error;

    // Wait for the main thread to sleep.
    usleep(100000);

    error = pthread_kill(contender, SIGHUP);
    if (error) errc(1, error, "pthread_kill");

    // Wait for the contender to lock umtx_lock
    // in umtx_repropagate_priority.
    usleep(100000);

    error = pthread_mutex_lock(&the_mutex);
    if (error) errc(1, error, "pthread_mutex_lock signaler");

    return (NULL);
}

int
main(void)
{
    int error;

    pthread_mutexattr_t mattr;

    error = pthread_mutexattr_init(&mattr);
    if (error) errc(1, error, "pthread_mutexattr_init");

    error = pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
    if (error) errc(1, error, "pthread_mutexattr_setprotocol");

    error = pthread_mutex_init(&the_mutex, &mattr);
    if (error) errc(1, error, "pthread_mutex_init");

    error = pthread_mutexattr_destroy(&mattr);
    if (error) errc(1, error, "pthread_mutexattr_destroy");

    //error = pthread_mutex_lock(&the_mutex);
    //if (error) errc(1, error, "pthread_mutex_lock");

    // Hack lock.
    *(int *)the_mutex = pthread_getthreadid_np();

    error = pthread_create(&contender, NULL, contender_func, NULL);
    if (error) errc(1, error, "pthread_create");

    // Wait for the contender to sleep.
    usleep(100000);

    pthread_t signaler;
    error = pthread_create(&signaler, NULL, signaler_func, NULL);
    if (error) errc(1, error, "pthread_create");

    error = pthread_mutex_lock(&the_mutex);
    if (error) errc(1, error, "pthread_mutex_lock recurse");

    return (0);
}

