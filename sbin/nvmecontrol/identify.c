/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
 * Copyright (C) 2018-2019 Alexander Motin <mav@FreeBSD.org>
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
#include "nvmecontrol_ext.h"

#define NONE 0xfffffffeu

static struct options {
	bool		hex;
	bool		verbose;
	const char	*dev;
	uint32_t	nsid;
} opt = {
	.hex = false,
	.verbose = false,
	.dev = NULL,
	.nsid = NONE,
};

void
print_namespace(struct nvme_namespace_data *nsdata)
{
	char cbuf[UINT128_DIG + 1];
	uint32_t	i;
	uint32_t	lbaf, lbads, ms, rp;
	uint8_t		thin_prov, ptype;
	uint8_t		flbas_fmt, t;

	thin_prov = NVMEV(NVME_NS_DATA_NSFEAT_THIN_PROV, nsdata->nsfeat);

	flbas_fmt = NVMEV(NVME_NS_DATA_FLBAS_FORMAT, nsdata->flbas);

	printf("Size:                        %lld blocks\n",
	    (long long)nsdata->nsze);
	printf("Capacity:                    %lld blocks\n",
	    (long long)nsdata->ncap);
	printf("Utilization:                 %lld blocks\n",
	    (long long)nsdata->nuse);
	printf("Thin Provisioning:           %s\n",
		thin_prov ? "Supported" : "Not Supported");
	printf("Number of LBA Formats:       %d\n", nsdata->nlbaf+1);
	printf("Current LBA Format:          LBA Format #%02d", flbas_fmt);
	if (NVMEV(NVME_NS_DATA_LBAF_MS, nsdata->lbaf[flbas_fmt]) != 0)
		printf(" %s metadata\n",
		    NVMEV(NVME_NS_DATA_FLBAS_EXTENDED, nsdata->flbas) != 0 ?
		    "Extended" : "Separate");
	else
		printf("\n");
	printf("Metadata Capabilities\n");
	printf("  Extended:                  %s\n",
	    NVMEV(NVME_NS_DATA_MC_EXTENDED, nsdata->mc) != 0 ? "Supported" :
	    "Not Supported");
	printf("  Separate:                  %s\n",
	    NVMEV(NVME_NS_DATA_MC_POINTER, nsdata->mc) != 0 ? "Supported" :
	    "Not Supported");
	printf("Data Protection Caps:        %s%s%s%s%s%s\n",
	    (nsdata->dpc == 0) ? "Not Supported" : "",
	    NVMEV(NVME_NS_DATA_DPC_MD_END, nsdata->dpc) != 0 ? "Last Bytes, " :
	    "",
	    NVMEV(NVME_NS_DATA_DPC_MD_START, nsdata->dpc) != 0 ?
	    "First Bytes, " : "",
	    NVMEV(NVME_NS_DATA_DPC_PIT3, nsdata->dpc) != 0 ? "Type 3, " : "",
	    NVMEV(NVME_NS_DATA_DPC_PIT2, nsdata->dpc) != 0 ? "Type 2, " : "",
	    NVMEV(NVME_NS_DATA_DPC_PIT1, nsdata->dpc) != 0 ? "Type 1" : "");
	printf("Data Protection Settings:    ");
	ptype = NVMEV(NVME_NS_DATA_DPS_PIT, nsdata->dps);
	if (ptype != 0) {
		printf("Type %d, %s Bytes\n", ptype,
		    NVMEV(NVME_NS_DATA_DPS_MD_START, nsdata->dps) != 0 ?
		    "First" : "Last");
	} else {
		printf("Not Enabled\n");
	}
	printf("Multi-Path I/O Capabilities: %s%s\n",
	    (nsdata->nmic == 0) ? "Not Supported" : "",
	    NVMEV(NVME_NS_DATA_NMIC_MAY_BE_SHARED, nsdata->nmic) != 0 ?
	    "May be shared" : "");
	printf("Reservation Capabilities:    %s%s%s%s%s%s%s%s%s\n",
	    (nsdata->rescap == 0) ? "Not Supported" : "",
	    NVMEV(NVME_NS_DATA_RESCAP_IEKEY13, nsdata->rescap) != 0 ?
	    "IEKEY13, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_EX_AC_AR, nsdata->rescap) != 0 ?
	    "EX_AC_AR, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_WR_EX_AR, nsdata->rescap) != 0 ?
	    "WR_EX_AR, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_EX_AC_RO, nsdata->rescap) != 0 ?
	    "EX_AC_RO, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_WR_EX_RO, nsdata->rescap) != 0 ?
	    "WR_EX_RO, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_EX_AC, nsdata->rescap) != 0 ?
	    "EX_AC, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_WR_EX, nsdata->rescap) != 0 ?
	    "WR_EX, " : "",
	    NVMEV(NVME_NS_DATA_RESCAP_PTPL, nsdata->rescap) != 0 ? "PTPL" : "");
	printf("Format Progress Indicator:   ");
	if (NVMEV(NVME_NS_DATA_FPI_SUPP, nsdata->fpi) != 0) {
		printf("%u%% remains\n",
		    NVMEV(NVME_NS_DATA_FPI_PERC, nsdata->fpi));
	} else
		printf("Not Supported\n");
	t = NVMEV(NVME_NS_DATA_DLFEAT_READ, nsdata->dlfeat);
	printf("Deallocate Logical Block:    Read %s%s%s\n",
	    (t == NVME_NS_DATA_DLFEAT_READ_NR) ? "Not Reported" :
	    (t == NVME_NS_DATA_DLFEAT_READ_00) ? "00h" :
	    (t == NVME_NS_DATA_DLFEAT_READ_FF) ? "FFh" : "Unknown",
	    NVMEV(NVME_NS_DATA_DLFEAT_DWZ, nsdata->dlfeat) != 0 ?
	    ", Write Zero" : "",
	    NVMEV(NVME_NS_DATA_DLFEAT_GCRC, nsdata->dlfeat) != 0 ?
	    ", Guard CRC" : "");
	printf("Optimal I/O Boundary:        %u blocks\n", nsdata->noiob);
	printf("NVM Capacity:                %s bytes\n",
	   uint128_to_str(to128(nsdata->nvmcap), cbuf, sizeof(cbuf)));
	if (NVMEV(NVME_NS_DATA_NSFEAT_NPVALID, nsdata->nsfeat) != 0) {
		printf("Preferred Write Granularity: %u blocks\n",
		    nsdata->npwg + 1);
		printf("Preferred Write Alignment:   %u blocks\n",
		    nsdata->npwa + 1);
		printf("Preferred Deallocate Granul: %u blocks\n",
		    nsdata->npdg + 1);
		printf("Preferred Deallocate Align:  %u blocks\n",
		    nsdata->npda + 1);
		printf("Optimal Write Size:          %u blocks\n",
		    nsdata->nows + 1);
	}
	printf("Globally Unique Identifier:  ");
	for (i = 0; i < sizeof(nsdata->nguid); i++)
		printf("%02x", nsdata->nguid[i]);
	printf("\n");
	printf("IEEE EUI64:                  ");
	for (i = 0; i < sizeof(nsdata->eui64); i++)
		printf("%02x", nsdata->eui64[i]);
	printf("\n");
	for (i = 0; i <= nsdata->nlbaf; i++) {
		lbaf = nsdata->lbaf[i];
		lbads = NVMEV(NVME_NS_DATA_LBAF_LBADS, lbaf);
		if (lbads == 0)
			continue;
		ms = NVMEV(NVME_NS_DATA_LBAF_MS, lbaf);
		rp = NVMEV(NVME_NS_DATA_LBAF_RP, lbaf);
		printf("LBA Format #%02d: Data Size: %5d  Metadata Size: %5d"
		    "  Performance: %s\n",
		    i, 1 << lbads, ms, (rp == 0) ? "Best" :
		    (rp == 1) ? "Better" : (rp == 2) ? "Good" : "Degraded");
	}
}

static void
identify_ctrlr(int fd)
{
	struct nvme_controller_data	cdata;
	int				hexlength;

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");
	close(fd);

	if (opt.hex) {
		if (opt.verbose)
			hexlength = sizeof(struct nvme_controller_data);
		else
			hexlength = offsetof(struct nvme_controller_data,
			    reserved8);
		print_hex(&cdata, hexlength);
		exit(0);
	}

	nvme_print_controller(&cdata);
	exit(0);
}

static void
identify_ns(int fd, uint32_t nsid)
{
	struct nvme_namespace_data	nsdata;
	int				hexlength;

	if (read_namespace_data(fd, nsid, &nsdata))
		errx(EX_IOERR, "Identify request failed");
	close(fd);

	if (opt.hex) {
		if (opt.verbose)
			hexlength = sizeof(struct nvme_namespace_data);
		else
			hexlength = offsetof(struct nvme_namespace_data,
			    reserved6);
		print_hex(&nsdata, hexlength);
		exit(0);
	}

	print_namespace(&nsdata);
	exit(0);
}

static void
identify(const struct cmd *f, int argc, char *argv[])
{
	char		*path;
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;

	open_dev(opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		/*
		 * We got namespace device, but we need to send IDENTIFY
		 * commands to the controller, not the namespace, since it
		 * is an admin cmd.  The namespace ID will be specified in
		 * the IDENTIFY command itself.
		 */
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);
	if (opt.nsid != NONE)
		nsid = opt.nsid;

	if (nsid == 0)
		identify_ctrlr(fd);
	else
		identify_ns(fd, nsid);
}

static const struct opts identify_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("hex", 'x', arg_none, opt, hex,
	    "Print identiy information in hex"),
	OPT("verbose", 'v', arg_none, opt, verbose,
	    "More verbosity: print entire identify table"),
	OPT("nsid", 'n', arg_uint32, opt, nsid,
	    "Namespace ID to use if not in device name"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args identify_args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd identify_cmd = {
	.name = "identify",
	.fn = identify,
	.descr = "Print summary of the IDENTIFY information",
	.ctx_size = sizeof(opt),
	.opts = identify_opts,
	.args = identify_args,
};

CMD_COMMAND(identify_cmd);
