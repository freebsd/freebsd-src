/* $FreeBSD$ */
/* Test stack unwinding for pthread_cond_wait function */

#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>

#include "Test.cpp"

static pthread_mutex_t mtx;
static pthread_cond_t cv;

static void *
thr(void *arg __unused)
{
	Test t;

	pthread_mutex_lock(&mtx);
	pthread_cond_wait(&cv, &mtx);
	pthread_mutex_unlock(&mtx);
	printf("Bug, thread shouldn't be here.\n");
	return (0);
}

int
main()
{
	pthread_t td;

	pthread_mutex_init(&mtx, NULL);
	pthread_cond_init(&cv, NULL);
	pthread_create(&td, NULL, thr, NULL);
	sleep(1);
	pthread_cancel(td);
	pthread_join(td, NULL);
	check_destruct();
	return (0);
}
