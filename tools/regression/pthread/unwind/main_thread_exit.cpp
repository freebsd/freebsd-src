/* $FreeBSD$ */
/* check unwinding for main thread */

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "Test.cpp"

int
main()
{
	Test test;

	atexit(check_destruct);
	pthread_exit((void *)1);
	return (0);
}
