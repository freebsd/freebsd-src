/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2013 Intel Corporation
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
 */

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

static void
print_bytes(void *data, uint32_t length)
{
	uint32_t	i, j;
	uint8_t		*p, *end;

	end = (uint8_t *)data + length;

	for (i = 0; i < length; i++) {
		p = (uint8_t *)data + (i*16);
		printf("%03x: ", i*16);
		for (j = 0; j < 16 && p < end; j++)
			printf("%02x ", *p++);
		if (p >= end)
			break;
		printf("\n");
	}
	printf("\n");
}

static void
print_dwords(void *data, uint32_t length)
{
	uint32_t	*p;
	uint32_t	i, j;

	p = (uint32_t *)data;
	length /= sizeof(uint32_t);

	for (i = 0; i < length; i+=8) {
		printf("%03x: ", i*4);
		for (j = 0; j < 8; j++)
			printf("%08x ", p[i+j]);
		printf("\n");
	}

	printf("\n");
}

void
print_hex(void *data, uint32_t length)
{
	if (length >= sizeof(uint32_t) || length % sizeof(uint32_t) == 0)
		print_dwords(data, length);
	else
		print_bytes(data, length);
}

int
read_controller_data(int fd, struct nvme_controller_data *cdata)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.cdw10 = htole32(1);
	pt.buf = cdata;
	pt.len = sizeof(*cdata);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		return (errno);

	/* Convert data to host endian */
	nvme_controller_data_swapbytes(cdata);

	if (nvme_completion_is_error(&pt.cpl))
		return (EIO);
	return (0);
}

int
read_namespace_data(int fd, uint32_t nsid, struct nvme_namespace_data *nsdata)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(0);
	pt.buf = nsdata;
	pt.len = sizeof(*nsdata);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		return (errno);

	/* Convert data to host endian */
	nvme_namespace_data_swapbytes(nsdata);

	if (nvme_completion_is_error(&pt.cpl))
		return (EIO);
	return (0);
}

int
open_dev(const char *str, int *fd, int write, int exit_on_error)
{
	char		full_path[64];

	snprintf(full_path, sizeof(full_path), _PATH_DEV"%s", str);
	*fd = open(full_path, write ? O_RDWR : O_RDONLY);
	if (*fd < 0) {
		if (exit_on_error) {
			err(EX_OSFILE, "could not open %s%s", full_path,
			    write ? " for write" : "");
		} else
			return (errno);
	}

	return (0);
}

void
get_nsid(int fd, char **ctrlr_str, uint32_t *nsid)
{
	struct nvme_get_nsid gnsid;

	if (ioctl(fd, NVME_GET_NSID, &gnsid) < 0)
		err(EX_OSERR, "NVME_GET_NSID ioctl failed");
	if (ctrlr_str != NULL)
		*ctrlr_str = strndup(gnsid.cdev, sizeof(gnsid.cdev));
	if (nsid != NULL)
		*nsid = gnsid.nsid;
}

int
main(int argc, char *argv[])
{
	static char dir[MAXPATHLEN];

	cmd_init();

	cmd_load_dir("/lib/nvmecontrol", NULL, NULL);
	snprintf(dir, MAXPATHLEN, "%s/lib/nvmecontrol", getlocalbase());
	cmd_load_dir(dir, NULL, NULL);

	cmd_dispatch(argc, argv, NULL);

	return (0);
}
