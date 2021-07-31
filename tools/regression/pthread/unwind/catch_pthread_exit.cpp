/* $FreeBSD$ */
/* try to catch thread exiting, and rethrow the exception */

#include <pthread.h>

static bool caught;

static void *
thr_routine(void *arg __unused)
{
	try {
		pthread_exit(NULL);
	} catch (...) {
		caught = true;
		std::printf("thread exiting exception caught\n");
		/* rethrow */
		throw;
	}
}

int
main()
{
	pthread_t td;

	pthread_create(&td, NULL, thr_routine, NULL);
	pthread_join(td, NULL);
	if (caught)
		std::printf("OK\n");
	else
		std::printf("failure\n");
	return (0);
}
