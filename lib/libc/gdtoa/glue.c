/*
 * Machine-independent glue to integrate David Gay's gdtoa
 * package into libc.
 *
 * $FreeBSD: src/lib/libc/gdtoa/glue.c,v 1.2.28.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <pthread.h>

pthread_mutex_t __gdtoa_locks[] = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
};
