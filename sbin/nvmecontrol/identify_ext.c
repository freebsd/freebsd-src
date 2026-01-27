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
#include <libxo/xo.h>
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

	xo_emit("{T:Controller Capabilities/Features}\n");
	xo_emit("{T:================================}\n");
	xo_emit("{Lc:Vendor ID}{P:                   }"
		"{:vendor-id/%04x}\n", cdata->vid);
	xo_emit("{Lc:Subsystem Vendor ID}{P:         }"
		"{:subsystem-vendor-id/%04x}\n", cdata->ssvid);
	nvme_strvis(str, cdata->sn, sizeof(str), NVME_SERIAL_NUMBER_LENGTH);
	xo_emit("{Lc:Serial Number}{P:               }{:serial-number/%s}\n", str);
	nvme_strvis(str, cdata->mn, sizeof(str), NVME_MODEL_NUMBER_LENGTH);
	xo_emit("{Lc:Model Number}{P:                }{:model-number/%s}\n", str);
	nvme_strvis(str, cdata->fr, sizeof(str), NVME_FIRMWARE_REVISION_LENGTH);
	xo_emit("{Lc:Firmware Version}{P:            }"
		"{:firmware-version/%s}\n", str);
	xo_emit("{Lc:Recommended Arb Burst}{P:       }"
		"{:arb-burst/%d}\n", cdata->rab);
	xo_emit("{Lc:IEEE OUI Identifier}{P:         }{:ieee-out-id-2/%02x}{P: }"
		"{:ieee-out-id-1/%02x}{P: }{:ieee-out-id-0/%02x}\n",
		cdata->ieee[2], cdata->ieee[1], cdata->ieee[0]);
	xo_emit("{Lc:Multi-Path I/O Capabilities}{P: }{:multi-path-io-support/%s}"
		"{:multi-path-io-assymetric/%s}{:multi-path-io-sr-ivo/%s}"
		"{:multi-path-io-multi-controllers/%s}"
		"{:multi-path-io-multi-ports/%s}\n",
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
	xo_emit("{Lc:Max Data Transfer Size}{P:      }");
	if (cdata->mdts == 0)
		xo_emit("Unlimited\n");
	else
		xo_emit("{:max-data-transfer-size/%ld} bytes\n",
			PAGE_SIZE * (1L << cdata->mdts));
	xo_emit("{Lc:Sanitize Crypto Erase}{P:       }{:sanitize-crypto/%s}\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_CES, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	xo_emit("{Lc:Sanitize Block Erase}{P:        }{:sanitize-block/%s}\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_BES, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	xo_emit("{Lc:Sanitize Overwrite}{P:          }{:sanitize-overwrite/%s}\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_OWS, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	xo_emit("{Lc:Sanitize NDI}{P:                }{:sanitize-ndi/%s}\n",
	    NVMEV(NVME_CTRLR_DATA_SANICAP_NDI, cdata->sanicap) != 0 ?
	    "Supported" : "Not Supported");
	xo_emit("{Lc:Sanitize NODMMAS}{P:            }");
	switch (NVMEV(NVME_CTRLR_DATA_SANICAP_NODMMAS, cdata->sanicap)) {
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_UNDEF:
		xo_emit("Undefined\n");
		break;
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_NO:
		xo_emit("No\n");
		break;
	case NVME_CTRLR_DATA_SANICAP_NODMMAS_YES:
		xo_emit("Yes\n");
		break;
	default:
		xo_emit("Unknown\n");
		break;
	}
	xo_emit("{Lc:Controller ID}{P:               }0x{:controller-id/%04x}\n",
		cdata->ctrlr_id);
	xo_emit("{Lc:Version}{P:                     }{:controller-version-2/%d}."
		"{:controller-version-2/%d}.{:controller-version-0/%d}\n",
	    (cdata->ver >> 16) & 0xffff, (cdata->ver >> 8) & 0xff,
	    cdata->ver & 0xff);
	xo_emit("Traffic Based Keep Alive:    {:traffic-keep-alive/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_CTRATT_TBKAS, cdata->ctratt) ? "" : "Not ");
	xo_emit("Controller Type:             ");
	switch (cdata->cntrltype) {
	case 0:
		xo_emit("Not Reported\n");
		break;
	case 1:
		xo_emit("I/O Controller\n");
		break;
	case 2:
		xo_emit("Discovery Controller\n");
		break;
	case 3:
		xo_emit("Administrative Controller\n");
		break;
	default:
		xo_emit("{:controller-type-reserved/%d} (Reserved)\n",
			cdata->cntrltype);
		break;
	}
	xo_emit("Keep Alive Timer{P:             }");
	if (cdata->kas == 0)
		xo_emit("Not Supported\n");
	else
		xo_emit("{:keep-alive-timer/%u} ms granularity\n", cdata->kas * 100);
	xo_emit("Maximum Outstanding Commands ");
	if (cdata->maxcmd == 0)
		xo_emit("Not Specified\n");
	else
		xo_emit("{:max-outstanding-commands/%u}\n", cdata->maxcmd);
	xo_emit("\n");

	xo_emit("{T:Admin Command Set Attributes}\n");
	xo_emit("{T:============================}\n");
	xo_emit("{Lc:Security Send/Receive}{P:       }"
		"{:security-send-receive/%s}\n",
		security ? "Supported" : "Not Supported");
	xo_emit("{Lc:Format NVM}{P:                  }{:format-nvm/%s}\n",
		fmt ? "Supported" : "Not Supported");
	xo_emit("{Lc:Firmware Activate/Download}{P:  }{:firmware-activate/%s}\n",
		fw ? "Supported" : "Not Supported");
	xo_emit("{Lc:Namespace Management}{P:        }{:namespace-management/%s}\n",
		nsmgmt ? "Supported" : "Not Supported");
	xo_emit("{Lc:Device Self-test}{P:            }"
		"{:device-self-test/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_SELFTEST, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Directives}{P:                  }{:directives/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_DIRECTIVES, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:NVMe-MI Send/Receive}{P:        }"
		"{:nvme-mi-send-receive/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_NVMEMI, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Virtualization Management}{P:   }"
		"{:virtual-management/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_VM, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Doorbell Buffer Config}{P:      }"
		"{:doorbell-buffer-config/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_DBBUFFER, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Get LBA Status}{P:              }{:lba-status/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_OACS_GETLBA, oacs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Sanitize}{P:                    }");
	if (cdata->sanicap != 0) {
		xo_emit("{:sanitize-crypto/%s}{:sanitize-block/%s}"
			"{:sanitize-overwrote/%s}\n",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_CES, cdata->sanicap) != 0 ?
		    "crypto, " : "",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_BES, cdata->sanicap) != 0 ?
		    "block, " : "",
		    NVMEV(NVME_CTRLR_DATA_SANICAP_OWS, cdata->sanicap) != 0 ?
		    "overwrite" : "");
	} else {
		xo_emit("Not Supported\n");
	}
	xo_emit("{Lc:Abort Command Limit}{P:         }"
		"{:abort-command-limit/%d}\n", cdata->acl+1);
	xo_emit("{Lc:Async Event Request Limit}{P:   }"
		"{:async-event-request-limit/%d}\n", cdata->aerl+1);
	xo_emit("{Lc:Number of Firmware Slots}{P:    }{:firmware-slots/%d}\n",
		fw_num_slots);
	xo_emit("{Lc:Firmware Slot 1 Read-Only}{P:   }{:firmware-slot-1-ro/%s}\n",
		fw_slot1_ro ? "Yes" : "No");
	xo_emit("{Lc:Per-Namespace SMART Log}{P:     }"
		"{:per-namespace-smart-log/%s}\n", ns_smart ? "Yes" : "No");
	xo_emit("{Lc:Error Log Page Entries}{P:      }"
		"{:error-log-page-entries/%d}\n", cdata->elpe+1);
	xo_emit("{Lc:Number of Power States}{P:      }"
		"{:number-of-power-states/%d}\n", cdata->npss+1);
	if (cdata->ver >= 0x010200) {
		xo_emit("{Lc:Total NVM Capacity}{P:          }"
			"{:nvm-capacity-total/%s} bytes\n",
		    uint128_to_str(to128(cdata->untncap.tnvmcap),
		    cbuf, sizeof(cbuf)));
		xo_emit("{Lc:Unallocated NVM Capacity}{P:    }"
			"{:nvm-capacity-unallocated/%s} bytes\n",
		    uint128_to_str(to128(cdata->untncap.unvmcap),
		    cbuf, sizeof(cbuf)));
	}
	xo_emit("{Lc:Firmware Update Granularity}{P: }"
		"{:firmware-update-granul/%02x} ", fwug);
	if (fwug == 0)
		xo_emit("(Not Reported)\n");
	else if (fwug == 0xFF)
		xo_emit("(No Granularity)\n");
	else
		xo_emit("({:firmware-update-granul-bytes/%d} bytes)\n",
			((uint32_t)fwug << 12));
	xo_emit("{Lc:Host Buffer Preferred Size}{P:  }"
		"{:host-buffer-size-preferred/%llu} bytes\n",
	    (long long unsigned)cdata->hmpre * 4096);
	xo_emit("{Lc:Host Buffer Minimum Size}{P:    }"
		"{:host-buffer-size-minimun/%llu} bytes\n",
	    (long long unsigned)cdata->hmmin * 4096);

	xo_emit("\n");
	xo_emit("{T:NVM Command Set Attributes}\n");
	xo_emit("{T:==========================}\n");
	xo_emit("Submission Queue Entry Size\n");
	xo_emit("{:P  }{Lc:Max}{P:                       }"
		"{:nvm-submission-queue-max/%d}\n", 1 << sqes_max);
	xo_emit("{:P  }{Lc:Min}{P:                       }"
		"{:nvm-submission-queue-min/%d}\n", 1 << sqes_min);
	xo_emit("Completion Queue Entry Size\n");
	xo_emit("{:P  }{Lc:Max}{P:                       }"
		"{:nvm-completion-max/%d}\n", 1 << cqes_max);
	xo_emit("{:P  }{Lc:Min}{P:                       }"
		"{:nvm-completion-min/%d}\n", 1 << cqes_min);
	xo_emit("{Lc:Number of Namespaces}{P:        }"
		"{:nvm-namespace-number/%d}\n", cdata->nn);
	xo_emit("{Lc:Compare Command}{P:             }"
		"{:nvm-compare/%s}\n",
		compare ? "Supported" : "Not Supported");
	xo_emit("{Lc:Write Uncorrectable Command}{P: }"
		"{:nvm-write-uncorrectable/%s}\n",
		write_unc ? "Supported" : "Not Supported");
	xo_emit("{Lc:Dataset Management Command}{P:  }"
		"{:nvm-dataset-management/%s}\n",
		dsm ? "Supported" : "Not Supported");
	xo_emit("{Lc:Write Zeroes Command}{P:        }"
		"{:nvm-write-zeroes/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_WRZERO, oncs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Save Features}{P:               }"
		"{:nvm-save-features/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_SAVEFEAT, oncs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Reservations}{P:                }"
		"{:nvm-reservations/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_RESERV, oncs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Timestamp feature}{P:           }"
		"{:nvm-timestamp/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_TIMESTAMP, oncs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Verify feature}{P:              }"
		"{:nvm-verify/%s}Supported\n",
	    NVMEV(NVME_CTRLR_DATA_ONCS_VERIFY, oncs) != 0 ? "" : "Not ");
	xo_emit("{Lc:Fused Operation Support}{P:     }"
		"{:nvm-fused-operation/%s}{:nvme-fused-compare-write/%s}\n",
	    (cdata->fuses == 0) ? "Not Supported" : "",
	    NVMEV(NVME_CTRLR_DATA_FUSES_CNW, cdata->fuses) != 0 ?
	    "Compare and Write" : "");
	xo_emit("{Lc:Format NVM Attributes}{P:       }"
		"{:nvm-crypto/%s}{:nvm-erase-all/%s} "
		"Erase, {:nvm-format-all/%s} Format\n",
	    NVMEV(NVME_CTRLR_DATA_FNA_CRYPTO_ERASE, cdata->fna) != 0 ?
	    "Crypto Erase, " : "",
	    NVMEV(NVME_CTRLR_DATA_FNA_ERASE_ALL, cdata->fna) != 0 ?
	    "All-NVM" : "Per-NS",
	    NVMEV(NVME_CTRLR_DATA_FNA_FORMAT_ALL, cdata->fna) != 0 ?
	    "All-NVM" : "Per-NS");
	t = NVMEV(NVME_CTRLR_DATA_VWC_ALL, cdata->vwc);
	xo_emit("{Lc:Volatile Write Cache}{P:        }{:volatile-write-present/%s}"
		"{:volatile-write-flush/%s}\n",
	    NVMEV(NVME_CTRLR_DATA_VWC_PRESENT, cdata->vwc) != 0 ?
	    "Present" : "Not Present",
	    (t == NVME_CTRLR_DATA_VWC_ALL_NO) ? ", no flush all" :
	    (t == NVME_CTRLR_DATA_VWC_ALL_YES) ? ", flush all" : "");

	if (cdata->ver >= 0x010201)
		xo_emit("\n{Lc:NVM Subsystem Name}{P:          }"
			"{:nvm-subsystem-name/%.256s}\n", cdata->subnqn);

	if (cdata->ioccsz != 0) {
		xo_emit("\n");
		xo_emit("{T:Fabrics Attributes}\n");
		xo_emit("{T:==================}\n");
		xo_emit("{Lc:I/O Command Capsule Size}{P:    }"
			"{:fabrics-command-capsule/%d} bytes\n", cdata->ioccsz * 16);
		xo_emit("{Lc:I/O Response Capsule Size}{P:   }"
			"{:fabrics-response-capsule/%d} bytes\n", cdata->iorcsz * 16);
		xo_emit("{Lc:In Capsule Data Offset}{P:      }"
			"{:fabrics-capsule-offset/%d} bytes\n", cdata->icdoff * 16);
		xo_emit("{Lc:Controller Model}{P:            }"
			"{:fabrics-controller-model/%s}\n",
		    (cdata->fcatt & 1) == 0 ? "Dynamic" : "Static");
		xo_emit("{Lc:Max SGL Descriptors}{P:         }");
		if (cdata->msdbd == 0)
			xo_emit("Unlimited\n");
		else
			xo_emit("{:fabrics-max-sgl/%d}\n", cdata->msdbd);
		xo_emit("{Lc:Disconnect of I/O Queues}{P:    }"
			"{:fabrics-queues-disconnect/%s}Supported\n",
		    (cdata->ofcs & 1) == 1 ? "" : "Not ");
	}
}
