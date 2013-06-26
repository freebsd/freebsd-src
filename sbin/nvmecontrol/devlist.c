/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

static void
devlist_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, DEVLIST_USAGE);
	exit(EX_USAGE);
}

static inline uint32_t
ns_get_sector_size(struct nvme_namespace_data *nsdata)
{

	return (1 << nsdata->lbaf[0].lbads);
}

void
devlist(int argc, char *argv[])
{
	struct nvme_controller_data	cdata;
	struct nvme_namespace_data	nsdata;
	char				name[64];
	uint32_t			i;
	int				ch, ctrlr, exit_code, fd, found;

	exit_code = EX_OK;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch ((char)ch) {
		default:
			devlist_usage();
		}
	}

	ctrlr = -1;
	found = 0;

	while (1) {
		ctrlr++;
		sprintf(name, "nvme%d", ctrlr);

		exit_code = open_dev(name, &fd, 0, 0);

		if (exit_code == EX_NOINPUT)
			break;
		else if (exit_code == EX_NOPERM) {
			printf("Could not open /dev/%s, errno = %d (%s)\n",
			    name, errno, strerror(errno));
			continue;
		}

		found++;
		read_controller_data(fd, &cdata);
		printf("%6s: %s\n", name, cdata.mn);

		for (i = 0; i < cdata.nn; i++) {
			sprintf(name, "nvme%dns%d", ctrlr, i+1);
			read_namespace_data(fd, i+1, &nsdata);
			printf("  %10s (%lldGB)\n",
				name,
				nsdata.nsze *
				(long long)ns_get_sector_size(&nsdata) /
				1024 / 1024 / 1024);
		}

		close(fd);
	}

	if (found == 0)
		printf("No NVMe controllers found.\n");

	exit(EX_OK);
}
