/*
 * poll.h
 *
 * Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 *
 */

#ifndef POLL_H
#  define POLL_H

#if (defined(HAS_POLL) && defined(I_POLL)) || defined(POLLWRBAND)
#  include <poll.h>
#else
#ifdef HAS_SELECT


/* We shall emulate poll using select */

#define EMULATE_POLL_WITH_SELECT

typedef struct pollfd {
    int fd;
    short events;
    short revents;
} pollfd_t;

#define	POLLIN		0x0001
#define	POLLPRI		0x0002
#define	POLLOUT		0x0004
#define	POLLRDNORM	0x0040
#define	POLLWRNORM	POLLOUT
#define	POLLRDBAND	0x0080
#define	POLLWRBAND	0x0100
#define	POLLNORM	POLLRDNORM

/* Return ONLY events (NON testable) */

#define	POLLERR		0x0008
#define	POLLHUP		0x0010
#define	POLLNVAL	0x0020

int poll (struct pollfd *, unsigned long, int);

#ifndef HAS_POLL
#  define HAS_POLL
#endif

#endif /* HAS_SELECT */

#endif /* I_POLL */

#endif /* POLL_H */

