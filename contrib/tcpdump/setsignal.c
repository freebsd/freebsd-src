/*
 * Copyright (c) 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/setsignal.c,v 1.7 2000/07/11 00:49:03 assar Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <signal.h>
#ifdef HAVE_SIGACTION
#include <string.h>
#endif

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "setsignal.h"

/*
 * An os independent signal() with BSD semantics, e.g. the signal
 * catcher is restored following service of the signal.
 *
 * When sigset() is available, signal() has SYSV semantics and sigset()
 * has BSD semantics and call interface. Unfortunately, Linux does not
 * have sigset() so we use the more complicated sigaction() interface
 * there.
 *
 * Did I mention that signals suck?
 */
RETSIGTYPE
(*setsignal (int sig, RETSIGTYPE (*func)(int)))(int)
{
#ifdef HAVE_SIGACTION
	struct sigaction old, new;

	memset(&new, 0, sizeof(new));
	new.sa_handler = func;
#ifdef SA_RESTART
	new.sa_flags |= SA_RESTART;
#endif
	if (sigaction(sig, &new, &old) < 0)
		return (SIG_ERR);
	return (old.sa_handler);

#else
#ifdef HAVE_SIGSET
	return (sigset(sig, func));
#else
	return (signal(sig, func));
#endif
#endif
}

