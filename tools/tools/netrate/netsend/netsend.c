/*-
 * Copyright (c) 2004 Robert N. M. Watson
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{

	fprintf(stderr,
	    "netsend [ip] [port] [payloadsize] [rate] [duration]\n");
	exit(-1);
}

#define	MAX_RATE	100000000

static __inline void
timespec_add(struct timespec *tsa, struct timespec *tsb)
{

	tsa->tv_sec += tsb->tv_sec;
	tsa->tv_nsec += tsb->tv_nsec;
	if (tsa->tv_nsec >= 1000000000) {
		tsa->tv_sec++;
		tsa->tv_nsec -= 1000000000;
	}
}

/*
 * Busy wait spinning until we reach (or slightly pass) the desired time.
 * Optionally return the current time as retrieved on the last time check
 * to the caller.
 */
int
wait_time(struct timespec ts, struct timespec *wakeup_ts)
{
	struct timespec curtime;

	curtime.tv_sec = 0;
	curtime.tv_nsec = 0;

	while (curtime.tv_sec < ts.tv_sec || curtime.tv_nsec < ts.tv_nsec) {
		if (clock_gettime(CLOCK_REALTIME, &curtime) < -1) {
			perror("clock_gettime");
			return (-1);
		}
	}
	if (wakeup_ts != NULL)
		*wakeup_ts = curtime;
	return (0);
}

/*
 * Calculate a second-aligned starting time for the packet stream.  Busy
 * wait between our calculated interval and dropping the provided packet
 * into the socket.  If we hit our duration limit, bail.
 */
int
timing_loop(int s, struct timespec interval, long duration, u_char *packet,
    u_int packet_len)
{
	struct timespec starttime, tmptime;
	u_int32_t counter;
	long finishtime;

	if (clock_gettime(CLOCK_REALTIME, &starttime) < -1) {
		perror("clock_gettime");
		return (-1);
	}
	tmptime.tv_sec = 2;
	tmptime.tv_nsec = 0;
	timespec_add(&starttime, &tmptime);
	starttime.tv_nsec = 0;
	if (wait_time(starttime, NULL) == -1)
		return (-1);
	finishtime = starttime.tv_sec + duration;

	counter = 0;
	while (1) {
		timespec_add(&starttime, &interval);
		if (wait_time(starttime, &tmptime) == -1)
			return (-1);
		/*
		 * We maintain and, if there's room, send a counter.  Note
		 * that even if the error is purely local, we still increment
		 * the counter, so missing sequence numbers on the receive
		 * side should not be assumed to be packets lost in transit.
		 * For example, if the UDP socket gets back an ICMP from a
		 * previous send, the error will turn up the current send
		 * operation, causing the current sequence number also to be
		 * skipped.
		 *
		 * XXXRW: Note alignment assumption.
		 */
		if (packet_len >= 4) {
			*((u_int32_t *)packet) = htonl(counter);
			counter++;
		}
		if (send(s, packet, packet_len, 0) < 0)
			perror("send");
		if (duration != 0 && tmptime.tv_sec >= finishtime)
			return (0);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	long rate, payloadsize, port, duration;
	struct timespec interval;
	struct sockaddr_in sin;
	char *dummy, *packet;
	int s;

	if (argc != 6)
		usage();

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	if (inet_aton(argv[1], &sin.sin_addr) == 0) {
		perror(argv[1]);
		return (-1);
	}

	port = strtoul(argv[2], &dummy, 10);
	if (port < 1 || port > 65535 || *dummy != '\0')
		usage();
	sin.sin_port = htons(port);

	payloadsize = strtoul(argv[3], &dummy, 10);
	if (payloadsize < 0 || *dummy != '\0')
		usage();
	if (payloadsize > 32768) {
		fprintf(stderr, "payloadsize > 32768\n");
		return (-1);
	}

	/*
	 * Specify an arbitrary limit.  It's exactly that, not selected by
	 * any particular strategy.
	 */
	rate = strtoul(argv[4], &dummy, 10);
	if (rate < 1 || *dummy != '\0')
		usage();
	if (rate > MAX_RATE) {
		fprintf(stderr, "rate > %d\n", MAX_RATE);
		return (-1);
	}

	duration = strtoul(argv[5], &dummy, 10);
	if (duration < 0 || *dummy != '\0')
		usage();

	packet = malloc(payloadsize);
	if (packet == NULL) {
		perror("malloc");
		return (-1);
	}
	bzero(packet, payloadsize);

	if (rate == 1) {
		interval.tv_sec = 1;
		interval.tv_nsec = 0;
	} else {
		interval.tv_sec = 0;
		interval.tv_nsec = ((1 * 1000000000) / rate);
	}
	printf("Sending packet of payload size %ld every %d.%09lu for %ld "
	    "seconds\n", payloadsize, interval.tv_sec, interval.tv_nsec,
	    duration);

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("socket");
		return (-1);
	}

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("connect");
		return (-1);
	}

	return (timing_loop(s, interval, duration, packet, payloadsize));
}
