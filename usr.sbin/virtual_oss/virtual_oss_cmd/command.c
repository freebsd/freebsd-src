/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <stdarg.h>
#include <fcntl.h>

#include "virtual_oss.h"

static void
message(const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);
}

static void
usage(void)
{
	message("Usage: virtual_oss_cmd /dev/vdsp.ctl [command line arguments to pass to virtual_oss]\n");
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	char options[VIRTUAL_OSS_OPTIONS_MAX] = {};
	size_t offset = 0;
	size_t len = VIRTUAL_OSS_OPTIONS_MAX - 1;
	int fd;

	/* check if no options */
	if (argc < 2)
		usage();

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		errx(EX_SOFTWARE, "Could not open '%s'", argv[1]);

	for (int x = 2; x != argc; x++) {
		size_t tmp = strlen(argv[x]) + 1;
		if (tmp > len)
			errx(EX_SOFTWARE, "Too many options passed");
		memcpy(options + offset, argv[x], tmp);
		options[offset + tmp - 1] = ' ';
		offset += tmp;
		len -= tmp;
	}

	if (options[0] == 0) {
		struct virtual_oss_system_info info;
		if (ioctl(fd, VIRTUAL_OSS_GET_SYSTEM_INFO, &info) < 0)
			errx(EX_SOFTWARE, "Cannot get system information");

		info.rx_device_name[sizeof(info.rx_device_name) - 1] = 0;
		info.tx_device_name[sizeof(info.tx_device_name) - 1] = 0;

		printf("Sample rate: %u Hz\n"
		       "Sample width: %u bits\n"
		       "Sample channels: %u\n"
		       "Output jitter: %u / %u\n"
		       "Input device name: %s\n"
		       "Output device name: %s\n",
		       info.sample_rate,
		       info.sample_bits,
		       info.sample_channels,
		       info.tx_jitter_down,
		       info.tx_jitter_up,
		       info.rx_device_name,
		       info.tx_device_name);
	} else {
		/* execute options */
		if (ioctl(fd, VIRTUAL_OSS_ADD_OPTIONS, options) < 0)
			errx(EX_SOFTWARE, "One or more invalid options");
		/* show error, if any */
		if (options[0] != '\0')
			errx(EX_SOFTWARE, "%s", options);
	}

	close(fd);
	return (0);
}
