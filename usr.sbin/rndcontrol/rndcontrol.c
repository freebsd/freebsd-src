/*
 * Copyright (c) 1995
 *	Mark Murray.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Murray
 *	and Theodore Ts'o
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MARK MURRAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL MARK MURRAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <machine/random.h>

void usage(char *myname)
{
	fprintf(stderr, "usage: %s [ [-q | -v] [-s N | -c M]... ]\n", myname);
}

int
main(int argc, char *argv[])
{
	int verbose, ch, fd, result, i;
	u_int16_t irq;

	verbose = 1;

	fd = open("/dev/random", O_RDONLY, 0);
	if (fd == -1) {
		perror("/dev/random");
		return (1);
	}
	else {
		while ((ch = getopt(argc, argv, "qs:c:")) != EOF)
			switch (ch) {
			case 'q':
				verbose = 0;
				break;
			case 's':
				irq = (u_int16_t)atoi(optarg);
				if (verbose)
					printf("%s: setting irq %d\n", argv[0], irq);
				result = ioctl(fd, MEM_SETIRQ, (char *)&irq);
				if (result == -1) {
					perror(argv[0]);
					return (1);
				}
				break;
			case 'c':
				irq = (u_int16_t)atoi(optarg);
				if (verbose)
					printf("%s: clearing irq %d\n", argv[0], irq);
				result = ioctl(fd, MEM_CLEARIRQ, (char *)&irq);
				if (result == -1) {
					perror(argv[0]);
					return (1);
				}
				break;
			case '?':
			default:
				usage(argv[0]);
				return (1);
			}
		}
		if (verbose) {
			result = ioctl(fd, MEM_RETURNIRQ, (char *)&irq);
			if (result == -1) {
				perror(argv[0]);
				return (1);
			}
			printf("%s: Interrupts in use:", argv[0]);
			for (i = 0; i < 16; i++)
				if (irq & (1 << i))
					printf(" %d", i);
			printf("\n");
		}
		argc -= optind;
		argv += optind;

		if (argc) {
			fprintf(stderr, "%s: Unknown argument(s):", argv[-optind]);
			for (i = 0; i < argc; i++)
				fprintf(stderr, " %s", argv[i]);
			fprintf(stderr, "\n");
		}

	return 0;
}
