/*	$OpenBSD: socks.c,v 1.9 2004/10/17 03:13:55 djm Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCKS_PORT	"1080"
#define HTTP_PROXY_PORT	"3128"
#define HTTP_MAXHDRS	64
#define SOCKS_V5	5
#define SOCKS_V4	4
#define SOCKS_NOAUTH	0
#define SOCKS_NOMETHOD	0xff
#define SOCKS_CONNECT	1
#define SOCKS_IPV4	1


int	remote_connect(char *, char *, struct addrinfo);
int	socks_connect(char *host, char *port, struct addrinfo hints,
	    char *proxyhost, char *proxyport, struct addrinfo proxyhints,
	    int socksv);

static in_addr_t
decode_addr(const char *s)
{
	struct hostent *hp = gethostbyname (s);
	struct in_addr retval;

	if (hp)
		return *(in_addr_t *)hp->h_addr_list[0];
	if (inet_aton (s, &retval))
		return retval.s_addr;
	errx (1, "cannot decode address \"%s\"", s);
}

static in_port_t
decode_port(const char *s)
{
	struct servent *sp;
	in_port_t port;
	char *p;

	port = strtol (s, &p, 10);
	if (s == p) {
		sp = getservbyname (s, "tcp");
		if (sp)
			return sp->s_port;
	}
	if (*s != '\0' && *p == '\0')
		return htons (port);
	errx (1, "cannot decode port \"%s\"", s);
}

static int
proxy_read_line(int fd, char *buf, int bufsz)
{
	int r, off;

	for(off = 0;;) {
		if (off >= bufsz)
			errx(1, "proxy read too long");
		if ((r = read(fd, buf + off, 1)) <= 0) {
			if (r == -1 && errno == EINTR)
				continue;
			err(1, "proxy read");
		}
		/* Skip CR */
		if (buf[off] == '\r')
			continue;
		if (buf[off] == '\n') {
			buf[off] = '\0';
			break;
		}
		off++;
	}
	return (off);
}

int
socks_connect(char *host, char *port, struct addrinfo hints,
    char *proxyhost, char *proxyport, struct addrinfo proxyhints,
    int socksv)
{
	int proxyfd, r;
	unsigned char buf[1024];
	ssize_t cnt;
	in_addr_t serveraddr;
	in_port_t serverport;

	if (proxyport == NULL)
		proxyport = (socksv == -1) ? HTTP_PROXY_PORT : SOCKS_PORT;

	proxyfd = remote_connect(proxyhost, proxyport, proxyhints);

	if (proxyfd < 0)
		return -1;

	serveraddr = decode_addr (host);
	serverport = decode_port (port);

	if (socksv == 5) {
		/* Version 5, one method: no authentication */
		buf[0] = SOCKS_V5;
		buf[1] = 1;
		buf[2] = SOCKS_NOAUTH;
		cnt = write (proxyfd, buf, 3);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 3)
			errx (1, "short write, %d (expected 3)", cnt);

		read (proxyfd, buf, 2);
		if (buf[1] == SOCKS_NOMETHOD)
			errx (1, "authentication method negotiation failed");

		/* Version 5, connect: IPv4 address */
		buf[0] = SOCKS_V5;
		buf[1] = SOCKS_CONNECT;
		buf[2] = 0;
		buf[3] = SOCKS_IPV4;
		memcpy (buf + 4, &serveraddr, sizeof serveraddr);
		memcpy (buf + 8, &serverport, sizeof serverport);

		/* XXX Handle short writes better */
		cnt = write (proxyfd, buf, 10);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 10)
			errx (1, "short write, %d (expected 10)", cnt);

		/* XXX Handle short reads better */
		cnt = read (proxyfd, buf, sizeof buf);
		if (cnt == -1)
			err (1, "read failed");
		if (cnt != 10)
			errx (1, "unexpected reply size %d (expected 10)", cnt);
		if (buf[1] != 0)
			errx (1, "connection failed, SOCKS error %d", buf[1]);
	} else if (socksv == 4) {
		/* Version 4 */
		buf[0] = SOCKS_V4;
		buf[1] = SOCKS_CONNECT;	/* connect */
		memcpy (buf + 2, &serverport, sizeof serverport);
		memcpy (buf + 4, &serveraddr, sizeof serveraddr);
		buf[8] = 0;	/* empty username */

		cnt = write (proxyfd, buf, 9);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 9)
			errx (1, "short write, %d (expected 9)", cnt);

		/* XXX Handle short reads better */
		cnt = read (proxyfd, buf, 8);
		if (cnt == -1)
			err (1, "read failed");
		if (cnt != 8)
			errx (1, "unexpected reply size %d (expected 8)", cnt);
		if (buf[1] != 90)
			errx (1, "connection failed, SOCKS error %d", buf[1]);
	} else if (socksv == -1) {
		/* HTTP proxy CONNECT */

		/* Disallow bad chars in hostname */
		if (strcspn(host, "\r\n\t []:") != strlen(host))
			errx (1, "Invalid hostname");

		/* Try to be sane about numeric IPv6 addresses */
		if (strchr(host, ':') != NULL) {
			r = snprintf(buf, sizeof(buf),
			    "CONNECT [%s]:%d HTTP/1.0\r\n\r\n",
			    host, ntohs(serverport));
		} else {
			r = snprintf(buf, sizeof(buf),
			    "CONNECT %s:%d HTTP/1.0\r\n\r\n",
			    host, ntohs(serverport));
		}
		if (r == -1 || r >= sizeof(buf))
			errx (1, "hostname too long");
		r = strlen(buf);

		/* XXX atomicio */
		cnt = write (proxyfd, buf, r);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != r)
			errx (1, "short write, %d (expected %d)", cnt, r);

		/* Read reply */
		for (r = 0; r < HTTP_MAXHDRS; r++) {
			proxy_read_line(proxyfd, buf, sizeof(buf));
			if (r == 0 && strncmp(buf, "HTTP/1.0 200 ", 12) != 0)
				errx (1, "Proxy error: \"%s\"", buf);
			/* Discard headers until we hit an empty line */
			if (*buf == '\0')
				break;
		}
	} else
		errx (1, "Unknown proxy protocol %d", socksv);

	return proxyfd;
}
