/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Daniel Austin <freebsd-ports@dan.me.uk>
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
#include <sys/param.h>
#include <sys/errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mpsutil.h"

static int set_ncq(int ac, char **av);

MPS_TABLE(top, set);

static int
set_ncq(int ac, char **av)
{
	MPI2_CONFIG_PAGE_HEADER header;
	MPI2_CONFIG_PAGE_IO_UNIT_1 *iounit1;
	MPI2_CONFIG_REQUEST req;
	MPI2_CONFIG_REPLY reply;
	int error, fd;

	bzero(&req, sizeof(req));
	bzero(&header, sizeof(header));
	bzero(&reply, sizeof(reply));

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	error = mps_read_config_page_header(fd, MPI2_CONFIG_PAGETYPE_IO_UNIT, 1, 0,
		&header, NULL);
	if (error) {
			error = errno;
			warn("Failed to get IOUNIT page 1 header");
			return (error);
	}

	iounit1 = mps_read_config_page(fd, MPI2_CONFIG_PAGETYPE_IO_UNIT, 1, 0, NULL);
	if (iounit1 == NULL) {
		error = errno;
		warn("Failed to get IOUNIT page 1 info");
		return (error);
	}

	if (ac == 1) {
		/* just show current setting */
		printf("SATA Native Command Queueing is currently: %s\n",
			((iounit1->Flags & MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE) == 0) ?
			"ENABLED" : "DISABLED");
	} else if (ac == 2) {
		if (!strcasecmp(av[1], "enable") || !strcmp(av[1], "1")) {
			iounit1->Flags &= ~MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE;
		} else if (!strcasecmp(av[1], "disable") || !strcmp(av[1], "0")) {
			iounit1->Flags |= MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE;
		} else {
			free(iounit1);
			error = EINVAL;
			warn("set ncq: Only 'enable' and 'disable' allowed.");
			return (EINVAL);
		}
		req.Function = MPI2_FUNCTION_CONFIG;
		req.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
		req.ExtPageLength = 0;
		req.ExtPageType = 0;
		req.Header = header;
		req.PageAddress = 0;
		if (mps_pass_command(fd, &req, sizeof(req) - sizeof(req.PageBufferSGE), &reply, sizeof(reply),
			NULL, 0, iounit1, sizeof(iounit1), 30) != 0) {
				free(iounit1);
				error = errno;
				warn("Failed to update config page");
		                return (error);
		}
		if (!IOC_STATUS_SUCCESS(reply.IOCStatus)) {
			free(iounit1);
			error = errno;
			warn("%s", mps_ioc_status(reply.IOCStatus));
			return (error);
		}
		printf("NCQ setting accepted.  It may not take effect until the controller is reset.\n");
	} else {
		free(iounit1);
		errno = EINVAL;
		warn("set ncq: too many arguments");
		return (EINVAL);
	}
	free(iounit1);

	close(fd);
	return (0);
}

MPS_COMMAND(set, ncq, set_ncq, "[enable|disable]", "set SATA NCQ function")

