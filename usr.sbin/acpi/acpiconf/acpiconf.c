/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: acpiconf.c,v 1.5 2000/08/08 14:12:19 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <dev/acpica/acpiio.h>

#include <contrib/dev/acpica/acgcc.h>
#include <contrib/dev/acpica//actypes.h>

#define ACPIDEV	"/dev/acpi"

static int
acpi_enable_disable(int enable)
{
	int	fd;

	fd  = open(ACPIDEV, O_RDWR);
	if (fd == -1) {
		err(1, NULL);
	}
	if (ioctl(fd, enable, NULL) == -1) {
		err(1, NULL);
	}
	close(fd);

	return (0);
}

static int
acpi_sleep(int sleep_type)
{
	int	fd;

	fd  = open(ACPIDEV, O_RDWR);
	if (fd == -1) {
		err(1, NULL);
	}
	if (ioctl(fd, ACPIIO_SETSLPSTATE, &sleep_type) == -1) {
		err(1, NULL);
	}
	close(fd);

	return (0);
}

static void
usage(const char* prog)
{
	printf("usage: %s [-deh] [-s [1|2|3|4|4b|5]]\n", prog);
	exit(0);
}

int
main(int argc, char *argv[])
{
	char	c, *prog;
	int	sleep_type;

	prog = argv[0];
	sleep_type = -1;
	while ((c = getopt(argc, argv, "dehs:")) != -1) {
		switch (c) {
		case 'd':
			acpi_enable_disable(ACPIIO_DISABLE);
			break;

		case 'e':
			acpi_enable_disable(ACPIIO_ENABLE);
			break;

		case 'h':
			usage(prog);
			break;

		case 's':
			sleep_type = optarg[0] - '0';
			if (sleep_type < 0 || sleep_type > 5) {
				fprintf(stderr, "%s: invalid sleep type (%d)\n",
				    argv[0], sleep_type);
				return -1;
			}

			/* convert sleep type value to ACPICA format */
			switch (sleep_type) {
			case 4:
				if (optarg[1] == 'b') {
					sleep_type = ACPI_STATE_S4BIOS;
				}
				break;

			case 5:
				sleep_type = ACPI_STATE_S5;
				break;

			default:
				break;
			}
			break;
		default:
			argc -= optind;
			argv += optind;
		}
	}

	if (sleep_type != -1) {
		sleep(1);	/* wait 1 sec. for key-release event */
		acpi_sleep(sleep_type);
	}
	return (0);
}
