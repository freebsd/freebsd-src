/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Johannes Totz
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

#include <sys/efiio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr, "Usage:\n"
	    "  efiwake                          -- print out current "
	        "EFI time and wake time\n"
	    "  efiwake -d                       -- disable wake time\n"
	    "  efiwake -e yyyy-mm-ddThh:mm:ss   -- enable wake time\n"
	);
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	struct efi_tm now;
	struct efi_waketime_ioc	waketime;
	int ch, error, efi_fd;
	bool disable = false, enable = false;

	memset(&waketime, 0, sizeof(waketime));

	while ((ch = getopt(argc, argv, "de:")) != -1) {
		switch (ch) {
		case 'd':
			disable = true;
			break;
		case 'e':
			if (sscanf(optarg,
			    "%hu-%02hhu-%02hhuT%02hhu:%02hhu:%02hhu",
			    &waketime.waketime.tm_year,
			    &waketime.waketime.tm_mon,
			    &waketime.waketime.tm_mday,
			    &waketime.waketime.tm_hour,
			    &waketime.waketime.tm_min,
			    &waketime.waketime.tm_sec) != 6) {
				usage();
			}
			enable = true;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();
	if (disable && enable)
		usage();

	efi_fd = open("/dev/efi", O_RDWR);
	if (efi_fd < 0)
		err(EX_OSERR, "cannot open /dev/efi");

	error = ioctl(efi_fd, EFIIOC_GET_TIME, &now);
	if (error != 0)
		err(EX_OSERR, "cannot get EFI time");

	/* EFI's time can be different from kernel's time. */
	printf("Current EFI time: %u-%02u-%02uT%02u:%02u:%02u\n",
	    now.tm_year, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min,
	    now.tm_sec);

	if (disable) {
		/*
		 * It's tempting to preserve the current timer value.
		 * However, wonky EFI implementations sometimes return bogus
		 * dates for the wake timer and would then fail disabling it
		 * here.
		 * A safe timer value is the current EFI time.
		 */
		waketime.waketime = now;
		waketime.enabled = 0;
		error = ioctl(efi_fd, EFIIOC_SET_WAKETIME, &waketime);
		if (error != 0)
			err(EX_OSERR, "cannot disable EFI wake time");
	}
	if (enable) {
		waketime.enabled = 1;
		error = ioctl(efi_fd, EFIIOC_SET_WAKETIME, &waketime);
		if (error != 0)
			err(EX_OSERR, "cannot enable EFI wake time");
	}

	/* Confirm to user what the wake time has been set to. */
	error = ioctl(efi_fd, EFIIOC_GET_WAKETIME, &waketime);
	if (error != 0)
		err(EX_OSERR, "cannot get EFI wake time");

	printf("EFI wake time: %u-%02u-%02uT%02u:%02u:%02u; "
	    "enabled=%i, pending=%i\n",
	    waketime.waketime.tm_year, waketime.waketime.tm_mon,
	    waketime.waketime.tm_mday, waketime.waketime.tm_hour,
	    waketime.waketime.tm_min, waketime.waketime.tm_sec,
	    waketime.enabled, waketime.pending);

	close(efi_fd);
	return (0);
}
