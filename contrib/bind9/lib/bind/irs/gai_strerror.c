/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <port_before.h>
#include <netdb.h>
#include <port_after.h>

#ifdef DO_PTHREADS
#include <pthread.h>
#include <stdlib.h>
#endif

static const char *gai_errlist[] = {
	"no error",
	"address family not supported for name",/* EAI_ADDRFAMILY */
	"temporary failure",			/* EAI_AGAIN */
	"invalid flags",			/* EAI_BADFLAGS */
	"permanent failure",			/* EAI_FAIL */
	"address family not supported",		/* EAI_FAMILY */
	"memory failure",			/* EAI_MEMORY */
	"no address",				/* EAI_NODATA */
	"unknown name or service",		/* EAI_NONAME */
	"service not supported for socktype",	/* EAI_SERVICE */
	"socktype not supported",		/* EAI_SOCKTYPE */
	"system failure",			/* EAI_SYSTEM */
	"bad hints",				/* EAI_BADHINTS */
	"bad protocol",				/* EAI_PROTOCOL */

	"unknown error"				/* Must be last. */
};

static const int gai_nerr = (sizeof(gai_errlist)/sizeof(*gai_errlist));

#define EAI_BUFSIZE 128

const char *
gai_strerror(int ecode) {
#ifndef DO_PTHREADS
	static char buf[EAI_BUFSIZE];
#else	/* DO_PTHREADS */
#ifndef LIBBIND_MUTEX_INITIALIZER
#define LIBBIND_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif
	static pthread_mutex_t lock = LIBBIND_MUTEX_INITIALIZER;
	static pthread_key_t key;
	static int once = 0;
	char *buf;
#endif

	if (ecode >= 0 && ecode < (gai_nerr - 1))
		return (gai_errlist[ecode]);

#ifdef DO_PTHREADS
        if (!once) {
                pthread_mutex_lock(&lock);
                if (!once++)
                        pthread_key_create(&key, free);
                pthread_mutex_unlock(&lock);
        }

	buf = pthread_getspecific(key);
        if (buf == NULL) {
		buf = malloc(EAI_BUFSIZE);
                if (buf == NULL)
                        return ("unknown error");
                pthread_setspecific(key, buf);
        }
#endif
	/* 
	 * XXX This really should be snprintf(buf, EAI_BUFSIZE, ...).
	 * It is safe until message catalogs are used.
	 */
	sprintf(buf, "%s: %d", gai_errlist[gai_nerr - 1], ecode);
	return (buf);
}
