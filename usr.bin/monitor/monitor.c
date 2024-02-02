/*
 * Copyright (c) 2008 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

typedef struct monitor_elm {
	const char *path;
	int	fd;
} *monitor_elm_t;

static void usage(int exit_code);
static void monitor_add(const char *path);
static void monitor_events(void);

static int VerboseOpt;
static int QuietOpt;
static int ExitOpt;
static int KQueueFd;
static int NumFiles;
static int MaxFiles;
static monitor_elm_t *Elms;

int
main(int ac, char **av)
{
	int ch;
	int i;

	while ((ch = getopt(ac, av, "qvx")) != -1) {
		switch (ch) {
		case 'q':
			if (VerboseOpt > 0)
				--VerboseOpt;
			else
				++QuietOpt;
			break;
		case 'v':
			if (QuietOpt > 0)
				--QuietOpt;
			else
				++VerboseOpt;
			break;
		case 'x':
			ExitOpt = 1;
			break;
		default:
			usage(1);
			/* not reached */
		}
	}
	ac -= optind;
	av += optind;

	if (ac < 1) {
		usage(1);
		/* not reached */
	}

	if ((KQueueFd = kqueue()) < 0) {
		perror("kqueue");
		exit(1);
	}
	NumFiles = MaxFiles = 16;
	Elms = calloc(MaxFiles, sizeof(monitor_elm_t));

	for (i = 0; i < ac; ++i) {
		monitor_add(av[i]);
	}
	fflush(stdout);
	do {
		monitor_events();
		fflush(stdout);
	} while (ExitOpt == 0);
	exit(0);
}

static
void
monitor_add(const char *path)
{
	monitor_elm_t elm;
	struct kevent kev;
	int n;

	elm = malloc(sizeof(*elm));
	bzero(elm, sizeof(*elm));
	elm->path = path;
	elm->fd = open(path, O_RDONLY);
	if (elm->fd < 0) {
		printf("%s\tnot found\n", path);
		return;
	}
	EV_SET(&kev, elm->fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
		NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB |
		NOTE_LINK | NOTE_RENAME | NOTE_REVOKE |
#if defined(__FreeBSD__) || defined(__NetBSD__)
		NOTE_OPEN | NOTE_CLOSE | NOTE_CLOSE_WRITE | NOTE_READ,
#elif defined(__OpenBSD__)
		NOTE_TRUNCATE,
#else
		0,
#endif
		0, NULL);
	n = kevent(KQueueFd, &kev, 1, NULL, 0, NULL);
	if (n < 0) {
		perror("kqueue");
		exit(1);
	}

	if (elm->fd >= NumFiles) {
		MaxFiles = (elm->fd + 16) * 3 / 2;
		Elms = realloc(Elms, MaxFiles * sizeof(elm));
		bzero(&Elms[NumFiles], (MaxFiles - NumFiles) * sizeof(elm));
		NumFiles = MaxFiles;
	}
	Elms[elm->fd] = elm;
}

static
void
monitor_events(void)
{
	struct kevent kev_array[1];
	struct kevent *kev;
	monitor_elm_t elm;
	struct stat st;
	int bno;
	int i;
	int n;

	n = kevent(KQueueFd, NULL, 0, kev_array, 1, NULL);
	for (i = 0; i < n; ++i) {
		kev = &kev_array[i];
		elm = Elms[kev->ident];
		printf("%-23s", elm->path);
		if (VerboseOpt && fstat(kev->ident, &st) == 0 &&
		    S_ISREG(st.st_mode)) {
			printf(" %10jd", (intmax_t)st.st_size);
		}
		while (QuietOpt == 0 && (bno = ffs(kev->fflags)) > 0) {
			printf(" ");
			--bno;
			kev->fflags &= ~(1 << bno);
			switch (1 << bno) {
			case NOTE_DELETE:
				printf("delete");
				break;
			case NOTE_WRITE:
				printf("write");
				break;
			case NOTE_EXTEND:
				printf("extend");
				break;
			case NOTE_ATTRIB:
				printf("attrib");
				break;
			case NOTE_LINK:
				printf("link");
				break;
			case NOTE_RENAME:
				printf("rename");
				break;
			case NOTE_REVOKE:
				printf("revoke");
				break;
#if defined(__FreeBSD__) || defined(__NetBSD__)
			case NOTE_OPEN:
				printf("open");
				break;
			case NOTE_CLOSE:
				printf("close");
				break;
			case NOTE_CLOSE_WRITE:
				printf("closew");
				break;
			case NOTE_READ:
				printf("read");
				break;
#endif
#ifdef NOTE_TRUNCATE
			case NOTE_TRUNCATE:
				printf("truncate");
				break;
#endif
			default:
				printf("%08x", 1 << bno);
				break;
			}
		}
		printf("\n");
	}
}

static
void
usage(int exit_code)
{
	fprintf(stderr,
		"monitor [-vx] files...\n"
		"    -q      Be more quiet\n"
		"    -v      Be more verbose\n"
		"    -x      Exit after first event reported\n"
	);
	exit(exit_code);
}

