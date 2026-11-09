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
 * Authors: Ken Merry           (Spectra Logic Corporation)
 *          Reid Linnemann      (Spectra Logic Corporation)
 */
/*
 * NVMe Reservation support for camcontrol(8).
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

/*
 * PERSISTENT RESERVE OUT style action names, translated to the NVMe
 * reservation opcode, the action field within that command, and whether
 * to set the Ignore Existing Key bit.  The names match the camcontrol
 * persist -o actions where SCSI and NVMe have equivalent operations;
 * unregister and replace are NVMe-only.
 */
struct nvres_out_action {
	uint8_t		opc;	/* NVMe reservation opcode */
	int		action;	/* action field within the command */
	int		iekey;	/* set Ignore Existing Key */
};

static struct nvres_out_action nvres_out_actions[] = {
	{ NVME_OPC_RESERVATION_REGISTER, NVME_RESV_REG_REGISTER, 0 },
	{ NVME_OPC_RESERVATION_REGISTER, NVME_RESV_REG_REGISTER, 1 },
	{ NVME_OPC_RESERVATION_REGISTER, NVME_RESV_REG_UNREGISTER, 0 },
	{ NVME_OPC_RESERVATION_REGISTER, NVME_RESV_REG_REPLACE, 0 },
	{ NVME_OPC_RESERVATION_ACQUIRE, NVME_RESV_ACQUIRE_ACQUIRE, 0 },
	{ NVME_OPC_RESERVATION_ACQUIRE, NVME_RESV_ACQUIRE_PREEMPT, 0 },
	{ NVME_OPC_RESERVATION_ACQUIRE, NVME_RESV_ACQUIRE_PREEMPT_ABT, 0 },
	{ NVME_OPC_RESERVATION_RELEASE, NVME_RESV_REL_RELEASE, 0 },
	{ NVME_OPC_RESERVATION_RELEASE, NVME_RESV_REL_CLEAR, 0 }
};

/* Values are indices into nvres_out_actions[]. */
static struct scsi_nv nvres_out_action_names[] = {
	{ "register", 0 },
	{ "register_ignore", 1 },
	{ "unregister", 2 },
	{ "replace", 3 },
	{ "reserve", 4 },
	{ "preempt", 5 },
	{ "preempt_abort", 6 },
	{ "release", 7 },
	{ "clear", 8 }
};

/*
 * PERSISTENT RESERVE IN style mode names.  All of them map to the NVMe
 * Reservation Report command; read_full_status additionally requests the
 * extended data structure (128-bit host IDs).  Values are the eds flag.
 */
static struct scsi_nv nvres_in_modes[] = {
	{ "read_keys", 0 },
	{ "read_reservation", 0 },
	{ "read_full_status", 1 }
};

/* Change Persist Through Power Loss State values for -c. */
static struct scsi_nv nvres_cptpl_vals[] = {
	{ "disable", NVME_RESV_CPTPL_0 },
	{ "enable", NVME_RESV_CPTPL_1 }
};

static struct scsi_nv nvres_res_types[] = {
	{ "write_exclusive", NVME_RESV_TYPE_WR_EX },
	{ "wr_ex", NVME_RESV_TYPE_WR_EX },
	{ "exclusive_access", NVME_RESV_TYPE_EX_AC },
	{ "ex_ac", NVME_RESV_TYPE_EX_AC },
	{ "write_exclusive_reg_only", NVME_RESV_TYPE_WR_EX_RO },
	{ "wr_ex_ro", NVME_RESV_TYPE_WR_EX_RO },
	{ "exclusive_access_reg_only", NVME_RESV_TYPE_EX_AC_RO },
	{ "ex_ac_ro", NVME_RESV_TYPE_EX_AC_RO },
	{ "write_exclusive_all_regs", NVME_RESV_TYPE_WR_EX_AR },
	{ "wr_ex_ar", NVME_RESV_TYPE_WR_EX_AR },
	{ "exclusive_access_all_regs", NVME_RESV_TYPE_EX_AC_AR },
	{ "ex_ac_ar", NVME_RESV_TYPE_EX_AC_AR }
};

static int
nvmeres_acquire(struct cam_device *device, int verbosemode, int err_recover,
		int rtype, int iekey, int racqa, uint64_t crkey, uint64_t prkey,
		int retry_count, int timeout)
{
	union ccb *ccb = NULL;
	uint64_t data[2];
	int error = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->nvmeio);

	data[0] = htole64(crkey);
	data[1] = htole64(prkey);

	nvme_resv_acquire(/*nvmeio*/ &ccb->nvmeio,
			  /*retries*/ retry_count,
			  /*cbfcnp*/ NULL,
			  /*nsid*/ device->target_lun,
			  /*rtype*/ rtype,
			  /*iekey*/ iekey,
			  /*rrega*/ racqa,
			  /*data_ptr*/ (uint8_t *)data,
			  /*dxfer_len*/ sizeof(data),
			  /*timeout*/ timeout ? timeout : 30000);

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending NVMe Reservation Acquire");
		error = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("NVMe Reservation Acquire failed");
		if (verbosemode != 0) {
			cam_error_print(device, ccb, CAM_ESF_ALL, CAM_EPF_ALL,
			    stderr);
		}
		error = 1;
		goto bailout;
	}

bailout:
	if (ccb != NULL) {
		cam_freeccb(ccb);
	}
	return (error);
}

static int
nvmeres_register(struct cam_device *device, int verbosemode, int err_recover,
		 int cptpl, int iekey, int rrega, uint64_t curkey,
		 uint64_t newkey, int retry_count, int timeout)
{
	union ccb *ccb = NULL;
	uint64_t data[2];
	int error = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->nvmeio);

	data[0] = htole64(curkey);
	data[1] = htole64(newkey);

	nvme_resv_register(/*nvmeio*/ &ccb->nvmeio,
			   /*retries*/ retry_count,
			   /*cbfcnp*/ NULL,
			   /*nsid*/ device->target_lun,
			   /*cptpl*/ cptpl,
			   /*iekey*/ iekey,
			   /*rrega*/ rrega,
			   /*data_ptr*/ (uint8_t*)data,
			   /*dxfer_len*/ sizeof(data),
			   /*timeout*/ timeout ? timeout: 30000);

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending NVMe Reservation Registration");
		error = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("NVMe Reservation Register failed");
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
	return (error);
}

static int
nvmeres_release(struct cam_device *device, int verbosemode, int err_recover,
		int rtype, int iekey, int rrela, uint64_t curkey,
		int retry_count, int timeout)
{
	union ccb *ccb = NULL;
	uint64_t data[1];
	int error = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->nvmeio);

	data[0] = htole64(curkey);

	nvme_resv_release(/*nvmeio*/ &ccb->nvmeio,
			  /*retries*/ retry_count,
			  /*cbfcnp*/ NULL,
			  /*nsid*/ device->target_lun,
			  /*rtype*/ rtype,
			  /*iekey*/ iekey,
			  /*rrela*/ rrela,
			  /*data_ptr*/ (uint8_t*)data,
			  /*dxfer_len*/ sizeof(data),
			  /*timeout*/ timeout ? timeout: 30000);

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending NVMe Reservation Release");
		error = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("NVMe Reservation Release failed");
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
	return (error);
}

static int
nvmeres_report(struct cam_device *device, int verbosemode, int err_recover,
	       int eds, int retry_count, int timeout)
{
	union {
		struct nvme_resv_status status;
		struct nvme_resv_status_ext ext;
		uint8_t bytes[8192];
	} report_data;
	union ccb *ccb = NULL;
	struct nvme_resv_status *rstatus;
	struct nvme_resv_status_ext *extstatus;
	struct sbuf *hostid_sbuf = NULL;
	const char *rtype_str = NULL;
	int num_ents = 0;
	int error = 0;
	int i;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		error = 1;
		goto bailout;
	}
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->nvmeio);

	bzero(&report_data, sizeof(report_data));

	nvme_resv_report(/*nvmeio*/ &ccb->nvmeio,
			 /*retries*/ retry_count,
			 /*cbfcnp*/ NULL,
			 /*nsid*/ device->target_lun,
			 /*report_type*/ eds,
			 /*data_ptr*/ report_data.bytes,
			 /*dxfer_len */ sizeof(report_data),
			 /*timeout*/ timeout ? timeout : 30000);

	if (err_recover != 0)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending NVMe Reservation Report");
		error = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("NVMe Reservation Report failed");
		if (verbosemode != 0) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		error = 1;
		goto bailout;
	}

	hostid_sbuf = sbuf_new(NULL, NULL, 64, SBUF_FIXEDLEN);
	if (hostid_sbuf == NULL) {
		warn("error allocating sbuf");
		error = 1;
		goto bailout;
	}
	rstatus = &report_data.status;
	/*
	 * XXX KDM evidently there is no concept of a residual in NVMe.
	 */
	if (eds == 0)
		nvme_resv_status_swapbytes(rstatus,sizeof(report_data));
	else
		nvme_resv_status_ext_swapbytes(&report_data.ext,
		    sizeof(report_data));

	num_ents = (rstatus->regctl[1] << 8) | rstatus->regctl[0];
	printf("Generation: %u\n", rstatus->gen);
	printf("Reservation type: %u", rstatus->rtype);
	if (rstatus->rtype > 0) {
		rtype_str = scsi_nv_to_str(nvres_res_types,
		    sizeof(nvres_res_types) / sizeof(nvres_res_types[0]),
		    rstatus->rtype);
		printf(" (%s)", rtype_str?rtype_str:"invalid");
	}
	printf("\n");
	printf("Number of Registered Controllers: %u\n", num_ents);
	printf("Persist Through Power Loss State: %u\n", rstatus->ptpls);

	/*
	 * The number of entries comes from the device.  Constrain it to
	 * the number that actually fit in our data buffer, in case the
	 * device reports more.
	 */
	if (eds == 1) {
		extstatus = &report_data.ext;
		if (num_ents > (int)((sizeof(report_data) -
		    sizeof(*extstatus)) / sizeof(extstatus->ctrlr[0])))
			num_ents = (sizeof(report_data) -
			    sizeof(*extstatus)) / sizeof(extstatus->ctrlr[0]);
		for (i = 0; i < num_ents; i++) {
			sbuf_clear(hostid_sbuf);
			nvme_hostid_sbuf(hostid_sbuf,
			    (uint8_t*)(extstatus->ctrlr[i].hostid), 1);
			sbuf_finish(hostid_sbuf);
			printf("Controller ID:  0x%04x\n",
			    extstatus->ctrlr[i].ctrlr_id);
			printf("Reservation Status:  %u\n",
			    extstatus->ctrlr[i].rcsts);
			printf("Host Identifier:  %s\n", sbuf_data(hostid_sbuf));
			printf("Reservation Key:  0x%016jx\n",
			    (uintmax_t)extstatus->ctrlr[i].rkey);
		}
	} else {
		if (num_ents > (int)((sizeof(report_data) -
		    sizeof(*rstatus)) / sizeof(rstatus->ctrlr[0])))
			num_ents = (sizeof(report_data) -
			    sizeof(*rstatus)) / sizeof(rstatus->ctrlr[0]);
		for (i = 0; i < num_ents; i++) {
			sbuf_clear(hostid_sbuf);
			nvme_hostid_sbuf(hostid_sbuf,
			    (uint8_t*)&(rstatus->ctrlr[i].hostid), 0);
			sbuf_finish(hostid_sbuf);
			printf("Controller ID:  0x%04x\n",
			    rstatus->ctrlr[i].ctrlr_id);
			printf("Reservation Status:  %u\n",
			    rstatus->ctrlr[i].rcsts);
			printf("Host Identifier:  %s\n", sbuf_data(hostid_sbuf));
			printf("Reservation Key:  0x%016jx\n",
			    (uintmax_t)rstatus->ctrlr[i].rkey);
		}
	}
bailout:
	if (ccb != NULL) {
		cam_freeccb(ccb);
	}
	if (hostid_sbuf != NULL) {
		sbuf_delete(hostid_sbuf);
	}
	return (error);
}

int
nvmepersist(struct cam_device *device, int argc, char **argv, char *combinedopt,
	    int retry_count, int timeout, int verbosemode, int err_recover)
{
	uint64_t curkey = 0, other_key = 0;
	int eds = 0, cptpl = 0, iekey = 0;
	int error = 0;
	int in = 0, out = 0;
	int opc = -1, op_action = -1, res_type = -1;
	int c;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'i': {
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvres_in_modes,
			    sizeof(nvres_in_modes) / sizeof(nvres_in_modes[0]),
			    optarg, &table_entry, SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND) {
				in = 1;
				opc = NVME_OPC_RESERVATION_REPORT;
				eds = nvres_in_modes[table_entry].value;
			} else {
				warnx("%s: %s in mode %s for NVMe devices",
				    __func__, (status == SCSI_NV_AMBIGUOUS) ?
				    "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}
			break;
		}
		case 'o': {
			scsi_nv_status status;
			struct nvres_out_action *oa;
			int table_entry = 0;

			status = scsi_get_nv(nvres_out_action_names,
			    sizeof(nvres_out_action_names) /
			    sizeof(nvres_out_action_names[0]),
			    optarg, &table_entry, SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND) {
				out = 1;
				oa = &nvres_out_actions[
				    nvres_out_action_names[table_entry].value];
				opc = oa->opc;
				op_action = oa->action;
				iekey = oa->iekey;
			} else {
				warnx("%s: %s out action %s for NVMe devices",
				    __func__, (status == SCSI_NV_AMBIGUOUS) ?
				    "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}
			break;
		}
		case 'k':
		case 'K': {
			char *endptr;
			uint64_t tmpval;

			tmpval = strtoumax(optarg, &endptr, 0);
			if (*endptr != '\0') {
				warnx("%s: invalid key argument %s", __func__,
				    optarg);
				error = 1;
				goto bailout;
			}
			if (c == 'k')
				curkey = tmpval;
			else
				other_key = tmpval;
			break;
		}
		case 'c': {
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvres_cptpl_vals,
			    sizeof(nvres_cptpl_vals) /
			    sizeof(nvres_cptpl_vals[0]),
			    optarg, &table_entry, SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND)
				cptpl = nvres_cptpl_vals[table_entry].value;
			else {
				warnx("%s: %s persist through power loss "
				    "value %s", __func__,
				    (status == SCSI_NV_AMBIGUOUS) ?
				    "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}
			break;
		}
		case 'p':
			/*
			 * SCSI APTPL is approximately NVMe Change Persist
			 * Through Power Loss State, enabled.  The default
			 * is to make no change to the current setting;
			 * -c gives explicit control.
			 */
			cptpl = NVME_RESV_CPTPL_1;
			break;
		case 'T': {
			scsi_nv_status status;
			int table_entry = 0;

			status = scsi_get_nv(nvres_res_types,
			    sizeof(nvres_res_types) /sizeof(nvres_res_types[0]),
			    optarg, &table_entry, SCSI_NV_FLAG_IG_CASE);
			if (status == SCSI_NV_FOUND)
				res_type = nvres_res_types[table_entry].value;
			else {
				warnx("%s: %s reservation type %s", __func__,
				    (status == SCSI_NV_AMBIGUOUS) ?
				    "ambiguous" : "invalid", optarg);
				error = 1;
				goto bailout;
			}

			break;
		}
		case 'a':
		case 'I':
		case 's':
		case 'S':
		case 'U':
			warnx("%s: option -%c is not supported for NVMe "
			    "devices", __func__, c);
			error = 1;
			goto bailout;
		}
	}
	if ((in + out) != 1) {
		warnx("%s: you must specify one of -i or -o", __func__);
		error = 1;
		goto bailout;
	}

	switch (opc) {
	case NVME_OPC_RESERVATION_REPORT:
		error = nvmeres_report(device, verbosemode, err_recover, eds,
		    retry_count, timeout);
		break;
	case NVME_OPC_RESERVATION_REGISTER:
		switch (op_action) {
		case NVME_RESV_REG_REGISTER:
			if (other_key == 0) {
				warnx("-K newkey is required");
				error = 1;
				goto bailout;
			}
			break;
		case NVME_RESV_REG_REPLACE:
			if (other_key == 0) {
				warnx("-K newkey is required");
				error = 1;
				goto bailout;
			}
			/* fallthrough */
		case NVME_RESV_REG_UNREGISTER:
			if (!iekey && curkey == 0) {
				warnx("-k curkey is required");
				error = 1;
				goto bailout;
			}
			break;
		}
		error = nvmeres_register(device, verbosemode, err_recover,
		    cptpl, iekey, op_action, curkey,
		    other_key, retry_count, timeout);
		break;
	case NVME_OPC_RESERVATION_ACQUIRE:
		switch (op_action) {
		case NVME_RESV_ACQUIRE_PREEMPT:
			/* fallthrough */
		case NVME_RESV_ACQUIRE_PREEMPT_ABT:
			if (other_key == 0) {
				warnx("-K prekey is required");
				error = 1;
				goto bailout;
			}
			/* fallthrough */
		case NVME_RESV_ACQUIRE_ACQUIRE:
			if (curkey == 0) {
				warnx("-k curkey is required");
				error = 1;
				goto bailout;
			}
			if (res_type == -1) {
				warnx("-T resv_type is required");
				error = 1;
				goto bailout;
			}
			break;
		}
		error = nvmeres_acquire(device, verbosemode, err_recover,
					res_type, iekey, op_action, curkey,
					other_key, retry_count, timeout);
		break;
	case NVME_OPC_RESERVATION_RELEASE:
		switch (op_action) {
		case NVME_RESV_REL_RELEASE:
			if (res_type == -1) {
				warnx("-T resv_type is required");
				error = 1;
				goto bailout;
			}
			/* fallthrough */
		case NVME_RESV_REL_CLEAR:
			if (curkey == 0) {
				warnx("-k curkey is required");
				error = 1;
				goto bailout;
			}
			/* The reservation type is ignored for a clear. */
			if (res_type == -1)
				res_type = 0;
			break;
		}

		error = nvmeres_release(device, verbosemode, err_recover,
					res_type, iekey, op_action, curkey,
					retry_count, timeout);
		break;
	default:
		warnx("%s: unknown reservation opcode %#x", __func__, opc);
		error = 1;
		goto bailout;
		break;
	}
bailout:

	return (error);
}
