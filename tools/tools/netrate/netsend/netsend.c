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
#include <stdint.h>
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

static __inline int
timespec_ge(struct timespec *a, struct timespec *b)
{

	if (a->tv_sec > b->tv_sec)
		return (1);
	if (a->tv_sec < b->tv_sec)
		return (0);
	if (a->tv_nsec >= b->tv_nsec)
		return (1);
	return (0);
}

/*
 * Busy wait spinning until we reach (or slightly pass) the desired time.
 * Optionally return the current time as retrieved on the last time check
 * to the caller.  Optionally also increment a counter provided by the
 * caller each time we loop.
 */
static int
wait_time(struct timespec ts, struct timespec *wakeup_ts, long long *waited)
{
	struct timespec curtime;

	curtime.tv_sec = 0;
	curtime.tv_nsec = 0;

	if (clock_gettime(CLOCK_REALTIME, &curtime) == -1) {
		perror("clock_gettime");
		return (-1);
	}
#if 0
	if (timespec_ge(&curtime, &ts))
		printf("warning: wait_time missed deadline without spinning\n");
#endif
	while (timespec_ge(&ts, &curtime)) {
		if (waited != NULL)
			(*waited)++;
		if (clock_gettime(CLOCK_REALTIME, &curtime) == -1) {
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
static int
timing_loop(int s, struct timespec interval, long duration, u_char *packet,
    u_int packet_len)
{
	struct timespec nexttime, starttime, tmptime;
	long long waited;
	u_int32_t counter;
	long finishtime;
	long send_errors, send_calls;
	/* do not call gettimeofday more than every 20us */
	long minres_ns = 20000;
	int ic, gettimeofday_cycles;

	if (clock_getres(CLOCK_REALTIME, &tmptime) == -1) {
		perror("clock_getres");
		return (-1);
	}

	if (timespec_ge(&tmptime, &interval))
		fprintf(stderr,
		    "warning: interval (%jd.%09ld) less than resolution (%jd.%09ld)\n",
		    (intmax_t)interval.tv_sec, interval.tv_nsec,
		    (intmax_t)tmptime.tv_sec, tmptime.tv_nsec);
	if (tmptime.tv_nsec < minres_ns) {
		gettimeofday_cycles = minres_ns/(tmptime.tv_nsec + 1);
		fprintf(stderr,
		    "calling time every %d cycles\n", gettimeofday_cycles);
	} else
		gettimeofday_cycles = 0;

	if (clock_gettime(CLOCK_REALTIME, &starttime) == -1) {
		perror("clock_gettime");
		return (-1);
	}
	tmptime.tv_sec = 2;
	tmptime.tv_nsec = 0;
	timespec_add(&starttime, &tmptime);
	starttime.tv_nsec = 0;
	if (wait_time(starttime, NULL, NULL) == -1)
		return (-1);
	nexttime = starttime;
	finishtime = starttime.tv_sec + duration;

	send_errors = send_calls = 0;
	counter = 0;
	waited = 0;
	ic = gettimeofday_cycles;
	while (1) {
		timespec_add(&nexttime, &interval);
		if (--ic <= 0) {
			ic = gettimeofday_cycles;
			if (wait_time(nexttime, &tmptime, &waited) == -1)
				return (-1);
		}
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
			send_errors++;
		send_calls++;
		if (duration != 0 && tmptime.tv_sec >= finishtime)
			goto done;
	}

done:
	if (clock_gettime(CLOCK_REALTIME, &tmptime) == -1) {
		perror("clock_gettime");
		return (-1);
	}

	printf("\n");
	printf("start:             %jd.%09ld\n", (intmax_t)starttime.tv_sec,
	    starttime.tv_nsec);
	printf("finish:            %jd.%09ld\n", (intmax_t)tmptime.tv_sec,
	    tmptime.tv_nsec);
	printf("send calls:        %ld\n", send_calls);
	printf("send errors:       %ld\n", send_errors);
	printf("approx send rate:  %ld\n", (send_calls - send_errors) /
	    duration);
	printf("approx error rate: %ld\n", (send_errors / send_calls));
	printf("waited:            %lld\n", waited);
	printf("approx waits/sec:  %lld\n", (long long)(waited / duration));
	printf("approx wait rate:  %lld\n", (long long)(waited / send_calls));

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
	 * any particular strategy.  '0' is a special value meaning "blast",
	 * and avoids the cost of a timing loop.
	 * XXX 0 is not actually implemented.
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

	if (rate == 0) {
		interval.tv_sec = 0;
		interval.tv_nsec = 0;
	} else if (rate == 1) {
		interval.tv_sec = 1;
		interval.tv_nsec = 0;
	} else {
		interval.tv_sec = 0;
		interval.tv_nsec = ((1 * 1000000000) / rate);
	}
	printf("Sending packet of payload size %ld every %jd.%09lds for %ld "
	    "seconds\n", payloadsize, (intmax_t)interval.tv_sec,
	    interval.tv_nsec, duration);

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
