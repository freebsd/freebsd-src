/****************************************************************************
 *
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************
 *
 * sem test.
 *
 * $FreeBSD: src/lib/libc_r/test/sem_d.c,v 1.1.2.1 2000/07/17 22:18:32 jasone Exp $
 *
 ****************************************************************************/

#define _REENTRANT

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>

#define NTHREADS 10

void *
entry(void * a_arg)
{
	sem_t * sem = (sem_t *) a_arg;

	sem_wait(sem);
	fprintf(stderr, "Got semaphore\n");
  
	return NULL;
}

int
main()
{
	sem_t sem_a, sem_b;
	pthread_t threads[NTHREADS];
	unsigned i;
	int val;
  
	fprintf(stderr, "Test begin\n");

#ifdef _LIBC_R_
	assert(-1 == sem_init(&sem_b, 1, 0));
	assert(EPERM == errno);
#endif

	assert(0 == sem_init(&sem_b, 0, 0));
	assert(0 == sem_getvalue(&sem_b, &val));
	assert(0 == val);
  
	assert(0 == sem_post(&sem_b));
	assert(0 == sem_getvalue(&sem_b, &val));
	assert(1 == val);
  
	assert(0 == sem_wait(&sem_b));
	assert(-1 == sem_trywait(&sem_b));
	assert(EAGAIN == errno);
	assert(0 == sem_post(&sem_b));
	assert(0 == sem_trywait(&sem_b));
	assert(0 == sem_post(&sem_b));
	assert(0 == sem_wait(&sem_b));
	assert(0 == sem_post(&sem_b));

#ifdef _LIBC_R_
	assert(SEM_FAILED == sem_open("/foo", O_CREAT | O_EXCL, 0644, 0));
	assert(ENOSYS == errno);

	assert(-1 == sem_close(&sem_b));
	assert(ENOSYS == errno);
  
	assert(-1 == sem_unlink("/foo"));
	assert(ENOSYS == errno);
#endif

	assert(0 == sem_destroy(&sem_b));
  
	assert(0 == sem_init(&sem_a, 0, 0));

	for (i = 0; i < NTHREADS; i++) {
		pthread_create(&threads[i], NULL, entry, (void *) &sem_a);
	}

	for (i = 0; i < NTHREADS; i++) {
		assert(0 == sem_post(&sem_a));
	}
  
	for (i = 0; i < NTHREADS; i++) {
		pthread_join(threads[i], NULL);
	}
  
	for (i = 0; i < NTHREADS; i++) {
		pthread_create(&threads[i], NULL, entry, (void *) &sem_a);
	}

	for (i = 0; i < NTHREADS; i++) {
		assert(0 == sem_post(&sem_a));
	}
  
	for (i = 0; i < NTHREADS; i++) {
		pthread_join(threads[i], NULL);
	}
  
	assert(0 == sem_destroy(&sem_a));

	fprintf(stderr, "Test end\n");
	return 0;
}
