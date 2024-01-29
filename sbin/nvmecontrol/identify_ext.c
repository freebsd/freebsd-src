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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"
#include "nvmecontrol_ext.h"

void
nvme_print_controller(struct nvme_controller_data *cdata)
{
	uint8_t str[128];
	char cbuf[UINT128_DIG + 1];
	uint16_t oncs, oacs;
	uint8_t compare, write_unc, dsm, t;
	uint8_t security, fmt, fw, nsmgmt;
	uint8_t	fw_slot1_ro, fw_num_slots;
	uint8_t ns_smart;
	uint8_t sqes_max, sqes_min;
	uint8_t cqes_max, cqes_min;
	uint8_t fwug;

	oncs = cdata->oncs;
	compare = NVMEV(NVME_CTRLR_DATA_ONCS_COMPARE, oncs);
	write_unc = NVMEV(NVME_CTRLR_DATA_ONCS_WRITE_UNC, oncs);
	dsm = NVMEV(NVME_CTRLR_DATA_ONCS_DSM, oncs);

	oacs = cdata->oacs;
	security = NVMEV(NVME_CTRLR_DATA_OACS_SECURITY, oacs);
	fmt = NVMEV(NVME_CTRLR_DATA_OACS_FORMAT, oacs);
	fw = NVMEV(NVME_CTRLR_DATA_OACS_FIRMWARE, oacs);
	nsmgmt = NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, oacs);

	fw_num_slots = NVMEV(NVME_CTRLR_DATA_FRMW_NUM_SLOTS, cdata->frmw);
	fw_slot1_ro = NVMEV(NVME_CTRLR_DATA_FRMW_SLOT1_RO, cdata->frmw);
	fwug = cdata->fwug;

	ns_smart = NVMEV(NVME_CTRLR_DATA_LPA_NS_SMART, cdata->lpa);

	sqes_min = NVMEV(NVME_CTRLR_DATA_SQES_MIN, cdata->sqes);
	sqes_max = NVMEV(NVME_CTRLR_DATA_SQES_MAX, cdata->sqes);

	cqes_min = NVMEV(NVME_CTRLR_DATA_CQES_MIN, cdata->cqes);
	cqes_max = NVMEV(NVME_CTRLR_DATA_CQES_MAX, cdata->cqes);

	printf("Controller Capabilities/Features\n");
	printf("================================\n");
	printf("Vendor ID:                   %04x\n", cdata->vid);
	printf("Subsystem Vendor ID:         %04x\n", cdata->ssvid);
	nvme_strvis(str, cdata->sn, sizeof(str), NVME_SERIAL_NUMBER_LENGTH);
	printf("Serial Number:               %s\n", str);
	nvme_strvis(str, cdata->mn, sizeof(str), NVME_MODEL_NUMBER_LENGTH);
	printf("Model Number:                %s\n", str);
	nvme_strvis(str, cdata->fr, sizeof(str), NVME_FIRMWARE_REVISION_LENGTH);
	printf("Firmware Version:            %s\n", str);
	printf("Recommended Arb Burst:       %d\n", cdata->rab);
	printf("IEEE OUI Identifier:         %02x %02x %02x\n",
		cdata->ieee[2], cdata->ieee[1], cdata->ieee[0]);
	printf("Multi-Path I/O Capabilities: %s%s%s%s%s\n",
	    (cdata->mic == 0) ? "Not Supported" : "",
	    NVMEV(NVME_CTRLR_DATA_MIC_ANAR, cdata->mic) != 0 ?
	    "Asymmetric, " : "",
	    NVMEV(NVME_CTRLR_DATA_MIC_SRIOVVF, cdata->mic) != 0 ?
	    "SR-IOV VF, " : "",
	    NVMEV(NVME_CTRLR_DATA_MIC_MCTRLRS, cdata->mic) != 0 ?
	    "Multiple controllers, " : "",
	    NVMEV(NVME_CTRLR_DATA_MIC_MPORTS, cdata->mic) != 0 ?
	    "Multiple ports" : "");
	/* TODO: Use CAP.MPSMIN to determine true memory page size. */
	printf("Max Data Transfer Size:      ");
	if (cdata->mdts == 0)
		printf("Unlimited\n");
	else
		printf("%ld bytes\n", PAGE_SIZE * (1L << cdata->mdts));
	printf("Sanitize Crypto Erase:       %s\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_CES, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	printf("Sanitize Block Erase:        %s\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_BES, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	printf("Sanitize Overwrite:          %s\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_OWS, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	printf("Sanitize NDI:                %s\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_NDI, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	printf("Sanitize NODMMAS:            ");
	switch (NVMEV(NVME_CTRLR_DATA_SANICAP_NODMMAS, cdata->sanicap)) {
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_UNDEF:
		printf("Undefined\n");
		break;
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_NO:
		printf("No\n");
		break;
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_YES:
		printf("Yes\n");
		break;
	default:
		printf("Unknown\n");
		break;
	}
	printf("Controller ID:               0x%04x\n", cdata->ctrlr_id);
	printf("Version:                     %d.%d.%d\n",
	    (cdata->ver >> 16) & 0xffff, (cdata->ver >> 8) & 0xff,
	    cdata->ver & 0xff);
	printf("\n");

	printf("Admin Command Set Attributes\n");
	printf("============================\n");
	printf("Security Send/Receive:       %s\n",
		security ? "Supported" : "Not Supported");
	printf("Format NVM:                  %s\n",
		fmt ? "Supported" : "Not Supported");
	printf("Firmware Activate/Download:  %s\n",
		fw ? "Supported" : "Not Supported");
	printf("Namespace Management:        %s\n",
		nsmgmt ? "Supported" : "Not Supported");
	printf("Device Self-test:            %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_SELFTEST, oacs) != 0 ? "" : "Not ");
	printf("Directives:                  %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_DIRECTIVES, oacs) != 0 ? "" : "Not ");
	printf("NVMe-MI Send/Receive:        %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_NVMEMI, oacs) != 0 ? "" : "Not ");
	printf("Virtualization Management:   %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_VM, oacs) != 0 ? "" : "Not ");
	printf("Doorbell Buffer Config:      %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_DBBUFFER, oacs) != 0 ? "" : "Not ");
	printf("Get LBA Status:              %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_GETLBA, oacs) != 0 ? "" : "Not ");
	printf("Sanitize:                    ");
	if (cdata->sanicap != 0) {
		printf("%s%s%s\n",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_CES, cdata->sanicap) != 0 ?
		    "crypto, " : "",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_BES, cdata->sanicap) != 0 ?
		    "block, " : "",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_OWS, cdata->sanicap) != 0 ?
		    "overwrite" : "");
	} else {
		printf("Not Supported\n");
	}
	printf("Abort Command Limit:         %d\n", cdata->acl+1);
	printf("Async Event Request Limit:   %d\n", cdata->aerl+1);
	printf("Number of Firmware Slots:    %d\n", fw_num_slots);
	printf("Firmware Slot 1 Read-Only:   %s\n", fw_slot1_ro ? "Yes" : "No");
	printf("Per-Namespace SMART Log:     %s\n",
		ns_smart ? "Yes" : "No");
	printf("Error Log Page Entries:      %d\n", cdata->elpe+1);
	printf("Number of Power States:      %d\n", cdata->npss+1);
	if (cdata->ver >= 0x010200) {
		printf("Total NVM Capacity:          %s bytes\n",
		    uint128_to_str(to128(cdata->untncap.tnvmcap),
		    cbuf, sizeof(cbuf)));
		printf("Unallocated NVM Capacity:    %s bytes\n",
		    uint128_to_str(to128(cdata->untncap.unvmcap),
		    cbuf, sizeof(cbuf)));
	}
	printf("Firmware Update Granularity: %02x ", fwug);
	if (fwug == 0)
		printf("(Not Reported)\n");
	else if (fwug == 0xFF)
		printf("(No Granularity)\n");
	else
		printf("(%d bytes)\n", ((uint32_t)fwug << 12));
	printf("Host Buffer Preferred Size:  %llu bytes\n",
	    (long long unsigned)cdata->hmpre * 4096);
	printf("Host Buffer Minimum Size:    %llu bytes\n",
	    (long long unsigned)cdata->hmmin * 4096);

	printf("\n");
	printf("NVM Command Set Attributes\n");
	printf("==========================\n");
	printf("Submission Queue Entry Size\n");
	printf("  Max:                       %d\n", 1 << sqes_max);
	printf("  Min:                       %d\n", 1 << sqes_min);
	printf("Completion Queue Entry Size\n");
	printf("  Max:                       %d\n", 1 << cqes_max);
	printf("  Min:                       %d\n", 1 << cqes_min);
	printf("Number of Namespaces:        %d\n", cdata->nn);
	printf("Compare Command:             %s\n",
		compare ? "Supported" : "Not Supported");
	printf("Write Uncorrectable Command: %s\n",
		write_unc ? "Supported" : "Not Supported");
	printf("Dataset Management Command:  %s\n",
		dsm ? "Supported" : "Not Supported");
	printf("Write Zeroes Command:        %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_WRZERO, oncs) != 0 ? "" : "Not ");
	printf("Save Features:               %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_SAVEFEAT, oncs) != 0 ? "" : "Not ");
	printf("Reservations:                %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_RESERV, oncs) != 0 ? "" : "Not ");
	printf("Timestamp feature:           %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_TIMESTAMP, oncs) != 0 ? "" : "Not ");
	printf("Verify feature:              %sSupported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_VERIFY, oncs) != 0 ? "" : "Not ");
	printf("Fused Operation Support:     %s%s\n",
	    (cdata->fuses == 0) ? "Not Supported" : "",
	    NVMEV(NVME_CTRLR_DATA_FUSES_CNW, cdata->fuses) != 0 ?
	    "Compare and Write" : "");
	printf("Format NVM Attributes:       %s%s Erase, %s Format\n",
	    NVMEV(NVME_CTRLR_DATA_FNA_CRYPTO_ERASE, cdata->fna) != 0 ?
	    "Crypto Erase, " : "",
	    NVMEV(NVME_CTRLR_DATA_FNA_ERASE_ALL, cdata->fna) != 0 ?
	    "All-NVM" : "Per-NS",
	    NVMEV(NVME_CTRLR_DATA_FNA_FORMAT_ALL, cdata->fna) != 0 ?
	    "All-NVM" : "Per-NS");
	t = NVMEV(NVME_CTRLR_DATA_VWC_ALL, cdata->vwc);
	printf("Volatile Write Cache:        %s%s\n",
	    NVMEV(NVME_CTRLR_DATA_VWC_PRESENT, cdata->vwc) != 0 ?
	    "Present" : "Not Present",
	    (t == NVME_CTRLR_DATA_VWC_ALL_NO) ? ", no flush all" :
	    (t == NVME_CTRLR_DATA_VWC_ALL_YES) ? ", flush all" : "");

	if (cdata->ver >= 0x010201)
		printf("\nNVM Subsystem Name:          %.256s\n", cdata->subnqn);
}
