/*
 * l2ping.c
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * $Id: l2ping.c,v 1.9 2002/09/04 21:28:05 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <bitstring.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>

static void	usage	(void);
static void	tv_sub	(struct timeval *, struct timeval const *);
static double	tv2msec	(struct timeval const *);

#undef	min
#define	min(x, y)	(((x) > (y))? (y) : (x))

static char const		pattern[] = "1234567890-";
#define PATTERN_SIZE		(sizeof(pattern) - 1)

/* 
 * Main 
 */

int
main(int argc, char *argv[])
{
	struct ng_btsocket_l2cap_raw_ping	r;
	int					n, s, count, wait, flood, fail;
	struct timeval				a, b;

	/* Set defaults */
	memset(&r, 0, sizeof(r));
	r.echo_data = calloc(NG_L2CAP_MAX_ECHO_SIZE, sizeof(u_int8_t));
	if (r.echo_data == NULL) {
		fprintf(stderr, "Failed to allocate echo data buffer");
		exit(1);
	}

	r.echo_size = 64; /* bytes */
	count = -1;       /* unlimited */
	wait = 1;         /* sec */
	flood = 0;

	/* Parse command line arguments */
	while ((n = getopt(argc, argv, "a:c:fi:n:s:S:")) != -1) {
		switch (n) {
		case 'a':
		case 'S': {
			int	a0, a1, a2, a3, a4, a5;

			if (sscanf(optarg, "%x:%x:%x:%x:%x:%x",
					&a5, &a4, &a3, &a2, &a1, &a0) != 6)
				usage();

			if (n == 'a') {
				/* destination bdaddr */
				r.echo_dst.b[0] = (a0 & 0xff);
				r.echo_dst.b[1] = (a1 & 0xff);
				r.echo_dst.b[2] = (a2 & 0xff);
				r.echo_dst.b[3] = (a3 & 0xff);
				r.echo_dst.b[4] = (a4 & 0xff);
				r.echo_dst.b[5] = (a5 & 0xff);
			} else {
				/* source bdaddr */
				r.echo_src.b[0] = (a0 & 0xff);
				r.echo_src.b[1] = (a1 & 0xff);
				r.echo_src.b[2] = (a2 & 0xff);
				r.echo_src.b[3] = (a3 & 0xff);
				r.echo_src.b[4] = (a4 & 0xff);
				r.echo_src.b[5] = (a5 & 0xff);
			}
			} break;

		case 'c':
			count = atoi(optarg);
			if (count <= 0)
				usage();
			break;

		case 'f':
			flood = 1;
			break;

		case 'i':
			wait = atoi(optarg);
			if (wait <= 0)
				usage();
			break;

		case 's':
			r.echo_size = atoi(optarg);
			if ((int) r.echo_size < sizeof(int))
				usage();

			if (r.echo_size > NG_L2CAP_MAX_ECHO_SIZE)
				r.echo_size = NG_L2CAP_MAX_ECHO_SIZE;
			break;

		default:
			usage();
			break;
		}
	}

	if (memcmp(&r.echo_dst, NG_HCI_BDADDR_ANY, sizeof(bdaddr_t)) == 0)
		usage();

	s = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_L2CAP);
	if (s < 0)
		err(2, "Could not create socket");

	if (memcmp(&r.echo_src, NG_HCI_BDADDR_ANY, sizeof(bdaddr_t)) != 0) {
		struct sockaddr_l2cap	sa;

		memset(&sa, 0, sizeof(sa));
		sa.l2cap_len = sizeof(sa);
		sa.l2cap_family = AF_BLUETOOTH;
		memcpy(&sa.l2cap_bdaddr, &r.echo_src, sizeof(bdaddr_t));

		if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
			err(3,
"Could not bind socket, src bdaddr=%x:%x:%x:%x:%x:%x",
				sa.l2cap_bdaddr.b[5], sa.l2cap_bdaddr.b[4],
				sa.l2cap_bdaddr.b[3], sa.l2cap_bdaddr.b[2],
				sa.l2cap_bdaddr.b[1], sa.l2cap_bdaddr.b[0]);

		if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
			err(4,
"Could not connect socket, src bdaddr=%x:%x:%x:%x:%x:%x",
				sa.l2cap_bdaddr.b[5], sa.l2cap_bdaddr.b[4],
				sa.l2cap_bdaddr.b[3], sa.l2cap_bdaddr.b[2],
				sa.l2cap_bdaddr.b[1], sa.l2cap_bdaddr.b[0]);
	}

	/* Fill pattern */
	for (n = 0; n < r.echo_size; ) {
		int	avail = min(r.echo_size - n, PATTERN_SIZE);

		memcpy(r.echo_data + n, pattern, avail);
		n += avail;
	}

	/* Start ping'ing */
	for (n = 0; count == -1 || count > 0; n ++) {
		if (gettimeofday(&a, NULL) < 0)
			err(5, "Could not gettimeofday(a)");

		fail = 0;
		*((int *)(r.echo_data)) = htonl(n);
		if (ioctl(s, SIOC_L2CAP_L2CA_PING, &r, sizeof(r)) < 0) {
			r.result = errno;
			fail = 1;
/*
			warn("Could not ping, dst bdaddr=%x:%x:%x:%x:%x:%x",
				r.echo_dst.b[5], r.echo_dst.b[4],
				r.echo_dst.b[3], r.echo_dst.b[2],
				r.echo_dst.b[1], r.echo_dst.b[0]);
*/
		}

		if (gettimeofday(&b, NULL) < 0)
			err(7, "Could not gettimeofday(b)");

		tv_sub(&b, &a);

		fprintf(stdout,
"%d bytes from %x:%x:%x:%x:%x:%x seq_no=%d time=%.3f ms result=%#x %s\n",
			r.echo_size, 
			r.echo_dst.b[5], r.echo_dst.b[4],
			r.echo_dst.b[3], r.echo_dst.b[2],
			r.echo_dst.b[1], r.echo_dst.b[0],
			ntohl(*((int *)(r.echo_data))),
			tv2msec(&b), r.result,
			((fail == 0)? "" : strerror(errno)));

		if (!flood) {
			/* Wait */
			a.tv_sec = wait;
			a.tv_usec = 0;
			select(0, NULL, NULL, NULL, &a);
		}

		if (count != -1)
			count --;
	}

	free(r.echo_data);
	close(s);

	return (0);
} /* main */

/* 
 * a -= b, for timevals 
 */

static void
tv_sub(struct timeval *a, struct timeval const *b)
{
	if (a->tv_usec < b->tv_usec) {
		a->tv_usec += 1000000;
		a->tv_sec -= 1;
	}

	a->tv_usec -= b->tv_usec;
	a->tv_sec -= b->tv_sec;
} /* tv_sub */

/* 
 * convert tv to msec 
 */

static double
tv2msec(struct timeval const *tvp)
{
	return(((double)tvp->tv_usec)/1000.0 + ((double)tvp->tv_sec)*1000.0);
} /* tv2msec */

/* 
 * Usage 
 */

static void
usage(void)
{
	fprintf(stderr, "Usage: l2ping -a bd_addr " \
		"[-S bd_addr -c count -i wait -s size]\n");
	fprintf(stderr, "Where:\n");
	fprintf(stderr, "\t-S bd_addr         - Source BD_ADDR\n");
	fprintf(stderr, "\t-a bd_addr         - Remote BD_ADDR to ping\n");
	fprintf(stderr, "\t-c count           - Number of packets to send\n");
	fprintf(stderr, "\t-f                 - No delay (soft of flood)\n");
	fprintf(stderr, "\t-i wait            - Delay between packets (sec)\n");
	fprintf(stderr, "\t-s size            - Packet size (bytes), " \
		"between %d and %d\n", sizeof(int), NG_L2CAP_MAX_ECHO_SIZE);
	
	exit(255);
} /* usage */

