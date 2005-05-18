/*
 * server.c
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
 * $Id: server.c,v 1.7 2004/11/17 21:59:42 max Exp $
 * $FreeBSD$
 */

#include <sys/queue.h>
#include <assert.h>
#include <bluetooth.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <usbhid.h>
#include "bthidd.h"
#include "bthid_config.h"
#include "kbd.h"

#undef	max
#define	max(x, y)	(((x) > (y))? (x) : (y))

static int	server_accept (bthid_server_p srv, int fd);
static int	server_process(bthid_server_p srv, int fd);

/*
 * Initialize server
 */

int
server_init(bthid_server_p srv)
{
	struct sockaddr_l2cap	l2addr;

	assert(srv != NULL);

	srv->ctrl = srv->intr = -1;
	FD_ZERO(&srv->rfdset);
	FD_ZERO(&srv->wfdset);
	LIST_INIT(&srv->sessions);

	/* Allocate HID keycodes buffer */
	srv->keys = bit_alloc(kbd_maxkey());
	if (srv->keys == NULL) {
		syslog(LOG_ERR, "Could not allocate HID keys buffer");
		return (-1);
	}
	memset(srv->keys, 0, bitstr_size(kbd_maxkey()));

	/* Get wired keyboard index (if was not specified) */
	if (srv->windex == -1) {
		srv->windex = kbd_get_index("/dev/console");
		if (srv->windex < 0) {
			syslog(LOG_ERR, "Could not open get wired keyboard " \
				"index. %s (%d)", strerror(errno), errno);
			free(srv->keys);
			return (-1);
		}
	}

	/* Open /dev/consolectl */
	srv->cons = open("/dev/consolectl", O_RDWR);
	if (srv->cons < 0) {
		syslog(LOG_ERR, "Could not open /dev/consolectl. %s (%d)",
			strerror(errno), errno);
		return (-1);
	}

	/* Open /dev/vkbdctl */
	srv->vkbd = open("/dev/vkbdctl", O_RDWR);
	if (srv->vkbd < 0) {
		syslog(LOG_ERR, "Could not open /dev/vkbdctl. %s (%d)",
			strerror(errno), errno);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	/* Create control socket */
	srv->ctrl = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BLUETOOTH_PROTO_L2CAP);
	if (srv->ctrl < 0) {
		syslog(LOG_ERR, "Could not create control L2CAP socket. " \
			"%s (%d)", strerror(errno), errno);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	l2addr.l2cap_len = sizeof(l2addr);
	l2addr.l2cap_family = AF_BLUETOOTH;
	memcpy(&l2addr.l2cap_bdaddr, &srv->bdaddr, sizeof(l2addr.l2cap_bdaddr));
	l2addr.l2cap_psm =  0x11;

	if (bind(srv->ctrl, (struct sockaddr *) &l2addr, sizeof(l2addr)) < 0) {
		syslog(LOG_ERR, "Could not bind control L2CAP socket. " \
			"%s (%d)", strerror(errno), errno);
		close(srv->ctrl);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	if (listen(srv->ctrl, 10) < 0) {
		syslog(LOG_ERR, "Could not listen on control L2CAP socket. " \
			"%s (%d)", strerror(errno), errno);
		close(srv->ctrl);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	/* Create intrrupt socket */
	srv->intr = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BLUETOOTH_PROTO_L2CAP);
	if (srv->intr < 0) {
		syslog(LOG_ERR, "Could not create interrupt L2CAP socket. " \
			"%s (%d)", strerror(errno), errno);
		close(srv->ctrl);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	l2addr.l2cap_psm = 0x13;

	if (bind(srv->intr, (struct sockaddr *) &l2addr, sizeof(l2addr)) < 0) {
		syslog(LOG_ERR, "Could not bind interrupt L2CAP socket. " \
			"%s (%d)", strerror(errno), errno);
		close(srv->intr);
		close(srv->ctrl);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	if (listen(srv->intr, 10) < 0) {
		syslog(LOG_ERR, "Could not listen on interrupt L2CAP socket. "\
			"%s (%d)", strerror(errno), errno);
		close(srv->intr);
		close(srv->ctrl);
		close(srv->vkbd);
		close(srv->cons);
		free(srv->keys);
		return (-1);
	}

	FD_SET(srv->ctrl, &srv->rfdset);
	FD_SET(srv->intr, &srv->rfdset);
	srv->maxfd = max(srv->ctrl, srv->intr);

	return (0);
}

/*
 * Shutdown server
 */

void
server_shutdown(bthid_server_p srv)
{
	assert(srv != NULL);

	close(srv->cons);
	close(srv->vkbd);
	close(srv->ctrl);
	close(srv->intr);

	while (!LIST_EMPTY(&srv->sessions))
		session_close(LIST_FIRST(&srv->sessions));

	free(srv->keys);

	memset(srv, 0, sizeof(*srv));
}

/*
 * Do one server iteration
 */

int
server_do(bthid_server_p srv)
{
	struct timeval	tv;
	fd_set		rfdset, wfdset;
	int		n, fd;

	assert(srv != NULL);

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* Copy cached version of the fd sets and call select */
	memcpy(&rfdset, &srv->rfdset, sizeof(rfdset));
	memcpy(&wfdset, &srv->wfdset, sizeof(wfdset));

	n = select(srv->maxfd + 1, &rfdset, &wfdset, NULL, &tv);
	if (n < 0) {
		if (errno == EINTR)  
			return (0);

		syslog(LOG_ERR, "Could not select(%d, %p, %p). %s (%d)",
			srv->maxfd + 1, &rfdset, &wfdset, strerror(errno), errno);

		return (-1);
	}

	/* Process descriptors (if any) */
	for (fd = 0; fd < srv->maxfd + 1 && n > 0; fd ++) {
		if (FD_ISSET(fd, &rfdset)) {
			n --;

			if (fd == srv->ctrl || fd == srv->intr)
				server_accept(srv, fd);
			else
				server_process(srv, fd);
		} else if (FD_ISSET(fd, &wfdset)) {
			n --;

			client_connect(srv, fd);
		}
	}

	return (0);
}

/*
 * Accept new connection 
 */

static int
server_accept(bthid_server_p srv, int fd)
{
	bthid_session_p		s = NULL;
	hid_device_p		d = NULL;
	struct sockaddr_l2cap	l2addr;
	int			len, new_fd;

	len = sizeof(l2addr);
	if ((new_fd = accept(fd, (struct sockaddr *) &l2addr, &len)) < 0) {
		syslog(LOG_ERR, "Could not accept %s connection. %s (%d)",
			(fd == srv->ctrl)? "control" : "interrupt",
			strerror(errno), errno);
		return (-1);
	}

	/* Check if we have session for the device */
	if ((s = session_by_bdaddr(srv, &l2addr.l2cap_bdaddr)) == NULL) {
		/* Is device configured? */
		if ((d = get_hid_device(&l2addr.l2cap_bdaddr)) == NULL) {
			syslog(LOG_ERR, "Rejecting %s connection from %s. " \
				"Device not configured",
				(fd == srv->ctrl)? "control" : "interrupt",
				bt_ntoa(&l2addr.l2cap_bdaddr, NULL));
			close(new_fd);
			return (-1);
		}

		d->new_device = 0; /* reset new device flag */
		write_hids_file();

		/* Create new inbound session */
		if ((s = session_open(srv, &l2addr.l2cap_bdaddr)) == NULL) {
			syslog(LOG_CRIT, "Could not open inbound session " \
				"for %s. Not enough memory",
				bt_ntoa(&l2addr.l2cap_bdaddr, NULL));
			close(new_fd);
			return (-1);
		}
	}

	/* Update descriptors */
	if (fd == srv->ctrl) {
		assert(s->ctrl == -1);
		s->ctrl = new_fd;
		s->state = (s->intr == -1)? W4INTR : OPEN;
	} else {
		assert(s->intr == -1);
		s->intr = new_fd;
		s->state = (s->ctrl == -1)? W4CTRL : OPEN;
	}

	FD_SET(new_fd, &srv->rfdset);
	if (new_fd > srv->maxfd)
		srv->maxfd = new_fd;

	syslog(LOG_NOTICE, "Accepted %s connection from %s",
		(fd == srv->ctrl)? "control" : "interrupt",
		bt_ntoa(&l2addr.l2cap_bdaddr, NULL));

	return (0);
}

/*
 * Process data on the connection
 */

static int
server_process(bthid_server_p srv, int fd)
{
	bthid_session_p	s = session_by_fd(srv, fd);
	char		data[1024];
	int		len;

	if (s == NULL)
		return (0); /* can happen on device disconnect */

	do {
		len = read(fd, data, sizeof(data));
	} while (len < 0 && errno == EINTR);

	if (len < 0) {
		syslog(LOG_ERR, "Could not read data from %s (%s). %s (%d)",
			bt_ntoa(&s->bdaddr, NULL),
			(fd == s->ctrl)? "control" : "interrupt",
			strerror(errno), errno);
		session_close(s);
		return (0);
	}

	if (len == 0) {
		syslog(LOG_NOTICE, "Remote device %s has closed %s connection",
			bt_ntoa(&s->bdaddr, NULL),
			(fd == s->ctrl)? "control" : "interrupt");
		session_close(s);
		return (0);
	}

	if (fd == s->ctrl)
		hid_control(s, data, len);
	else
		hid_interrupt(s, data, len);

	return (0);
}

