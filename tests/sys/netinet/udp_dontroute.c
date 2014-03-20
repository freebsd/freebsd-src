/*
 *  Copyright (c) 2014 Spectra Logic Corporation
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions, and the following disclaimer,
 *     without modification.
 *  2. Redistributions in binary form must reproduce at minimum a disclaimer
 *     substantially similar to the "NO WARRANTY" disclaimer below
 *     ("Disclaimer") and any redistribution must be conditioned upon
 *     including a substantially similar Disclaimer requirement for further
 *     binary redistribution.
 * 
 *  NO WARRANTY
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGES.
 * 
 *  Authors: Alan Somers         (Spectra Logic Corporation)
 *
 * $FreeBSD$
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * Sends a single UDP packet to the provided address, with SO_DONTROUTE set
 * I couldn't find a way to do this with builtin utilities like nc(1)
 */
int main(int argc, char **argv)
{
	struct sockaddr_in dst;
	int s;
	int opt;
	int ret;
	const char* buf = "Hello, World!";

	if (argc != 2) {
		fprintf(stderr, "Usage: %s ip_address\n", argv[0]);
		exit(2);
	}
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(errno, "socket");
	opt = 1;

	ret = setsockopt(s, SOL_SOCKET, SO_DONTROUTE, &opt, sizeof(opt));
	if (ret == -1)
		err(errno, "setsockopt(SO_DONTROUTE)");

	dst.sin_len = sizeof(dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(46120);
	dst.sin_addr.s_addr = inet_addr(argv[1]);
	if (dst.sin_addr.s_addr == htonl(INADDR_NONE)) {
		fprintf(stderr, "Invalid address: %s\n", argv[1]);
		exit(2);
	}
	ret = sendto(s, buf, strlen(buf), 0, (struct sockaddr*)&dst,
	    dst.sin_len);
	if (ret == -1)
		err(errno, "sendto");
	
	return (0);
}
