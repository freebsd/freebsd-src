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

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"

static void
print_controller(struct nvme_controller_data *cdata)
{
	uint8_t str[128];

	printf("Controller Capabilities/Features\n");
	printf("================================\n");
	printf("Vendor ID:                  %04x\n", cdata->vid);
	printf("Subsystem Vendor ID:        %04x\n", cdata->ssvid);
	nvme_strvis(str, cdata->sn, sizeof(str), NVME_SERIAL_NUMBER_LENGTH);
	printf("Serial Number:              %s\n", str);
	nvme_strvis(str, cdata->mn, sizeof(str), NVME_MODEL_NUMBER_LENGTH);
	printf("Model Number:               %s\n", str);
	nvme_strvis(str, cdata->fr, sizeof(str), NVME_FIRMWARE_REVISION_LENGTH);
	printf("Firmware Version:           %s\n", str);
	printf("Recommended Arb Burst:      %d\n", cdata->rab);
	printf("IEEE OUI Identifier:        %02x %02x %02x\n",
		cdata->ieee[0], cdata->ieee[1], cdata->ieee[2]);
	printf("Multi-Interface Cap:        %02x\n", cdata->mic);
	/* TODO: Use CAP.MPSMIN to determine true memory page size. */
	printf("Max Data Transfer Size:     ");
	if (cdata->mdts == 0)
		printf("Unlimited\n");
	else
		printf("%d\n", PAGE_SIZE * (1 << cdata->mdts));
	printf("\n");

	printf("Admin Command Set Attributes\n");
	printf("============================\n");
	printf("Security Send/Receive:       %s\n",
		cdata->oacs.security ? "Supported" : "Not Supported");
	printf("Format NVM:                  %s\n",
		cdata->oacs.format ? "Supported" : "Not Supported");
	printf("Firmware Activate/Download:  %s\n",
		cdata->oacs.firmware ? "Supported" : "Not Supported");
	printf("Abort Command Limit:         %d\n", cdata->acl+1);
	printf("Async Event Request Limit:   %d\n", cdata->aerl+1);
	printf("Number of Firmware Slots:    ");
	if (cdata->oacs.firmware != 0)
		printf("%d\n", cdata->frmw.num_slots);
	else
		printf("N/A\n");
	printf("Firmware Slot 1 Read-Only:   ");
	if (cdata->oacs.firmware != 0)
		printf("%s\n", cdata->frmw.slot1_ro ? "Yes" : "No");
	else
		printf("N/A\n");
	printf("Per-Namespace SMART Log:     %s\n",
		cdata->lpa.ns_smart ? "Yes" : "No");
	printf("Error Log Page Entries:      %d\n", cdata->elpe+1);
	printf("Number of Power States:      %d\n", cdata->npss+1);
	printf("\n");

	printf("NVM Command Set Attributes\n");
	printf("==========================\n");
	printf("Submission Queue Entry Size\n");
	printf("  Max:                       %d\n", 1 << cdata->sqes.max);
	printf("  Min:                       %d\n", 1 << cdata->sqes.min);
	printf("Completion Queue Entry Size\n");
	printf("  Max:                       %d\n", 1 << cdata->cqes.max);
	printf("  Min:                       %d\n", 1 << cdata->cqes.min);
	printf("Number of Namespaces:        %d\n", cdata->nn);
	printf("Compare Command:             %s\n",
		cdata->oncs.compare ? "Supported" : "Not Supported");
	printf("Write Uncorrectable Command: %s\n",
		cdata->oncs.write_unc ? "Supported" : "Not Supported");
	printf("Dataset Management Command:  %s\n",
		cdata->oncs.dsm ? "Supported" : "Not Supported");
	printf("Volatile Write Cache:        %s\n",
		cdata->vwc.present ? "Present" : "Not Present");
}

static void
print_namespace(struct nvme_namespace_data *nsdata)
{
	uint32_t	i;

	printf("Size (in LBAs):              %lld (%lldM)\n",
		(long long)nsdata->nsze,
		(long long)nsdata->nsze / 1024 / 1024);
	printf("Capacity (in LBAs):          %lld (%lldM)\n",
		(long long)nsdata->ncap,
		(long long)nsdata->ncap / 1024 / 1024);
	printf("Utilization (in LBAs):       %lld (%lldM)\n",
		(long long)nsdata->nuse,
		(long long)nsdata->nuse / 1024 / 1024);
	printf("Thin Provisioning:           %s\n",
		nsdata->nsfeat.thin_prov ? "Supported" : "Not Supported");
	printf("Number of LBA Formats:       %d\n", nsdata->nlbaf+1);
	printf("Current LBA Format:          LBA Format #%02d\n",
		nsdata->flbas.format);
	for (i = 0; i <= nsdata->nlbaf; i++)
		printf("LBA Format #%02d: Data Size: %5d  Metadata Size: %5d\n",
		    i, 1 << nsdata->lbaf[i].lbads, nsdata->lbaf[i].ms);
}

static void
identify_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, IDENTIFY_USAGE);
	exit(1);
}

static void
identify_ctrlr(int argc, char *argv[])
{
	struct nvme_controller_data	cdata;
	int				ch, fd, hexflag = 0, hexlength;
	int				verboseflag = 0;

	while ((ch = getopt(argc, argv, "vx")) != -1) {
		switch ((char)ch) {
		case 'v':
			verboseflag = 1;
			break;
		case 'x':
			hexflag = 1;
			break;
		default:
			identify_usage();
		}
	}

	/* Check that a controller was specified. */
	if (optind >= argc)
		identify_usage();

	open_dev(argv[optind], &fd, 1, 1);
	read_controller_data(fd, &cdata);
	close(fd);

	if (hexflag == 1) {
		if (verboseflag == 1)
			hexlength = sizeof(struct nvme_controller_data);
		else
			hexlength = offsetof(struct nvme_controller_data,
			    reserved5);
		print_hex(&cdata, hexlength);
		exit(0);
	}

	if (verboseflag == 1) {
		fprintf(stderr, "-v not currently supported without -x\n");
		identify_usage();
	}

	print_controller(&cdata);
	exit(0);
}

static void
identify_ns(int argc, char *argv[])
{
	struct nvme_namespace_data	nsdata;
	char				path[64];
	int				ch, fd, hexflag = 0, hexlength, nsid;
	int				verboseflag = 0;

	while ((ch = getopt(argc, argv, "vx")) != -1) {
		switch ((char)ch) {
		case 'v':
			verboseflag = 1;
			break;
		case 'x':
			hexflag = 1;
			break;
		default:
			identify_usage();
		}
	}

	/* Check that a namespace was specified. */
	if (optind >= argc)
		identify_usage();

	/*
	 * Check if the specified device node exists before continuing.
	 *  This is a cleaner check for cases where the correct controller
	 *  is specified, but an invalid namespace on that controller.
	 */
	open_dev(argv[optind], &fd, 1, 1);
	close(fd);

	/*
	 * We send IDENTIFY commands to the controller, not the namespace,
	 *  since it is an admin cmd.  The namespace ID will be specified in
	 *  the IDENTIFY command itself.  So parse the namespace's device node
	 *  string to get the controller substring and namespace ID.
	 */
	parse_ns_str(argv[optind], path, &nsid);
	open_dev(path, &fd, 1, 1);
	read_namespace_data(fd, nsid, &nsdata);
	close(fd);

	if (hexflag == 1) {
		if (verboseflag == 1)
			hexlength = sizeof(struct nvme_namespace_data);
		else
			hexlength = offsetof(struct nvme_namespace_data,
			    reserved6);
		print_hex(&nsdata, hexlength);
		exit(0);
	}

	if (verboseflag == 1) {
		fprintf(stderr, "-v not currently supported without -x\n");
		identify_usage();
	}

	print_namespace(&nsdata);
	exit(0);
}

void
identify(int argc, char *argv[])
{
	char	*target;

	if (argc < 2)
		identify_usage();

	while (getopt(argc, argv, "vx") != -1) ;

	/* Check that a controller or namespace was specified. */
	if (optind >= argc)
		identify_usage();

	target = argv[optind];

	optreset = 1;
	optind = 1;

	/*
	 * If device node contains "ns", we consider it a namespace,
	 *  otherwise, consider it a controller.
	 */
	if (strstr(target, NVME_NS_PREFIX) == NULL)
		identify_ctrlr(argc, argv);
	else
		identify_ns(argc, argv);
}
