/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 NetApp, Inc.
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <machine/vmm.h>
#include <machine/vmm_snapshot.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <assert.h>
#include <capsicum_helpers.h>
#include <err.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>

#include "debug.h"
#include "mevent.h"
#include "uart_backend.h"

struct ttyfd {
	bool	opened;
	bool	is_socket;
	int	rfd;		/* fd for reading */
	int	wfd;		/* fd for writing, may be == rfd */
};

#define	FIFOSZ	16

struct fifo {
	uint8_t	buf[FIFOSZ];
	int	rindex;		/* index to read from */
	int	windex;		/* index to write to */
	int	num;		/* number of characters in the fifo */
	int	size;		/* size of the fifo */
};

struct uart_softc {
	struct ttyfd	tty;
	struct fifo	rxfifo;
	struct mevent	*mev;
	pthread_mutex_t mtx;
};

struct uart_socket_softc {
	struct uart_softc *softc;
	void (*drain)(int, enum ev_type, void *);
	void *arg;
};

static bool uart_stdio;		/* stdio in use for i/o */
static struct termios tio_stdio_orig;

static void uart_tcp_disconnect(struct uart_softc *);

static void
ttyclose(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_stdio_orig);
}

static void
ttyopen(struct ttyfd *tf)
{
	struct termios orig, new;

	tcgetattr(tf->rfd, &orig);
	new = orig;
	cfmakeraw(&new);
	new.c_cflag |= CLOCAL;
	tcsetattr(tf->rfd, TCSANOW, &new);
	if (uart_stdio) {
		tio_stdio_orig = orig;
		atexit(ttyclose);
	}
	raw_stdio = 1;
}

static int
ttyread(struct ttyfd *tf, uint8_t *ret)
{
	uint8_t rb;
	int len;

	len = read(tf->rfd, &rb, 1);
	if (ret && len == 1)
		*ret = rb;

	return (len);
}

static int
ttywrite(struct ttyfd *tf, unsigned char wb)
{
	return (write(tf->wfd, &wb, 1));
}

static bool
rxfifo_available(struct uart_softc *sc)
{
	return (sc->rxfifo.num < sc->rxfifo.size);
}

int
uart_rxfifo_getchar(struct uart_softc *sc)
{
	struct fifo *fifo;
	int c, error, wasfull;

	wasfull = 0;
	fifo = &sc->rxfifo;
	if (fifo->num > 0) {
		if (!rxfifo_available(sc))
			wasfull = 1;
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		if (wasfull) {
			if (sc->tty.opened) {
				error = mevent_enable(sc->mev);
				assert(error == 0);
			}
		}
		return (c);
	} else
		return (-1);
}

int
uart_rxfifo_numchars(struct uart_softc *sc)
{
	return (sc->rxfifo.num);
}

static int
rxfifo_putchar(struct uart_softc *sc, uint8_t ch)
{
	struct fifo *fifo;
	int error;

	fifo = &sc->rxfifo;

	if (fifo->num < fifo->size) {
		fifo->buf[fifo->windex] = ch;
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
		if (!rxfifo_available(sc)) {
			if (sc->tty.opened) {
				/*
				 * Disable mevent callback if the FIFO is full.
				 */
				error = mevent_disable(sc->mev);
				assert(error == 0);
			}
		}
		return (0);
	} else
		return (-1);
}

void
uart_rxfifo_drain(struct uart_softc *sc, bool loopback)
{
	uint8_t ch;
	int len;

	if (loopback) {
		if (ttyread(&sc->tty, &ch) == 0 && sc->tty.is_socket)
			uart_tcp_disconnect(sc);
	} else {
		while (rxfifo_available(sc)) {
			len = ttyread(&sc->tty, &ch);
			if (len <= 0) {
				/* read returning 0 means disconnected. */
				if (len == 0 && sc->tty.is_socket)
					uart_tcp_disconnect(sc);
				break;
			}

			rxfifo_putchar(sc, ch);
		}
	}
}

int
uart_rxfifo_putchar(struct uart_softc *sc, uint8_t ch, bool loopback)
{
	if (loopback) {
		return (rxfifo_putchar(sc, ch));
	} else if (sc->tty.opened) {
		/* write returning -1 means disconnected. */
		if (ttywrite(&sc->tty, ch) == -1 && sc->tty.is_socket)
			uart_tcp_disconnect(sc);
		return (0);
	} else {
		/* Drop on the floor. */
		return (0);
	}
}

void
uart_rxfifo_reset(struct uart_softc *sc, int size)
{
	char flushbuf[32];
	struct fifo *fifo;
	ssize_t nread;
	int error;

	fifo = &sc->rxfifo;
	bzero(fifo, sizeof(struct fifo));
	fifo->size = size;

	if (sc->tty.opened) {
		/*
		 * Flush any unread input from the tty buffer.
		 */
		while (1) {
			nread = read(sc->tty.rfd, flushbuf, sizeof(flushbuf));
			if (nread != sizeof(flushbuf))
				break;
		}

		/*
		 * Enable mevent to trigger when new characters are available
		 * on the tty fd.
		 */
		error = mevent_enable(sc->mev);
		assert(error == 0);
	}
}

int
uart_rxfifo_size(struct uart_softc *sc __unused)
{
	return (FIFOSZ);
}

#ifdef BHYVE_SNAPSHOT
int
uart_rxfifo_snapshot(struct uart_softc *sc, struct vm_snapshot_meta *meta)
{
	int ret;

	SNAPSHOT_VAR_OR_LEAVE(sc->rxfifo.rindex, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->rxfifo.windex, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->rxfifo.num, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->rxfifo.size, meta, ret, done);
	SNAPSHOT_BUF_OR_LEAVE(sc->rxfifo.buf, sizeof(sc->rxfifo.buf),
	    meta, ret, done);

done:
	return (ret);
}
#endif

/*
 * Listen on the TCP port, wait for a connection, then accept it.
 */
static void
uart_tcp_listener(int fd, enum ev_type type __unused, void *arg)
{
	static const char tcp_error_msg[] = "Socket already connected\n";
	struct uart_socket_softc *socket_softc = (struct uart_socket_softc *)
	    arg;
	struct uart_softc *sc = socket_softc->softc;
	int conn_fd;

	conn_fd = accept(fd, NULL, NULL);
	if (conn_fd == -1)
		goto clean;

	if (fcntl(conn_fd, F_SETFL, O_NONBLOCK) != 0)
		goto clean;

	pthread_mutex_lock(&sc->mtx);

	if (sc->tty.opened) {
		(void)send(conn_fd, tcp_error_msg, sizeof(tcp_error_msg), 0);
		pthread_mutex_unlock(&sc->mtx);
		goto clean;
	} else {
		sc->tty.rfd = sc->tty.wfd = conn_fd;
		sc->tty.opened = true;
		sc->mev = mevent_add(sc->tty.rfd, EVF_READ, socket_softc->drain,
		    socket_softc->arg);
	}

	pthread_mutex_unlock(&sc->mtx);
	return;

clean:
	if (conn_fd != -1)
		close(conn_fd);
}

/*
 * When a connection-oriented protocol disconnects, this handler is used to
 * clean it up.
 *
 * Note that this function is a helper, so the caller is responsible for
 * locking the softc.
 */
static void
uart_tcp_disconnect(struct uart_softc *sc)
{
	mevent_delete_close(sc->mev);
	sc->mev = NULL;
	sc->tty.opened = false;
	sc->tty.rfd = sc->tty.wfd = -1;
}

static int
uart_stdio_backend(struct uart_softc *sc)
{
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_ioctl_t cmds[] = { TIOCGETA, TIOCSETA, TIOCGWINSZ };
#endif

	if (uart_stdio)
		return (-1);

	sc->tty.rfd = STDIN_FILENO;
	sc->tty.wfd = STDOUT_FILENO;
	sc->tty.opened = true;

	if (fcntl(sc->tty.rfd, F_SETFL, O_NONBLOCK) != 0)
		return (-1);
	if (fcntl(sc->tty.wfd, F_SETFL, O_NONBLOCK) != 0)
		return (-1);

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_IOCTL, CAP_READ);
	if (caph_rights_limit(sc->tty.rfd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	if (caph_ioctls_limit(sc->tty.rfd, cmds, nitems(cmds)) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	uart_stdio = true;

	return (0);
}

static int
uart_tty_backend(struct uart_softc *sc, const char *path)
{
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_ioctl_t cmds[] = { TIOCGETA, TIOCSETA, TIOCGWINSZ };
#endif
	int fd;

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return (-1);

	if (!isatty(fd)) {
		close(fd);
		return (-1);
	}

	sc->tty.rfd = sc->tty.wfd = fd;
	sc->tty.opened = true;

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_IOCTL, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	if (caph_ioctls_limit(fd, cmds, nitems(cmds)) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	return (0);
}

/*
 * Listen on the address and add it to the kqueue.
 *
 * If a connection is established (e.g., the TCP handler is triggered),
 * replace the handler with the connected handler.
 */
static int
uart_tcp_backend(struct uart_softc *sc, const char *path,
    void (*drain)(int, enum ev_type, void *), void *arg)
{
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_ioctl_t cmds[] = { TIOCGETA, TIOCSETA, TIOCGWINSZ };
#endif
	int bind_fd = -1;
	char addr[256], port[6];
	int domain;
	struct addrinfo hints, *src_addr = NULL;
	struct uart_socket_softc *socket_softc = NULL;

	if (sscanf(path, "tcp=[%255[^]]]:%5s", addr, port) == 2) {
		domain = AF_INET6;
	} else if (sscanf(path, "tcp=%255[^:]:%5s", addr, port) == 2) {
		domain = AF_INET;
	} else {
		warnx("Invalid number of parameter");
		goto clean;
	}

	bind_fd = socket(domain, SOCK_STREAM, 0);
	if (bind_fd < 0)
		goto clean;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = domain;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;

	if (getaddrinfo(addr, port, &hints, &src_addr) != 0) {
		warnx("Invalid address %s:%s", addr, port);
		goto clean;
	}

	if (bind(bind_fd, src_addr->ai_addr, src_addr->ai_addrlen) == -1) {
		warn(
		    "bind(%s:%s)",
		    addr, port);
		goto clean;
	}

	freeaddrinfo(src_addr);
	src_addr = NULL;

	if (fcntl(bind_fd, F_SETFL, O_NONBLOCK) == -1)
		goto clean;

	if (listen(bind_fd, 1) == -1) {
		warnx("listen(%s:%s)", addr, port);
		goto clean;
	}

	/*
	 * Set the connection softc structure, which includes both the softc
	 * and the drain function provided by the frontend.
	 */
	if ((socket_softc = calloc(1, sizeof(struct uart_socket_softc))) ==
	    NULL)
		goto clean;

	sc->tty.is_socket = true;

	socket_softc->softc = sc;
	socket_softc->drain = drain;
	socket_softc->arg = arg;

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_ACCEPT, CAP_RECV, CAP_SEND,
	    CAP_FCNTL, CAP_IOCTL);
	if (caph_rights_limit(bind_fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	if (caph_ioctls_limit(bind_fd, cmds, nitems(cmds)) == -1)
		errx(EX_OSERR, "Unable to apply ioctls for sandbox");
	if (caph_fcntls_limit(bind_fd, CAP_FCNTL_SETFL) == -1)
		errx(EX_OSERR, "Unable to apply fcntls for sandbox");
#endif

	if ((sc->mev = mevent_add(bind_fd, EVF_READ, uart_tcp_listener,
	    socket_softc)) == NULL)
		goto clean;

	return (0);

clean:
	if (bind_fd != -1)
		close(bind_fd);
	if (socket_softc != NULL)
		free(socket_softc);
	if (src_addr)
		freeaddrinfo(src_addr);
	return (-1);
}

struct uart_softc *
uart_init(void)
{
	struct uart_softc *sc = calloc(1, sizeof(struct uart_softc));
	if (sc == NULL)
		return (NULL);

	pthread_mutex_init(&sc->mtx, NULL);

	return (sc);
}

int
uart_tty_open(struct uart_softc *sc, const char *path,
    void (*drain)(int, enum ev_type, void *), void *arg)
{
	int retval;

	if (strcmp("stdio", path) == 0)
		retval = uart_stdio_backend(sc);
	else if (strncmp("tcp", path, 3) == 0)
		retval = uart_tcp_backend(sc, path, drain, arg);
	else
		retval = uart_tty_backend(sc, path);

	/*
	 * A connection-oriented protocol should wait for a connection,
	 * so it may not listen to anything during initialization.
	 */
	if (retval == 0 && !sc->tty.is_socket) {
		ttyopen(&sc->tty);
		sc->mev = mevent_add(sc->tty.rfd, EVF_READ, drain, arg);
		assert(sc->mev != NULL);
	}

	return (retval);
}

void
uart_softc_lock(struct uart_softc *sc)
{
	pthread_mutex_lock(&sc->mtx);
}

void
uart_softc_unlock(struct uart_softc *sc)
{
	pthread_mutex_unlock(&sc->mtx);
}
