/* test stack unwinding for a new thread */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "Test.cpp"

static void *
thr_routine(void *arg __unused)
{
	Test test;

	pthread_exit(NULL);
	printf("Bug, thread shouldn't be here\n");
}

int
main()
{
	pthread_t td;

	pthread_create(&td, NULL, thr_routine, NULL);
	pthread_join(td, NULL);
	check_destruct();
	return (0);
}
