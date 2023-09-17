/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Christian Kramer
 * Copyright (c) 2020 Ian Lepore <ian@FreeBSD.org>
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
 * make LDFLAGS+=-lgpio gpioevents
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <aio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/endian.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>

#include <libgpio.h>

static bool be_verbose = false;
static int report_format = GPIO_EVENT_REPORT_DETAIL;
static struct timespec utc_offset;

static volatile sig_atomic_t sigio = 0;

static void
sigio_handler(int sig __unused){
	sigio = 1;
}

static void
usage()
{
	fprintf(stderr, "usage: %s [-f ctldev] [-m method] [-s] [-n] [-S] [-u]"
	    "[-t timeout] [-d delay-usec] pin intr-config pin-mode [pin intr-config pin-mode ...]\n\n",
	    getprogname());
	fprintf(stderr, "  -d  delay before each call to read/poll/select/etc\n");
	fprintf(stderr, "  -n  Non-blocking IO\n");
	fprintf(stderr, "  -s  Single-shot (else loop continuously)\n");
	fprintf(stderr, "  -S  Report summary data (else report each event)\n");
	fprintf(stderr, "  -u  Show timestamps as UTC (else monotonic time)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Possible options for method:\n\n");
	fprintf(stderr, "  r\tread (default)\n");
	fprintf(stderr, "  p\tpoll\n");
	fprintf(stderr, "  s\tselect\n");
	fprintf(stderr, "  k\tkqueue\n");
	fprintf(stderr, "  a\taio_read (needs sysctl vfs.aio.enable_unsafe=1)\n");
	fprintf(stderr, "  i\tsignal-driven I/O\n\n");
	fprintf(stderr, "Possible options for intr-config:\n\n");
	fprintf(stderr, "  no\t no interrupt\n");
	fprintf(stderr, "  er\t edge rising\n");
	fprintf(stderr, "  ef\t edge falling\n");
	fprintf(stderr, "  eb\t edge both\n\n");
	fprintf(stderr, "Possible options for pin-mode:\n\n");
	fprintf(stderr, "  ft\t floating\n");
	fprintf(stderr, "  pd\t pull-down\n");
	fprintf(stderr, "  pu\t pull-up\n");
}

static void
verbose(const char *fmt, ...)
{
	va_list args;

	if (!be_verbose)
		return;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

static const char*
poll_event_to_str(short event)
{
	switch (event) {
	case POLLIN:
		return "POLLIN";
	case POLLPRI:
		return "POLLPRI:";
	case POLLOUT:
		return "POLLOUT:";
	case POLLRDNORM:
		return "POLLRDNORM";
	case POLLRDBAND:
		return "POLLRDBAND";
	case POLLWRBAND:
		return "POLLWRBAND";
	case POLLINIGNEOF:
		return "POLLINIGNEOF";
	case POLLERR:
		return "POLLERR";
	case POLLHUP:
		return "POLLHUP";
	case POLLNVAL:
		return "POLLNVAL";
	default:
		return "unknown event";
	}
}

static void
print_poll_events(short event)
{
	short curr_event = 0;
	bool first = true;

	for (size_t i = 0; i <= sizeof(short) * CHAR_BIT - 1; i++) {
		curr_event = 1 << i;
		if ((event & curr_event) == 0)
			continue;
		if (!first) {
			printf(" | ");
		} else {
			first = false;
		}
		printf("%s", poll_event_to_str(curr_event));
	}
}

static void
calc_utc_offset()
{
	struct timespec monotime, utctime;

	clock_gettime(CLOCK_MONOTONIC, &monotime);
	clock_gettime(CLOCK_REALTIME, &utctime);
	timespecsub(&utctime, &monotime, &utc_offset);
}

static void
print_timestamp(const char *str, sbintime_t timestamp)
{
	struct timespec ts;
	char timebuf[32];

	ts = sbttots(timestamp);

	if (!timespecisset(&utc_offset)) {
		printf("%s %jd.%09ld ", str, (intmax_t)ts.tv_sec, ts.tv_nsec);
	} else {
                timespecadd(&utc_offset, &ts, &ts);
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S",
		    gmtime(&ts.tv_sec));
		printf("%s %s.%09ld ", str, timebuf, ts.tv_nsec);
	}
}

static void
print_event_detail(const struct gpio_event_detail *det)
{
	print_timestamp("time", det->gp_time);
	printf("pin %hu state %u\n", det->gp_pin, det->gp_pinstate);
}

static void
print_event_summary(const struct gpio_event_summary *sum)
{
	print_timestamp("first_time", sum->gp_first_time);
	print_timestamp("last_time", sum->gp_last_time);
	printf("pin %hu count %hu first state %u last state %u\n",
	    sum->gp_pin, sum->gp_count,
	    sum->gp_first_state, sum->gp_last_state);
}

static void
print_gpio_event(const void *buf)
{
	if (report_format == GPIO_EVENT_REPORT_DETAIL)
		print_event_detail((const struct gpio_event_detail *)buf);
	else
		print_event_summary((const struct gpio_event_summary *)buf);
}

static void
run_read(bool loop, int handle, const char *file, u_int delayus)
{
	const size_t numrecs = 64;
	union {
		const struct gpio_event_summary sum[numrecs];
		const struct gpio_event_detail  det[numrecs];
		uint8_t                         data[1];
	} buffer;
	ssize_t reccount, recsize, res;

	if (report_format == GPIO_EVENT_REPORT_DETAIL)
		recsize = sizeof(struct gpio_event_detail);
	else
		recsize = sizeof(struct gpio_event_summary);

	do {
		if (delayus != 0) {
			verbose("sleep %f seconds before read()\n",
			    delayus / 1000000.0);
			usleep(delayus);
		}
		verbose("read into %zd byte buffer\n", sizeof(buffer));
		res = read(handle, buffer.data, sizeof(buffer));
		if (res < 0)
			err(EXIT_FAILURE, "Cannot read from %s", file);
		
		if ((res % recsize) != 0) {
			fprintf(stderr, "%s: read() %zd bytes from %s; "
			    "expected a multiple of %zu\n",
			    getprogname(), res, file, recsize);
		} else {
			reccount = res / recsize;
			verbose("read returned %zd bytes; %zd events\n", res,
			    reccount);
			for (ssize_t i = 0; i < reccount; ++i) {
				if (report_format == GPIO_EVENT_REPORT_DETAIL)
					print_event_detail(&buffer.det[i]);
				else
					print_event_summary(&buffer.sum[i]);
			}
		}
	} while (loop);
}

static void
run_poll(bool loop, int handle, const char *file, int timeout, u_int delayus)
{
	struct pollfd fds;
	int res;

        fds.fd = handle;
        fds.events = POLLIN | POLLRDNORM;
        fds.revents = 0;

	do {
		if (delayus != 0) {
			verbose("sleep %f seconds before poll()\n",
			    delayus / 1000000.0);
			usleep(delayus);
		}
		res = poll(&fds, 1, timeout);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot poll() %s", file);
		} else if (res == 0) {
			printf("%s: poll() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: poll() returned %i (revents: ",
			    getprogname(), res);
			print_poll_events(fds.revents);
			printf(") on %s\n", file);
			if (fds.revents & (POLLHUP | POLLERR)) {
				err(EXIT_FAILURE, "Recieved POLLHUP or POLLERR "
				    "on %s", file);
			}
			run_read(false, handle, file, 0);
		}
	} while (loop);
}

static void
run_select(bool loop, int handle, const char *file, int timeout, u_int delayus)
{
	fd_set readfds;
	struct timeval tv;
	struct timeval *tv_ptr;
	int res;

	FD_ZERO(&readfds);
	FD_SET(handle, &readfds);
	if (timeout != INFTIM) {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tv_ptr = &tv;
	} else {
		tv_ptr = NULL;
	}

	do {
		if (delayus != 0) {
			verbose("sleep %f seconds before select()\n",
			    delayus / 1000000.0);
			usleep(delayus);
		}
		res = select(FD_SETSIZE, &readfds, NULL, NULL, tv_ptr);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot select() %s", file);
		} else if (res == 0) {
			printf("%s: select() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: select() returned %i on %s\n",
			    getprogname(), res, file);
			run_read(false, handle, file, 0);
		}
	} while (loop);
}

static void
run_kqueue(bool loop, int handle, const char *file, int timeout, u_int delayus)
{
	struct kevent event[1];
	struct kevent tevent[1];
	int kq = -1;
	int nev = -1;
	struct timespec tv;
	struct timespec *tv_ptr;

	if (timeout != INFTIM) {
		tv.tv_sec = timeout / 1000;
		tv.tv_nsec = (timeout % 1000) * 10000000;
		tv_ptr = &tv;
	} else {
		tv_ptr = NULL;
	}

	kq = kqueue();
	if (kq == -1)
		err(EXIT_FAILURE, "kqueue() %s", file);

	EV_SET(&event[0], handle, EVFILT_READ, EV_ADD, 0, 0, NULL);
	nev = kevent(kq, event, 1, NULL, 0, NULL);
	if (nev == -1)
		err(EXIT_FAILURE, "kevent() %s", file);

	do {
		if (delayus != 0) {
			verbose("sleep %f seconds before kevent()\n",
			    delayus / 1000000.0);
			usleep(delayus);
		}
		nev = kevent(kq, NULL, 0, tevent, 1, tv_ptr);
		if (nev == -1) {
			err(EXIT_FAILURE, "kevent() %s", file);
		} else if (nev == 0) {
			printf("%s: kevent() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: kevent() returned %i events (flags: %d) on "
			    "%s\n", getprogname(), nev, tevent[0].flags, file);
			if (tevent[0].flags & EV_EOF) {
				err(EXIT_FAILURE, "Recieved EV_EOF on %s",
				    file);
			}
			run_read(false, handle, file, 0);
		}
	} while (loop);
}

static void
run_aio_read(bool loop, int handle, const char *file, u_int delayus)
{
	uint8_t buffer[1024];
	size_t recsize;
	ssize_t res;
	struct aiocb iocb;

	/*
	 * Note that async IO to character devices is no longer allowed by
	 * default (since freebsd 11).  This code is still here (for now)
	 * because you can use sysctl vfs.aio.enable_unsafe=1 to bypass the
	 * prohibition and run this code.
	 */

	if (report_format == GPIO_EVENT_REPORT_DETAIL)
		recsize = sizeof(struct gpio_event_detail);
	else
		recsize = sizeof(struct gpio_event_summary);

	bzero(&iocb, sizeof(iocb));

	iocb.aio_fildes = handle;
	iocb.aio_nbytes = sizeof(buffer);
	iocb.aio_offset = 0;
	iocb.aio_buf = buffer;

	do {
		if (delayus != 0) {
			verbose("sleep %f seconds before aio_read()\n",
			    delayus / 1000000.0);
			usleep(delayus);
		}
		res = aio_read(&iocb);
		if (res < 0)
			err(EXIT_FAILURE, "Cannot aio_read from %s", file);
		do {
			res = aio_error(&iocb);
		} while (res == EINPROGRESS);
		if (res < 0)
			err(EXIT_FAILURE, "aio_error on %s", file);
		res = aio_return(&iocb);
		if (res < 0)
			err(EXIT_FAILURE, "aio_return on %s", file);
		if ((res % recsize) != 0) {
			fprintf(stderr, "%s: aio_read() %zd bytes from %s; "
			    "expected a multiple of %zu\n",
			    getprogname(), res, file, recsize);
		} else {
			for (ssize_t i = 0; i < res; i += recsize)
				print_gpio_event(&buffer[i]);
		}
	} while (loop);
}


static void
run_sigio(bool loop, int handle, const char *file)
{
	int res;
	struct sigaction sigact;
	int flags;
	int pid;

	bzero(&sigact, sizeof(sigact));
	sigact.sa_handler = sigio_handler;
	if (sigaction(SIGIO, &sigact, NULL) < 0)
		err(EXIT_FAILURE, "cannot set SIGIO handler on %s", file);
	flags = fcntl(handle, F_GETFL);
	flags |= O_ASYNC;
	res = fcntl(handle, F_SETFL, flags);
	if (res < 0)
		err(EXIT_FAILURE, "fcntl(F_SETFL) on %s", file);
	pid = getpid();
	res = fcntl(handle, F_SETOWN, pid);
	if (res < 0)
		err(EXIT_FAILURE, "fnctl(F_SETOWN) on %s", file);

	do {
		if (sigio == 1) {
			sigio = 0;
			printf("%s: recieved SIGIO on %s\n", getprogname(),
			    file);
			run_read(false, handle, file, 0);
		}
		pause();
	} while (loop);
}

int
main(int argc, char *argv[])
{
	int ch;
	const char *file = "/dev/gpioc0";
	char method = 'r';
	bool loop = true;
	bool nonblock = false;
	u_int delayus = 0;
	int flags;
	int timeout = INFTIM;
	int handle;
	int res;
	gpio_config_t pin_config;

	while ((ch = getopt(argc, argv, "d:f:m:sSnt:uv")) != -1) {
		switch (ch) {
		case 'd':
			delayus = strtol(optarg, NULL, 10);
			if (errno != 0) {
				warn("Invalid delay value");
				usage();
				return EXIT_FAILURE;
			}
			break;
		case 'f':
			file = optarg;
			break;
		case 'm':
			method = optarg[0];
			break;
		case 's':
			loop = false;
			break;
		case 'S':
			report_format = GPIO_EVENT_REPORT_SUMMARY;
			break;
		case 'n':
			nonblock= true;
			break;
		case 't':
			errno = 0;
			timeout = strtol(optarg, NULL, 10);
			if (errno != 0) {
				warn("Invalid timeout value");
				usage();
				return EXIT_FAILURE;
			}
			break;
		case 'u':
			calc_utc_offset();
			break;
		case 'v':
			be_verbose = true;
			break;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}
	argv += optind;
	argc -= optind;

	if (argc == 0) {
		fprintf(stderr, "%s: No pin number specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		fprintf(stderr, "%s: No trigger type specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		fprintf(stderr, "%s: No trigger type specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc % 3 != 0) {
		fprintf(stderr, "%s: Invalid number of (pin intr-conf mode) triplets.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	handle = gpio_open_device(file);
	if (handle == GPIO_INVALID_HANDLE)
		err(EXIT_FAILURE, "Cannot open %s", file);

	if (report_format == GPIO_EVENT_REPORT_SUMMARY) {
		struct gpio_event_config cfg = 
		    {GPIO_EVENT_REPORT_SUMMARY, 0};

		res = ioctl(handle, GPIOCONFIGEVENTS, &cfg);
		if (res < 0)
			err(EXIT_FAILURE, "GPIOCONFIGEVENTS failed on %s", file);
	}

	if (nonblock == true) {
		flags = fcntl(handle, F_GETFL);
		flags |= O_NONBLOCK;
		res = fcntl(handle, F_SETFL, flags);
		if (res < 0)
			err(EXIT_FAILURE, "cannot set O_NONBLOCK on %s", file);
	}

	for (int i = 0; i <= argc - 3; i += 3) {

		errno = 0;
		pin_config.g_pin = strtol(argv[i], NULL, 10);
		if (errno != 0) {
			warn("Invalid pin number");
			usage();
			return EXIT_FAILURE;
		}

		if (strnlen(argv[i + 1], 2) < 2) {
			fprintf(stderr, "%s: Invalid trigger type (argument "
			    "too short).\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		switch((argv[i + 1][0] << 8) + argv[i + 1][1]) {
		case ('n' << 8) + 'o':
			pin_config.g_flags = GPIO_INTR_NONE;
			break;
		case ('e' << 8) + 'r':
			pin_config.g_flags = GPIO_INTR_EDGE_RISING;
			break;
		case ('e' << 8) + 'f':
			pin_config.g_flags = GPIO_INTR_EDGE_FALLING;
			break;
		case ('e' << 8) + 'b':
			pin_config.g_flags = GPIO_INTR_EDGE_BOTH;
			break;
		default:
			fprintf(stderr, "%s: Invalid trigger type.\n",
			    getprogname());
			usage();
			return EXIT_FAILURE;
		}

		if (strnlen(argv[i + 2], 2) < 2) {
			fprintf(stderr, "%s: Invalid pin mode (argument "
			    "too short).\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		switch((argv[i + 2][0] << 8) + argv[i + 2][1]) {
		case ('f' << 8) + 't':
			/* no changes to pin_config */
			break;
		case ('p' << 8) + 'd':
			pin_config.g_flags |= GPIO_PIN_PULLDOWN;
			break;
		case ('p' << 8) + 'u':
			pin_config.g_flags |= GPIO_PIN_PULLUP;
			break;
		default:
			fprintf(stderr, "%s: Invalid pin mode.\n",
			    getprogname());
			usage();
			return EXIT_FAILURE;
		}

		pin_config.g_flags |= GPIO_PIN_INPUT;

		res = gpio_pin_set_flags(handle, &pin_config);
		if (res < 0)
			err(EXIT_FAILURE, "configuration of pin %d on %s "
			    "failed (flags=%d)", pin_config.g_pin, file,
			    pin_config.g_flags);
	}

	switch (method) {
	case 'r':
		run_read(loop, handle, file, delayus);
		break;
	case 'p':
		run_poll(loop, handle, file, timeout, delayus);
		break;
	case 's':
		run_select(loop, handle, file, timeout, delayus);
		break;
	case 'k':
		run_kqueue(loop, handle, file, timeout, delayus);
		break;
	case 'a':
		run_aio_read(loop, handle, file, delayus);
		break;
	case 'i':
		run_sigio(loop, handle, file);
		break;
	default:
		fprintf(stderr, "%s: Unknown method.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
