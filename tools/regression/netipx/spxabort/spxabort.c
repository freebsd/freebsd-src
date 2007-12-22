/*-
 * Copyright (c) 2006 Robert N. M. Watson
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Exercise the pru_abort() code for SPX by opening an SPX connection to a
 * listen socket, then closing the listen socket before accepting.
 *
 * We would also like to be able to test the other two abort cases, in which
 * incomplete connections are aborted due to overflow, and due to close of
 * the listen socket, but that requires a packet level test rather than using
 * the socket API.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netipx/ipx.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	IPX_ENDPOINT	"0xbebe.1.0x8a13"

int
main(int argc, char *argv[])
{
	struct sockaddr_ipx sipx;
	int sock_listen, sock;

	sock_listen = socket(PF_IPX, SOCK_STREAM, 0);
	if (sock_listen < 0)
		err(-1, "sock_listen = socket(PF_IPX, SOCK_STREAM, 0)");

	bzero(&sipx, sizeof(sipx));
	sipx.sipx_len = sizeof(sipx);
	sipx.sipx_family = AF_IPX;
	sipx.sipx_addr = ipx_addr(IPX_ENDPOINT);

	if (bind(sock_listen, (struct sockaddr *)&sipx, sizeof(sipx)) < 0)
		err(-1, "bind(sock_listen)");

	if (listen(sock_listen, -1) < 0)
		err(-1, "listen(sock_listen)");

	sock = socket(PF_IPX, SOCK_STREAM, 0);
	if (sock < 0)
		err(-1, "sock = socket(PF_IPX, SOCK_STREAM, 0)");

	bzero(&sipx, sizeof(sipx));
	sipx.sipx_len = sizeof(sipx);
	sipx.sipx_family = AF_IPX;
	sipx.sipx_addr = ipx_addr(IPX_ENDPOINT);

	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
		err(-1, "fcntl(sock, F_SETFL, O_NONBLOCKING)");

	if (connect(sock, (struct sockaddr *)&sipx, sizeof(sipx)) < 0) {
		if (errno != EINPROGRESS)
			err(-1, "sock = socket(PF_IPX, SOCK_STREAM, 0)");
	}

	sleep(1);	/* Arbitrary. */

	close(sock_listen);

	return (0);
};
