/*-
 * Copyright (c) 2016 Netflix, Inc.
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
#include <sys/ioccom.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

_Static_assert(sizeof(struct nvme_power_state) == 256 / NBBY,
	       "nvme_power_state size wrong");

#define POWER_NONE 0xffffffffu

static struct options {
	bool		list;
	uint32_t	power;
	uint32_t	workload;
	const char	*dev;
} opt = {
	.list = false,
	.power = POWER_NONE,
	.workload = 0,
	.dev = NULL,
};

static void
power_list_one(int i, struct nvme_power_state *nps)
{
	int mpower, apower, ipower;
	uint8_t mps, nops, aps, apw;

	mps = (nps->mps_nops >> NVME_PWR_ST_MPS_SHIFT) &
		NVME_PWR_ST_MPS_MASK;
	nops = (nps->mps_nops >> NVME_PWR_ST_NOPS_SHIFT) &
		NVME_PWR_ST_NOPS_MASK;
	apw = (nps->apw_aps >> NVME_PWR_ST_APW_SHIFT) &
		NVME_PWR_ST_APW_MASK;
	aps = (nps->apw_aps >> NVME_PWR_ST_APS_SHIFT) &
		NVME_PWR_ST_APS_MASK;

	mpower = nps->mp;
	if (mps == 0)
		mpower *= 100;
	ipower = nps->idlp;
	if (nps->ips == 1)
		ipower *= 100;
	apower = nps->actp;
	if (aps == 1)
		apower *= 100;
	printf("%2d: %2d.%04dW%c %3d.%03dms %3d.%03dms %2d %2d %2d %2d %2d.%04dW %2d.%04dW %d\n",
	       i, mpower / 10000, mpower % 10000,
	       nops ? '*' : ' ', nps->enlat / 1000, nps->enlat % 1000,
	       nps->exlat / 1000, nps->exlat % 1000, nps->rrt, nps->rrl,
	       nps->rwt, nps->rwl, ipower / 10000, ipower % 10000,
	       apower / 10000, apower % 10000, apw);
}

static void
power_list(struct nvme_controller_data *cdata)
{
	int i;

	printf("\nPower States Supported: %d\n\n", cdata->npss + 1);
	printf(" #   Max pwr  Enter Lat  Exit Lat RT RL WT WL Idle Pwr  Act Pwr Workloadd\n");
	printf("--  --------  --------- --------- -- -- -- -- -------- -------- --\n");
	for (i = 0; i <= cdata->npss; i++)
		power_list_one(i, &cdata->power_state[i]);
}

static void
power_set(int fd, int power_val, int workload, int perm)
{
	struct nvme_pt_command	pt;
	uint32_t p;

	p = perm ? (1u << 31) : 0;
	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_SET_FEATURES;
	pt.cmd.cdw10 = htole32(NVME_FEAT_POWER_MANAGEMENT | p);
	pt.cmd.cdw11 = htole32(power_val | (workload << 5));

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "set feature power mgmt request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "set feature power mgmt request returned error");
}

static void
power_show(int fd)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_GET_FEATURES;
	pt.cmd.cdw10 = htole32(NVME_FEAT_POWER_MANAGEMENT);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "set feature power mgmt request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "set feature power mgmt request returned error");

	printf("Current Power State is %d\n", pt.cpl.cdw0 & 0x1F);
	printf("Current Workload Hint is %d\n", pt.cpl.cdw0 >> 5);
}

static void
power(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_controller_data	cdata;
	int				fd;
	char				*path;
	uint32_t			nsid;

	if (arg_parse(argc, argv, f))
		return;

	if (opt.list && opt.power != POWER_NONE) {
		fprintf(stderr, "Can't set power and list power states\n");
		arg_help(argc, argv, f);
	}

	open_dev(opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	}
	free(path);

	if (opt.list) {
		if (read_controller_data(fd, &cdata))
			errx(EX_IOERR, "Identify request failed");
		power_list(&cdata);
		goto out;
	}

	if (opt.power != POWER_NONE) {
		power_set(fd, opt.power, opt.workload, 0);
		goto out;
	}
	power_show(fd);

out:
	close(fd);
	exit(0);
}

static const struct opts power_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("list", 'l', arg_none, opt, list,
	    "List the valid power states"),
	OPT("power", 'p', arg_uint32, opt, power,
	    "Set the power state"),
	OPT("workload", 'w', arg_uint32, opt, workload,
	    "Set the workload hint"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args power_args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd power_cmd = {
	.name = "power",
	.fn = power,
	.descr = "Manage power states for the drive",
	.ctx_size = sizeof(opt),
	.opts = power_opts,
	.args = power_args,
};

CMD_COMMAND(power_cmd);
