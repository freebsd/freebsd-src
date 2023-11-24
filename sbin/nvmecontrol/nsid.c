/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2019 Alexander Motin <mav@FreeBSD.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvmecontrol.h"
#include "comnd.h"

/* Tables for command line parsing */

static cmd_fn_t gnsid;

static struct nsid_options {
	const char	*dev;
} nsid_opt = {
	.dev = NULL,
};

static const struct args nsid_args[] = {
	{ arg_string, &nsid_opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd nsid_cmd = {
	.name = "nsid",
	.fn = gnsid,
	.descr = "Get controller and NSID for namespace",
	.ctx_size = sizeof(nsid_opt),
	.opts = NULL,
	.args = nsid_args,
};

CMD_COMMAND(nsid_cmd);

static void
gnsid(const struct cmd *f, int argc, char *argv[])
{
	char		*path;
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;

	open_dev(nsid_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	close(fd);
	printf("%s\t%u\n", path, nsid);
	free(path);
}
