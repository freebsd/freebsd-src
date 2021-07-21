/* $FreeBSD$ */
/* check unwinding for main thread */

#include <pthread.h>
#include <cstdio>
#include <stdlib.h>

#include "Test.cpp"

int
main()
{
	Test test;

	atexit(check_destruct);
	pthread_exit((void *)1);
	return (0);
}
