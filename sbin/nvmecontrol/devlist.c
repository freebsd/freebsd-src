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
#include <sys/nv.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <libnvmf.h>
#include <libutil.h>
#include <paths.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "nvmecontrol.h"
#include "comnd.h"

/* Tables for command line parsing */

#define NVME_MAX_UNIT 256

static cmd_fn_t devlist;

static struct options {
	bool	human;
} opt = {
	.human = false,
};

static const struct opts devlist_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("human", 'h', arg_none, opt, human,
	    "Show human readable disk size"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static struct cmd devlist_cmd = {
	.name = "devlist",
	.fn = devlist,
	.descr = "List NVMe controllers and namespaces",
	.ctx_size = sizeof(opt),
	.opts = devlist_opts,
	.args = NULL,
};

CMD_COMMAND(devlist_cmd);

/* End of tables for command line parsing */

static inline uint32_t
ns_get_sector_size(struct nvme_namespace_data *nsdata)
{
	uint8_t flbas_fmt, lbads;

	flbas_fmt = NVMEV(NVME_NS_DATA_FLBAS_FORMAT, nsdata->flbas);
	lbads = NVMEV(NVME_NS_DATA_LBAF_LBADS, nsdata->lbaf[flbas_fmt]);

	return (1 << lbads);
}

static void
scan_namespace(int fd, int ctrlr, uint32_t nsid)
{
	struct nvme_namespace_data 	nsdata;
	char				name[64];
	uint8_t				buf[7];
	uint64_t			size;

	if (read_namespace_data(fd, nsid, &nsdata) != 0)
		return;
	if (nsdata.nsze == 0)
		return;
	snprintf(name, sizeof(name), "%s%d%s%d", NVME_CTRLR_PREFIX, ctrlr,
	    NVME_NS_PREFIX, nsid);
	size = nsdata.nsze * (uint64_t)ns_get_sector_size(&nsdata);
	if (opt.human) {
		humanize_number(buf, sizeof(buf), size, "B",
		    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		printf("  %10s (%s)\n", name, buf);
	} else {
		printf("  %10s (%juMB)\n", name, (uintmax_t)size / 1024 / 1024);
	}
}

static bool
print_controller_info(const char *name, int fd)
{
	static struct timespec		now;
	struct nvme_controller_data	cdata;
	struct timespec			last_disconnect, delta;
	uint8_t				mn[64];
	nvlist_t			*nvl;
	const nvlist_t			*nvl_ts;
	bool				connected;

	/*
	 * If the controller doesn't support connection status, assume
	 * it is connected.
	 */
	if (nvmf_connection_status(fd, &nvl) != 0) {
		connected = true;
		nvl = NULL;
	} else {
		connected = nvlist_get_bool(nvl, "connected");
	}

	if (connected) {
		if (read_controller_data(fd, &cdata) != 0) {
			nvlist_destroy(nvl);
			return (false);
		}
	} else {
		if (ioctl(fd, NVME_GET_CONTROLLER_DATA, &cdata) == -1) {
			nvlist_destroy(nvl);
			return (false);
		}
	}

	nvme_strvis(mn, cdata.mn, sizeof(mn), NVME_MODEL_NUMBER_LENGTH);
	printf("%6s: %s", name, mn);
	if (connected) {
		const struct nvme_discovery_log_entry *dle;
		size_t len;

		nvlist_destroy(nvl);
		if (nvmf_reconnect_params(fd, &nvl) == 0) {
			dle = nvlist_get_binary(nvl, "dle", &len);
			if (len == sizeof(*dle)) {
				printf(" (connected via %s %.*s:%.*s)",
				    nvmf_transport_type(dle->trtype),
				    (int)sizeof(dle->traddr), dle->traddr,
				    (int)sizeof(dle->trsvcid), dle->trsvcid);
			}
		} else {
			nvl = NULL;
		}
	} else {
		if (now.tv_sec == 0)
			clock_gettime(CLOCK_REALTIME, &now);

		nvl_ts = nvlist_get_nvlist(nvl, "last_disconnect");
		last_disconnect.tv_sec = nvlist_get_number(nvl_ts, "tv_sec");
		last_disconnect.tv_nsec = nvlist_get_number(nvl_ts, "tv_nsec");
		timespecsub(&now, &last_disconnect, &delta);
		printf(" (disconnected for %ju seconds)",
		    (uintmax_t)delta.tv_sec);
	}
	printf("\n");
	nvlist_destroy(nvl);
	return (connected);
}

static bool
scan_controller(int ctrlr)
{
	struct nvme_ns_list		nslist;
	char				name[64];
	uint32_t			nsid;
	int				fd, ret;

	snprintf(name, sizeof(name), "%s%d", NVME_CTRLR_PREFIX, ctrlr);

	ret = open_dev(name, &fd, 0, 0);

	if (ret == EACCES) {
		warnx("could not open "_PATH_DEV"%s\n", name);
		return (false);
	} else if (ret != 0)
		return (false);

	if (!print_controller_info(name, fd)) {
		close(fd);
		return (true);
	}

	nsid = 0;
	for (;;) {
		if (read_active_namespaces(fd, nsid, &nslist) != 0)
			break;
		for (u_int i = 0; i < nitems(nslist.ns); i++) {
			nsid = nslist.ns[i];
			if (nsid == 0) {
				break;
			}

			scan_namespace(fd, ctrlr, nsid);
		}
		if (nsid == 0 || nsid >= NVME_GLOBAL_NAMESPACE_TAG - 1)
			break;
	}

	close(fd);
	return (true);
}

static void
devlist(const struct cmd *f, int argc, char *argv[])
{
	int				ctrlr, found;

	if (arg_parse(argc, argv, f))
		return;

	ctrlr = -1;
	found = 0;

	while (ctrlr < NVME_MAX_UNIT) {
		ctrlr++;
		if (scan_controller(ctrlr))
			found++;
	}

	if (found == 0) {
		printf("No NVMe controllers found.\n");
		exit(EX_UNAVAILABLE);
	}

	exit(0);
}
