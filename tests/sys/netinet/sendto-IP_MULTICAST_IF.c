/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	struct in_addr in;
	int s, rv;

	if (argc < 2)
		errx(1, "Usage: %s IPv4-address", argv[0]);

	if (inet_pton(AF_INET, argv[1], &in) != 1)
		err(1, "inet_pton(%s) failed", argv[1]);

	assert((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	assert(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	assert(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &in, sizeof(in))
	  == 0);
	/* RFC 6676 */
	assert(inet_pton(AF_INET, "233.252.0.1", &sin.sin_addr) == 1);
	sin.sin_port = htons(6676);
	rv = sendto(s, &sin, sizeof(sin), 0,
	    (struct sockaddr *)&sin, sizeof(sin));
	if (rv != sizeof(sin))
		err(1, "sendto failed");

	return (0);
}
