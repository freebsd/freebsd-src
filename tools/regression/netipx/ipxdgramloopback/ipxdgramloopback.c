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
 * Simple netipx regression test that attempts to build an IPX datagram
 * socket pair and send a packet from one to the other.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netipx/ipx.h>

#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define	IPX_ENDPOINT	"0xbebe.1.0x8a13"
#define	PACKETLEN	128

int
main(int argc, char *argv[])
{
	struct sockaddr_ipx sipx_recv, sipx_send;
	u_char packet[PACKETLEN];
	int i, sock_recv, sock_send;
	ssize_t len;

	/*
	 * Socket to receive with.
	 */
	sock_recv = socket(PF_IPX, SOCK_DGRAM, 0);
	if (sock_recv < 0)
		err(-1, "sock_recv = socket(PF_IPX, SOCK_DGRAM, 0)");

	bzero(&sipx_recv, sizeof(sipx_recv));
	sipx_recv.sipx_len = sizeof(sipx_recv);
	sipx_recv.sipx_family = AF_IPX;
	sipx_recv.sipx_addr = ipx_addr(IPX_ENDPOINT);

	if (bind(sock_recv, (struct sockaddr *)&sipx_recv, sizeof(sipx_recv))
	    < 0)
		err(-1, "bind(sock_recv)");

	/*
	 * Set non-blocking to try to avoid blocking indefinitely if the
	 * packet doesn't end up in the right place.
	 */
	if (fcntl(sock_recv, F_SETFL, O_NONBLOCK) < 0)
		err(-1, "fcntl(O_NONBLOCK, sock_recv)");

	/*
	 * Socket to send with.
	 */
	sock_send = socket(PF_IPX, SOCK_DGRAM, 0);
	if (sock_send < 0)
		err(-1, "sock_send = socket(PF_IPX, SOCK_DGRAM, 0)");

	bzero(&sipx_send, sizeof(sipx_send));
	sipx_send.sipx_len = sizeof(sipx_send);
	sipx_send.sipx_family = AF_IPX;
	sipx_send.sipx_addr = ipx_addr(IPX_ENDPOINT);

	for (i = 0; i < PACKETLEN; i++)
		packet[i] = (i & 0xff);

	len = sendto(sock_send, packet, sizeof(packet), 0,
	    (struct sockaddr *)&sipx_send, sizeof(sipx_send));
	if (len < 0)
		err(-1, "sendto()");
	if (len != sizeof(packet))
		errx(-1, "sendto(): short send (%zu length, %zd sent)",
		    sizeof(packet), len);

	sleep(1);	/* Arbitrary non-zero amount. */

	bzero(packet, sizeof(packet));
	len = recv(sock_recv, packet, sizeof(packet), 0);
	if (len < 0)
		err(-1, "recv()");
	if (len != sizeof(packet))
		errx(-1, "recv(): short receive (%zu length, %zd received)",
		    sizeof(packet), len);

	for (i = 0; i < PACKETLEN; i++) {
		if (packet[i] != (i & 0xff))
			errx(-1, "recv(): byte %d wrong (%d instead of %d)",
			    i, packet[i], i & 0xff);
	}

	return (0);
}
