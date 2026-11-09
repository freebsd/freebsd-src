/*-
 * Copyright (c) 2020-2021 Spectra Logic Corporation
 * All rights reserved.
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
 * Authors: Reid Linnemann      (Spectra Logic Corporation)
 */
/*
 * NVMe Feature support for camcontrol(8).
 */

#include <sys/endian.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/sbuf.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <uuid.h>

#include <cam/cam.h>
#include <cam/nvme/nvme_all.h>
#include <cam/cam_ccb.h>
#include <dev/nvme/nvme.h>
#include <camlib.h>

#include "camcontrol.h"


#include <cam/scsi/scsi_all.h>

static struct scsi_nv nvfeature_actions[] = {
	{"get", NVME_OPC_GET_FEATURES},
	{"set", NVME_OPC_SET_FEATURES}
};

static struct scsi_nv nvfeature_features[] = {
	{"arbitration", NVME_FEAT_ARBITRATION},
	{"powermgmt", NVME_FEAT_POWER_MANAGEMENT},
	{"lbaranget", NVME_FEAT_LBA_RANGE_TYPE},
	{"tempthresh", NVME_FEAT_TEMPERATURE_THRESHOLD},
	{"erecover", NVME_FEAT_ERROR_RECOVERY},
	{"volatilewc", NVME_FEAT_VOLATILE_WRITE_CACHE},
	{"nqueues", NVME_FEAT_NUMBER_OF_QUEUES},
	{"intrcoalesc", NVME_FEAT_INTERRUPT_COALESCING},
	{"intrvectcfg", NVME_FEAT_INTERRUPT_VECTOR_CONFIGURATION},
	{"wratomicity", NVME_FEAT_WRITE_ATOMICITY},
	{"asyncevcfg", NVME_FEAT_ASYNC_EVENT_CONFIGURATION},
	{"autopwrstate", NVME_FEAT_AUTONOMOUS_POWER_STATE_TRANSITION},
	{"hostmem", NVME_FEAT_HOST_MEMORY_BUFFER},
	{"timestamp", NVME_FEAT_TIMESTAMP},
	{"keepalive", NVME_FEAT_KEEP_ALIVE_TIMER},
	{"hctherm", NVME_FEAT_HOST_CONTROLLED_THERMAL_MGMT},
	{"noppwrstate", NVME_FEAT_NON_OP_POWER_STATE_CONFIG},
	{"rdreclvl", NVME_FEAT_READ_RECOVERY_LEVEL_CONFIG},
	{"prdlatencycfg", NVME_FEAT_PREDICTABLE_LATENCY_MODE_CONFIG},
	{"prdlatencywin", NVME_FEAT_PREDICTABLE_LATENCY_MODE_WINDOW},
	{"lbastatinfattrs", NVME_FEAT_LBA_STATUS_INFORMATION_ATTRIBUTES},
	{"hostbehavior", NVME_FEAT_HOST_BEHAVIOR_SUPPORT},
	{"sanitizecfg", NVME_FEAT_SANITIZE_CONFIG},
	{"egroupevcfg", NVME_FEAT_ENDURANCE_GROUP_EVENT_CONFIGURATION},
	{"swprogmarker", NVME_FEAT_SOFTWARE_PROGRESS_MARKER},
	{"hostid", NVME_FEAT_HOST_IDENTIFIER},
	{"rsvnotmask", NVME_FEAT_RESERVATION_NOTIFICATION_MASK},
	{"rsvpersist", NVME_FEAT_RESERVATION_PERSISTENCE},
	{"nswpcfg", NVME_FEAT_NAMESPACE_WRITE_PROTECTION_CONFIG},
	{"raw", 0} /* raw feature on commandline */
};


/*
 * If bit 4 is set to '1' in the Optional NVM Command Support (ONCS) field
 * of the Identify Controller data then feature get supports these
 * select values
 */
enum nvme_feature_select {
	NVME_FEAT_SEL_CURRENT		= 0x0,
	NVME_FEAT_SEL_DEFAULT		= 0x1,
	NVME_FEAT_SEL_SAVED		= 0x2,
	NVME_FEAT_SEL_CAPABILITIES	= 0x3,
};

static const char *nvfeature_select_to_str[] = {
	[NVME_FEAT_SEL_CURRENT] = "current",
	[NVME_FEAT_SEL_DEFAULT] = "default",
	[NVME_FEAT_SEL_SAVED] = "saved",
	[NVME_FEAT_SEL_CAPABILITIES] = "capabilities",
};

/*
 * Get feature command completion when select is NVME_FEAT_SELECT_CAPABILITIES
 */
enum nvme_feature_capabilities {
	NVME_FEAT_CAPS_SAVEABLE		= 0x1,
	NVME_FEAT_CAPS_NS_SPECIFIC 	= 0x2,
	NVME_FEAT_CAPS_CHANGEABLE	= 0x4,
};

static const char *nvfeature_caps_to_str[] = {
	[NVME_FEAT_CAPS_SAVEABLE] = "saveable",
	[NVME_FEAT_CAPS_NS_SPECIFIC] = "ns-specific",
	[NVME_FEAT_CAPS_CHANGEABLE] = "changeable",
};

static int
nvfeature_validate_select(int sel)
{
	switch (sel) {
	case NVME_FEAT_SEL_CURRENT:
	case NVME_FEAT_SEL_DEFAULT:
	case NVME_FEAT_SEL_SAVED:
	case NVME_FEAT_SEL_CAPABILITIES:
		return 1;

	default:
		warnx("%s: feature select value 0x%x must be one of "
			"0x%x(%s), 0x%x(%s), "
			"0x%x(%s), or 0x%x(%s)", __func__, sel,
			NVME_FEAT_SEL_CURRENT,
			nvfeature_select_to_str[NVME_FEAT_SEL_CURRENT],
			NVME_FEAT_SEL_DEFAULT,
			nvfeature_select_to_str[NVME_FEAT_SEL_DEFAULT],
			NVME_FEAT_SEL_SAVED,
			nvfeature_select_to_str[NVME_FEAT_SEL_SAVED],
			NVME_FEAT_SEL_CAPABILITIES,
			nvfeature_select_to_str[NVME_FEAT_SEL_CAPABILITIES]);
		break;
	}
	return 0;
}

static int
nvfeature_validate_save(int save)
{
	if (save < 0 || save > 1) {
		warnx("%s: feature save value must either 0 or 1", __func__);
		return 0;
	}
	return 1;
}

static int
nvfeature_print_caps(union ccb *ccb, const char *msg)
{
	uint32_t  cdw0 = le32toh(ccb->nvmeio.cpl.cdw0);
	int p = 0;
	int b = 1;

	printf("%s  0x%.4x", msg, cdw0);
	if (cdw0 & NVME_FEAT_CAPS_CHANGEABLE) {
		printf("%s%s",
			b ? " [" : "",
			nvfeature_caps_to_str[NVME_FEAT_CAPS_CHANGEABLE]);
		p++;
		b = 0;
	}
	if (cdw0 & NVME_FEAT_CAPS_NS_SPECIFIC) {
		printf("%s%s%s",
			b ? " [" : "",
			p ? " | " : "",
			nvfeature_caps_to_str[NVME_FEAT_CAPS_NS_SPECIFIC]);
			p++;
			b = 0;
	}
	if (cdw0 & NVME_FEAT_CAPS_SAVEABLE) {
		printf("%s%s%s",
			b ? " [" : "",
			p ? " | " : "",
			nvfeature_caps_to_str[NVME_FEAT_CAPS_SAVEABLE]);
			p++;
			b = 0;
	}
	if (b == 0)
		printf("]\n");
	else
		printf("\n");
	return 0;
}

static int
nvfeature_print_hostid(uint8_t *hostid, int sel, int exhid, int output_uuid)
{
	int error = 0;
	char *hostid_str = NULL;

	if (output_uuid == 1) {
		uint32_t status = 0;
		uuid_t uuid;

		uuid_dec_be(hostid, &uuid);
		uuid_to_string(&uuid, &hostid_str, &status);
		if (status != uuid_s_ok) {
			warn("failed to convert host ID to UUID\n");
			error = 1;
			goto bailout;
		}
	} else {
		struct sbuf *hostid_sbuf;

		hostid_sbuf = sbuf_new(NULL, NULL, 64, SBUF_FIXEDLEN);
		if (hostid_sbuf == NULL) {
			warn("error allocating sbuf");
			error = 1;
			goto bailout;
		}

		nvme_hostid_sbuf(hostid_sbuf, hostid, exhid);
		sbuf_finish(hostid_sbuf);
		hostid_str = strndup(sbuf_data(hostid_sbuf), 64);
		sbuf_delete(hostid_sbuf);
	}
	printf("Host Identifier (%s): %s\n",
		nvfeature_select_to_str[sel], hostid_str);

bailout:
	if (hostid_str != NULL)
		free(hostid_str);

	return error;
}

static int
nvmefeature_get_hostid(struct cam_device *device, int sel, int verbosemode,
		       int err_recover, int output_uuid, int retry_count,
		       int timeout)
{
	union ccb *ccb = NULL;
	struct nvme_controller_data cdata;
	uint8_t hostid[16];
	uint8_t *data = NULL;
	uint32_t datalen = 0;
	int exhid = 0;
	int error = 0;

	if (! nvfeature_validate_select(sel)) {
		error = 1;
		goto bailout;
	}

	/* Getting capabilities does not output as uuid nor
	 * is the data buffer (hostid) used/needed
	 */
	if (sel == NVME_FEAT_SEL_CAPABILITIES) {
		output_uuid = 0;
	} else {
		/*
		 * Request and decode the form of the host identifier
		 * (64-bit or extended 128-bit) that the controller
		 * supports.
		 */
		error = nvme_get_data(device, CDAI_TYPE_NVME_CNTRL, &cdata,
		    sizeof(cdata));
		if (error != 0)
			goto bailout;
		exhid = (cdata.ctratt & NVME_CD_CTRATT_EXHID) ? 1 : 0;

		if (output_uuid == 1 && exhid == 0) {
			warnx("%s: -U requires a 128-bit host identifier, "
			    "which this controller does not support",
			    __func__);
			error = 1;
			goto bailout;
		}

		bzero(hostid, sizeof(hostid));
		data = hostid;
		datalen = exhid ? 16 : 8;
	}

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->csio);

	nvme_get_feature_hostid(/*nvmeio*/ &ccb->nvmeio,
				/*retries*/ retry_count,
				/*cbfcnp*/ NULL,
				/*sel*/ sel,
				/*exhid*/ exhid,
				/*uuid_idx*/ 0,
				/*hostid*/ data,
				/*dxfer_len*/ datalen,
				/*timeout*/ timeout);

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warnx("%s: error sending NVMe Get Feature", __func__);
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

	if (sel == NVME_FEAT_SEL_CAPABILITIES) {
		error = nvfeature_print_caps(ccb, "Host ID capabilities:");
	} else {
		error = nvfeature_print_hostid(hostid, sel, exhid,
				output_uuid);
	}

bailout:
	if (ccb != NULL)
		free(ccb);

	return error;
}

static int
nvmefeature_set_hostid(struct cam_device *device, int save, int verbosemode,
		       int err_recover, char *hostid_str,
		       int retry_count, int timeout)
{
	uint8_t data[16] = { 0 };
	uuid_t hostid;
	union ccb *ccb = NULL;
	uint32_t status = 0;
	int exhid = 0;
	int error = 0;


	if (! nvfeature_validate_save(save)) {
		error = 1;
		goto bailout;
	}

	if (hostid_str == NULL) {
		warnx("%s: no host id provided", __func__);
		error = 1;
		goto bailout;
	}

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->csio);

	uuid_from_string(hostid_str, &hostid, &status);
	if (status == uuid_s_ok) {
		exhid = 1;
		uuid_enc_be((void *)data, &hostid);
	} else {
		char *end;
		uintmax_t id = strtoumax((const char *)hostid_str, &end, 0);
		if (*end != '\0') {
			warnx("%s: error parsing hostid value", __func__);
			error = 1;
			goto bailout;
		}
		le64enc(data, id);
	}
	nvme_set_feature_hostid(/*nvmeio*/ &ccb->nvmeio,
				/*retries*/ retry_count,
				/*cbfcnp*/ NULL,
				/*save*/ save,
				/*exhid*/ exhid,
				/*data_ptr*/ data,
				/*dxfer_len*/ exhid ? 16 : 8,
				/*timeout*/ timeout);
	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warnx("error sending NVMe Set Feature");
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
	if (ccb != NULL) {
		cam_freeccb(ccb);
	}
	return error;
}

int
nvmefeature(struct cam_device *device, int argc, char **argv,
	    char *combinedopt, int retry_count, int timeout,
	    int verbosemode, int err_recover)
{
	int error = 0;
	int s = 0;
	struct scsi_nv *action = NULL, *feature = NULL;
	char *data_str = NULL;
	char *endptr = NULL;
	int output_uuid = 0;
	int c;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'a':{
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvfeature_actions,
					     sizeof(nvfeature_actions) /
					     sizeof(nvfeature_actions[0]),
					     optarg, &table_entry,
					     SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND)
				action = &nvfeature_actions[table_entry];
			else {
				warnx("%s: %s nvfeature operation %s", __func__,
				      (status == SCSI_NV_AMBIGUOUS) ?
				      "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}
			break;
		}
		case 'd':
			data_str = strndup((const char *)optarg, 64);
			break;
		case 'f': {
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvfeature_features,
					     sizeof(nvfeature_features) /
					     sizeof(nvfeature_features[0]),
					     optarg, &table_entry,
					     SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND)
				feature = &nvfeature_features[table_entry];
			else {
				warnx("%s: %s nvfeature feature %s", __func__,
				      (status == SCSI_NV_AMBIGUOUS) ?
				      "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}
			break;
		}
		case 's':
			s = strtol(optarg, &endptr, 0);
			if (*endptr != '\0') {
				warnx("%s: invalid -s \"%s\", must be a number",
					__func__, optarg);
				error = 1;
				goto bailout;
			}
			break;
		case 'U':
			output_uuid = 1;
			break;
		default:
			break;
		}
	}

	if (action == NULL) {
		warnx("%s: you must specify an action with -a", __func__);
		error = 1;
		goto bailout;
	}
	if (feature == NULL) {
		warnx("%s: you must specify a feature with -f", __func__);
		error = 1;
		goto bailout;
	}

	switch (action->value) {
	case NVME_OPC_SET_FEATURES:
		switch (feature->value) {
		case NVME_FEAT_HOST_IDENTIFIER:
			error = nvmefeature_set_hostid(device, /*save*/s,
					verbosemode, err_recover, data_str,
					retry_count, timeout);
			break;
		default:
			warnx("%s: set feature %s not yet implemented",
			      __func__, feature->name);
			error = 1;
		}
		break;

	case NVME_OPC_GET_FEATURES:
		switch (feature->value) {
		case NVME_FEAT_HOST_IDENTIFIER:
			error = nvmefeature_get_hostid(device, /*select*/s,
					verbosemode, err_recover, output_uuid,
					retry_count, timeout);
			break;
		default:
			warnx("%s: get feature %s not yet implemented",
			      __func__, feature->name);
			error = 1;

		}
		break;
	default:
		error = 1;
		goto bailout;
	}
bailout:
	if (data_str != NULL) {
		free(data_str);
	}
	return error;
}
