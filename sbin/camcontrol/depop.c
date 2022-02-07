/*-
 * Copyright (c) 2021 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */
/*
 * SCSI disk depop (head depopulation) support
 *
 * The standard defines 'storage elements' as the generic way of referring to a
 * disk drive head. Each storage element has an identifier and an active status.
 * The health of an element can be queried. Active elements may be removed from
 * service with a REMOVE ELEMENT AND TRUNCATE (RET) command. Inactive element
 * may be returned to service with a RESTORE ELEMENTS AND REBUILD (RER)
 * command. GET PHYSICAL ELEMENT STATUS (GPES) will return a list of elements,
 * their health, whether they are in service, how much capacity the element is
 * used for, etc.
 *
 * When a depop operation starts, the drive becomes format corrupt. No normal
 * I/O can be done to the drive and a limited number of CDBs will
 * succeed. Status can be obtained by either a TEST UNIT READY or a GPES
 * command. A drive reset will not stop a depop operation, but a power cycle
 * will. A failed depop operation will be reported when the next TEST UNIT READY
 * is sent to the drive. Drives that are format corrupt after an interrupted
 * operation need to have that operation repeated.
 *
 * 'depop' provides a wrapper around all these functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <camlib.h>
#include <scsi_wrap.h>
#include "camcontrol.h"

enum depop_action {
	DEPOP_NONE,
	DEPOP_LIST,
	DEPOP_RESTORE,
	DEPOP_REMOVE,
};

static int
depop_list(struct cam_device *device, int task_attr, int retry_count,
    int timeout, int verbosemode __unused)
{
	int error = 0;
	uint32_t dtors;
	struct scsi_get_physical_element_hdr *hdr;
	struct scsi_get_physical_element_descriptor *dtor_ptr;

	hdr = scsi_wrap_get_physical_element_status(device, task_attr, retry_count, timeout,
	    SCSI_GPES_FILTER_ALL | SCSI_GPES_REPORT_TYPE_PHYS, 1);
	if (hdr == NULL)
		errx(1, "scsi_wrap_get_physical_element_status returned an error");

	/*
	 * OK, we have the data, not report it out.
	 */
	dtor_ptr = (struct scsi_get_physical_element_descriptor *)(hdr + 1);
	dtors = scsi_4btoul(hdr->num_descriptors);
	printf("Elem ID    * Health Capacity\n");
	for (uint32_t i = 0; i < dtors; i++) {
		uint32_t id = scsi_4btoul(dtor_ptr[i].element_identifier);
		uint8_t ralwd = dtor_ptr[i].ralwd;
		uint8_t type = dtor_ptr[i].physical_element_type;
		uint8_t health = dtor_ptr[i].physical_element_health;
		uint64_t cap = scsi_8btou64(dtor_ptr[i].capacity);
		if (type != GPED_TYPE_STORAGE)
			printf("0x%08x -- type unknown %d\n", id, type);
		else
			printf("0x%08x %c 0x%02x   %jd\n", id, ralwd ? '*' : ' ', health, cap);
	}
	printf("* -- Element can be restored\n");

	free(hdr);
	return (error);
}

static int
depop_remove(struct cam_device *device, int task_attr, int retry_count,
    int timeout, int verbosemode __unused, uint32_t elem, uint64_t capacity)
{
	union ccb *ccb;
	int error = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("Can't allocate ccb");
		return (1);
	}
	scsi_remove_element_and_truncate(&ccb->csio,
	    retry_count,
	    NULL,
	    task_attr,
	    capacity,
	    elem,
	    SSD_FULL_SIZE,
	    timeout);
	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;
	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending GET PHYSICAL ELEMENT STATUS command");
		error = 1;
		goto out;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL,
				CAM_EPF_ALL, stderr);
		error = 1;
	}

out:
	cam_freeccb(ccb);
	return (error);
}

static int
depop_restore(struct cam_device *device, int task_attr, int retry_count,
    int timeout, int verbosemode __unused)
{
	union ccb *ccb;
	int error = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("Can't allocate ccb");
		return (1);
	}
	scsi_restore_elements_and_rebuild(&ccb->csio,
	    retry_count,
	    NULL,
	    task_attr,
	    SSD_FULL_SIZE,
	    timeout);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;
	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending GET PHYSICAL ELEMENT STATUS command");
		error = 1;
		goto out;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL,
				CAM_EPF_ALL, stderr);
		error = 1;
	}

out:
	cam_freeccb(ccb);
	return (error);
}

#define MUST_BE_NONE() \
	if (action != DEPOP_NONE) { \
		warnx("Use only one of -d, -l, or -r"); \
		error = 1; \
		goto bailout; \
	}

int
depop(struct cam_device *device, int argc, char **argv, char *combinedopt,
    int task_attr, int retry_count, int timeout, int verbosemode)
{
	int c;
	int action = DEPOP_NONE;
	char *endptr;
	int error = 0;
	uint32_t elem = 0;
	uint64_t capacity = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'c':
			capacity = strtoumax(optarg, &endptr, 0);
			if (*endptr != '\0') {
				warnx("Invalid capacity: %s", optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'e':
			elem = strtoul(optarg, &endptr, 0);
			if (*endptr != '\0') {
				warnx("Invalid element: %s", optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'd':
			MUST_BE_NONE();
			action = DEPOP_REMOVE;
			break;
		case 'l':
			MUST_BE_NONE();
			action  = DEPOP_LIST;
			break;
		case 'r':
			MUST_BE_NONE();
			action  = DEPOP_RESTORE;
			break;
		default:
			break;
		}
	}

	/*
	 * Compute a sane timeout if none given. 5 seconds for the list command
	 * and whatever the block device characteristics VPD says for other
	 * depop commands. If there's no value in that field, default to 1
	 * day. Experience has shown that these operations take the better part
	 * of a day to complete, so a 1 day timeout default seems appropriate.
	 */
	if (timeout == 0 && action != DEPOP_NONE) {
		if (action == DEPOP_LIST) {
			timeout = 5 * 1000;
		} else {
			struct scsi_vpd_block_device_characteristics *bdc;

			timeout = 24 * 60 * 60 * 1000;	/* 1 day */
			bdc = scsi_wrap_vpd_block_device_characteristics(device);
			if (bdc != NULL) {
				timeout = scsi_4btoul(bdc->depopulation_time);
			}
			free(bdc);
		}
	}

	switch (action) {
	case DEPOP_NONE:
		warnx("Must specify one of -d, -l, or -r");
		error = 1;
		break;
	case DEPOP_REMOVE:
		if (elem == 0 && capacity == 0) {
			warnx("Must specify at least one of -e and/or -c");
			error = 1;
			break;
		}
		error = depop_remove(device, task_attr, retry_count, timeout,
		    verbosemode, elem, capacity);
		break;
	case DEPOP_RESTORE:
		error = depop_restore(device, task_attr, retry_count, timeout,
		    verbosemode);
		break;
	case DEPOP_LIST:
		error = depop_list(device, task_attr, retry_count, timeout,
		    verbosemode);
		break;
	}

bailout:

	return (error);
}
