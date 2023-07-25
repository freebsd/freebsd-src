/*-
 * Copyright (c) 2021 Netflix, Inc.
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
 * Wrapper functions to make requests and get answers w/o managing the
 * details.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_pass.h>
#include <cam/scsi/scsi_message.h>
#include "camlib.h"
#include "scsi_wrap.h"

void *
scsi_wrap_get_physical_element_status(struct cam_device *device, int task_attr, int retry_count,
    int timeout, uint8_t report_type, uint32_t start_element)
{
	uint32_t allocation_length;
	union ccb *ccb = NULL;
	struct scsi_get_physical_element_hdr *hdr = NULL;
	uint32_t dtors;
	uint32_t reported;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("Can't allocate ccb");
		return (NULL);
	}

	/*
	 * Do the request up to twice. Once to get the length and once to get
	 * the data. We'll guess that 4096 is enough to almost always long
	 * enough since that's 127 entries and most drives have < 20 heads.  If
	 * by chance it's not, then we'll loop once as we'll then know the
	 * proper length.
	 */
	allocation_length = MAX(sizeof(*hdr), 4096);
again:
	free(hdr);
	hdr = calloc(allocation_length, 1);
	if (hdr == NULL) {
		warnx("Can't allocate memory for physical element list");
		return (NULL);
	}

	scsi_get_physical_element_status(&ccb->csio,
	    retry_count,
	    NULL,
	    task_attr,
	    (uint8_t *)hdr,
	    allocation_length,
	    report_type,
	    start_element,
	    SSD_FULL_SIZE,
	    timeout);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending GET PHYSICAL ELEMENT STATUS command");
		goto errout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL,
				CAM_EPF_ALL, stderr);
		goto errout;
	}

	dtors = scsi_4btoul(hdr->num_descriptors);
	reported  = scsi_4btoul(hdr->num_returned);
	if (dtors != 0 && dtors != reported) {
		/*
		 * Get all the data... in the future we may need to step through
		 * a long list, but so far all drives I've found fit into one
		 * response. A 4k transfer can do 128 heads and current designs
		 * have 16.
		 */
		allocation_length = dtors * sizeof(struct scsi_get_physical_element_descriptor) +
		    sizeof(*hdr);
		goto again;
	}
	cam_freeccb(ccb);
	return (hdr);
errout:
	cam_freeccb(ccb);
	free(hdr);
	return (NULL);
}

void *
scsi_wrap_inquiry(struct cam_device *device, uint32_t page, uint32_t length)
{
	union ccb *ccb;
	uint8_t *buf;

	ccb = cam_getccb(device);

	if (ccb == NULL)
		return (NULL);

	buf = malloc(length);

	if (buf == NULL) {
		cam_freeccb(ccb);
		return (NULL);
	}

	scsi_inquiry(&ccb->csio,
		     /*retries*/ 0,
		     /*cbfcnp*/ NULL,
		     /* tag_action */ MSG_SIMPLE_Q_TAG,
		     /* inq_buf */ (uint8_t *)buf,
		     /* inq_len */ length,
		     /* evpd */ 1,
		     /* page_code */ page,
		     /* sense_len */ SSD_FULL_SIZE,
		     /* timeout */ 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;
	// ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending INQUIRY command");
		cam_freeccb(ccb);
		free(buf);
		return (NULL);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		free(buf);
		buf = NULL;
	}
	cam_freeccb(ccb);
	return (buf);
}

struct scsi_vpd_block_device_characteristics *
scsi_wrap_vpd_block_device_characteristics(struct cam_device *device)
{

	return ((struct scsi_vpd_block_device_characteristics *)scsi_wrap_inquiry(
	    device, SVPD_BDC, sizeof(struct scsi_vpd_block_device_characteristics)));
}
