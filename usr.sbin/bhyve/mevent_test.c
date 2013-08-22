/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

/*
 * Test program for the micro event library. Set up a simple TCP echo
 * service.
 *
 *  cc mevent_test.c mevent.c -lpthread
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "mevent.h"

#define TEST_PORT	4321

static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t accept_condvar = PTHREAD_COND_INITIALIZER;

#define MEVENT_ECHO

#ifdef MEVENT_ECHO
struct esync {
	pthread_mutex_t	e_mt;
	pthread_cond_t	e_cond;       
};

static void
echoer_callback(int fd, enum ev_type type, void *param)
{
	struct esync *sync = param;

	pthread_mutex_lock(&sync->e_mt);
	pthread_cond_signal(&sync->e_cond);
	pthread_mutex_unlock(&sync->e_mt);
}

static void *
echoer(void *param)
{
	struct esync sync;
	struct mevent *mev;
	char buf[128];
	int fd = (int)(uintptr_t) param;
	int len;

	pthread_mutex_init(&sync.e_mt, NULL);
	pthread_cond_init(&sync.e_cond, NULL);

	pthread_mutex_lock(&sync.e_mt);

	mev = mevent_add(fd, EVF_READ, echoer_callback, &sync);
	if (mev == NULL) {
		printf("Could not allocate echoer event\n");
		exit(1);
	}

	while (!pthread_cond_wait(&sync.e_cond, &sync.e_mt)) {
		len = read(fd, buf, sizeof(buf));
		if (len > 0) {
			write(fd, buf, len);
			write(0, buf, len);
		} else {
			break;
		}
	}

	mevent_delete_close(mev);

	pthread_mutex_unlock(&sync.e_mt);
	pthread_mutex_destroy(&sync.e_mt);
	pthread_cond_destroy(&sync.e_cond);
}

#else

static void *
echoer(void *param)
{
	char buf[128];
	int fd = (int)(uintptr_t) param;
	int len;

	while ((len = read(fd, buf, sizeof(buf))) > 0) {
		write(1, buf, len);
	}
}
#endif /* MEVENT_ECHO */

static void
acceptor_callback(int fd, enum ev_type type, void *param)
{
	pthread_mutex_lock(&accept_mutex);
	pthread_cond_signal(&accept_condvar);
	pthread_mutex_unlock(&accept_mutex);
}

static void *
acceptor(void *param)
{
	struct sockaddr_in sin;
	pthread_t tid;
	int news;
	int s;

        if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("socket");
                exit(1);
        }

        sin.sin_len = sizeof(sin);
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
        sin.sin_port = htons(TEST_PORT);

        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
                perror("bind");
                exit(1);
        }

        if (listen(s, 1) < 0) {
                perror("listen");
                exit(1);
        }

	(void) mevent_add(s, EVF_READ, acceptor_callback, NULL);

	pthread_mutex_lock(&accept_mutex);

	while (!pthread_cond_wait(&accept_condvar, &accept_mutex)) {
		news = accept(s, NULL, NULL);
		if (news < 0) {
			perror("accept error");
		} else {
			printf("incoming connection, spawning thread\n");
			pthread_create(&tid, NULL, echoer,
				       (void *)(uintptr_t)news);
		}
	}
}

main()
{
	pthread_t tid;

	pthread_create(&tid, NULL, acceptor, NULL);

	mevent_dispatch();
}
