/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#ifdef __APPLE__
# include "../apple_endian.h"
#else
# include <sys/endian.h>
#endif
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/uio.h>
#include <netdb.h>
#include "../lib9p.h"
#include "../lib9p_impl.h"
#include "../log.h"
#include "socket.h"

struct l9p_socket_softc
{
	struct l9p_connection *ls_conn;
	struct sockaddr ls_sockaddr;
	socklen_t ls_socklen;
	pthread_t ls_thread;
	int ls_fd;
};

static int l9p_socket_readmsg(struct l9p_socket_softc *, void **, size_t *);
static int l9p_socket_get_response_buffer(struct l9p_request *,
    struct iovec *, size_t *, void *);
static int l9p_socket_send_response(struct l9p_request *, const struct iovec *,
    const size_t, const size_t, void *);
static void l9p_socket_drop_response(struct l9p_request *, const struct iovec *,
    size_t, void *);
static void *l9p_socket_thread(void *);
static ssize_t xread(int, void *, size_t);
static ssize_t xwrite(int, void *, size_t);

int
l9p_start_server(struct l9p_server *server, const char *host, const char *port)
{
	struct addrinfo *res, *res0, hints;
	struct kevent kev[2];
	struct kevent event[2];
	int err, kq, i, val, evs, nsockets = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	err = getaddrinfo(host, port, &hints, &res0);

	if (err)
		return (-1);

	for (res = res0; res; res = res->ai_next) {
		int s = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);

		val = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

		if (s < 0)
			continue;

		if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
			close(s);
			continue;
		}

		EV_SET(&kev[nsockets++], s, EVFILT_READ, EV_ADD | EV_ENABLE, 0,
		    0, 0);
		listen(s, 10);
	}

	if (nsockets < 1) {
		L9P_LOG(L9P_ERROR, "bind(): %s", strerror(errno));
		return(-1);
	}

	kq = kqueue();

	if (kevent(kq, kev, nsockets, NULL, 0, NULL) < 0) {
		L9P_LOG(L9P_ERROR, "kevent(): %s", strerror(errno));
		return (-1);
	}

	for (;;) {
		evs = kevent(kq, NULL, 0, event, nsockets, NULL);
		if (evs < 0) {
			if (errno == EINTR)
				continue;

			L9P_LOG(L9P_ERROR, "kevent(): %s", strerror(errno));
			return (-1);
		}

		for (i = 0; i < evs; i++) {
			struct sockaddr client_addr;
			socklen_t client_addr_len = sizeof(client_addr);
			int news = accept((int)event[i].ident, &client_addr,
			    &client_addr_len);

			if (news < 0) {
				L9P_LOG(L9P_WARNING, "accept(): %s",
				    strerror(errno));
				continue;
			}

			l9p_socket_accept(server, news, &client_addr,
			    client_addr_len);
		}
	}

}

void
l9p_socket_accept(struct l9p_server *server, int conn_fd,
    struct sockaddr *client_addr, socklen_t client_addr_len)
{
	struct l9p_socket_softc *sc;
	struct l9p_connection *conn;
	char host[NI_MAXHOST + 1];
	char serv[NI_MAXSERV + 1];
	int err;

	err = getnameinfo(client_addr, client_addr_len, host, NI_MAXHOST, serv,
	    NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

	if (err != 0) {
		L9P_LOG(L9P_WARNING, "cannot look up client name: %s",
		    gai_strerror(err));
	} else {
		L9P_LOG(L9P_INFO, "new connection from %s:%s", host, serv);
	}

	if (l9p_connection_init(server, &conn) != 0) {
		L9P_LOG(L9P_ERROR, "cannot create new connection");
		return;
	}

	sc = l9p_calloc(1, sizeof(*sc));
	sc->ls_conn = conn;
	sc->ls_fd = conn_fd;

	/*
	 * Fill in transport handler functions and aux argument.
	 */
	conn->lc_lt.lt_aux = sc;
	conn->lc_lt.lt_get_response_buffer = l9p_socket_get_response_buffer;
	conn->lc_lt.lt_send_response = l9p_socket_send_response;
	conn->lc_lt.lt_drop_response = l9p_socket_drop_response;

	err = pthread_create(&sc->ls_thread, NULL, l9p_socket_thread, sc);
	if (err) {
		L9P_LOG(L9P_ERROR,
		    "pthread_create (for connection from %s:%s): error %s",
		    host, serv, strerror(err));
		l9p_connection_close(sc->ls_conn);
		free(sc);
	}
}

static void *
l9p_socket_thread(void *arg)
{
	struct l9p_socket_softc *sc = (struct l9p_socket_softc *)arg;
	struct iovec iov;
	void *buf;
	size_t length;

	for (;;) {
		if (l9p_socket_readmsg(sc, &buf, &length) != 0)
			break;

		iov.iov_base = buf;
		iov.iov_len = length;
		l9p_connection_recv(sc->ls_conn, &iov, 1, NULL);
		free(buf);
	}

	L9P_LOG(L9P_INFO, "connection closed");
	l9p_connection_close(sc->ls_conn);
	free(sc);
	return (NULL);
}

static int
l9p_socket_readmsg(struct l9p_socket_softc *sc, void **buf, size_t *size)
{
	uint32_t msize;
	size_t toread;
	ssize_t ret;
	void *buffer;
	int fd = sc->ls_fd;

	assert(fd > 0);

	buffer = l9p_malloc(sizeof(uint32_t));

	ret = xread(fd, buffer, sizeof(uint32_t));
	if (ret < 0) {
		L9P_LOG(L9P_ERROR, "read(): %s", strerror(errno));
		return (-1);
	}

	if (ret != sizeof(uint32_t)) {
		if (ret == 0)
			L9P_LOG(L9P_DEBUG, "%p: EOF", (void *)sc->ls_conn);
		else
			L9P_LOG(L9P_ERROR,
			    "short read: %zd bytes of %zd expected",
			    ret, sizeof(uint32_t));
		return (-1);
	}

	msize = le32toh(*(uint32_t *)buffer);
	toread = msize - sizeof(uint32_t);
	buffer = l9p_realloc(buffer, msize);

	ret = xread(fd, (char *)buffer + sizeof(uint32_t), toread);
	if (ret < 0) {
		L9P_LOG(L9P_ERROR, "read(): %s", strerror(errno));
		return (-1);
	}

	if (ret != (ssize_t)toread) {
		L9P_LOG(L9P_ERROR, "short read: %zd bytes of %zd expected",
		    ret, toread);
		return (-1);
	}

	*size = msize;
	*buf = buffer;
	L9P_LOG(L9P_INFO, "%p: read complete message, buf=%p size=%d",
	    (void *)sc->ls_conn, buffer, msize);

	return (0);
}

static int
l9p_socket_get_response_buffer(struct l9p_request *req, struct iovec *iov,
    size_t *niovp, void *arg __unused)
{
	size_t size = req->lr_conn->lc_msize;
	void *buf;

	buf = l9p_malloc(size);
	iov[0].iov_base = buf;
	iov[0].iov_len = size;

	*niovp = 1;
	return (0);
}

static int
l9p_socket_send_response(struct l9p_request *req __unused,
    const struct iovec *iov, const size_t niov __unused, const size_t iolen,
    void *arg)
{
	struct l9p_socket_softc *sc = (struct l9p_socket_softc *)arg;

	assert(sc->ls_fd >= 0);

	L9P_LOG(L9P_DEBUG, "%p: sending reply, buf=%p, size=%d", arg,
	    iov[0].iov_base, iolen);

	if (xwrite(sc->ls_fd, iov[0].iov_base, iolen) != (int)iolen) {
		L9P_LOG(L9P_ERROR, "short write: %s", strerror(errno));
		return (-1);
	}

	free(iov[0].iov_base);
	return (0);
}

static void
l9p_socket_drop_response(struct l9p_request *req __unused,
    const struct iovec *iov, size_t niov __unused, void *arg __unused)
{

	L9P_LOG(L9P_DEBUG, "%p: drop buf=%p", arg, iov[0].iov_base);
	free(iov[0].iov_base);
}

static ssize_t
xread(int fd, void *buf, size_t count)
{
	size_t done = 0;
	ssize_t ret;

	while (done < count) {
		ret = read(fd, (char *)buf + done, count - done);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			return (-1);
		}

		if (ret == 0)
			return ((ssize_t)done);

		done += (size_t)ret;
	}

	return ((ssize_t)done);
}

static ssize_t
xwrite(int fd, void *buf, size_t count)
{
	size_t done = 0;
	ssize_t ret;

	while (done < count) {
		ret = write(fd, (char *)buf + done, count - done);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			return (-1);
		}

		if (ret == 0)
			return ((ssize_t)done);

		done += (size_t)ret;
	}

	return ((ssize_t)done);
}
