/*
 * Machine-independent glue to integrate David Gay's gdtoa
 * package into libc.
 *
 * $FreeBSD: src/lib/libc/gdtoa/glue.c,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $
 */

#include <pthread.h>

pthread_mutex_t __gdtoa_locks[] = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
};
