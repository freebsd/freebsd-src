/*
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */


#ifndef _IBCS2_POLL_H
#define _IBCS2_POLL_H 1

/* iBCS2 poll commands */  
#define IBCS2_POLLIN            0x0001
#define IBCS2_POLLPRI           0x0002
#define IBCS2_POLLOUT           0x0004
#define IBCS2_POLLERR           0x0008
#define IBCS2_POLLHUP           0x0010
#define IBCS2_POLLNVAL          0x0020
#define IBCS2_POLLRDNORM        0x0040
#define IBCS2_POLLWRNORM        0x0004
#define IBCS2_POLLRDBAND        0x0080
#define IBCS2_POLLWRBAND        0x0100
#define IBCS2_READPOLL  (IBCS2_POLLIN|IBCS2_POLLRDNORM|IBCS2_POLLRDBAND)
#define IBCS2_WRITEPOLL (IBCS2_POLLOUT|IBCS2_POLLWRNORM|IBCS2_POLLWRBAND)

struct ibcs2_poll {
	int fd;
	short events;
	short revents;
};

#endif /* _IBCS2_POLL_H */
