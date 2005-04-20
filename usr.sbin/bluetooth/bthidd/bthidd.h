/*
 * bthidd.h
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
 * $Id: bthidd.h,v 1.4 2004/02/26 21:44:20 max Exp $
 * $FreeBSD$
 */

#ifndef _BTHIDD_H_
#define _BTHIDD_H_ 1

#define BTHIDD_IDENT	"bthidd"
#define BTHIDD_PIDFILE	"/var/run/" BTHIDD_IDENT ".pid"

struct bthid_session;

struct bthid_server
{
	bdaddr_t			bdaddr;	/* local bdaddr */
	int				cons;	/* /dev/consolectl */
	int				ctrl;   /* control channel (listen) */
	int				intr;   /* interrupt channel (listen) */
	int				maxfd;	/* max fd in sets */
	fd_set				rfdset;	/* read descriptor set */
	fd_set				wfdset;	/* write descriptor set */
	LIST_HEAD(, bthid_session)	sessions;
};

typedef struct bthid_server	bthid_server_t;
typedef struct bthid_server *	bthid_server_p;

struct bthid_session
{
	bthid_server_p			srv;	/* pointer back to server */
	int				ctrl;	/* control channel */
	int				intr;	/* interrupt channel */
	bdaddr_t			bdaddr;	/* remote bdaddr */
	short				state;	/* session state */
#define CLOSED	0
#define	W4CTRL	1
#define	W4INTR	2
#define	OPEN	3
	LIST_ENTRY(bthid_session)	next;	/* link to next */
};

typedef struct bthid_session	bthid_session_t;
typedef struct bthid_session *	bthid_session_p;

int		server_init      (bthid_server_p srv);
void		server_shutdown  (bthid_server_p srv);
int		server_do        (bthid_server_p srv);

int		client_rescan    (bthid_server_p srv);
int		client_connect   (bthid_server_p srv, int fd);

bthid_session_p	session_open     (bthid_server_p srv, bdaddr_p bdaddr);
bthid_session_p	session_by_bdaddr(bthid_server_p srv, bdaddr_p bdaddr);
bthid_session_p	session_by_fd    (bthid_server_p srv, int fd);
void		session_close    (bthid_session_p s);

int		hid_control      (bthid_session_p s, char *data, int len);
int		hid_interrupt    (bthid_session_p s, char *data, int len);

#endif /* ndef _BTHIDD_H_ */

