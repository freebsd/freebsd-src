/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andriy Gapon <avg@FreeBSD.org>
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

#include <sys/cdefs.h>
__RCSID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/endian.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mpsutil.h"

MPS_TABLE(top, slot);

static int
slot_set(int argc, char **argv)
{
	char *endptr;
	unsigned long ux;
	long x;
	int error;
	int fd;
	U32 status;
	U16 handle;
	U16 slot;

	if (argc != 5) {
		warnx("Incorrect number of arguments");
		return (EINVAL);
	}

	if (strcmp(argv[1], "status") != 0) {
		warnx("Invalid argument '%s', expecting 'status'",
		    argv[1]);
		return (EINVAL);
	}

	errno = 0;
	x = strtol(argv[2], &endptr, 0);
	if (*endptr != '\0' || errno != 0 || x < 0 || x > UINT16_MAX) {
		warnx("Invalid enclosure handle argument '%s'", argv[2]);
		return (EINVAL);
	}
	handle = x;

	errno = 0;
	x = strtol(argv[3], &endptr, 0);
	if (*endptr != '\0' || errno != 0 || x < 0 || x > UINT16_MAX) {
		warnx("Invalid slot argument '%s'", argv[3]);
		return (EINVAL);
	}
	slot = x;

	errno = 0;
	ux = strtoul(argv[4], &endptr, 0);
	if (*endptr != '\0' || errno != 0 || ux > UINT32_MAX) {
		warnx("Invalid status argument '%s'", argv[4]);
		return (EINVAL);
	}
	status = ux;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	if (mps_set_slot_status(fd, htole16(handle), htole16(slot),
	    htole32(status)) != 0) {
		warnx("Failed to set status");
		close(fd);
		return (1);
	}

	close(fd);
	printf("Successfully set slot status\n");
	return (0);
}

MPS_COMMAND(slot, set, slot_set, "status <enclosure handle> <slot number> "
    "<status>", "Set status of the slot in the directly attached enclosure");
