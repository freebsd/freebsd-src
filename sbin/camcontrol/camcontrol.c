/*
 * Copyright (c) 1997, 1998 Kenneth D. Merry
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 *	$Id$
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>
#include <cam/scsi/scsi_message.h>
#include <camlib.h>
#include "camcontrol.h"

#define DEFAULT_DEVICE "da"
#define DEFAULT_UNIT 	0

typedef enum {
	CAM_ARG_NONE		= 0x00000000,
	CAM_ARG_DEVLIST		= 0x00000001,
	CAM_ARG_TUR		= 0x00000002,
	CAM_ARG_INQUIRY		= 0x00000003,
	CAM_ARG_STARTSTOP	= 0x00000004,
	CAM_ARG_RESCAN		= 0x00000005,
	CAM_ARG_READ_DEFECTS	= 0x00000006,
	CAM_ARG_MODE_PAGE	= 0x00000007,
	CAM_ARG_SCSI_CMD	= 0x00000008,
	CAM_ARG_DEVTREE		= 0x00000009,
	CAM_ARG_USAGE		= 0x0000000a,
	CAM_ARG_DEBUG		= 0x0000000b,
	CAM_ARG_OPT_MASK	= 0x0000000f,
	CAM_ARG_VERBOSE		= 0x00000010,
	CAM_ARG_DEVICE		= 0x00000020,
	CAM_ARG_BUS		= 0x00000040,
	CAM_ARG_TARGET		= 0x00000080,
	CAM_ARG_LUN		= 0x00000100,
	CAM_ARG_EJECT		= 0x00000200,
	CAM_ARG_UNIT		= 0x00000400,
	CAM_ARG_FORMAT_BLOCK	= 0x00000800,
	CAM_ARG_FORMAT_BFI	= 0x00001000,
	CAM_ARG_FORMAT_PHYS	= 0x00002000,
	CAM_ARG_PLIST		= 0x00004000,
	CAM_ARG_GLIST		= 0x00008000,
	CAM_ARG_GET_SERIAL	= 0x00010000,
	CAM_ARG_GET_STDINQ	= 0x00020000,
	CAM_ARG_GET_XFERRATE	= 0x00040000,
	CAM_ARG_INQ_MASK	= 0x00070000,
	CAM_ARG_MODE_EDIT	= 0x00080000,
	CAM_ARG_PAGE_CNTL	= 0x00100000,
	CAM_ARG_TIMEOUT		= 0x00200000,
	CAM_ARG_CMD_IN		= 0x00400000,
	CAM_ARG_CMD_OUT		= 0x00800000,
	CAM_ARG_DBD		= 0x01000000,
	CAM_ARG_ERR_RECOVER	= 0x02000000,
	CAM_ARG_RETRIES		= 0x04000000,
	CAM_ARG_START_UNIT	= 0x08000000,
	CAM_ARG_DEBUG_INFO	= 0x10000000,
	CAM_ARG_DEBUG_TRACE	= 0x20000000,
	CAM_ARG_DEBUG_SUBTRACE	= 0x40000000,
	CAM_ARG_FLAG_MASK	= 0xfffffff0
} cam_argmask;

struct camcontrol_opts {
	char 		*optname;	
	cam_argmask	argnum;
	const char	*subopt;
};

extern int optreset;

static const char scsicmd_opts[] = "c:i:o:";
static const char readdefect_opts[] = "f:GP";

struct camcontrol_opts option_table[] = {
	{"tur", CAM_ARG_TUR, NULL},
	{"inquiry", CAM_ARG_INQUIRY, "DSR"},
	{"start", CAM_ARG_STARTSTOP | CAM_ARG_START_UNIT, NULL},
	{"stop", CAM_ARG_STARTSTOP, NULL},
	{"eject", CAM_ARG_STARTSTOP | CAM_ARG_EJECT, NULL},
	{"rescan", CAM_ARG_RESCAN, NULL},
	{"cmd", CAM_ARG_SCSI_CMD, scsicmd_opts},
	{"command", CAM_ARG_SCSI_CMD, scsicmd_opts},
	{"defects", CAM_ARG_READ_DEFECTS, readdefect_opts},
	{"defectlist", CAM_ARG_READ_DEFECTS, readdefect_opts},
	{"devlist", CAM_ARG_DEVTREE, NULL},
	{"periphlist", CAM_ARG_DEVLIST, NULL},
	{"modepage", CAM_ARG_MODE_PAGE, "dem:P:"},
	{"debug", CAM_ARG_DEBUG, "ITS"},
	{"help", CAM_ARG_USAGE, NULL},
	{"-?", CAM_ARG_USAGE, NULL},
	{"-h", CAM_ARG_USAGE, NULL},
	{NULL, 0, NULL}
};

typedef enum {
	CC_OR_NOT_FOUND,
	CC_OR_AMBIGUOUS,
	CC_OR_FOUND
} camcontrol_optret;

cam_argmask arglist;


camcontrol_optret getoption(char *arg, cam_argmask *argnum, char **subopt);
static int getdevlist(struct cam_device *device);
static int getdevtree(void);
static int testunitready(struct cam_device *device, int retry_count,
			 int timeout);
static int scsistart(struct cam_device *device, int startstop, int loadeject,
		     int retry_count, int timeout);
static int scsidoinquiry(struct cam_device *device, int argc, char **argv,
			 char *combinedopt, int retry_count, int timeout);
static int scsiinquiry(struct cam_device *device, int retry_count, int timeout);
static int scsiserial(struct cam_device *device, int retry_count, int timeout);
static int scsixferrate(struct cam_device *device);
static int dorescan(int argc, char **argv);
static int rescanbus(int bus);
static int scanlun(int bus, int target, int lun);
static int readdefects(struct cam_device *device, int argc, char **argv,
		       char *combinedopt, int retry_count, int timeout);
static void modepage(struct cam_device *device, int argc, char **argv,
		     char *combinedopt, int retry_count, int timeout);
static int scsicmd(struct cam_device *device, int argc, char **argv, 
		   char *combinedopt, int retry_count, int timeout);

camcontrol_optret
getoption(char *arg, cam_argmask *argnum, char **subopt)
{
	struct camcontrol_opts *opts;
	int num_matches = 0;

	for (opts = option_table; (opts != NULL) && (opts->optname != NULL);
	     opts++) {
		if (strncmp(opts->optname, arg, strlen(arg)) == 0) {
			*argnum = opts->argnum;
			*subopt = (char *)opts->subopt;
			if (++num_matches > 1)
				return(CC_OR_AMBIGUOUS);
		}
	}

	if (num_matches > 0)
		return(CC_OR_FOUND);
	else
		return(CC_OR_NOT_FOUND);
}

static int
getdevlist(struct cam_device *device)
{
	union ccb *ccb;
	char status[32];
	int error = 0;

	ccb = cam_getccb(device);

	ccb->ccb_h.func_code = XPT_GDEVLIST;
	ccb->ccb_h.flags = CAM_DIR_NONE;
	ccb->ccb_h.retry_count = 1;
	ccb->cgdl.index = 0;
	ccb->cgdl.status = CAM_GDEVLIST_MORE_DEVS;
	while (ccb->cgdl.status == CAM_GDEVLIST_MORE_DEVS) {
		if (cam_send_ccb(device, ccb) < 0) {
			perror("error getting device list");
			cam_freeccb(ccb);
			return(1);
		}

		status[0] = '\0';

		switch (ccb->cgdl.status) {
			case CAM_GDEVLIST_MORE_DEVS:
				strcpy(status, "MORE");
				break;
			case CAM_GDEVLIST_LAST_DEVICE:
				strcpy(status, "LAST");
				break;
			case CAM_GDEVLIST_LIST_CHANGED:
				strcpy(status, "CHANGED");
				break;
			case CAM_GDEVLIST_ERROR:
				strcpy(status, "ERROR");
				error = 1;
				break;
		}

		fprintf(stdout, "%s%d:  generation: %d index: %d status: %s\n",
			ccb->cgdl.periph_name,
			ccb->cgdl.unit_number,
			ccb->cgdl.generation,
			ccb->cgdl.index,
			status);

		/*
		 * If the list has changed, we need to start over from the
		 * beginning.
		 */
		if (ccb->cgdl.status == CAM_GDEVLIST_LIST_CHANGED)
			ccb->cgdl.index = 0;
	}

	cam_freeccb(ccb);

	return(error);
}

static int
getdevtree(void)
{
	union ccb ccb;
	int bufsize, i, fd;
	int need_close = 0;
	int error = 0;

	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		warn("couldn't open %s", XPT_DEVICE);
		return(1);
	}

	bzero(&(&ccb.ccb_h)[1], sizeof(struct ccb_dev_match));

	ccb.ccb_h.func_code = XPT_DEV_MATCH;
	bufsize = sizeof(struct dev_match_result) * 100;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
	ccb.cdm.num_matches = 0;

	/*
	 * We fetch all nodes, since we display most of them in the default
	 * case, and all in the verbose case.
	 */
	ccb.cdm.num_patterns = 0;
	ccb.cdm.pattern_buf_len = 0;

	/*
	 * We do the ioctl multiple times if necessary, in case there are
	 * more than 100 nodes in the EDT.
	 */
	do {
		if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
			warn("error sending CAMIOCOMMAND ioctl");
			error = 1;
			break;
		}

		if ((ccb.ccb_h.status != CAM_REQ_CMP)
		 || ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
		    && (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
			fprintf(stderr, "got CAM error %#x, CDM error %d\n",
				ccb.ccb_h.status, ccb.cdm.status);
			error = 1;
			break;
		}

		for (i = 0; i < ccb.cdm.num_matches; i++) {
			switch(ccb.cdm.matches[i].type) {
			case DEV_MATCH_BUS: {
				struct bus_match_result *bus_result;

				/*
				 * Only print the bus information if the
				 * user turns on the verbose flag.
				 */
				if ((arglist & CAM_ARG_VERBOSE) == 0)
					break;

				bus_result =
					&ccb.cdm.matches[i].result.bus_result;

				if (need_close) {
					fprintf(stdout, ")\n");
					need_close = 0;
				}

				fprintf(stdout, "scbus%d on %s%d bus %d:\n",
					bus_result->path_id,
					bus_result->dev_name,
					bus_result->unit_number,
					bus_result->bus_id);
				break;
			}
			case DEV_MATCH_DEVICE: {
				struct device_match_result *dev_result;
				char vendor[16], product[48], revision[16];
				char tmpstr[256];

				dev_result =
				     &ccb.cdm.matches[i].result.device_result;

				cam_strvis(vendor, dev_result->inq_data.vendor,
					   sizeof(dev_result->inq_data.vendor),
					   sizeof(vendor));
				cam_strvis(product,
					   dev_result->inq_data.product,
					   sizeof(dev_result->inq_data.product),
					   sizeof(product));
				cam_strvis(revision,
					   dev_result->inq_data.revision,
					  sizeof(dev_result->inq_data.revision),
					   sizeof(revision));
				sprintf(tmpstr, "<%s %s %s>", vendor, product,
					revision);
				if (need_close) {
					fprintf(stdout, ")\n");
					need_close = 0;
				}

				fprintf(stdout, "%-33s  at scbus%d "
					"target %d lun %d (",
					tmpstr,
					dev_result->path_id,
					dev_result->target_id,
					dev_result->target_lun);
				break;
			}
			case DEV_MATCH_PERIPH: {
				struct periph_match_result *periph_result;

				periph_result =
				      &ccb.cdm.matches[i].result.periph_result;

				if (need_close)
					fprintf(stdout, ",");

				fprintf(stdout, "%s%d",
					periph_result->periph_name,
					periph_result->unit_number);

				need_close = 1;
				break;
			}
			default:
				fprintf(stdout, "unknown match type\n");
				break;
			}
		}

	} while ((ccb.ccb_h.status == CAM_REQ_CMP)
		&& (ccb.cdm.status == CAM_DEV_MATCH_MORE));

	if (need_close)
		fprintf(stdout, ")\n");

	close(fd);

	return(error);
}

static int
testunitready(struct cam_device *device, int retry_count, int timeout)
{
	int error = 0;
	union ccb *ccb;

	ccb = cam_getccb(device);

	scsi_test_unit_ready(&ccb->csio,
			     /* retries */ retry_count,
			     /* cbfcnp */ NULL,
			     /* tag_action */ MSG_SIMPLE_Q_TAG,
			     /* sense_len */ SSD_FULL_SIZE,
			     /* timeout */ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending test unit ready");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) 
		fprintf(stdout, "Unit is ready\n");
	else {
		fprintf(stdout, "Unit is not ready\n");
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
	}

	cam_freeccb(ccb);

	return(error);
}

static int
scsistart(struct cam_device *device, int startstop, int loadeject,
	  int retry_count, int timeout)
{
	union ccb *ccb;
	int error = 0;

	if ((startstop < 0) || (startstop > 1)) {
		warnx("SCSI start/stop argument must be 0 or 1");
		return(1);
	}

	ccb = cam_getccb(device);

	scsi_start_stop(&ccb->csio,
			/* retries */ retry_count,
			/* cbfcnp */ NULL,
			/* tag_action */ MSG_SIMPLE_Q_TAG,
			/* start/stop */ startstop,
			/* load_eject */ loadeject,
			/* immediate */ 0,
			/* sense_len */ SSD_FULL_SIZE,
			/* timeout */ timeout ? timeout : 120000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending start unit");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
		if (startstop) {
			fprintf(stdout, "Unit started successfully");
			if (loadeject)
				fprintf(stdout,", Media loaded\n");
			else
				fprintf(stdout,"\n");
		} else {
			fprintf(stdout, "Unit stopped successfully");
			if (loadeject)
				fprintf(stdout, ", Media ejected\n");
			else
				fprintf(stdout, "\n");
		}
	else {
		error = 1;
		if (startstop)
			fprintf(stdout,
				"Error received from start unit command\n");
		else
			fprintf(stdout,
				"Error received from stop unit command\n");
			
		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
	}

	cam_freeccb(ccb);

	return(error);
}

static int
scsidoinquiry(struct cam_device *device, int argc, char **argv,
	      char *combinedopt, int retry_count, int timeout)
{
	int c;
	int error = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'D':
			arglist |= CAM_ARG_GET_STDINQ;
			break;
		case 'R':
			arglist |= CAM_ARG_GET_XFERRATE;
			break;
		case 'S':
			arglist |= CAM_ARG_GET_SERIAL;
			break;
		default:
			break;
		}
	}

	/*
	 * If the user didn't specify any inquiry options, he wants all of
	 * them.
	 */
	if ((arglist & CAM_ARG_INQ_MASK) == 0)
		arglist |= CAM_ARG_INQ_MASK;

	if (arglist & CAM_ARG_GET_STDINQ)
		error = scsiinquiry(device, retry_count, timeout);

	if (error != 0)
		return(error);

	if (arglist & CAM_ARG_GET_SERIAL)
		scsiserial(device, retry_count, timeout);

	if (error != 0)
		return(error);

	if (arglist & CAM_ARG_GET_XFERRATE)
		error = scsixferrate(device);

	return(error);
}

static int
scsiinquiry(struct cam_device *device, int retry_count, int timeout)
{
	union ccb *ccb;
	struct scsi_inquiry_data *inq_buf;
	int error = 0;
	
	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("couldn't allocate CCB");
		return(1);
	}

	/* cam_getccb cleans up the header, caller has to zero the payload */
	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	inq_buf = (struct scsi_inquiry_data *)malloc(
		sizeof(struct scsi_inquiry_data));

	if (inq_buf == NULL) {
		cam_freeccb(ccb);
		warnx("can't malloc memory for inquiry\n");
		return(1);
	}
	bzero(inq_buf, sizeof(*inq_buf));

	scsi_inquiry(&ccb->csio,
		     /* retries */ retry_count,
		     /* cbfcnp */ NULL,
		     /* tag_action */ MSG_SIMPLE_Q_TAG,
		     /* inq_buf */ (u_int8_t *)inq_buf,
		     /* inq_len */ sizeof(struct scsi_inquiry_data),
		     /* evpd */ 0,
		     /* page_code */ 0,
		     /* sense_len */ SSD_FULL_SIZE,
		     /* timeout */ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending SCSI inquiry");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
	}

	cam_freeccb(ccb);

	if (error != 0) {
		free(inq_buf);
		return(error);
	}

	scsi_print_inquiry(inq_buf);

	free(inq_buf);

	if (arglist & CAM_ARG_GET_SERIAL)
		fprintf(stdout, "Serial Number ");

	return(0);
}

static int
scsiserial(struct cam_device *device, int retry_count, int timeout)
{
	union ccb *ccb;
	struct scsi_vpd_unit_serial_number *serial_buf;
	char serial_num[SVPD_SERIAL_NUM_SIZE + 1];
	int error = 0;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("couldn't allocate CCB");
		return(1);
	}

	/* cam_getccb cleans up the header, caller has to zero the payload */
	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	serial_buf = (struct scsi_vpd_unit_serial_number *)
		malloc(sizeof(*serial_buf));

	if (serial_buf == NULL) {
		cam_freeccb(ccb);
		warnx("can't malloc memory for serial number");
		return(1);
	}

	scsi_inquiry(&ccb->csio,
		     /*retries*/ retry_count,
		     /*cbfcnp*/ NULL,
		     /* tag_action */ MSG_SIMPLE_Q_TAG,
		     /* inq_buf */ (u_int8_t *)serial_buf,
		     /* inq_len */ sizeof(*serial_buf),
		     /* evpd */ 1,
		     /* page_code */ SVPD_UNIT_SERIAL_NUMBER,
		     /* sense_len */ SSD_FULL_SIZE,
		     /* timeout */ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error getting serial number");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		cam_freeccb(ccb);
		free(serial_buf);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
	}

	cam_freeccb(ccb);

	if (error != 0) {
		free(serial_buf);
		return(error);
	}

	bcopy(serial_buf->serial_num, serial_num, serial_buf->length);
	serial_num[serial_buf->length] = '\0';

	if (((arglist & CAM_ARG_GET_STDINQ) == 0)
	 && (arglist & CAM_ARG_GET_XFERRATE))
		fprintf(stdout, "Serial Number ");

	fprintf(stdout, "%.60s\n", serial_num);

	free(serial_buf);

	return(0);
}

static int
scsixferrate(struct cam_device *device)
{
	u_int32_t freq;
	u_int32_t speed;

	if (device->sync_period != 0)
		freq = scsi_calc_syncsrate(device->sync_period);
	else 
		freq = 0;

	speed = freq;
	speed *= (0x01 << device->bus_width);
	fprintf(stdout, "%d.%dMB/s transfers ", speed / 1000, speed % 1000);

	if (device->sync_period != 0)
                fprintf(stdout, "(%d.%dMHz, offset %d", freq / 1000,
			freq % 1000, device->sync_offset);

        if (device->bus_width != 0) {
                if (device->sync_period == 0)
                        fprintf(stdout, "(");
                else
                        fprintf(stdout, ", "); 
                fprintf(stdout, "%dbit)", 8 * (0x01 << device->bus_width));
        } else if (device->sync_period != 0)
                fprintf(stdout, ")");
                                 
        if (device->inq_data.flags & SID_CmdQue)
                fprintf(stdout, ", Tagged Queueing Enabled");
 
        fprintf(stdout, "\n");

	return(0);
}

static int
dorescan(int argc, char **argv)
{
	int error = 0;
	int bus = -1, target = -1, lun = -1;
	char *tstr, *tmpstr = NULL;

	if (argc < 3) {
		warnx("you must specify a bus, or a bus:target:lun to rescan");
		return(1);
	}
	/*
	 * Parse out a bus, or a bus, target and lun in the following
	 * format:
	 * bus
	 * bus:target:lun
	 * It is an error to specify a bus and target, but not a lun.
	 */
	tstr = argv[optind];

	while (isspace(*tstr) && (*tstr != '\0'))
		tstr++;

	tmpstr = (char *)strtok(tstr, ":");
	if ((tmpstr != NULL) && (*tmpstr != '\0')){
		bus = strtol(tmpstr, NULL, 0);
		arglist |= CAM_ARG_BUS;
		tmpstr = (char *)strtok(NULL, ":");
		if ((tmpstr != NULL) && (*tmpstr != '\0')){
			target = strtol(tmpstr, NULL, 0);
			arglist |= CAM_ARG_TARGET;
			tmpstr = (char *)strtok(NULL, ":");
			if ((tmpstr != NULL) && (*tmpstr != '\0')){
				lun = strtol(tmpstr, NULL, 0);
				arglist |= CAM_ARG_LUN;
			} else {
				error = 1;
				warnx("you must specify either a bus or");
				warnx("a bus, target and lun for rescanning");
			}
		}
	} else {
		error = 1;
		warnx("you must at least specify a bus to rescan");
	}


	if (error == 0) {
		if ((arglist & CAM_ARG_BUS)
		 && (arglist & CAM_ARG_TARGET)
		 && (arglist & CAM_ARG_LUN))
			error = scanlun(bus, target, lun);
		else if (arglist & CAM_ARG_BUS)
			error = rescanbus(bus);
		else {
			error = 1;
			warnx("you must specify either a bus or");
			warnx("a bus, target and lun for rescanning");
		}
	}
	return(error);
}

static int
rescanbus(int bus)
{
	union ccb ccb;
	int fd;

	if (bus < 0) {
		warnx("invalid bus number %d", bus);
		return(1);
	}

	if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
		warnx("error opening tranport layer device %s", XPT_DEVICE);
		warn("%s", XPT_DEVICE);
		return(1);
	}

	ccb.ccb_h.func_code = XPT_SCAN_BUS;
	ccb.ccb_h.path_id = bus;
	ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
	ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;
	ccb.crcn.flags = CAM_FLAG_NONE;

	/* run this at a low priority */
	ccb.ccb_h.pinfo.priority = 5;

	if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
		warn("CAMIOCOMMAND ioctl failed");
		close(fd);
		return(1);
	}

	close(fd);

	if ((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
		fprintf(stdout, "Re-scan of bus %d was successful\n", bus);
		return(0);
	} else {
		fprintf(stdout, "Re-scan of bus %d returned error %#x\n",
			bus, ccb.ccb_h.status & CAM_STATUS_MASK);
		return(1);
	}
}

static int
scanlun(int bus, int target, int lun)
{
	union ccb ccb;
	int fd;

	if (bus < 0) {
		warnx("invalid bus number %d", bus);
		return(1);
	}

	if (target < 0) {
		warnx("invalid target number %d", target);
		return(1);
	}

	if (lun < 0) {
		warnx("invalid lun number %d", lun);
		return(1);
	}

	if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
		warnx("error opening tranport layer device %s\n",
			XPT_DEVICE);
		warn("%s", XPT_DEVICE);
		return(1);
	}

	ccb.ccb_h.func_code = XPT_SCAN_LUN;
	ccb.ccb_h.path_id = bus;
	ccb.ccb_h.target_id = target;
	ccb.ccb_h.target_lun = lun;
	ccb.crcn.flags = CAM_FLAG_NONE;

	/* run this at a low priority */
	ccb.ccb_h.pinfo.priority = 5;

	if (ioctl(fd, CAMIOCOMMAND, &ccb) < 0) {
		warn("CAMIOCOMMAND ioctl failed");
		close(fd);
		return(1);
	}

	close(fd);

	if ((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
		fprintf(stdout, "Re-scan of %d:%d:%d was successful\n",
			bus, target, lun);
		return(0);
	} else {
		fprintf(stdout, "Re-scan of %d:%d:%d returned error %#x\n",
			bus, target, lun, ccb.ccb_h.status & CAM_STATUS_MASK);
		return(1);
	}
}

static int
readdefects(struct cam_device *device, int argc, char **argv,
	    char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb = NULL;
	struct scsi_read_defect_data_10 *rdd_cdb;
	u_int8_t *defect_list = NULL;
	u_int32_t dlist_length = 65000;
	u_int32_t returned_length = 0;
	u_int32_t num_returned = 0;
	u_int8_t returned_format;
	register int i;
	int c, error = 0;
	int lists_specified = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c){
		case 'f':
		{
			char *tstr;
			tstr = optarg;
			while (isspace(*tstr) && (*tstr != '\0'))
				tstr++;
			if (strcmp(tstr, "block") == 0)
				arglist |= CAM_ARG_FORMAT_BLOCK;
			else if (strcmp(tstr, "bfi") == 0)
				arglist |= CAM_ARG_FORMAT_BFI;
			else if (strcmp(tstr, "phys") == 0)
				arglist |= CAM_ARG_FORMAT_PHYS;
			else {
				error = 1;
				warnx("invalid defect format %s", tstr);
				goto defect_bailout;
			}
			break;
		}
		case 'G':
			arglist |= CAM_ARG_GLIST;
			break;
		case 'P':
			arglist |= CAM_ARG_PLIST;
			break;
		default:
			break;
		}
	}

	ccb = cam_getccb(device);

	/*
	 * Hopefully 65000 bytes is enough to hold the defect list.  If it
	 * isn't, the disk is probably dead already.  We'd have to go with
	 * 12 byte command (i.e. alloc_length is 32 bits instead of 16)
	 * to hold them all.
	 */
	defect_list = malloc(dlist_length);

	rdd_cdb =(struct scsi_read_defect_data_10 *)&ccb->csio.cdb_io.cdb_bytes;

	/*
	 * cam_getccb() zeros the CCB header only.  So we need to zero the
	 * payload portion of the ccb.
	 */
	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	cam_fill_csio(&ccb->csio,
		      /*retries*/ retry_count,
		      /*cbfcnp*/ NULL,
		      /*flags*/ CAM_DIR_IN | (arglist & CAM_ARG_ERR_RECOVER) ?
					      CAM_PASS_ERR_RECOVER : 0,
		      /*tag_action*/ MSG_SIMPLE_Q_TAG,
		      /*data_ptr*/ defect_list,
		      /*dxfer_len*/ dlist_length,
		      /*sense_len*/ SSD_FULL_SIZE,
		      /*cdb_len*/ sizeof(struct scsi_read_defect_data_10),
		      /*timeout*/ timeout ? timeout : 5000);

	rdd_cdb->opcode = READ_DEFECT_DATA_10;
	if (arglist & CAM_ARG_FORMAT_BLOCK)
		rdd_cdb->format = SRDD10_BLOCK_FORMAT;
	else if (arglist & CAM_ARG_FORMAT_BFI)
		rdd_cdb->format = SRDD10_BYTES_FROM_INDEX_FORMAT;
	else if (arglist & CAM_ARG_FORMAT_PHYS)
		rdd_cdb->format = SRDD10_PHYSICAL_SECTOR_FORMAT;
	else {
		error = 1;
		warnx("no defect list format specified");
		goto defect_bailout;
	}
	if (arglist & CAM_ARG_PLIST) {
		rdd_cdb->format |= SRDD10_PLIST;
		lists_specified++;
	}

	if (arglist & CAM_ARG_GLIST) {
		rdd_cdb->format |= SRDD10_GLIST;
		lists_specified++;
	}

	scsi_ulto2b(dlist_length, rdd_cdb->alloc_length);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error reading defect list");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		error = 1;
		goto defect_bailout;
	}

	if (arglist & CAM_ARG_VERBOSE)
		scsi_sense_print(device, &ccb->csio, stderr);

	returned_length = scsi_2btoul(((struct
		scsi_read_defect_data_hdr_10 *)defect_list)->length);

	returned_format = ((struct scsi_read_defect_data_hdr_10 *)
			defect_list)->format;

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		struct scsi_sense_data *sense;
		int error_code, sense_key, asc, ascq;

		sense = &ccb->csio.sense_data;
		scsi_extract_sense(sense, &error_code, &sense_key, &asc, &ascq);

		/*
		 * According to the SCSI spec, if the disk doesn't support
		 * the requested format, it will generally return a sense
		 * key of RECOVERED ERROR, and an additional sense code
		 * of "DEFECT LIST NOT FOUND".  So, we check for that, and
		 * also check to make sure that the returned length is
		 * greater than 0, and then print out whatever format the
		 * disk gave us.
		 */
		if ((sense_key == SSD_KEY_RECOVERED_ERROR)
		 && (asc == 0x1c) && (ascq == 0x00)
		 && (returned_length > 0)) {
			warnx("requested defect format not available");
			switch(returned_format & SRDDH10_DLIST_FORMAT_MASK) {
			case SRDD10_BLOCK_FORMAT:
				warnx("Device returned block format");
				break;
			case SRDD10_BYTES_FROM_INDEX_FORMAT:
				warnx("Device returned bytes from index"
				      " format");
				break;
			case SRDD10_PHYSICAL_SECTOR_FORMAT:
				warnx("Device returned physical sector format");
				break;
			default:
				error = 1;
				warnx("Device returned unknown defect"
				     " data format %#x", returned_format);
				goto defect_bailout;
				break; /* NOTREACHED */
			}
		} else {
			error = 1;
			warnx("Error returned from read defect data command");
			goto defect_bailout;
		}
	}

	/*
	 * XXX KDM  I should probably clean up the printout format for the
	 * disk defects. 
	 */
	switch (returned_format & SRDDH10_DLIST_FORMAT_MASK){
		case SRDDH10_PHYSICAL_SECTOR_FORMAT:
		{
			struct scsi_defect_desc_phys_sector *dlist;

			dlist = (struct scsi_defect_desc_phys_sector *)
				(defect_list +
				sizeof(struct scsi_read_defect_data_hdr_10));

			num_returned = returned_length /
				sizeof(struct scsi_defect_desc_phys_sector);

			fprintf(stderr, "Got %d defect", num_returned);

			if ((lists_specified == 0) || (num_returned == 0)) {
				fprintf(stderr, "s.\n");
				break;
			} else if (num_returned == 1)
				fprintf(stderr, ":\n");
			else
				fprintf(stderr, "s:\n");

			for (i = 0; i < num_returned; i++) {
				fprintf(stdout, "%d:%d:%d\n",
					scsi_3btoul(dlist[i].cylinder),
					dlist[i].head,
					scsi_4btoul(dlist[i].sector));
			}
			break;
		}
		case SRDDH10_BYTES_FROM_INDEX_FORMAT:
		{
			struct scsi_defect_desc_bytes_from_index *dlist;

			dlist = (struct scsi_defect_desc_bytes_from_index *)
				(defect_list +
				sizeof(struct scsi_read_defect_data_hdr_10));

			num_returned = returned_length /
			      sizeof(struct scsi_defect_desc_bytes_from_index);

			fprintf(stderr, "Got %d defect", num_returned);

			if ((lists_specified == 0) || (num_returned == 0)) {
				fprintf(stderr, "s.\n");
				break;
			} else if (num_returned == 1)
				fprintf(stderr, ":\n");
			else
				fprintf(stderr, "s:\n");

			for (i = 0; i < num_returned; i++) {
				fprintf(stdout, "%d:%d:%d\n",
					scsi_3btoul(dlist[i].cylinder),
					dlist[i].head,
					scsi_4btoul(dlist[i].bytes_from_index));
			}
			break;
		}
		case SRDDH10_BLOCK_FORMAT:
		{
			struct scsi_defect_desc_block *dlist;

			dlist = (struct scsi_defect_desc_block *)(defect_list +
				sizeof(struct scsi_read_defect_data_hdr_10));

			num_returned = returned_length /
			      sizeof(struct scsi_defect_desc_block);

			fprintf(stderr, "Got %d defect", num_returned);

			if ((lists_specified == 0) || (num_returned == 0)) {
				fprintf(stderr, "s.\n");
				break;
			} else if (num_returned == 1)
				fprintf(stderr, ":\n");
			else
				fprintf(stderr, "s:\n");

			for (i = 0; i < num_returned; i++)
				fprintf(stdout, "%u\n",
					scsi_4btoul(dlist[i].address));
			break;
		}
		default:
			fprintf(stderr, "Unknown defect format %d\n",
				returned_format & SRDDH10_DLIST_FORMAT_MASK);
			error = 1;
			break;
	}
defect_bailout:

	if (defect_list != NULL)
		free(defect_list);

	if (ccb != NULL)
		cam_freeccb(ccb);

	return(error);
}

#if 0
void
reassignblocks(struct cam_device *device, u_int32_t *blocks, int num_blocks)
{
	union ccb *ccb;
	
	ccb = cam_getccb(device);

	cam_freeccb(ccb);
}
#endif

void
mode_sense(struct cam_device *device, int mode_page, int page_control,
	   int dbd, int retry_count, int timeout, u_int8_t *data, int datalen)
{
	union ccb *ccb;
	int retval;

	ccb = cam_getccb(device);

	if (ccb == NULL)
		errx(1, "mode_sense: couldn't allocate CCB");

	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	scsi_mode_sense(&ccb->csio,
			/* retries */ retry_count,
			/* cbfcnp */ NULL,
			/* tag_action */ MSG_SIMPLE_Q_TAG,
			/* dbd */ dbd,
			/* page_code */ page_control << 6,
			/* page */ mode_page,
			/* param_buf */ data,
			/* param_len */ datalen,
			/* sense_len */ SSD_FULL_SIZE,
			/* timeout */ timeout ? timeout : 5000);

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
		cam_freeccb(ccb);
		cam_close_device(device);
		if (retval < 0)
			err(1, "error sending mode sense command");
		else
			errx(1, "error sending mode sense command");
	}

	cam_freeccb(ccb);
}

void
mode_select(struct cam_device *device, int save_pages, int retry_count,
	   int timeout, u_int8_t *data, int datalen)
{
	union ccb *ccb;
	int retval;

	ccb = cam_getccb(device);

	if (ccb == NULL)
		errx(1, "mode_select: couldn't allocate CCB");

	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	scsi_mode_select(&ccb->csio,
			 /* retries */ retry_count,
			 /* cbfcnp */ NULL,
			 /* tag_action */ MSG_SIMPLE_Q_TAG,
			 /* scsi_page_fmt */ 1,
			 /* save_pages */ save_pages,
			 /* param_buf */ data,
			 /* param_len */ datalen,
			 /* sense_len */ SSD_FULL_SIZE,
			 /* timeout */ timeout ? timeout : 5000);

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}
		cam_freeccb(ccb);
		cam_close_device(device);

		if (retval < 0)
			err(1, "error sending mode select command");
		else
			errx(1, "error sending mode select command");
		
	}

	cam_freeccb(ccb);
}

void
modepage(struct cam_device *device, int argc, char **argv, char *combinedopt,
	 int retry_count, int timeout)
{
	int c, mode_page = -1, page_control = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'd':
			arglist |= CAM_ARG_DBD;
			break;
		case 'e':
			arglist |= CAM_ARG_MODE_EDIT;
			break;
		case 'm':
			mode_page = strtol(optarg, NULL, 0);
			if (mode_page < 0)
				errx(1, "invalid mode page %d", mode_page);
			break;
		case 'P':
			page_control = strtol(optarg, NULL, 0);
			if ((page_control < 0) || (page_control > 3))
				errx(1, "invalid page control field %d",
				     page_control);
			arglist |= CAM_ARG_PAGE_CNTL;
			break;
		default:
			break;
		}
	}

	if (mode_page == -1)
		errx(1, "you must specify a mode page!");

	mode_edit(device, mode_page, page_control, arglist & CAM_ARG_DBD,
		  arglist & CAM_ARG_MODE_EDIT, retry_count, timeout);
}

static int
scsicmd(struct cam_device *device, int argc, char **argv, char *combinedopt,
	int retry_count, int timeout)
{
	union ccb *ccb;
	u_int32_t flags = CAM_DIR_NONE;
	u_int8_t *data_ptr = NULL;
	u_int8_t cdb[20];
	struct get_hook hook;
	int c, data_bytes = 0;
	int cdb_len = 0;
	char *datastr = NULL, *tstr;
	int error = 0;
	int fd_data = 0;
	int retval;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("scsicmd: error allocating ccb");
		return(1);
	}

	bzero(&(&ccb->ccb_h)[1], sizeof(struct ccb_scsiio));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'c':
			tstr = optarg;
			while (isspace(*tstr) && (*tstr != '\0'))
				tstr++;
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			buff_encode_visit(cdb, sizeof(cdb), tstr,
					  iget, &hook);
			/*
			 * Increment optind by the number of arguments the
			 * encoding routine processed.  After each call to
			 * getopt(3), optind points to the argument that
			 * getopt should process _next_.  In this case,
			 * that means it points to the first command string
			 * argument, if there is one.  Once we increment
			 * this, it should point to either the next command
			 * line argument, or it should be past the end of
			 * the list.
			 */
			optind += hook.got;
			break;
		case 'i':
			if (arglist & CAM_ARG_CMD_OUT) {
				warnx("command must either be "
				      "read or write, not both");
				error = 1;
				goto scsicmd_bailout;
			}
			arglist |= CAM_ARG_CMD_IN;
			flags = CAM_DIR_IN;
			data_bytes = strtol(optarg, NULL, 0);
			if (data_bytes <= 0) {
				warnx("invalid number of input bytes %d",
				      data_bytes);
				error = 1;
				goto scsicmd_bailout;
			}
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			optind++;
			datastr = cget(&hook, NULL);
			/*
			 * If the user supplied "-" instead of a format, he
			 * wants the data to be written to stdout.
			 */
			if ((datastr != NULL)
			 && (datastr[0] == '-'))
				fd_data = 1;

			data_ptr = (u_int8_t *)malloc(data_bytes);
			break;
		case 'o':
			if (arglist & CAM_ARG_CMD_IN) {
				warnx("command must either be "
				      "read or write, not both");
				error = 1;	
				goto scsicmd_bailout;
			}
			arglist |= CAM_ARG_CMD_OUT;
			flags = CAM_DIR_OUT;
			data_bytes = strtol(optarg, NULL, 0);
			if (data_bytes <= 0) {
				warnx("invalid number of output bytes %d",
				      data_bytes);
				error = 1;
				goto scsicmd_bailout;
			}
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			datastr = cget(&hook, NULL);
			data_ptr = (u_int8_t *)malloc(data_bytes);
			/*
			 * If the user supplied "-" instead of a format, he
			 * wants the data to be read from stdin.
			 */
			if ((datastr != NULL)
			 && (datastr[0] == '-'))
				fd_data = 1;
			else
				buff_encode_visit(data_ptr, data_bytes, datastr,
						  iget, &hook);
			optind += hook.got;
			break;
		default:
			break;
		}
	}

	/*
	 * If fd_data is set, and we're writing to the device, we need to
	 * read the data the user wants written from stdin.
	 */
	if ((fd_data == 1) && (arglist & CAM_ARG_CMD_OUT)) {
		size_t amt_read;
		int amt_to_read = data_bytes;
		u_int8_t *buf_ptr = data_ptr;

		for (amt_read = 0; amt_to_read > 0;
		     amt_read = read(0, buf_ptr, amt_to_read)) {
			if (amt_read == -1) {
				warn("error reading data from stdin");
				error = 1;
				goto scsicmd_bailout;
			}
			amt_to_read -= amt_read;
			buf_ptr += amt_read;
		}
	}

	if (arglist & CAM_ARG_ERR_RECOVER)
		flags |= CAM_PASS_ERR_RECOVER;

	/* Disable freezing the device queue */
	flags |= CAM_DEV_QFRZDIS;

	/*
	 * This is taken from the SCSI-3 draft spec.
	 * (T10/1157D revision 0.3)
	 * The top 3 bits of an opcode are the group code.  The next 5 bits
	 * are the command code.
	 * Group 0:  six byte commands
	 * Group 1:  ten byte commands
	 * Group 2:  ten byte commands
	 * Group 3:  reserved
	 * Group 4:  sixteen byte commands
	 * Group 5:  twelve byte commands
	 * Group 6:  vendor specific
	 * Group 7:  vendor specific
	 */
	switch((cdb[0] >> 5) & 0x7) {
		case 0:
			cdb_len = 6;
			break;
		case 1:
		case 2:
			cdb_len = 10;
			break;
		case 3:
		case 6:
		case 7:
			cdb_len = 1;
			break;
		case 4:
			cdb_len = 16;
			break;
		case 5:
			cdb_len = 12;
			break;
	}

	/*
	 * We should probably use csio_build_visit or something like that
	 * here, but it's easier to encode arguments as you go.  The
	 * alternative would be skipping the CDB argument and then encoding
	 * it here, since we've got the data buffer argument by now.
	 */
	bcopy(cdb, &ccb->csio.cdb_io.cdb_bytes, cdb_len);

	cam_fill_csio(&ccb->csio,
		      /*retries*/ retry_count,
		      /*cbfcnp*/ NULL,
		      /*flags*/ flags,
		      /*tag_action*/ MSG_SIMPLE_Q_TAG,
		      /*data_ptr*/ data_ptr,
		      /*dxfer_len*/ data_bytes,
		      /*sense_len*/ SSD_FULL_SIZE,
		      /*cdb_len*/ cdb_len,
		      /*timeout*/ timeout ? timeout : 5000);

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		if (retval < 0)
			warn("error sending command");
		else
			warnx("error sending command");

		if (arglist & CAM_ARG_VERBOSE) {
		 	if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
			    CAM_SCSI_STATUS_ERROR)
				scsi_sense_print(device, &ccb->csio, stderr);
			else
				fprintf(stderr, "CAM status is %#x\n",
					ccb->ccb_h.status);
		}

		error = 1;
		goto scsicmd_bailout;
	}


	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
	 && (arglist & CAM_ARG_CMD_IN)
	 && (data_bytes > 0)) {
		if (fd_data == 0) {
			buff_decode_visit(data_ptr, data_bytes, datastr,
					  arg_put, NULL);
			fprintf(stdout, "\n");
		} else {
			size_t amt_written;
			int amt_to_write = data_bytes;
			u_int8_t *buf_ptr = data_ptr;

			for (amt_written = 0; (amt_to_write > 0) &&
			     (amt_written =write(1, buf_ptr,amt_to_write))> 0;){
				amt_to_write -= amt_written;
				buf_ptr += amt_written;
			}
			if (amt_written == -1) {
				warn("error writing data to stdout");
				error = 1;
				goto scsicmd_bailout;
			} else if ((amt_written == 0)
				&& (amt_to_write > 0)) {
				warnx("only wrote %u bytes out of %u",
				      data_bytes - amt_to_write, data_bytes);
			}
		}
	}

scsicmd_bailout:

	if ((data_bytes > 0) && (data_ptr != NULL))
		free(data_ptr);

	cam_freeccb(ccb);

	return(error);
}

static int
camdebug(int argc, char **argv, char *combinedopt)
{
	int c, fd;
	int bus = -1, target = -1, lun = -1;
	char *tstr, *tmpstr = NULL;
	union ccb ccb;
	int error = 0;

	bzero(&ccb, sizeof(union ccb));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'I':
			arglist |= CAM_ARG_DEBUG_INFO;
			ccb.cdbg.flags |= CAM_DEBUG_INFO;
			break;
		case 'S':
			arglist |= CAM_ARG_DEBUG_TRACE;
			ccb.cdbg.flags |= CAM_DEBUG_TRACE;
			break;
		case 'T':
			arglist |= CAM_ARG_DEBUG_SUBTRACE;
			ccb.cdbg.flags |= CAM_DEBUG_SUBTRACE;
			break;
		default:
			break;
		}
	}

	if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
		warnx("error opening transport layer device %s", XPT_DEVICE);
		warn("%s", XPT_DEVICE);
		return(1);
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		warnx("you must specify \"off\", \"all\" or a bus,");
		warnx("bus:target, or bus:target:lun");
		close(fd);
		return(1);
	}

	tstr = *argv;

	while (isspace(*tstr) && (*tstr != '\0'))
		tstr++;

	if (strncmp(tstr, "off", 3) == 0) {
		ccb.cdbg.flags = CAM_DEBUG_NONE;
		arglist &= ~(CAM_ARG_DEBUG_INFO|CAM_ARG_DEBUG_TRACE|
			     CAM_ARG_DEBUG_SUBTRACE);
	} else if (strncmp(tstr, "all", 3) != 0) {
		tmpstr = (char *)strtok(tstr, ":");
		if ((tmpstr != NULL) && (*tmpstr != '\0')){
			bus = strtol(tmpstr, NULL, 0);
			arglist |= CAM_ARG_BUS;
			tmpstr = (char *)strtok(NULL, ":");
			if ((tmpstr != NULL) && (*tmpstr != '\0')){
				target = strtol(tmpstr, NULL, 0);
				arglist |= CAM_ARG_TARGET;
				tmpstr = (char *)strtok(NULL, ":");
				if ((tmpstr != NULL) && (*tmpstr != '\0')){
					lun = strtol(tmpstr, NULL, 0);
					arglist |= CAM_ARG_LUN;
				}
			}
		} else {
			error = 1;
			warnx("you must specify \"all\", \"off\", or a bus,");
			warnx("bus:target, or bus:target:lun to debug");
		}
	}
	
	if (error == 0) {

		ccb.ccb_h.func_code = XPT_DEBUG;
		ccb.ccb_h.path_id = bus;
		ccb.ccb_h.target_id = target;
		ccb.ccb_h.target_lun = lun;

		if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
			warn("CAMIOCOMMAND ioctl failed");
			error = 1;
		}

		if (error == 0) {
			if ((ccb.ccb_h.status & CAM_STATUS_MASK) ==
			     CAM_FUNC_NOTAVAIL) {
				warnx("CAM debugging not available");
				warnx("you need to put options CAMDEBUG in"
				      " your kernel config file!");
				error = 1;
			} else if ((ccb.ccb_h.status & CAM_STATUS_MASK) !=
				    CAM_REQ_CMP) {
				warnx("XPT_DEBUG CCB failed with status %#x",
				      ccb.ccb_h.status);
				error = 1;
			} else {
				if (ccb.cdbg.flags == CAM_DEBUG_NONE) {
					fprintf(stderr,
						"Debugging turned off\n");
				} else {
					fprintf(stderr,
						"Debugging enabled for "
						"%d:%d:%d\n",
						bus, target, lun);
				}
			}
		}
		close(fd);	
	}

	return(error);
}

void 
usage(void)
{
	fprintf(stderr,
"usage:  camcontrol <command> [ generic args ] [ command args ]\n"
"        camcontrol devlist    [-v]\n"
"        camcontrol periphlist [-n dev_name] [-u unit]\n"
"        camcontrol tur        [generic args]\n"
"        camcontrol inquiry    [generic args] [-D] [-S] [-R]\n"
"        camcontrol start      [generic args]\n"
"        camcontrol stop       [generic args]\n"
"        camcontrol eject      [generic args]\n"
"        camcontrol rescan     <bus[:target:lun]>\n"
"        camcontrol defects    [generic args] <-f format> [-P][-G]\n"
"        camcontrol modepage   [generic args] <-m page> [-P pagectl][-e][-d]\n"
"        camcontrol cmd        [generic args] <-c cmd [args]> \n"
"                              [-i len fmt|-o len fmt [args]]\n"
"        camcontrol debug      [-I][-T][-S] <all|bus[:target[:lun]]|off>\n"
"Specify one of the following options:\n"
"devlist     list all CAM devices\n"
"periphlist  list all CAM peripheral drivers attached to a device\n"
"tur         send a test unit ready to the named device\n"
"inquiry     send a SCSI inquiry command to the named device\n"
"start       send a Start Unit command to the device\n"
"stop        send a Stop Unit command to the device\n"
"eject       send a Stop Unit command to the device with the eject bit set\n"
"rescan      rescan the given bus, or bus:target:lun\n"
"defects     read the defect list of the specified device\n"
"modepage    display or edit (-e) the given mode page\n"
"cmd         send the given scsi command, may need -i or -o as well\n"
"debug       turn debugging on/off for a bus, target, or lun, or all devices\n"
"Generic arguments:\n"
"-v                be verbose, print out sense information\n"
"-t timeout        command timeout in seconds, overrides default timeout\n"
"-n dev_name       specify device name (default is %s)\n"
"-u unit           specify unit number (default is %d)\n"
"-E                have the kernel attempt to perform SCSI error recovery\n"
"-C count          specify the SCSI command retry count (needs -E to work)\n"
"modepage arguments:\n"
"-e                edit the specified mode page\n"
"-B                disable block descriptors for mode sense\n"
"-P pgctl          page control field 0-3\n"
"defects arguments:\n"
"-f format         specify defect list format (block, bfi or phys)\n"
"-G                get the grown defect list\n"
"-P                get the permanant defect list\n"
"inquiry arguments:\n"
"-D                get the standard inquiry data\n"
"-S                get the serial number\n"
"-R                get the transfer rate, etc.\n"
"cmd arguments:\n"
"-c cdb [args]     specify the SCSI CDB\n"
"-i len fmt        specify input data and input data format\n"
"-o len fmt [args] specify output data and output data fmt\n"
"debug arguments:\n"
"-I                CAM_DEBUG_INFO -- scsi commands, errors, data\n"
"-T                CAM_DEBUG_TRACE -- routine flow tracking\n"
"-S                CAM_DEBUG_SUBTRACE -- internal routine command flow\n",
DEFAULT_DEVICE, DEFAULT_UNIT);
}

int 
main(int argc, char **argv)
{
	int c;
	char *device = NULL;
	int unit = 0;
	struct cam_device *cam_dev = NULL;
	int timeout = 0, retry_count = 1;
	camcontrol_optret optreturn;
	char *tstr;
	char *mainopt = "C:En:t:u:v";
	char *subopt = NULL;
	char combinedopt[256];
	int error = 0;

	arglist = CAM_ARG_NONE;

	if (argc < 2) {
		usage();
		exit(1);
	}

	/*
	 * Get the base option.
	 */
	optreturn = getoption(argv[1], &arglist, &subopt);

	if (optreturn == CC_OR_AMBIGUOUS) {
		warnx("ambiguous option %s", argv[1]);
		usage();
		exit(1);
	} else if (optreturn == CC_OR_NOT_FOUND) {
		warnx("option %s not found", argv[1]);
		usage();
		exit(1);
	}

	/*
	 * Ahh, getopt(3) is a pain.
	 *
	 * This is a gross hack.  There really aren't many other good
	 * options (excuse the pun) for parsing options in a situation like
	 * this.  getopt is kinda braindead, so you end up having to run
	 * through the options twice, and give each invocation of getopt
	 * the option string for the other invocation.
	 * 
	 * You would think that you could just have two groups of options.
	 * The first group would get parsed by the first invocation of
	 * getopt, and the second group would get parsed by the second
	 * invocation of getopt.  It doesn't quite work out that way.  When
	 * the first invocation of getopt finishes, it leaves optind pointing
	 * to the argument _after_ the first argument in the second group.
	 * So when the second invocation of getopt comes around, it doesn't
	 * recognize the first argument it gets and then bails out.
	 * 
	 * A nice alternative would be to have a flag for getopt that says
	 * "just keep parsing arguments even when you encounter an unknown
	 * argument", but there isn't one.  So there's no real clean way to
	 * easily parse two sets of arguments without having one invocation
	 * of getopt know about the other.
	 * 
	 * Without this hack, the first invocation of getopt would work as
	 * long as the generic arguments are first, but the second invocation
	 * (in the subfunction) would fail in one of two ways.  In the case
	 * where you don't set optreset, it would fail because optind may be
	 * pointing to the argument after the one it should be pointing at.
	 * In the case where you do set optreset, and reset optind, it would
	 * fail because getopt would run into the first set of options, which
	 * it doesn't understand.
	 *
	 * All of this would "sort of" work if you could somehow figure out
	 * whether optind had been incremented one option too far.  The
	 * mechanics of that, however, are more daunting than just giving
	 * both invocations all of the expect options for either invocation.
	 * 
	 * Needless to say, I wouldn't mind if someone invented a better
	 * (non-GPL!) command line parsing interface than getopt.  I
	 * wouldn't mind if someone added more knobs to getopt to make it
	 * work better.  Who knows, I may talk myself into doing it someday,
	 * if the standards weenies let me.  As it is, it just leads to
	 * hackery like this and causes people to avoid it in some cases.
	 * 
	 * KDM, September 8th, 1998
	 */
	if (subopt != NULL)
		sprintf(combinedopt, "%s%s", mainopt, subopt);
	else
		sprintf(combinedopt, "%s", mainopt);

	/*
	 * Start getopt processing at argv[2], since we've already accepted
	 * argv[1] as the command name.
	 */
	optind = 2;

	/*
	 * Now we run through the argument list looking for generic
	 * options, and ignoring options that possibly belong to
	 * subfunctions.
	 */
	while ((c = getopt(argc, argv, combinedopt))!= -1){
		switch(c) {
			case 'C':
				retry_count = strtol(optarg, NULL, 0);
				if (retry_count < 0)
					errx(1, "retry count %d is < 0",
					     retry_count);
				arglist |= CAM_ARG_RETRIES;
				break;
			case 'E':
				arglist |= CAM_ARG_ERR_RECOVER;
				break;
			case 'n':
				arglist |= CAM_ARG_DEVICE;
				tstr = optarg;
				while (isspace(*tstr) && (*tstr != '\0'))
					tstr++;
				device = (char *)strdup(tstr);
				break;
			case 't':
				timeout = strtol(optarg, NULL, 0);
				if (timeout < 0)
					errx(1, "invalid timeout %d", timeout);
				/* Convert the timeout from seconds to ms */
				timeout *= 1000;
				arglist |= CAM_ARG_TIMEOUT;
				break;
			case 'u':
				arglist |= CAM_ARG_UNIT;
				unit = strtol(optarg, NULL, 0);
				break;
			case 'v':
				arglist |= CAM_ARG_VERBOSE;
				break;
			default:
				break;
		}
	}

	if ((arglist & CAM_ARG_DEVICE) == 0)
		device = (char *)strdup(DEFAULT_DEVICE);

	if ((arglist & CAM_ARG_UNIT) == 0)
		unit = DEFAULT_UNIT;

	/*
	 * For most commands we'll want to open the passthrough device
	 * associated with the specified device.  In the case of the rescan
	 * commands, we don't use a passthrough device at all, just the
	 * transport layer device.
	 */
	if (((arglist & CAM_ARG_OPT_MASK) != CAM_ARG_RESCAN) 
	 && ((arglist & CAM_ARG_OPT_MASK) != CAM_ARG_DEVTREE)
	 && ((arglist & CAM_ARG_OPT_MASK) != CAM_ARG_USAGE)
	 && ((arglist & CAM_ARG_OPT_MASK) != CAM_ARG_DEBUG)) {

		if ((cam_dev = cam_open_spec_device(device,unit,O_RDWR,
		     NULL))== NULL)
			errx(1,"%s", cam_errbuf);
	}

	/*
	 * Reset optind to 2, and reset getopt, so these routines cam parse
	 * the arguments again.
	 */
	optind = 2;
	optreset = 1;

	switch(arglist & CAM_ARG_OPT_MASK) {
		case CAM_ARG_DEVLIST:
			error = getdevlist(cam_dev);
			break;
		case CAM_ARG_DEVTREE:
			error = getdevtree();
			break;
		case CAM_ARG_TUR:
			error = testunitready(cam_dev, retry_count, timeout);
			break;
		case CAM_ARG_INQUIRY:
			error = scsidoinquiry(cam_dev, argc, argv, combinedopt,
					      retry_count, timeout);
			break;
		case CAM_ARG_STARTSTOP:
			error = scsistart(cam_dev, arglist & CAM_ARG_START_UNIT,
					  arglist & CAM_ARG_EJECT, retry_count,
					  timeout);
			break;
		case CAM_ARG_RESCAN:
			error = dorescan(argc, argv);
			break;
		case CAM_ARG_READ_DEFECTS:
			error = readdefects(cam_dev, argc, argv, combinedopt,
					    retry_count, timeout);
			break;
		case CAM_ARG_MODE_PAGE:
			modepage(cam_dev, argc, argv, combinedopt,
				 retry_count, timeout);
			break;
		case CAM_ARG_SCSI_CMD:
			error = scsicmd(cam_dev, argc, argv, combinedopt,
					retry_count, timeout);
			break;
		case CAM_ARG_DEBUG:
			error = camdebug(argc, argv, combinedopt);
			break;
		default:
			usage();
			error = 1;
			break;
	}

	if (cam_dev != NULL)
		cam_close_device(cam_dev);

	exit(error);
}
