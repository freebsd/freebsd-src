/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <err.h>
#include <libnvmf.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

static struct options {
	const char *dev;
} opt = {
	.dev = NULL
};

static const struct args args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id|SubNQN" },
	{ arg_none, NULL, NULL },
};

static void
disconnect(const struct cmd *f, int argc, char *argv[])
{
	int	error, fd;
	char	*path;

	if (arg_parse(argc, argv, f))
		return;
	if (nvmf_nqn_valid(opt.dev)) {
		error = nvmf_disconnect_host(opt.dev);
		if (error != 0)
			errc(EX_IOERR, error, "failed to disconnect from %s",
			    opt.dev);
	} else {
		open_dev(opt.dev, &fd, 1, 1);
		get_nsid(fd, &path, NULL);
		close(fd);

		error = nvmf_disconnect_host(path);
		if (error != 0)
			errc(EX_IOERR, error, "failed to disconnect from %s",
			    path);
	}

	exit(0);
}

static void
disconnect_all(const struct cmd *f __unused, int argc __unused,
    char *argv[] __unused)
{
	int	error;

	error = nvmf_disconnect_all();
	if (error != 0)
		errc(EX_IOERR, error,
		    "failed to disconnect from remote controllers");

	exit(0);
}

static struct cmd disconnect_cmd = {
	.name = "disconnect",
	.fn = disconnect,
	.descr = "Disconnect from a fabrics controller",
	.args = args,
};

static struct cmd disconnect_all_cmd = {
	.name = "disconnect-all",
	.fn = disconnect_all,
	.descr = "Disconnect from all fabrics controllers",
};

CMD_COMMAND(disconnect_cmd);
CMD_COMMAND(disconnect_all_cmd);
