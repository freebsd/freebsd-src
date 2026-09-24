/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#define NONE 0xffffffffu

static struct options {
	bool		list;
	uint32_t	apst;
	uint32_t	apst_limit;
	uint32_t	power;
	uint32_t	workload;
	const char	*apst_data;
	const char	*dev;
} opt = {
	.list = false,
	.apst = NONE,
	.apst_limit = NONE,
	.power = NONE,
	.workload = 0,
	.apst_data = NULL,
	.dev = NULL,
};

static void
power_list_one(int i, struct nvme_power_state *nps)
{
	int mpower, apower, ipower;
	uint8_t mps, nops, aps, apw;

	mps = NVMEV(NVME_PWR_ST_MPS, nps->mps_nops);
	nops = NVMEV(NVME_PWR_ST_NOPS, nps->mps_nops);
	apw = NVMEV(NVME_PWR_ST_APW, nps->apw_aps);
	aps = NVMEV(NVME_PWR_ST_APS, nps->apw_aps);

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
	printf(" #   Max pwr  Enter Lat  Exit Lat RT RL WT WL Idle Pwr  Act Pwr Workload\n");
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

enum feat_opc { GET, SET };

static int
power_apst_cmd(int fd, enum feat_opc opc, bool enable, uint64_t *data,
    int size)
{
	struct nvme_pt_command pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = (opc == SET) ?
	    NVME_OPC_SET_FEATURES : NVME_OPC_GET_FEATURES;
	pt.cmd.cdw10 = htole32(NVME_FEAT_AUTONOMOUS_POWER_STATE_TRANSITION);
	pt.cmd.cdw11 = htole32(enable);
	pt.buf = data;
	pt.len = size;
	pt.is_read = (opc == GET) ? 1 : 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) == -1)
		err(EX_IOERR, "APST %s command failed",
		    (opc == SET) ? "set" : "get");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "APST %s command returned error",
		    (opc == SET) ? "set" : "get");

	return (pt.cpl.cdw0);
}

static void
power_apst_data_generate(struct nvme_controller_data *cdata,
    uint64_t *data, int num, int limit)
{
	int i, itpt, latency;

	if (cdata->npss >= num)
		errx(EX_UNAVAILABLE, "controller reports too many power states");

	for (i = cdata->npss; i > 0; --i) {
		if (!NVMEV(NVME_PWR_ST_NOPS,
		    cdata->power_state[i].mps_nops)) {
			data[i - 1] = data[i];
			continue;
		}

		latency = (cdata->power_state[i].enlat +
		    cdata->power_state[i].exlat) / 1000;
		if (latency > limit)
			continue;

		/* Wait 50x the latency before each transition. */
		itpt = MIN(latency * 50, (1 << 24) - 1);
		data[i - 1] = htole64((uint64_t)itpt << 8 |
		    (uint64_t)i << 3);
	}
}

static void
power_apst_data_parse(struct nvme_controller_data *cdata,
    uint64_t *data, int num, const char *dstr)
{
	int i, itps, itpt;
	char *str, *token;

	str = strdup(dstr);

	for (i = 0; (token = strsep(&str, " ,")) != NULL && i < num; ++i) {
		if (sscanf(token, "%i:%i", &itps, &itpt) != 2)
			errx(EX_USAGE, "cannot parse provided configuration");

		if (itps < 0 || itps > cdata->npss)
			errx(EX_USAGE, "invalid ITPS=%d (must be 0..%d)",
			    itps, cdata->npss);
		if (itpt < 0 || itpt >= 1 << 24)
			errx(EX_USAGE, "invalid ITPT=%d (must be 0..%d)",
			    itpt, (1 << 24) - 1);

		data[i] = htole64((uint64_t)itpt << 8 |
		    (uint64_t)itps << 3);
	}
}

static void
power_apst_show(uint64_t *data, int num, bool enabled)
{
	uint32_t entry;
	int i;

	while (num > 0 && data[num - 1] == 0)
		--num;

	printf("APST %s\n", enabled ? "enabled" : "disabled");
	printf("\n #  ITPS    ITPT       Hex\n");
	printf("--  ----  ------  --------\n");
	for (i = 0; i < num || i == 0; ++i) {
		entry = letoh(data[i]);
		printf("%2d: %4d  %4dms  %#8x\n",
		    i, (entry & 0xF8) >> 3, entry >> 8, entry);
	}
}

static void
power_apst(int fd, struct nvme_controller_data *cdata,
    uint32_t enable, const char *dstr, uint32_t limit)
{
	uint64_t data[32];

	if (cdata->apsta == 0)
		errx(EX_UNAVAILABLE, "Not supported by the controller");

	if (enable == NONE)
		enable = power_apst_cmd(fd, GET, 0, NULL, 0);

	memset(&data, 0, sizeof(data));

	if (dstr != NULL)
		power_apst_data_parse(cdata, data, nitems(data), dstr);
	else if (limit != NONE)
		power_apst_data_generate(cdata, data, nitems(data), limit);
	else
		power_apst_cmd(fd, GET, 0, data, sizeof(data));

	power_apst_cmd(fd, SET, enable, data, sizeof(data));
}

static void
power_show(int fd, struct nvme_controller_data *cdata)
{
	struct nvme_pt_command	pt;
	uint64_t data[32];
	int status;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_GET_FEATURES;
	pt.cmd.cdw10 = htole32(NVME_FEAT_POWER_MANAGEMENT);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "set feature power mgmt request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "set feature power mgmt request returned error");

	printf("Current Power State is %d\n", pt.cpl.cdw0 & 0x1F);
	printf("Current Workload Hint is %d\n", pt.cpl.cdw0 >> 5);

	if (cdata->apsta != 0) {
		status = power_apst_cmd(fd, GET, 0, data, sizeof(data));
		power_apst_show(data, nitems(data), status);
	}
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

	if (opt.list && opt.power != NONE) {
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

	if (opt.power != NONE) {
		power_set(fd, opt.power, opt.workload, 0);
		goto out;
	}

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");

	if (opt.list) {
		power_list(&cdata);
		goto out;
	}

	if (opt.apst != NONE || opt.apst_limit != NONE ||
	    opt.apst_data != NULL) {
		power_apst(fd, &cdata, opt.apst,
		    opt.apst_data, opt.apst_limit);
		goto out;
	}

	power_show(fd, &cdata);

out:
	close(fd);
	exit(0);
}

static const struct opts power_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("apst", 'a', arg_uint32, opt, apst,
	    "Enable or disable APST"),
	OPT("data", 'd', arg_string, opt, apst_data,
	    "Set the APST configuration"),
	OPT("list", 'l', arg_none, opt, list,
	    "List the valid power states"),
	OPT("limit", 'm', arg_uint32, opt, apst_limit,
	    "Set the APST latency limit"),
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
