/*
 * session.c
 *
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: session.c,v 1.1 2004/02/12 22:46:59 max Exp $
 * $FreeBSD$
 */

#include <sys/queue.h>
#include <assert.h>
#include <bluetooth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bthidd.h"

/*
 * Create new session
 */

bthid_session_p
session_open(bthid_server_p srv, bdaddr_p bdaddr)
{
	bthid_session_p	s = NULL;

	assert(srv != NULL);
	assert(bdaddr != NULL);

	if ((s = (bthid_session_p) malloc(sizeof(*s))) != NULL) {
		s->srv = srv;  
		memcpy(&s->bdaddr, bdaddr, sizeof(s->bdaddr));
		s->ctrl = s->intr = -1;
		s->state = CLOSED;

		LIST_INSERT_HEAD(&srv->sessions, s, next);
	}

	return (s);
}

/*
 * Lookup session by bdaddr
 */

bthid_session_p
session_by_bdaddr(bthid_server_p srv, bdaddr_p bdaddr)
{
	bthid_session_p	s = NULL;

	assert(srv != NULL);
	assert(bdaddr != NULL);

	LIST_FOREACH(s, &srv->sessions, next)
		if (memcmp(&s->bdaddr, bdaddr, sizeof(s->bdaddr)) == 0)
			break;

	return (s);
}

/*
 * Lookup session by fd
 */

bthid_session_p
session_by_fd(bthid_server_p srv, int fd)
{
	bthid_session_p	s = NULL;

	assert(srv != NULL);
	assert(fd >= 0);

	LIST_FOREACH(s, &srv->sessions, next)
		if (s->ctrl == fd || s->intr == fd)
			break;

	return (s);
}

/*
 * Close session
 */

void
session_close(bthid_session_p s)
{
	assert(s != NULL);
	assert(s->srv != NULL);

	LIST_REMOVE(s, next);

	if (s->intr != -1) {
		FD_CLR(s->intr, &s->srv->rfdset);
		FD_CLR(s->intr, &s->srv->wfdset);
		close(s->intr);

		if (s->srv->maxfd == s->intr)
			s->srv->maxfd --;
	}

	if (s->ctrl != -1) {
		FD_CLR(s->ctrl, &s->srv->rfdset);
		FD_CLR(s->ctrl, &s->srv->wfdset);
		close(s->ctrl);

		if (s->srv->maxfd == s->ctrl)
			s->srv->maxfd --;
	}

	memset(s, 0, sizeof(*s));
	free(s);
}

