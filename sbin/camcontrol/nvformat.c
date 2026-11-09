/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2018-2019 Alexander Motin <mav@FreeBSD.org>
 * Copyright (C) 2021 Spectra Logic Corporation
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
/*
 * NVMe format support for camcontrol(8).
 */

#include <sys/ioctl.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/sbuf.h>
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <err.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_pass.h>
#include <dev/nvme/nvme.h>
#include <camlib.h>
#include "camcontrol.h"

static struct scsi_nv nvme_ses_settings[] = {
	{ "none", NVME_FORMAT_SES_NONE },
	{ "user", NVME_FORMAT_SES_USER },
	{ "crypto", NVME_FORMAT_SES_CRYPTO },
};

int
nvmeformat(struct cam_device *device, int argc, char **argv, char *combinedopt,
	   int retry_count, int timeout, int verbosemode, int err_recover)
{
	union ccb *ccb = NULL;
	struct nvme_controller_data cdata;
	struct nvme_namespace_data nsdata;
	char *end;
	int global_op = 0;
	int lbaf = -1, mset = -1, pi = -1, pil = -1, ses = -1;
	int quiet = 0, ycount = 0;
	int error = 0;
	int c;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}

	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->nvmeio);

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'e': {
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvme_ses_settings,
			    sizeof(nvme_ses_settings) /
			    sizeof(nvme_ses_settings[0]), optarg,
			    &table_entry, SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND)
				ses = nvme_ses_settings[table_entry].value;
			else if (status == SCSI_NV_AMBIGUOUS) {
				warnx("%s: ambiguous Secure Erase Setting %s",
				    __func__, optarg);
				error = 1;
				goto bailout;
			} else {
				ses = strtol(optarg, &end, 0);
				if ((*end != '\0')
				 || ((ses < 0) || (ses > 0x7))) {
					warnx("%s: invalid Secure Erase "
					    "setting %s", __func__, optarg);
					error = 1;
					goto bailout;
				}
			}
			break;
		}
		case 'G':
			global_op = 1;
			break;
		case 'f':
			lbaf = strtol(optarg, &end, 0);
			if (*end != '\0') {
				warnx("%s: invalid format argument %s",
				    __func__, optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'l':
			pil = strtol(optarg, &end, 0);
			if ((*end != '\0') || ((pil < 0) || (pil > 1))) {
				warnx("%s: invalid location argument %s",
				    __func__, optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'm':
			mset = strtol(optarg, &end, 0);
			if ((*end != '\0') || ((mset < 0) || (mset > 1))) {
				warnx("%s: invalid metadata setting %s",
				    __func__, optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'p':
			pi = strtol(optarg, &end, 0);
			if ((*end != '\0') || ((pi < 0) || (pi > 0x7))) {
				warnx("%s: invalid Protection Information "
				    "type %s", __func__, optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			warnx("%s: option -r is not supported for NVMe "
			    "devices", __func__);
			error = 1;
			goto bailout;
		case 'w':
			/*
			 * The NVMe format command does not complete until
			 * the format is finished, so there is nothing extra
			 * to wait for.
			 */
			break;
		case 'y':
			ycount++;
			break;
		}
	}

	bzero(&cdata, sizeof(cdata));
	error = nvme_get_data(device, CDAI_TYPE_NVME_CNTRL, &cdata,
	    sizeof(cdata));
	if (error != 0)
		goto bailout;

	if (((cdata.oacs >> NVME_CTRLR_DATA_OACS_FORMAT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_FORMAT_MASK) == 0) {
		warnx("%s: controller doesn't support format", __func__);
		error = 1;
		goto bailout;
	}

	if ((((cdata.fna >> NVME_CTRLR_DATA_FNA_CRYPTO_ERASE_SHIFT) &
	    NVME_CTRLR_DATA_FNA_CRYPTO_ERASE_MASK) == 0) && (ses == 2)){
		warnx("%s: controller doesn't support crypto erase", __func__);
		error = 1;
		goto bailout;
	}

	if (global_op == 0) {

		if ((cdata.fna >> NVME_CTRLR_DATA_FNA_FORMAT_ALL_SHIFT) &
		     NVME_CTRLR_DATA_FNA_FORMAT_ALL_MASK) {
			warnx("%s: controller does not support per-NS format, "
			    "use -G", __func__);
			error = 1;
			goto bailout;
		}
                if ((cdata.fna >> NVME_CTRLR_DATA_FNA_ERASE_ALL_SHIFT) &
                    NVME_CTRLR_DATA_FNA_ERASE_ALL_MASK) {
                        warnx("%s: controller does not support per-NS erase, "
			    "use -G", __func__);
			error = 1;
			goto bailout;
		}

		bzero(&nsdata, sizeof(nsdata));
		error = nvme_get_data(device, CDAI_TYPE_NVME_NS, &nsdata,
		    sizeof(nsdata));
		if (error != 0)
			goto bailout;

		if (lbaf < 0)
			lbaf = nvme_ns_data_format_index(&nsdata);

		if (lbaf > nsdata.nlbaf) {
			warnx("%s: LBA format %d is out of range", __func__,
			    lbaf);
			error = 1;
			goto bailout;
		}
		if (mset < 0) {
			mset = (nsdata.flbas >>
			    NVME_NS_DATA_FLBAS_EXTENDED_SHIFT)
			    & NVME_NS_DATA_FLBAS_EXTENDED_MASK;
		}
		if (pi < 0) {
			pi = (nsdata.dps >> NVME_NS_DATA_DPS_PIT_SHIFT) &
			    NVME_NS_DATA_DPS_PIT_MASK;
		}
		if (pil < 0) {
			pil = (nsdata.dps >> NVME_NS_DATA_DPS_MD_START_SHIFT)
			    & NVME_NS_DATA_DPS_MD_START_MASK;
		}
		if (ses < 0)
			ses = 0;
	} else {
		if (ses < 0)
			ses = 0;
		if (lbaf < 0)
			lbaf = 0;
		else if (lbaf > 63) {
			warnx("%s: LBA format %d is out of range", __func__,
			    lbaf);
			error = 1;
			goto bailout;
		}
		if (mset < 0)
			mset = 0;
		if (pi < 0)
			pi = 0;
		if (pil < 0)
			pil = 0;
	}

	if (quiet == 0 && ycount == 0) {
		char product[41];

		nvme_strvis((uint8_t *)product, cdata.mn, sizeof(product),
		    sizeof(cdata.mn));
		fprintf(stdout, "You are about to REMOVE ALL DATA from %s on "
		    "the following device:\n<%s>\n", (global_op != 0) ?
		    "ALL NAMESPACES" : "the namespace", product);
	}

	if (ycount == 0) {
		if (!get_confirmation()) {
			error = 1;
			goto bailout;
		}
	}

	nvme_format(/*nvmeio*/ &ccb->nvmeio,
		    /*retries*/ retry_count,
		    /*cbfcnp*/ NULL,
		    /*nsid*/ global_op ? NVME_GLOBAL_NAMESPACE_TAG :
					 device->target_lun,
		    /*ses*/ ses,
		    /*pil*/ pil,
		    /*pi*/ pi,
		    /*mset*/ mset,
		    /*lbaf*/ lbaf,
		    /*
		     * A cryptographic erase just changes the media
		     * encryption key, so it should be quick.  A user data
		     * erase or a plain format can take a long time.
		     */
		    /*timeout*/ timeout ? timeout :
		    (ses == NVME_FORMAT_SES_CRYPTO ?
		    (30 * 1000) : (30 * 60 * 1000)));

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending NVMe Format command");
		error = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (verbosemode != 0) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		error = 1;
		goto bailout;
	}

bailout:
	return (error);
}
