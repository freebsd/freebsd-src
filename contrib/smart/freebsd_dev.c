/*
 * Copyright (c) 2016-2021 Chuck Tuffli <chuck@tuffli.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>

#include "libsmart.h"
#include "libsmart_priv.h"
#include "libsmart_dev.h"

/* Provide compatibility for FreeBSD 11.0 */
#if (__FreeBSD_version < 1101000)

struct scsi_log_informational_exceptions {
        struct scsi_log_param_header hdr;
#define SLP_IE_GEN                      0x0000
        uint8_t ie_asc;
        uint8_t ie_ascq;
        uint8_t temperature;
};

#endif

struct fbsd_smart {
	smart_t	common;
	struct cam_device *camdev;
};

static smart_protocol_e __device_get_proto(struct fbsd_smart *);
static bool __device_proto_tunneled(struct fbsd_smart *);
static int32_t __device_get_info(struct fbsd_smart *);

smart_h
device_open(smart_protocol_e protocol, char *devname)
{
	struct fbsd_smart *h = NULL;

	h = malloc(sizeof(struct fbsd_smart));
	if (h == NULL)
		return NULL;

	memset(h, 0, sizeof(struct fbsd_smart));

	h->common.protocol = SMART_PROTO_MAX;
	h->camdev = cam_open_device(devname, O_RDWR);
	if (h->camdev == NULL) {
		printf("%s: error opening %s - %s\n",
				__func__, devname,
				cam_errbuf);
		free(h);
		h = NULL;
	} else {
		smart_protocol_e proto = __device_get_proto(h);

		if ((protocol == SMART_PROTO_AUTO) ||
				(protocol == proto)) {
			h->common.protocol = proto;
		} else {
			printf("%s: protocol mismatch %d vs %d\n",
					__func__, protocol, proto);
		}

		if (proto == SMART_PROTO_SCSI) {
			if (__device_proto_tunneled(h)) {
				h->common.protocol = SMART_PROTO_ATA;
				h->common.info.tunneled = 1;
			}
		}

		__device_get_info(h);
	}

	return h;
}

void
device_close(smart_h h)
{
	struct fbsd_smart *fsmart = h;

	if (fsmart != NULL) {
		if (fsmart->camdev != NULL) {
			cam_close_device(fsmart->camdev);
		}

		free(fsmart);
	}
}

static const uint8_t smart_read_data[] = {
	0xb0, 0xd0, 0x00, 0x4f, 0xc2, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t smart_return_status[] = {
	0xb0, 0xda, 0x00, 0x4f, 0xc2, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int32_t
__device_read_ata(smart_h h, uint32_t page, void *buf, size_t bsize, union ccb *ccb)
{
	struct fbsd_smart *fsmart = h;
	const uint8_t *smart_fis;
	uint32_t smart_fis_size = 0;
	uint32_t cam_flags = 0;
	uint16_t sector_count = 0;
	uint8_t protocol = 0;

	switch (page) {
	case PAGE_ID_ATA_SMART_READ_DATA: /* Support SMART READ DATA */
		smart_fis = smart_read_data;
		smart_fis_size = sizeof(smart_read_data);
		cam_flags = CAM_DIR_IN;
		sector_count = 1;
		protocol = AP_PROTO_PIO_IN;
		break;
	case PAGE_ID_ATA_SMART_RET_STATUS: /* Support SMART RETURN STATUS */
		smart_fis = smart_return_status;
		smart_fis_size = sizeof(smart_return_status);
		/* Command has no data but uses the return status */
		cam_flags = CAM_DIR_NONE;
		protocol = AP_PROTO_NON_DATA;
		bsize = 0;
		break;
	default:
		return EINVAL;
	}

	if (fsmart->common.info.tunneled) {
		struct ata_pass_16 *cdb;
		uint8_t cdb_flags;

		if (bsize > 0) {
			cdb_flags = AP_FLAG_TDIR_FROM_DEV |
				AP_FLAG_BYT_BLOK_BLOCKS |
				AP_FLAG_TLEN_SECT_CNT;
		} else {
			cdb_flags = AP_FLAG_CHK_COND |
				AP_FLAG_TDIR_FROM_DEV |
				AP_FLAG_BYT_BLOK_BLOCKS;
		}

		cdb = (struct ata_pass_16 *)ccb->csio.cdb_io.cdb_bytes;
		memset(cdb, 0, sizeof(*cdb));

		scsi_ata_pass_16(&ccb->csio,
				/*retries*/	1,
				/*cbfcnp*/	NULL,
				/*flags*/	cam_flags,
				/*tag_action*/	MSG_SIMPLE_Q_TAG,
				/*protocol*/	protocol,
				/*ata_flags*/	cdb_flags,
				/*features*/	page,
				/*sector_count*/sector_count,
				/*lba*/		0,
				/*command*/	ATA_SMART_CMD,
				/*control*/	0,
				/*data_ptr*/	buf,
				/*dxfer_len*/	bsize,
				/*sense_len*/	SSD_FULL_SIZE,
				/*timeout*/	5000
				);
		cdb->lba_mid = 0x4f;
		cdb->lba_high = 0xc2;
		cdb->device = 0;	/* scsi_ata_pass_16() sets this */
	} else {
		memcpy(&ccb->ataio.cmd.command, smart_fis, smart_fis_size);

		cam_fill_ataio(&ccb->ataio,
				/* retries */1,
				/* cbfcnp */NULL,
				/* flags */cam_flags,
				/* tag_action */0,
				/* data_ptr */buf,
				/* dxfer_len */bsize,
				/* timeout */5000);
		ccb->ataio.cmd.flags |= CAM_ATAIO_NEEDRESULT;
		ccb->ataio.cmd.control = 0;
	}

	return 0;
}

static int32_t
__device_read_scsi(__attribute__((unused)) smart_h h, uint32_t page, void *buf, size_t bsize, union ccb *ccb)
{

	scsi_log_sense(&ccb->csio,
			/* retries */1,
			/* cbfcnp */NULL,
			/* tag_action */0,
			/* page_code */SLS_PAGE_CTRL_CUMULATIVE,
			/* page */page,
			/* save_pages */0,
			/* ppc */0,
			/* paramptr */0,
			/* param_buf */buf,
			/* param_len */bsize,
			/* sense_len */0,
			/* timeout */5000);

	return 0;
}

static int32_t
__device_read_nvme(__attribute__((unused)) smart_h h, uint32_t page, void *buf, size_t bsize, union ccb *ccb)
{
	struct ccb_nvmeio *nvmeio = &ccb->nvmeio;
	uint32_t numd = 0;	/* number of dwords */

	/*
	 * NVME CAM passthru
	 *    1200000 > version > 1101510 uses nvmeio->cmd.opc
	 *    1200059 > version > 1200038 uses nvmeio->cmd.opc
	 *    1200081 > version > 1200058 uses nvmeio->cmd.opc_fuse
	 *                      > 1200080 uses nvmeio->cmd.opc
	 * This code doesn't support the brief 'opc_fuse' period.
	 */
#if ((__FreeBSD_version > 1200038) || ((__FreeBSD_version > 1101510) && (__FreeBSD_version < 1200000)))
	switch (page) {
	case NVME_LOG_HEALTH_INFORMATION:
		numd = (sizeof(struct nvme_health_information_page) / sizeof(uint32_t));
		break;
	default:
		/* Unsupported log page */
		return EINVAL;
	}

	/* Subtract 1 because NUMD is a zero based value */
	numd--;

	nvmeio->cmd.opc = NVME_OPC_GET_LOG_PAGE;
	nvmeio->cmd.nsid = NVME_GLOBAL_NAMESPACE_TAG;
	nvmeio->cmd.cdw10 = page | (numd << 16);

	cam_fill_nvmeadmin(&ccb->nvmeio,
			/* retries */1,
			/* cbfcnp */NULL,
			/* flags */CAM_DIR_IN,
			/* data_ptr */buf,
			/* dxfer_len */bsize,
			/* timeout */5000);
#endif
	return 0;
}

/*
 * Retrieve the SMART RETURN STATUS
 *
 * SMART RETURN STATUS provides the reliability status of the
 * device and can be used as a high-level indication of health.
 */
static int32_t
__device_status_ata(smart_h h, union ccb *ccb)
{
	struct fbsd_smart *fsmart = h;
	uint8_t *buf = NULL;
	uint32_t page = 0;
	uint8_t lba_high = 0, lba_mid = 0, device = 0, status = 0;

	if (fsmart->common.info.tunneled) {
		struct ata_res_pass16 {
			u_int16_t reserved[5];
			u_int8_t flags;
			u_int8_t error;
			u_int8_t sector_count_exp;
			u_int8_t sector_count;
			u_int8_t lba_low_exp;
			u_int8_t lba_low;
			u_int8_t lba_mid_exp;
			u_int8_t lba_mid;
			u_int8_t lba_high_exp;
			u_int8_t lba_high;
			u_int8_t device;
			u_int8_t status;
		} *res_pass16 = (struct ata_res_pass16 *)(uintptr_t)
			    &ccb->csio.sense_data;

		buf = ccb->csio.data_ptr;
		page = ((struct ata_pass_16 *)ccb->csio.cdb_io.cdb_bytes)->features;
		lba_high = res_pass16->lba_high;
		lba_mid = res_pass16->lba_mid;
		device = res_pass16->device;
		status = res_pass16->status;

		/*
		 * Note that this generates an expected CHECK CONDITION.
		 * Mask it so the outer function doesn't print an error
		 * message.
		 */
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_REQ_CMP;
	} else {
		struct ccb_ataio *ataio = (struct ccb_ataio *)&ccb->ataio;

		buf = ataio->data_ptr;
		page = ataio->cmd.features;
		lba_high = ataio->res.lba_high;
		lba_mid = ataio->res.lba_mid;
		device = ataio->res.device;
		status = ataio->res.status;
	}

	switch (page) {
	case PAGE_ID_ATA_SMART_RET_STATUS:
		/*
		 * Typically, SMART related log pages return data, but this
		 * command is different in that the data is encoded in the
		 * result registers.
		 *
		 * Handle this in a UNIX-like way by writing a 0 (no errors)
		 * or 1 (threshold exceeded condition) to the output buffer.
		 */
		dprintf("SMART_RET_STATUS: lba mid=%#x high=%#x device=%#x status=%#x\n",
				lba_mid,
				lba_high,
				device,
				status);
		if ((lba_high == 0x2c) && (lba_mid == 0xf4)) {
			buf[0] = 1;
		} else if ((lba_high == 0xc2) && (lba_mid == 0x4f)) {
			buf[0] = 0;
		} else {
			/* Ruh-roh ... */
			buf[0] = 255;
		}
		break;
	default:
		;
	}

	return 0;
}

int32_t
device_read_log(smart_h h, uint32_t page, void *buf, size_t bsize)
{
	struct fbsd_smart *fsmart = h;
	union ccb *ccb = NULL;
	int rc = 0;

	if (fsmart == NULL)
		return EINVAL;

	dprintf("read log page %#x\n", page);

	ccb = cam_getccb(fsmart->camdev);
	if (ccb == NULL)
		return ENOMEM;

	CCB_CLEAR_ALL_EXCEPT_HDR(ccb);

	switch (fsmart->common.protocol) {
	case SMART_PROTO_ATA:
		rc = __device_read_ata(h, page, buf, bsize, ccb);
		break;
	case SMART_PROTO_SCSI:
		rc = __device_read_scsi(h, page, buf, bsize, ccb);
		break;
	case SMART_PROTO_NVME:
		rc = __device_read_nvme(h, page, buf, bsize, ccb);
		break;
	default:
		warnx("unsupported protocol %d", fsmart->common.protocol);
		cam_freeccb(ccb);
		return ENODEV;
	}

	if (rc) {
		if (rc == EINVAL)
			warnx("unsupported page %#x", page);

		return rc;
	}

	if (((rc = cam_send_ccb(fsmart->camdev, ccb)) < 0)
			|| ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		if (rc < 0)
			warn("error sending command");
	}

	/*
	 * Most commands don't need any post-processing. But then there's
	 * ATA. It's why we can't have nice things :(
	 */
	switch (fsmart->common.protocol) {
	case SMART_PROTO_ATA:
		__device_status_ata(h, ccb);
		break;
	default:
		;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(fsmart->camdev, ccb, CAM_ESF_ALL,
				CAM_EPF_ALL, stderr);
	}

	cam_freeccb(ccb);

	return 0;
}

/*
 * The SCSI / ATA Translation (SAT) requires devices to support the ATA
 * Information VPD Page (T10/2126-D Revision 04). Use the existence of
 * this page to identify tunneled devices.
 */
static bool
__device_proto_tunneled(struct fbsd_smart *fsmart)
{
	union ccb *ccb = NULL;
	struct scsi_vpd_supported_page_list supportedp;
	uint32_t i;
	bool is_tunneled = false;

	if (fsmart->common.protocol != SMART_PROTO_SCSI) {
		return false;
	}

	ccb = cam_getccb(fsmart->camdev);
	if (!ccb) {
		warn("Allocation failure ccb=%p", ccb);
		goto __device_proto_tunneled_out;
	}

	scsi_inquiry(&ccb->csio,
			3, // retries
			NULL, // callback function
			MSG_SIMPLE_Q_TAG, // tag action
			(uint8_t *)&supportedp,
			sizeof(struct scsi_vpd_supported_page_list),
			1, // EVPD
			SVPD_SUPPORTED_PAGE_LIST, // page code
			SSD_FULL_SIZE, // sense length
			5000); // timeout

	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if ((cam_send_ccb(fsmart->camdev, ccb) >= 0) &&
			((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)) {
		dprintf("Looking for page %#x (total = %u):\n", SVPD_ATA_INFORMATION,
				supportedp.length);
		for (i = 0; i < supportedp.length; i++) {
			dprintf("\t[%u] = %#x\n", i, supportedp.list[i]);
			if (supportedp.list[i] == SVPD_ATA_INFORMATION) {
				is_tunneled = true;
				break;
			}
		}
	}

	cam_freeccb(ccb);

__device_proto_tunneled_out:
	return is_tunneled;
}

/**
 * Retrieve the device protocol type via the transport settings
 *
 * @return protocol type or SMART_PROTO_MAX on error
 */
static smart_protocol_e
__device_get_proto(struct fbsd_smart *fsmart)
{
	smart_protocol_e proto = SMART_PROTO_MAX;
	union ccb *ccb;

	if (!fsmart || !fsmart->camdev) {
		warn("Bad handle %p", fsmart);
		return proto;
	}

	ccb = cam_getccb(fsmart->camdev);
	if (ccb != NULL) {
		CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->cts);

		ccb->ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
		ccb->cts.type = CTS_TYPE_CURRENT_SETTINGS;

		if (cam_send_ccb(fsmart->camdev, ccb) >= 0) {
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
				struct ccb_trans_settings *cts = &ccb->cts;

				switch (cts->protocol) {
				case PROTO_ATA:
					proto = SMART_PROTO_ATA;
					break;
				case PROTO_SCSI:
					proto = SMART_PROTO_SCSI;
					break;
				case PROTO_NVME:
					proto = SMART_PROTO_NVME;
					break;
				default:
					printf("%s: unknown protocol %d\n",
							__func__,
							cts->protocol);
				}
			}
		}

		cam_freeccb(ccb);
	}

	return proto;
}

static int32_t
__device_info_ata(struct fbsd_smart *fsmart, struct ccb_getdev *cgd)
{
	smart_info_t *sinfo = NULL;

	if (!fsmart || !cgd) {
		return -1;
	}

	sinfo = &fsmart->common.info;

	sinfo->supported = cgd->ident_data.support.command1 &
		ATA_SUPPORT_SMART;

	dprintf("ATA command1 = %#x\n", cgd->ident_data.support.command1);

	cam_strvis((uint8_t *)sinfo->device, cgd->ident_data.model,
			sizeof(cgd->ident_data.model),
			sizeof(sinfo->device));
	cam_strvis((uint8_t *)sinfo->rev, cgd->ident_data.revision,
			sizeof(cgd->ident_data.revision),
			sizeof(sinfo->rev));
	cam_strvis((uint8_t *)sinfo->serial, cgd->ident_data.serial,
			sizeof(cgd->ident_data.serial),
			sizeof(sinfo->serial));

	return 0;
}

static int32_t
__device_info_scsi(struct fbsd_smart *fsmart, struct ccb_getdev *cgd)
{
	smart_info_t *sinfo = NULL;
	union ccb *ccb = NULL;
	struct scsi_vpd_unit_serial_number *snum = NULL;
	struct scsi_log_informational_exceptions ie = {0};

	if (!fsmart || !cgd) {
		return -1;
	}

	sinfo = &fsmart->common.info;

	cam_strvis((uint8_t *)sinfo->vendor, (uint8_t *)cgd->inq_data.vendor,
			sizeof(cgd->inq_data.vendor),
			sizeof(sinfo->vendor));
	cam_strvis((uint8_t *)sinfo->device, (uint8_t *)cgd->inq_data.product,
			sizeof(cgd->inq_data.product),
			sizeof(sinfo->device));
	cam_strvis((uint8_t *)sinfo->rev, (uint8_t *)cgd->inq_data.revision,
			sizeof(cgd->inq_data.revision),
			sizeof(sinfo->rev));

	ccb = cam_getccb(fsmart->camdev);
	snum = malloc(sizeof(struct scsi_vpd_unit_serial_number));
	if (!ccb || !snum) {
		warn("Allocation failure ccb=%p snum=%p", ccb, snum);
		goto __device_info_scsi_out;
	}

	/* Get the serial number */
	CCB_CLEAR_ALL_EXCEPT_HDR(&ccb->csio);

	scsi_inquiry(&ccb->csio,
			3, // retries
			NULL, // callback function
			MSG_SIMPLE_Q_TAG, // tag action
			(uint8_t *)snum,
			sizeof(struct scsi_vpd_unit_serial_number),
			1, // EVPD
			SVPD_UNIT_SERIAL_NUMBER, // page code
			SSD_FULL_SIZE, // sense length
			5000); // timeout

	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if ((cam_send_ccb(fsmart->camdev, ccb) >= 0) &&
			((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)) {
		cam_strvis((uint8_t *)sinfo->serial, snum->serial_num,
				snum->length,
				sizeof(sinfo->serial));
		sinfo->serial[sizeof(sinfo->serial) - 1] = '\0';
	}

	memset(ccb, 0, sizeof(*ccb));

	scsi_log_sense(&ccb->csio,
			/* retries */1,
			/* cbfcnp */NULL,
			/* tag_action */0,
			/* page_code */SLS_PAGE_CTRL_CUMULATIVE,
			/* page */SLS_IE_PAGE,
			/* save_pages */0,
			/* ppc */0,
			/* paramptr */0,
			/* param_buf */(uint8_t *)&ie,
			/* param_len */sizeof(ie),
			/* sense_len */0,
			/* timeout */5000);

	/*
	 * Note: The existance of the Informational Exceptions (IE) log page
	 *       appears to be the litmus test for SMART support in SCSI
	 *       devices. Confusingly, smartctl will report SMART health
	 *       status as 'OK' if the device doesn't support the IE page.
	 *       For now, just report the facts.
	 */
	if ((cam_send_ccb(fsmart->camdev, ccb) >= 0) &&
			((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)) {
		if ((ie.hdr.param_len < 4) || ie.ie_asc || ie.ie_ascq) {
			printf("Log Sense, Informational Exceptions failed "
					"(length=%u asc=%#x ascq=%#x)\n",
					ie.hdr.param_len, ie.ie_asc, ie.ie_ascq);
		} else {
			sinfo->supported = true;
		}
	}

__device_info_scsi_out:
	free(snum);
	if (ccb)
		cam_freeccb(ccb);

	return 0;
}

static int32_t
__device_info_nvme(struct fbsd_smart *fsmart, struct ccb_getdev *cgd)
{
	union ccb *ccb;
	smart_info_t *sinfo = NULL;
	struct nvme_controller_data cd;

	if (!fsmart || !cgd) {
		return -1;
	}

	sinfo = &fsmart->common.info;

	sinfo->supported = true;

	ccb = cam_getccb(fsmart->camdev);
	if (ccb != NULL) {
		struct ccb_dev_advinfo *cdai = &ccb->cdai;

		CCB_CLEAR_ALL_EXCEPT_HDR(cdai);

		cdai->ccb_h.func_code = XPT_DEV_ADVINFO;
		cdai->ccb_h.flags = CAM_DIR_IN;
		cdai->flags = CDAI_FLAG_NONE;
#ifdef CDAI_TYPE_NVME_CNTRL
		cdai->buftype = CDAI_TYPE_NVME_CNTRL;
#else
		cdai->buftype = 6;
#endif
		cdai->bufsiz = sizeof(struct nvme_controller_data);
		cdai->buf = (uint8_t *)&cd;

		if (cam_send_ccb(fsmart->camdev, ccb) >= 0) {
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
				cam_strvis((uint8_t *)sinfo->device, cd.mn,
						sizeof(cd.mn),
						sizeof(sinfo->device));
				cam_strvis((uint8_t *)sinfo->rev, cd.fr,
						sizeof(cd.fr),
						sizeof(sinfo->rev));
				cam_strvis((uint8_t *)sinfo->serial, cd.sn,
						sizeof(cd.sn),
						sizeof(sinfo->serial));
			}
		}

		cam_freeccb(ccb);
	}

	return 0;
}

static int32_t
__device_info_tunneled_ata(struct fbsd_smart *fsmart)
{
	struct ata_params ident_data;
	union ccb *ccb = NULL;
	struct ata_pass_16 *ata_pass_16;
	struct ata_cmd ata_cmd;
	int32_t rc = -1;

	ccb = cam_getccb(fsmart->camdev);
	if (ccb == NULL) {
		goto __device_info_tunneled_ata_out;
	}

	memset(&ident_data, 0, sizeof(struct ata_params));

	CCB_CLEAR_ALL_EXCEPT_HDR(ccb);

	scsi_ata_pass_16(&ccb->csio,
			/*retries*/	1,
			/*cbfcnp*/	NULL,
			/*flags*/	CAM_DIR_IN,
			/*tag_action*/	MSG_SIMPLE_Q_TAG,
			/*protocol*/	AP_PROTO_PIO_IN,
			/*ata_flags*/	AP_FLAG_TLEN_SECT_CNT |
					AP_FLAG_BYT_BLOK_BLOCKS |
					AP_FLAG_TDIR_FROM_DEV,
			/*features*/	0,
			/*sector_count*/sizeof(struct ata_params),
			/*lba*/		0,
			/*command*/	ATA_ATA_IDENTIFY,
			/*control*/	0,
			/*data_ptr*/	(uint8_t *)&ident_data,
			/*dxfer_len*/	sizeof(struct ata_params),
			/*sense_len*/	SSD_FULL_SIZE,
			/*timeout*/	5000
			);

	ata_pass_16 = (struct ata_pass_16 *)ccb->csio.cdb_io.cdb_bytes;
	ata_cmd.command = ata_pass_16->command;
	ata_cmd.control = ata_pass_16->control;
	ata_cmd.features = ata_pass_16->features;

	rc = cam_send_ccb(fsmart->camdev, ccb);
	if (rc != 0) {
		warnx("%s: scsi_ata_pass_16() failed (programmer error?)",
				__func__);
		goto __device_info_tunneled_ata_out;
	}

	fsmart->common.info.supported = ident_data.support.command1 &
		ATA_SUPPORT_SMART;

	dprintf("ATA command1 = %#x\n", ident_data.support.command1);

__device_info_tunneled_ata_out:
	if (ccb) {
		cam_freeccb(ccb);
	}

	return rc;
}

/**
 * Retrieve the device information and use to populate the info structure
 */
static int32_t
__device_get_info(struct fbsd_smart *fsmart)
{
	union ccb *ccb;
	int32_t rc = -1;

	if (!fsmart || !fsmart->camdev) {
		warn("Bad handle %p", fsmart);
		return -1;
	}

	ccb = cam_getccb(fsmart->camdev);
	if (ccb != NULL) {
		struct ccb_getdev *cgd = &ccb->cgd;

		CCB_CLEAR_ALL_EXCEPT_HDR(cgd);

		/*
		 * GDEV_TYPE doesn't support NVMe. What we do get is:
		 *  - device (ata/model, scsi/product)
		 *  - revision (ata, scsi)
		 *  - serial (ata)
		 *  - vendor (scsi)
		 *  - supported (ata)
		 *
		 *  Serial # for all proto via ccb_dev_advinfo (buftype CDAI_TYPE_SERIAL_NUM)
		 */
		ccb->ccb_h.func_code = XPT_GDEV_TYPE;

		if (cam_send_ccb(fsmart->camdev, ccb) >= 0) {
			if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
				switch (cgd->protocol) {
				case PROTO_ATA:
					rc = __device_info_ata(fsmart, cgd);
					break;
				case PROTO_SCSI:
					rc = __device_info_scsi(fsmart, cgd);
					if (!rc && fsmart->common.protocol == SMART_PROTO_ATA) {
						rc = __device_info_tunneled_ata(fsmart);
					}
					break;
				case PROTO_NVME:
					rc = __device_info_nvme(fsmart, cgd);
					break;
				default:
					printf("%s: unsupported protocol %d\n",
							__func__, cgd->protocol);
				}
			}
		}

		cam_freeccb(ccb);
	}

	return rc;
}
