/*
 * Copyright (c) 1997-2007 Kenneth D. Merry
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/endian.h>
#include <sys/sbuf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <err.h>
#include <libutil.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/smp_all.h>
#include <cam/ata/ata_all.h>
#include <camlib.h>
#include "camcontrol.h"

typedef enum {
	CAM_CMD_NONE		= 0x00000000,
	CAM_CMD_DEVLIST		= 0x00000001,
	CAM_CMD_TUR		= 0x00000002,
	CAM_CMD_INQUIRY		= 0x00000003,
	CAM_CMD_STARTSTOP	= 0x00000004,
	CAM_CMD_RESCAN		= 0x00000005,
	CAM_CMD_READ_DEFECTS	= 0x00000006,
	CAM_CMD_MODE_PAGE	= 0x00000007,
	CAM_CMD_SCSI_CMD	= 0x00000008,
	CAM_CMD_DEVTREE		= 0x00000009,
	CAM_CMD_USAGE		= 0x0000000a,
	CAM_CMD_DEBUG		= 0x0000000b,
	CAM_CMD_RESET		= 0x0000000c,
	CAM_CMD_FORMAT		= 0x0000000d,
	CAM_CMD_TAG		= 0x0000000e,
	CAM_CMD_RATE		= 0x0000000f,
	CAM_CMD_DETACH		= 0x00000010,
	CAM_CMD_REPORTLUNS	= 0x00000011,
	CAM_CMD_READCAP		= 0x00000012,
	CAM_CMD_IDENTIFY	= 0x00000013,
	CAM_CMD_IDLE		= 0x00000014,
	CAM_CMD_STANDBY		= 0x00000015,
	CAM_CMD_SLEEP		= 0x00000016,
	CAM_CMD_SMP_CMD		= 0x00000017,
	CAM_CMD_SMP_RG		= 0x00000018,
	CAM_CMD_SMP_PC		= 0x00000019,
	CAM_CMD_SMP_PHYLIST	= 0x0000001a,
	CAM_CMD_SMP_MANINFO	= 0x0000001b
} cam_cmdmask;

typedef enum {
	CAM_ARG_NONE		= 0x00000000,
	CAM_ARG_VERBOSE		= 0x00000001,
	CAM_ARG_DEVICE		= 0x00000002,
	CAM_ARG_BUS		= 0x00000004,
	CAM_ARG_TARGET		= 0x00000008,
	CAM_ARG_LUN		= 0x00000010,
	CAM_ARG_EJECT		= 0x00000020,
	CAM_ARG_UNIT		= 0x00000040,
	CAM_ARG_FORMAT_BLOCK	= 0x00000080,
	CAM_ARG_FORMAT_BFI	= 0x00000100,
	CAM_ARG_FORMAT_PHYS	= 0x00000200,
	CAM_ARG_PLIST		= 0x00000400,
	CAM_ARG_GLIST		= 0x00000800,
	CAM_ARG_GET_SERIAL	= 0x00001000,
	CAM_ARG_GET_STDINQ	= 0x00002000,
	CAM_ARG_GET_XFERRATE	= 0x00004000,
	CAM_ARG_INQ_MASK	= 0x00007000,
	CAM_ARG_MODE_EDIT	= 0x00008000,
	CAM_ARG_PAGE_CNTL	= 0x00010000,
	CAM_ARG_TIMEOUT		= 0x00020000,
	CAM_ARG_CMD_IN		= 0x00040000,
	CAM_ARG_CMD_OUT		= 0x00080000,
	CAM_ARG_DBD		= 0x00100000,
	CAM_ARG_ERR_RECOVER	= 0x00200000,
	CAM_ARG_RETRIES		= 0x00400000,
	CAM_ARG_START_UNIT	= 0x00800000,
	CAM_ARG_DEBUG_INFO	= 0x01000000,
	CAM_ARG_DEBUG_TRACE	= 0x02000000,
	CAM_ARG_DEBUG_SUBTRACE	= 0x04000000,
	CAM_ARG_DEBUG_CDB	= 0x08000000,
	CAM_ARG_DEBUG_XPT	= 0x10000000,
	CAM_ARG_DEBUG_PERIPH	= 0x20000000,
} cam_argmask;

struct camcontrol_opts {
	const char	*optname;
	uint32_t	cmdnum;
	cam_argmask	argnum;
	const char	*subopt;
};

#ifndef MINIMALISTIC
static const char scsicmd_opts[] = "a:c:dfi:o:r";
static const char readdefect_opts[] = "f:GP";
static const char negotiate_opts[] = "acD:M:O:qR:T:UW:";
static const char smprg_opts[] = "l";
static const char smppc_opts[] = "a:A:d:lm:M:o:p:s:S:T:";
static const char smpphylist_opts[] = "lq";
#endif

struct camcontrol_opts option_table[] = {
#ifndef MINIMALISTIC
	{"tur", CAM_CMD_TUR, CAM_ARG_NONE, NULL},
	{"inquiry", CAM_CMD_INQUIRY, CAM_ARG_NONE, "DSR"},
	{"identify", CAM_CMD_IDENTIFY, CAM_ARG_NONE, NULL},
	{"start", CAM_CMD_STARTSTOP, CAM_ARG_START_UNIT, NULL},
	{"stop", CAM_CMD_STARTSTOP, CAM_ARG_NONE, NULL},
	{"load", CAM_CMD_STARTSTOP, CAM_ARG_START_UNIT | CAM_ARG_EJECT, NULL},
	{"eject", CAM_CMD_STARTSTOP, CAM_ARG_EJECT, NULL},
	{"reportluns", CAM_CMD_REPORTLUNS, CAM_ARG_NONE, "clr:"},
	{"readcapacity", CAM_CMD_READCAP, CAM_ARG_NONE, "bhHNqs"},
#endif /* MINIMALISTIC */
	{"rescan", CAM_CMD_RESCAN, CAM_ARG_NONE, NULL},
	{"reset", CAM_CMD_RESET, CAM_ARG_NONE, NULL},
#ifndef MINIMALISTIC
	{"cmd", CAM_CMD_SCSI_CMD, CAM_ARG_NONE, scsicmd_opts},
	{"command", CAM_CMD_SCSI_CMD, CAM_ARG_NONE, scsicmd_opts},
	{"smpcmd", CAM_CMD_SMP_CMD, CAM_ARG_NONE, "r:R:"},
	{"smprg", CAM_CMD_SMP_RG, CAM_ARG_NONE, smprg_opts},
	{"smpreportgeneral", CAM_CMD_SMP_RG, CAM_ARG_NONE, smprg_opts},
	{"smppc", CAM_CMD_SMP_PC, CAM_ARG_NONE, smppc_opts},
	{"smpphycontrol", CAM_CMD_SMP_PC, CAM_ARG_NONE, smppc_opts},
	{"smpplist", CAM_CMD_SMP_PHYLIST, CAM_ARG_NONE, smpphylist_opts},
	{"smpphylist", CAM_CMD_SMP_PHYLIST, CAM_ARG_NONE, smpphylist_opts},
	{"smpmaninfo", CAM_CMD_SMP_MANINFO, CAM_ARG_NONE, "l"},
	{"defects", CAM_CMD_READ_DEFECTS, CAM_ARG_NONE, readdefect_opts},
	{"defectlist", CAM_CMD_READ_DEFECTS, CAM_ARG_NONE, readdefect_opts},
#endif /* MINIMALISTIC */
	{"devlist", CAM_CMD_DEVTREE, CAM_ARG_NONE, NULL},
#ifndef MINIMALISTIC
	{"periphlist", CAM_CMD_DEVLIST, CAM_ARG_NONE, NULL},
	{"modepage", CAM_CMD_MODE_PAGE, CAM_ARG_NONE, "bdelm:P:"},
	{"tags", CAM_CMD_TAG, CAM_ARG_NONE, "N:q"},
	{"negotiate", CAM_CMD_RATE, CAM_ARG_NONE, negotiate_opts},
	{"rate", CAM_CMD_RATE, CAM_ARG_NONE, negotiate_opts},
	{"debug", CAM_CMD_DEBUG, CAM_ARG_NONE, "IPTSXc"},
	{"format", CAM_CMD_FORMAT, CAM_ARG_NONE, "qrwy"},
	{"idle", CAM_CMD_IDLE, CAM_ARG_NONE, "t:"},
	{"standby", CAM_CMD_STANDBY, CAM_ARG_NONE, "t:"},
	{"sleep", CAM_CMD_SLEEP, CAM_ARG_NONE, ""},
#endif /* MINIMALISTIC */
	{"help", CAM_CMD_USAGE, CAM_ARG_NONE, NULL},
	{"-?", CAM_CMD_USAGE, CAM_ARG_NONE, NULL},
	{"-h", CAM_CMD_USAGE, CAM_ARG_NONE, NULL},
	{NULL, 0, 0, NULL}
};

typedef enum {
	CC_OR_NOT_FOUND,
	CC_OR_AMBIGUOUS,
	CC_OR_FOUND
} camcontrol_optret;

struct cam_devitem {
	struct device_match_result dev_match;
	int num_periphs;
	struct periph_match_result *periph_matches;
	struct scsi_vpd_device_id *device_id;
	int device_id_len;
	STAILQ_ENTRY(cam_devitem) links;
};

struct cam_devlist {
	STAILQ_HEAD(, cam_devitem) dev_queue;
	path_id_t path_id;
};

cam_cmdmask cmdlist;
cam_argmask arglist;

camcontrol_optret getoption(struct camcontrol_opts *table, char *arg,
			    uint32_t *cmdnum, cam_argmask *argnum,
			    const char **subopt);
#ifndef MINIMALISTIC
static int getdevlist(struct cam_device *device);
#endif /* MINIMALISTIC */
static int getdevtree(void);
#ifndef MINIMALISTIC
static int testunitready(struct cam_device *device, int retry_count,
			 int timeout, int quiet);
static int scsistart(struct cam_device *device, int startstop, int loadeject,
		     int retry_count, int timeout);
static int scsidoinquiry(struct cam_device *device, int argc, char **argv,
			 char *combinedopt, int retry_count, int timeout);
static int scsiinquiry(struct cam_device *device, int retry_count, int timeout);
static int scsiserial(struct cam_device *device, int retry_count, int timeout);
static int camxferrate(struct cam_device *device);
#endif /* MINIMALISTIC */
static int parse_btl(char *tstr, int *bus, int *target, int *lun,
		     cam_argmask *arglst);
static int dorescan_or_reset(int argc, char **argv, int rescan);
static int rescan_or_reset_bus(int bus, int rescan);
static int scanlun_or_reset_dev(int bus, int target, int lun, int scan);
#ifndef MINIMALISTIC
static int readdefects(struct cam_device *device, int argc, char **argv,
		       char *combinedopt, int retry_count, int timeout);
static void modepage(struct cam_device *device, int argc, char **argv,
		     char *combinedopt, int retry_count, int timeout);
static int scsicmd(struct cam_device *device, int argc, char **argv,
		   char *combinedopt, int retry_count, int timeout);
static int smpcmd(struct cam_device *device, int argc, char **argv,
		  char *combinedopt, int retry_count, int timeout);
static int smpreportgeneral(struct cam_device *device, int argc, char **argv,
			    char *combinedopt, int retry_count, int timeout);
static int smpphycontrol(struct cam_device *device, int argc, char **argv,
			 char *combinedopt, int retry_count, int timeout);
static int smpmaninfo(struct cam_device *device, int argc, char **argv,
		      char *combinedopt, int retry_count, int timeout);
static int getdevid(struct cam_devitem *item);
static int buildbusdevlist(struct cam_devlist *devlist);
static void freebusdevlist(struct cam_devlist *devlist);
static struct cam_devitem *findsasdevice(struct cam_devlist *devlist,
					 uint64_t sasaddr);
static int smpphylist(struct cam_device *device, int argc, char **argv,
		      char *combinedopt, int retry_count, int timeout);
static int tagcontrol(struct cam_device *device, int argc, char **argv,
		      char *combinedopt);
static void cts_print(struct cam_device *device,
		      struct ccb_trans_settings *cts);
static void cpi_print(struct ccb_pathinq *cpi);
static int get_cpi(struct cam_device *device, struct ccb_pathinq *cpi);
static int get_cgd(struct cam_device *device, struct ccb_getdev *cgd);
static int get_print_cts(struct cam_device *device, int user_settings,
			 int quiet, struct ccb_trans_settings *cts);
static int ratecontrol(struct cam_device *device, int retry_count,
		       int timeout, int argc, char **argv, char *combinedopt);
static int scsiformat(struct cam_device *device, int argc, char **argv,
		      char *combinedopt, int retry_count, int timeout);
static int scsireportluns(struct cam_device *device, int argc, char **argv,
			  char *combinedopt, int retry_count, int timeout);
static int scsireadcapacity(struct cam_device *device, int argc, char **argv,
			    char *combinedopt, int retry_count, int timeout);
static int atapm(struct cam_device *device, int argc, char **argv,
			    char *combinedopt, int retry_count, int timeout);
#endif /* MINIMALISTIC */
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

camcontrol_optret
getoption(struct camcontrol_opts *table, char *arg, uint32_t *cmdnum,
	  cam_argmask *argnum, const char **subopt)
{
	struct camcontrol_opts *opts;
	int num_matches = 0;

	for (opts = table; (opts != NULL) && (opts->optname != NULL);
	     opts++) {
		if (strncmp(opts->optname, arg, strlen(arg)) == 0) {
			*cmdnum = opts->cmdnum;
			*argnum = opts->argnum;
			*subopt = opts->subopt;
			if (++num_matches > 1)
				return(CC_OR_AMBIGUOUS);
		}
	}

	if (num_matches > 0)
		return(CC_OR_FOUND);
	else
		return(CC_OR_NOT_FOUND);
}

#ifndef MINIMALISTIC
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
#endif /* MINIMALISTIC */

static int
getdevtree(void)
{
	union ccb ccb;
	int bufsize, fd;
	unsigned int i;
	int need_close = 0;
	int error = 0;
	int skip_device = 0;

	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		warn("couldn't open %s", XPT_DEVICE);
		return(1);
	}

	bzero(&ccb, sizeof(union ccb));

	ccb.ccb_h.path_id = CAM_XPT_PATH_ID;
	ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
	ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

	ccb.ccb_h.func_code = XPT_DEV_MATCH;
	bufsize = sizeof(struct dev_match_result) * 100;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
	if (ccb.cdm.matches == NULL) {
		warnx("can't malloc memory for matches");
		close(fd);
		return(1);
	}
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
			warnx("got CAM error %#x, CDM error %d\n",
			      ccb.ccb_h.status, ccb.cdm.status);
			error = 1;
			break;
		}

		for (i = 0; i < ccb.cdm.num_matches; i++) {
			switch (ccb.cdm.matches[i].type) {
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

				if ((dev_result->flags
				     & DEV_RESULT_UNCONFIGURED)
				 && ((arglist & CAM_ARG_VERBOSE) == 0)) {
					skip_device = 1;
					break;
				} else
					skip_device = 0;

				if (dev_result->protocol == PROTO_SCSI) {
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
				} else if (dev_result->protocol == PROTO_ATA ||
				    dev_result->protocol == PROTO_SATAPM) {
				    cam_strvis(product,
					   dev_result->ident_data.model,
					   sizeof(dev_result->ident_data.model),
					   sizeof(product));
				    cam_strvis(revision,
					   dev_result->ident_data.revision,
					  sizeof(dev_result->ident_data.revision),
					   sizeof(revision));
				    sprintf(tmpstr, "<%s %s>", product,
					revision);
				} else {
				    sprintf(tmpstr, "<>");
				}
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

				need_close = 1;

				break;
			}
			case DEV_MATCH_PERIPH: {
				struct periph_match_result *periph_result;

				periph_result =
				      &ccb.cdm.matches[i].result.periph_result;

				if (skip_device != 0)
					break;

				if (need_close > 1)
					fprintf(stdout, ",");

				fprintf(stdout, "%s%d",
					periph_result->periph_name,
					periph_result->unit_number);

				need_close++;
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

#ifndef MINIMALISTIC
static int
testunitready(struct cam_device *device, int retry_count, int timeout,
	      int quiet)
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
		if (quiet == 0)
			perror("error sending test unit ready");

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
		if (quiet == 0)
			fprintf(stdout, "Unit is ready\n");
	} else {
		if (quiet == 0)
			fprintf(stdout, "Unit is not ready\n");
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
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

	ccb = cam_getccb(device);

	/*
	 * If we're stopping, send an ordered tag so the drive in question
	 * will finish any previously queued writes before stopping.  If
	 * the device isn't capable of tagged queueing, or if tagged
	 * queueing is turned off, the tag action is a no-op.
	 */
	scsi_start_stop(&ccb->csio,
			/* retries */ retry_count,
			/* cbfcnp */ NULL,
			/* tag_action */ startstop ? MSG_SIMPLE_Q_TAG :
						     MSG_ORDERED_Q_TAG,
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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
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
		error = camxferrate(device);

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
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

	inq_buf = (struct scsi_inquiry_data *)malloc(
		sizeof(struct scsi_inquiry_data));

	if (inq_buf == NULL) {
		cam_freeccb(ccb);
		warnx("can't malloc memory for inquiry\n");
		return(1);
	}
	bzero(inq_buf, sizeof(*inq_buf));

	/*
	 * Note that although the size of the inquiry buffer is the full
	 * 256 bytes specified in the SCSI spec, we only tell the device
	 * that we have allocated SHORT_INQUIRY_LENGTH bytes.  There are
	 * two reasons for this:
	 *
	 *  - The SCSI spec says that when a length field is only 1 byte,
	 *    a value of 0 will be interpreted as 256.  Therefore
	 *    scsi_inquiry() will convert an inq_len (which is passed in as
	 *    a u_int32_t, but the field in the CDB is only 1 byte) of 256
	 *    to 0.  Evidently, very few devices meet the spec in that
	 *    regard.  Some devices, like many Seagate disks, take the 0 as
	 *    0, and don't return any data.  One Pioneer DVD-R drive
	 *    returns more data than the command asked for.
	 *
	 *    So, since there are numerous devices that just don't work
	 *    right with the full inquiry size, we don't send the full size.
	 *
	 *  - The second reason not to use the full inquiry data length is
	 *    that we don't need it here.  The only reason we issue a
	 *    standard inquiry is to get the vendor name, device name,
	 *    and revision so scsi_print_inquiry() can print them.
	 *
	 * If, at some point in the future, more inquiry data is needed for
	 * some reason, this code should use a procedure similar to the
	 * probe code.  i.e., issue a short inquiry, and determine from
	 * the additional length passed back from the device how much
	 * inquiry data the device supports.  Once the amount the device
	 * supports is determined, issue an inquiry for that amount and no
	 * more.
	 *
	 * KDM, 2/18/2000
	 */
	scsi_inquiry(&ccb->csio,
		     /* retries */ retry_count,
		     /* cbfcnp */ NULL,
		     /* tag_action */ MSG_SIMPLE_Q_TAG,
		     /* inq_buf */ (u_int8_t *)inq_buf,
		     /* inq_len */ SHORT_INQUIRY_LENGTH,
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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
	}

	cam_freeccb(ccb);

	if (error != 0) {
		free(inq_buf);
		return(error);
	}

	fprintf(stdout, "%s%d: ", device->device_name,
		device->dev_unit_num);
	scsi_print_inquiry(inq_buf);

	free(inq_buf);

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
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		cam_freeccb(ccb);
		free(serial_buf);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
	}

	cam_freeccb(ccb);

	if (error != 0) {
		free(serial_buf);
		return(error);
	}

	bcopy(serial_buf->serial_num, serial_num, serial_buf->length);
	serial_num[serial_buf->length] = '\0';

	if ((arglist & CAM_ARG_GET_STDINQ)
	 || (arglist & CAM_ARG_GET_XFERRATE))
		fprintf(stdout, "%s%d: Serial Number ",
			device->device_name, device->dev_unit_num);

	fprintf(stdout, "%.60s\n", serial_num);

	free(serial_buf);

	return(0);
}

static int
camxferrate(struct cam_device *device)
{
	struct ccb_pathinq cpi;
	u_int32_t freq = 0;
	u_int32_t speed = 0;
	union ccb *ccb;
	u_int mb;
	int retval = 0;

	if ((retval = get_cpi(device, &cpi)) != 0)
		return (1);

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("couldn't allocate CCB");
		return(1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_trans_settings) - sizeof(struct ccb_hdr));

	ccb->ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	ccb->cts.type = CTS_TYPE_CURRENT_SETTINGS;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char error_string[] = "error getting transfer settings";

		if (retval < 0)
			warn(error_string);
		else
			warnx(error_string);

		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);

		retval = 1;

		goto xferrate_bailout;

	}

	speed = cpi.base_transfer_speed;
	freq = 0;
	if (ccb->cts.transport == XPORT_SPI) {
		struct ccb_trans_settings_spi *spi =
		    &ccb->cts.xport_specific.spi;

		if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) != 0) {
			freq = scsi_calc_syncsrate(spi->sync_period);
			speed = freq;
		}
		if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0) {
			speed *= (0x01 << spi->bus_width);
		}
	} else if (ccb->cts.transport == XPORT_FC) {
		struct ccb_trans_settings_fc *fc =
		    &ccb->cts.xport_specific.fc;

		if (fc->valid & CTS_FC_VALID_SPEED)
			speed = fc->bitrate;
	} else if (ccb->cts.transport == XPORT_SAS) {
		struct ccb_trans_settings_sas *sas =
		    &ccb->cts.xport_specific.sas;

		if (sas->valid & CTS_SAS_VALID_SPEED)
			speed = sas->bitrate;
	} else if (ccb->cts.transport == XPORT_ATA) {
		struct ccb_trans_settings_ata *ata =
		    &ccb->cts.xport_specific.ata;

		if (ata->valid & CTS_ATA_VALID_MODE)
			speed = ata_mode2speed(ata->mode);
	} else if (ccb->cts.transport == XPORT_SATA) {
		struct	ccb_trans_settings_sata *sata =
		    &ccb->cts.xport_specific.sata;

		if (sata->valid & CTS_SATA_VALID_REVISION)
			speed = ata_revision2speed(sata->revision);
	}

	mb = speed / 1000;
	if (mb > 0) {
		fprintf(stdout, "%s%d: %d.%03dMB/s transfers",
			device->device_name, device->dev_unit_num,
			mb, speed % 1000);
	} else {
		fprintf(stdout, "%s%d: %dKB/s transfers",
			device->device_name, device->dev_unit_num,
			speed);
	}

	if (ccb->cts.transport == XPORT_SPI) {
		struct ccb_trans_settings_spi *spi =
		    &ccb->cts.xport_specific.spi;

		if (((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0)
		 && (spi->sync_offset != 0))
			fprintf(stdout, " (%d.%03dMHz, offset %d", freq / 1000,
				freq % 1000, spi->sync_offset);

		if (((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0)
		 && (spi->bus_width > 0)) {
			if (((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0)
			 && (spi->sync_offset != 0)) {
				fprintf(stdout, ", ");
			} else {
				fprintf(stdout, " (");
			}
			fprintf(stdout, "%dbit)", 8 * (0x01 << spi->bus_width));
		} else if (((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) != 0)
		 && (spi->sync_offset != 0)) {
			fprintf(stdout, ")");
		}
	} else if (ccb->cts.transport == XPORT_ATA) {
		struct ccb_trans_settings_ata *ata =
		    &ccb->cts.xport_specific.ata;

		printf(" (");
		if (ata->valid & CTS_ATA_VALID_MODE)
			printf("%s, ", ata_mode2string(ata->mode));
		if ((ata->valid & CTS_ATA_VALID_ATAPI) && ata->atapi != 0)
			printf("ATAPI %dbytes, ", ata->atapi);
		if (ata->valid & CTS_ATA_VALID_BYTECOUNT)
			printf("PIO %dbytes", ata->bytecount);
		printf(")");
	} else if (ccb->cts.transport == XPORT_SATA) {
		struct ccb_trans_settings_sata *sata =
		    &ccb->cts.xport_specific.sata;

		printf(" (");
		if (sata->valid & CTS_SATA_VALID_REVISION)
			printf("SATA %d.x, ", sata->revision);
		else
			printf("SATA, ");
		if (sata->valid & CTS_SATA_VALID_MODE)
			printf("%s, ", ata_mode2string(sata->mode));
		if ((sata->valid & CTS_SATA_VALID_ATAPI) && sata->atapi != 0)
			printf("ATAPI %dbytes, ", sata->atapi);
		if (sata->valid & CTS_SATA_VALID_BYTECOUNT)
			printf("PIO %dbytes", sata->bytecount);
		printf(")");
	}

	if (ccb->cts.protocol == PROTO_SCSI) {
		struct ccb_trans_settings_scsi *scsi =
		    &ccb->cts.proto_specific.scsi;
		if (scsi->valid & CTS_SCSI_VALID_TQ) {
			if (scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) {
				fprintf(stdout, ", Command Queueing Enabled");
			}
		}
	}

        fprintf(stdout, "\n");

xferrate_bailout:

	cam_freeccb(ccb);

	return(retval);
}

static void
atacapprint(struct ata_params *parm)
{
	u_int32_t lbasize = (u_int32_t)parm->lba_size_1 |
				((u_int32_t)parm->lba_size_2 << 16);

	u_int64_t lbasize48 = ((u_int64_t)parm->lba_size48_1) |
				((u_int64_t)parm->lba_size48_2 << 16) |
				((u_int64_t)parm->lba_size48_3 << 32) |
				((u_int64_t)parm->lba_size48_4 << 48);

	printf("\n");
	printf("protocol              ");
	printf("ATA/ATAPI-%d", ata_version(parm->version_major));
	if (parm->satacapabilities && parm->satacapabilities != 0xffff) {
		if (parm->satacapabilities & ATA_SATA_GEN3)
			printf(" SATA 3.x\n");
		else if (parm->satacapabilities & ATA_SATA_GEN2)
			printf(" SATA 2.x\n");
		else if (parm->satacapabilities & ATA_SATA_GEN1)
			printf(" SATA 1.x\n");
		else
			printf(" SATA\n");
	}
	else
		printf("\n");
	printf("device model          %.40s\n", parm->model);
	printf("firmware revision     %.8s\n", parm->revision);
	printf("serial number         %.20s\n", parm->serial);
	if (parm->enabled.extension & ATA_SUPPORT_64BITWWN) {
		printf("WWN                   %02x%02x%02x%02x\n",
		    parm->wwn[0], parm->wwn[1], parm->wwn[2], parm->wwn[3]);
	}
	if (parm->enabled.extension & ATA_SUPPORT_MEDIASN) {
		printf("media serial number   %.30s\n",
		    parm->media_serial);
	}

	printf("cylinders             %d\n", parm->cylinders);
	printf("heads                 %d\n", parm->heads);
	printf("sectors/track         %d\n", parm->sectors);
	printf("sector size           logical %u, physical %lu, offset %lu\n",
	    ata_logical_sector_size(parm),
	    (unsigned long)ata_physical_sector_size(parm),
	    (unsigned long)ata_logical_sector_offset(parm));

	if (parm->config == ATA_PROTO_CFA ||
	    (parm->support.command2 & ATA_SUPPORT_CFA))
		printf("CFA supported\n");

	printf("LBA%ssupported         ",
		parm->capabilities1 & ATA_SUPPORT_LBA ? " " : " not ");
	if (lbasize)
		printf("%d sectors\n", lbasize);
	else
		printf("\n");

	printf("LBA48%ssupported       ",
		parm->support.command2 & ATA_SUPPORT_ADDRESS48 ? " " : " not ");
	if (lbasize48)
		printf("%ju sectors\n", (uintmax_t)lbasize48);
	else
		printf("\n");

	printf("PIO supported         PIO");
	switch (ata_max_pmode(parm)) {
	case ATA_PIO4:
		printf("4");
		break;
	case ATA_PIO3:
		printf("3");
		break;
	case ATA_PIO2:
		printf("2");
		break;
	case ATA_PIO1:
		printf("1");
		break;
	default:
		printf("0");
	}
	if ((parm->capabilities1 & ATA_SUPPORT_IORDY) == 0)
		printf(" w/o IORDY");
	printf("\n");

	printf("DMA%ssupported         ",
		parm->capabilities1 & ATA_SUPPORT_DMA ? " " : " not ");
	if (parm->capabilities1 & ATA_SUPPORT_DMA) {
		if (parm->mwdmamodes & 0xff) {
			printf("WDMA");
			if (parm->mwdmamodes & 0x04)
				printf("2");
			else if (parm->mwdmamodes & 0x02)
				printf("1");
			else if (parm->mwdmamodes & 0x01)
				printf("0");
			printf(" ");
		}
		if ((parm->atavalid & ATA_FLAG_88) &&
		    (parm->udmamodes & 0xff)) {
			printf("UDMA");
			if (parm->udmamodes & 0x40)
				printf("6");
			else if (parm->udmamodes & 0x20)
				printf("5");
			else if (parm->udmamodes & 0x10)
				printf("4");
			else if (parm->udmamodes & 0x08)
				printf("3");
			else if (parm->udmamodes & 0x04)
				printf("2");
			else if (parm->udmamodes & 0x02)
				printf("1");
			else if (parm->udmamodes & 0x01)
				printf("0");
			printf(" ");
		}
	}
	printf("\n");

	if (parm->media_rotation_rate == 1) {
		printf("media RPM             non-rotating\n");
	} else if (parm->media_rotation_rate >= 0x0401 &&
	    parm->media_rotation_rate <= 0xFFFE) {
		printf("media RPM             %d\n",
			parm->media_rotation_rate);
	}

	printf("\nFeature                      "
		"Support  Enabled   Value           Vendor\n");
	printf("read ahead                     %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_LOOKAHEAD ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_LOOKAHEAD ? "yes" : "no");
	printf("write cache                    %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_WRITECACHE ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_WRITECACHE ? "yes" : "no");
	printf("flush cache                    %s	%s\n",
		parm->support.command2 & ATA_SUPPORT_FLUSHCACHE ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_FLUSHCACHE ? "yes" : "no");
	printf("overlap                        %s\n",
		parm->capabilities1 & ATA_SUPPORT_OVERLAP ? "yes" : "no");
	printf("Tagged Command Queuing (TCQ)   %s	%s",
		parm->support.command2 & ATA_SUPPORT_QUEUED ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_QUEUED ? "yes" : "no");
		if (parm->support.command2 & ATA_SUPPORT_QUEUED) {
			printf("	%d tags\n",
			    ATA_QUEUE_LEN(parm->queue) + 1);
		} else
			printf("\n");
	printf("Native Command Queuing (NCQ)   ");
	if (parm->satacapabilities != 0xffff &&
	    (parm->satacapabilities & ATA_SUPPORT_NCQ)) {
		printf("yes		%d tags\n",
		    ATA_QUEUE_LEN(parm->queue) + 1);
	} else
		printf("no\n");
	printf("SMART                          %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_SMART ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_SMART ? "yes" : "no");
	printf("microcode download             %s	%s\n",
		parm->support.command2 & ATA_SUPPORT_MICROCODE ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_MICROCODE ? "yes" : "no");
	printf("security                       %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_SECURITY ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_SECURITY ? "yes" : "no");
	printf("power management               %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_POWERMGT ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_POWERMGT ? "yes" : "no");
	printf("advanced power management      %s	%s",
		parm->support.command2 & ATA_SUPPORT_APM ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_APM ? "yes" : "no");
		if (parm->support.command2 & ATA_SUPPORT_APM) {
			printf("	%d/0x%02X\n",
			    parm->apm_value, parm->apm_value);
		} else
			printf("\n");
	printf("automatic acoustic management  %s	%s",
		parm->support.command2 & ATA_SUPPORT_AUTOACOUSTIC ? "yes" :"no",
		parm->enabled.command2 & ATA_SUPPORT_AUTOACOUSTIC ? "yes" :"no");
		if (parm->support.command2 & ATA_SUPPORT_AUTOACOUSTIC) {
			printf("	%d/0x%02X	%d/0x%02X\n",
			    ATA_ACOUSTIC_CURRENT(parm->acoustic),
			    ATA_ACOUSTIC_CURRENT(parm->acoustic),
			    ATA_ACOUSTIC_VENDOR(parm->acoustic),
			    ATA_ACOUSTIC_VENDOR(parm->acoustic));
		} else
			printf("\n");
	printf("media status notification      %s	%s\n",
		parm->support.command2 & ATA_SUPPORT_NOTIFY ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_NOTIFY ? "yes" : "no");
	printf("power-up in Standby            %s	%s\n",
		parm->support.command2 & ATA_SUPPORT_STANDBY ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_STANDBY ? "yes" : "no");
	printf("write-read-verify              %s	%s",
		parm->support2 & ATA_SUPPORT_WRITEREADVERIFY ? "yes" : "no",
		parm->enabled2 & ATA_SUPPORT_WRITEREADVERIFY ? "yes" : "no");
		if (parm->support2 & ATA_SUPPORT_WRITEREADVERIFY) {
			printf("	%d/0x%x\n",
			    parm->wrv_mode, parm->wrv_mode);
		} else
			printf("\n");
	printf("unload                         %s	%s\n",
		parm->support.extension & ATA_SUPPORT_UNLOAD ? "yes" : "no",
		parm->enabled.extension & ATA_SUPPORT_UNLOAD ? "yes" : "no");
	printf("free-fall                      %s	%s\n",
		parm->support2 & ATA_SUPPORT_FREEFALL ? "yes" : "no",
		parm->enabled2 & ATA_SUPPORT_FREEFALL ? "yes" : "no");
	printf("data set management (TRIM)     %s\n",
		parm->support_dsm & ATA_SUPPORT_DSM_TRIM ? "yes" : "no");
}

static int
ataidentify(struct cam_device *device, int retry_count, int timeout)
{
	union ccb *ccb;
	struct ata_params *ident_buf;
	struct ccb_getdev cgd;
	u_int i, error = 0;
	int16_t *ptr;

	if (get_cgd(device, &cgd) != 0) {
		warnx("couldn't get CGD");
		return(1);
	}
	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("couldn't allocate CCB");
		return(1);
	}

	/* cam_getccb cleans up the header, caller has to zero the payload */
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_ataio) - sizeof(struct ccb_hdr));

	ptr = (uint16_t *)malloc(sizeof(struct ata_params));

	if (ptr == NULL) {
		cam_freeccb(ccb);
		warnx("can't malloc memory for identify\n");
		return(1);
	}
	bzero(ptr, sizeof(struct ata_params));

	cam_fill_ataio(&ccb->ataio,
		      retry_count,
		      NULL,
		      /*flags*/CAM_DIR_IN,
		      MSG_SIMPLE_Q_TAG,
		      /*data_ptr*/(u_int8_t *)ptr,
		      /*dxfer_len*/sizeof(struct ata_params),
		      timeout ? timeout : 30 * 1000);
	if (cgd.protocol == PROTO_ATA)
		ata_28bit_cmd(&ccb->ataio, ATA_ATA_IDENTIFY, 0, 0, 0);
	else
		ata_28bit_cmd(&ccb->ataio, ATA_ATAPI_IDENTIFY, 0, 0, 0);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending ATA identify");

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		free(ptr);
		cam_freeccb(ccb);
		return(1);
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
	}

	cam_freeccb(ccb);

	if (error != 0) {
		free(ptr);
		return(error);
	}

	for (i = 0; i < sizeof(struct ata_params) / 2; i++)
		ptr[i] = le16toh(ptr[i]);
	if (arglist & CAM_ARG_VERBOSE) {
		fprintf(stdout, "%s%d: Raw identify data:\n",
		    device->device_name, device->dev_unit_num);
		for (i = 0; i < sizeof(struct ata_params) / 2; i++) {
			if ((i % 8) == 0)
			    fprintf(stdout, " %3d: ", i);
			fprintf(stdout, "%04x ", (uint16_t)ptr[i]);
			if ((i % 8) == 7)
			    fprintf(stdout, "\n");
		}
	}
	ident_buf = (struct ata_params *)ptr;
	if (strncmp(ident_buf->model, "FX", 2) &&
	    strncmp(ident_buf->model, "NEC", 3) &&
	    strncmp(ident_buf->model, "Pioneer", 7) &&
	    strncmp(ident_buf->model, "SHARP", 5)) {
		ata_bswap(ident_buf->model, sizeof(ident_buf->model));
		ata_bswap(ident_buf->revision, sizeof(ident_buf->revision));
		ata_bswap(ident_buf->serial, sizeof(ident_buf->serial));
		ata_bswap(ident_buf->media_serial, sizeof(ident_buf->media_serial));
	}
	ata_btrim(ident_buf->model, sizeof(ident_buf->model));
	ata_bpack(ident_buf->model, ident_buf->model, sizeof(ident_buf->model));
	ata_btrim(ident_buf->revision, sizeof(ident_buf->revision));
	ata_bpack(ident_buf->revision, ident_buf->revision, sizeof(ident_buf->revision));
	ata_btrim(ident_buf->serial, sizeof(ident_buf->serial));
	ata_bpack(ident_buf->serial, ident_buf->serial, sizeof(ident_buf->serial));
	ata_btrim(ident_buf->media_serial, sizeof(ident_buf->media_serial));
	ata_bpack(ident_buf->media_serial, ident_buf->media_serial,
	    sizeof(ident_buf->media_serial));

	fprintf(stdout, "%s%d: ", device->device_name,
		device->dev_unit_num);
	ata_print_ident(ident_buf);
	camxferrate(device);
	atacapprint(ident_buf);

	free(ident_buf);

	return(0);
}
#endif /* MINIMALISTIC */

/*
 * Parse out a bus, or a bus, target and lun in the following
 * format:
 * bus
 * bus:target
 * bus:target:lun
 *
 * Returns the number of parsed components, or 0.
 */
static int
parse_btl(char *tstr, int *bus, int *target, int *lun, cam_argmask *arglst)
{
	char *tmpstr;
	int convs = 0;

	while (isspace(*tstr) && (*tstr != '\0'))
		tstr++;

	tmpstr = (char *)strtok(tstr, ":");
	if ((tmpstr != NULL) && (*tmpstr != '\0')) {
		*bus = strtol(tmpstr, NULL, 0);
		*arglst |= CAM_ARG_BUS;
		convs++;
		tmpstr = (char *)strtok(NULL, ":");
		if ((tmpstr != NULL) && (*tmpstr != '\0')) {
			*target = strtol(tmpstr, NULL, 0);
			*arglst |= CAM_ARG_TARGET;
			convs++;
			tmpstr = (char *)strtok(NULL, ":");
			if ((tmpstr != NULL) && (*tmpstr != '\0')) {
				*lun = strtol(tmpstr, NULL, 0);
				*arglst |= CAM_ARG_LUN;
				convs++;
			}
		}
	}

	return convs;
}

static int
dorescan_or_reset(int argc, char **argv, int rescan)
{
	static const char must[] =
		"you must specify \"all\", a bus, or a bus:target:lun to %s";
	int rv, error = 0;
	int bus = -1, target = -1, lun = -1;
	char *tstr;

	if (argc < 3) {
		warnx(must, rescan? "rescan" : "reset");
		return(1);
	}

	tstr = argv[optind];
	while (isspace(*tstr) && (*tstr != '\0'))
		tstr++;
	if (strncasecmp(tstr, "all", strlen("all")) == 0)
		arglist |= CAM_ARG_BUS;
	else {
		rv = parse_btl(argv[optind], &bus, &target, &lun, &arglist);
		if (rv != 1 && rv != 3) {
			warnx(must, rescan? "rescan" : "reset");
			return(1);
		}
	}

	if ((arglist & CAM_ARG_BUS)
	    && (arglist & CAM_ARG_TARGET)
	    && (arglist & CAM_ARG_LUN))
		error = scanlun_or_reset_dev(bus, target, lun, rescan);
	else
		error = rescan_or_reset_bus(bus, rescan);

	return(error);
}

static int
rescan_or_reset_bus(int bus, int rescan)
{
	union ccb ccb, matchccb;
	int fd, retval;
	int bufsize;

	retval = 0;

	if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
		warnx("error opening transport layer device %s", XPT_DEVICE);
		warn("%s", XPT_DEVICE);
		return(1);
	}

	if (bus != -1) {
		ccb.ccb_h.func_code = rescan ? XPT_SCAN_BUS : XPT_RESET_BUS;
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

		if ((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			fprintf(stdout, "%s of bus %d was successful\n",
			    rescan ? "Re-scan" : "Reset", bus);
		} else {
			fprintf(stdout, "%s of bus %d returned error %#x\n",
				rescan ? "Re-scan" : "Reset", bus,
				ccb.ccb_h.status & CAM_STATUS_MASK);
			retval = 1;
		}

		close(fd);
		return(retval);

	}


	/*
	 * The right way to handle this is to modify the xpt so that it can
	 * handle a wildcarded bus in a rescan or reset CCB.  At the moment
	 * that isn't implemented, so instead we enumerate the busses and
	 * send the rescan or reset to those busses in the case where the
	 * given bus is -1 (wildcard).  We don't send a rescan or reset
	 * to the xpt bus; sending a rescan to the xpt bus is effectively a
	 * no-op, sending a rescan to the xpt bus would result in a status of
	 * CAM_REQ_INVALID.
	 */
	bzero(&(&matchccb.ccb_h)[1],
	      sizeof(struct ccb_dev_match) - sizeof(struct ccb_hdr));
	matchccb.ccb_h.func_code = XPT_DEV_MATCH;
	matchccb.ccb_h.path_id = CAM_BUS_WILDCARD;
	bufsize = sizeof(struct dev_match_result) * 20;
	matchccb.cdm.match_buf_len = bufsize;
	matchccb.cdm.matches=(struct dev_match_result *)malloc(bufsize);
	if (matchccb.cdm.matches == NULL) {
		warnx("can't malloc memory for matches");
		retval = 1;
		goto bailout;
	}
	matchccb.cdm.num_matches = 0;

	matchccb.cdm.num_patterns = 1;
	matchccb.cdm.pattern_buf_len = sizeof(struct dev_match_pattern);

	matchccb.cdm.patterns = (struct dev_match_pattern *)malloc(
		matchccb.cdm.pattern_buf_len);
	if (matchccb.cdm.patterns == NULL) {
		warnx("can't malloc memory for patterns");
		retval = 1;
		goto bailout;
	}
	matchccb.cdm.patterns[0].type = DEV_MATCH_BUS;
	matchccb.cdm.patterns[0].pattern.bus_pattern.flags = BUS_MATCH_ANY;

	do {
		unsigned int i;

		if (ioctl(fd, CAMIOCOMMAND, &matchccb) == -1) {
			warn("CAMIOCOMMAND ioctl failed");
			retval = 1;
			goto bailout;
		}

		if ((matchccb.ccb_h.status != CAM_REQ_CMP)
		 || ((matchccb.cdm.status != CAM_DEV_MATCH_LAST)
		   && (matchccb.cdm.status != CAM_DEV_MATCH_MORE))) {
			warnx("got CAM error %#x, CDM error %d\n",
			      matchccb.ccb_h.status, matchccb.cdm.status);
			retval = 1;
			goto bailout;
		}

		for (i = 0; i < matchccb.cdm.num_matches; i++) {
			struct bus_match_result *bus_result;

			/* This shouldn't happen. */
			if (matchccb.cdm.matches[i].type != DEV_MATCH_BUS)
				continue;

			bus_result = &matchccb.cdm.matches[i].result.bus_result;

			/*
			 * We don't want to rescan or reset the xpt bus.
			 * See above.
			 */
			if ((int)bus_result->path_id == -1)
				continue;

			ccb.ccb_h.func_code = rescan ? XPT_SCAN_BUS :
						       XPT_RESET_BUS;
			ccb.ccb_h.path_id = bus_result->path_id;
			ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
			ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;
			ccb.crcn.flags = CAM_FLAG_NONE;

			/* run this at a low priority */
			ccb.ccb_h.pinfo.priority = 5;

			if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
				warn("CAMIOCOMMAND ioctl failed");
				retval = 1;
				goto bailout;
			}

			if ((ccb.ccb_h.status & CAM_STATUS_MASK) ==CAM_REQ_CMP){
				fprintf(stdout, "%s of bus %d was successful\n",
					rescan? "Re-scan" : "Reset",
					bus_result->path_id);
			} else {
				/*
				 * Don't bail out just yet, maybe the other
				 * rescan or reset commands will complete
				 * successfully.
				 */
				fprintf(stderr, "%s of bus %d returned error "
					"%#x\n", rescan? "Re-scan" : "Reset",
					bus_result->path_id,
					ccb.ccb_h.status & CAM_STATUS_MASK);
				retval = 1;
			}
		}
	} while ((matchccb.ccb_h.status == CAM_REQ_CMP)
		 && (matchccb.cdm.status == CAM_DEV_MATCH_MORE));

bailout:

	if (fd != -1)
		close(fd);

	if (matchccb.cdm.patterns != NULL)
		free(matchccb.cdm.patterns);
	if (matchccb.cdm.matches != NULL)
		free(matchccb.cdm.matches);

	return(retval);
}

static int
scanlun_or_reset_dev(int bus, int target, int lun, int scan)
{
	union ccb ccb;
	struct cam_device *device;
	int fd;

	device = NULL;

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

	fd = -1;

	bzero(&ccb, sizeof(union ccb));

	if (scan) {
		if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
			warnx("error opening transport layer device %s\n",
			    XPT_DEVICE);
			warn("%s", XPT_DEVICE);
			return(1);
		}
	} else {
		device = cam_open_btl(bus, target, lun, O_RDWR, NULL);
		if (device == NULL) {
			warnx("%s", cam_errbuf);
			return(1);
		}
	}

	ccb.ccb_h.func_code = (scan)? XPT_SCAN_LUN : XPT_RESET_DEV;
	ccb.ccb_h.path_id = bus;
	ccb.ccb_h.target_id = target;
	ccb.ccb_h.target_lun = lun;
	ccb.ccb_h.timeout = 5000;
	ccb.crcn.flags = CAM_FLAG_NONE;

	/* run this at a low priority */
	ccb.ccb_h.pinfo.priority = 5;

	if (scan) {
		if (ioctl(fd, CAMIOCOMMAND, &ccb) < 0) {
			warn("CAMIOCOMMAND ioctl failed");
			close(fd);
			return(1);
		}
	} else {
		if (cam_send_ccb(device, &ccb) < 0) {
			warn("error sending XPT_RESET_DEV CCB");
			cam_close_device(device);
			return(1);
		}
	}

	if (scan)
		close(fd);
	else
		cam_close_device(device);

	/*
	 * An error code of CAM_BDR_SENT is normal for a BDR request.
	 */
	if (((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
	 || ((!scan)
	  && ((ccb.ccb_h.status & CAM_STATUS_MASK) == CAM_BDR_SENT))) {
		fprintf(stdout, "%s of %d:%d:%d was successful\n",
		    scan? "Re-scan" : "Reset", bus, target, lun);
		return(0);
	} else {
		fprintf(stdout, "%s of %d:%d:%d returned error %#x\n",
		    scan? "Re-scan" : "Reset", bus, target, lun,
		    ccb.ccb_h.status & CAM_STATUS_MASK);
		return(1);
	}
}

#ifndef MINIMALISTIC
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
	unsigned int i;
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
	if (defect_list == NULL) {
		warnx("can't malloc memory for defect list");
		error = 1;
		goto defect_bailout;
	}

	rdd_cdb =(struct scsi_read_defect_data_10 *)&ccb->csio.cdb_io.cdb_bytes;

	/*
	 * cam_getccb() zeros the CCB header only.  So we need to zero the
	 * payload portion of the ccb.
	 */
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

	cam_fill_csio(&ccb->csio,
		      /*retries*/ retry_count,
		      /*cbfcnp*/ NULL,
		      /*flags*/ CAM_DIR_IN | ((arglist & CAM_ARG_ERR_RECOVER) ?
					      CAM_PASS_ERR_RECOVER : 0),
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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		error = 1;
		goto defect_bailout;
	}

	returned_length = scsi_2btoul(((struct
		scsi_read_defect_data_hdr_10 *)defect_list)->length);

	returned_format = ((struct scsi_read_defect_data_hdr_10 *)
			defect_list)->format;

	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR)
	 && (ccb->csio.scsi_status == SCSI_STATUS_CHECK_COND)
	 && ((ccb->ccb_h.status & CAM_AUTOSNS_VALID) != 0)) {
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
			if (arglist & CAM_ARG_VERBOSE)
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			goto defect_bailout;
		}
	} else if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		error = 1;
		warnx("Error returned from read defect data command");
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		goto defect_bailout;
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
#endif /* MINIMALISTIC */

#if 0
void
reassignblocks(struct cam_device *device, u_int32_t *blocks, int num_blocks)
{
	union ccb *ccb;

	ccb = cam_getccb(device);

	cam_freeccb(ccb);
}
#endif

#ifndef MINIMALISTIC
void
mode_sense(struct cam_device *device, int mode_page, int page_control,
	   int dbd, int retry_count, int timeout, u_int8_t *data, int datalen)
{
	union ccb *ccb;
	int retval;

	ccb = cam_getccb(device);

	if (ccb == NULL)
		errx(1, "mode_sense: couldn't allocate CCB");

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
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

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

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
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
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
	int binary = 0, list = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'b':
			binary = 1;
			break;
		case 'd':
			arglist |= CAM_ARG_DBD;
			break;
		case 'e':
			arglist |= CAM_ARG_MODE_EDIT;
			break;
		case 'l':
			list = 1;
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

	if (mode_page == -1 && list == 0)
		errx(1, "you must specify a mode page!");

	if (list) {
		mode_list(device, page_control, arglist & CAM_ARG_DBD,
		    retry_count, timeout);
	} else {
		mode_edit(device, mode_page, page_control,
		    arglist & CAM_ARG_DBD, arglist & CAM_ARG_MODE_EDIT, binary,
		    retry_count, timeout);
	}
}

static int
scsicmd(struct cam_device *device, int argc, char **argv, char *combinedopt,
	int retry_count, int timeout)
{
	union ccb *ccb;
	u_int32_t flags = CAM_DIR_NONE;
	u_int8_t *data_ptr = NULL;
	u_int8_t cdb[20];
	u_int8_t atacmd[12];
	struct get_hook hook;
	int c, data_bytes = 0;
	int cdb_len = 0;
	int atacmd_len = 0;
	int dmacmd = 0;
	int fpdmacmd = 0;
	int need_res = 0;
	char *datastr = NULL, *tstr, *resstr = NULL;
	int error = 0;
	int fd_data = 0, fd_res = 0;
	int retval;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("scsicmd: error allocating ccb");
		return(1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'a':
			tstr = optarg;
			while (isspace(*tstr) && (*tstr != '\0'))
				tstr++;
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			atacmd_len = buff_encode_visit(atacmd, sizeof(atacmd), tstr,
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
		case 'c':
			tstr = optarg;
			while (isspace(*tstr) && (*tstr != '\0'))
				tstr++;
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			cdb_len = buff_encode_visit(cdb, sizeof(cdb), tstr,
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
		case 'd':
			dmacmd = 1;
			break;
		case 'f':
			fpdmacmd = 1;
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
			if (data_ptr == NULL) {
				warnx("can't malloc memory for data_ptr");
				error = 1;
				goto scsicmd_bailout;
			}
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
			if (data_ptr == NULL) {
				warnx("can't malloc memory for data_ptr");
				error = 1;
				goto scsicmd_bailout;
			}
			bzero(data_ptr, data_bytes);
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
		case 'r':
			need_res = 1;
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			resstr = cget(&hook, NULL);
			if ((resstr != NULL) && (resstr[0] == '-'))
				fd_res = 1;
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
		ssize_t amt_read;
		int amt_to_read = data_bytes;
		u_int8_t *buf_ptr = data_ptr;

		for (amt_read = 0; amt_to_read > 0;
		     amt_read = read(STDIN_FILENO, buf_ptr, amt_to_read)) {
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

	if (cdb_len) {
		/*
		 * This is taken from the SCSI-3 draft spec.
		 * (T10/1157D revision 0.3)
		 * The top 3 bits of an opcode are the group code.
		 * The next 5 bits are the command code.
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
			        /* computed by buff_encode_visit */
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
	} else {
		atacmd_len = 12;
		bcopy(atacmd, &ccb->ataio.cmd.command, atacmd_len);
		if (need_res)
			ccb->ataio.cmd.flags |= CAM_ATAIO_NEEDRESULT;
		if (dmacmd)
			ccb->ataio.cmd.flags |= CAM_ATAIO_DMA;
		if (fpdmacmd)
			ccb->ataio.cmd.flags |= CAM_ATAIO_FPDMA;

		cam_fill_ataio(&ccb->ataio,
		      /*retries*/ retry_count,
		      /*cbfcnp*/ NULL,
		      /*flags*/ flags,
		      /*tag_action*/ 0,
		      /*data_ptr*/ data_ptr,
		      /*dxfer_len*/ data_bytes,
		      /*timeout*/ timeout ? timeout : 5000);
	}

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}

		error = 1;
		goto scsicmd_bailout;
	}

	if (atacmd_len && need_res) {
		if (fd_res == 0) {
			buff_decode_visit(&ccb->ataio.res.status, 11, resstr,
					  arg_put, NULL);
			fprintf(stdout, "\n");
		} else {
			fprintf(stdout,
			    "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			    ccb->ataio.res.status,
			    ccb->ataio.res.error,
			    ccb->ataio.res.lba_low,
			    ccb->ataio.res.lba_mid,
			    ccb->ataio.res.lba_high,
			    ccb->ataio.res.device,
			    ccb->ataio.res.lba_low_exp,
			    ccb->ataio.res.lba_mid_exp,
			    ccb->ataio.res.lba_high_exp,
			    ccb->ataio.res.sector_count,
			    ccb->ataio.res.sector_count_exp);
			fflush(stdout);
		}
	}

	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
	 && (arglist & CAM_ARG_CMD_IN)
	 && (data_bytes > 0)) {
		if (fd_data == 0) {
			buff_decode_visit(data_ptr, data_bytes, datastr,
					  arg_put, NULL);
			fprintf(stdout, "\n");
		} else {
			ssize_t amt_written;
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
		case 'P':
			arglist |= CAM_ARG_DEBUG_PERIPH;
			ccb.cdbg.flags |= CAM_DEBUG_PERIPH;
			break;
		case 'S':
			arglist |= CAM_ARG_DEBUG_SUBTRACE;
			ccb.cdbg.flags |= CAM_DEBUG_SUBTRACE;
			break;
		case 'T':
			arglist |= CAM_ARG_DEBUG_TRACE;
			ccb.cdbg.flags |= CAM_DEBUG_TRACE;
			break;
		case 'X':
			arglist |= CAM_ARG_DEBUG_XPT;
			ccb.cdbg.flags |= CAM_DEBUG_XPT;
			break;
		case 'c':
			arglist |= CAM_ARG_DEBUG_CDB;
			ccb.cdbg.flags |= CAM_DEBUG_CDB;
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
		arglist &= ~(CAM_ARG_DEBUG_INFO|CAM_ARG_DEBUG_PERIPH|
			     CAM_ARG_DEBUG_TRACE|CAM_ARG_DEBUG_SUBTRACE|
			     CAM_ARG_DEBUG_XPT);
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

static int
tagcontrol(struct cam_device *device, int argc, char **argv,
	   char *combinedopt)
{
	int c;
	union ccb *ccb;
	int numtags = -1;
	int retval = 0;
	int quiet = 0;
	char pathstr[1024];

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("tagcontrol: error allocating ccb");
		return(1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'N':
			numtags = strtol(optarg, NULL, 0);
			if (numtags < 0) {
				warnx("tag count %d is < 0", numtags);
				retval = 1;
				goto tagcontrol_bailout;
			}
			break;
		case 'q':
			quiet++;
			break;
		default:
			break;
		}
	}

	cam_path_string(device, pathstr, sizeof(pathstr));

	if (numtags >= 0) {
		bzero(&(&ccb->ccb_h)[1],
		      sizeof(struct ccb_relsim) - sizeof(struct ccb_hdr));
		ccb->ccb_h.func_code = XPT_REL_SIMQ;
		ccb->ccb_h.flags = CAM_DEV_QFREEZE;
		ccb->crs.release_flags = RELSIM_ADJUST_OPENINGS;
		ccb->crs.openings = numtags;


		if (cam_send_ccb(device, ccb) < 0) {
			perror("error sending XPT_REL_SIMQ CCB");
			retval = 1;
			goto tagcontrol_bailout;
		}

		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			warnx("XPT_REL_SIMQ CCB failed");
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
			retval = 1;
			goto tagcontrol_bailout;
		}


		if (quiet == 0)
			fprintf(stdout, "%stagged openings now %d\n",
				pathstr, ccb->crs.openings);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_getdevstats) - sizeof(struct ccb_hdr));

	ccb->ccb_h.func_code = XPT_GDEV_STATS;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending XPT_GDEV_STATS CCB");
		retval = 1;
		goto tagcontrol_bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("XPT_GDEV_STATS CCB failed");
		cam_error_print(device, ccb, CAM_ESF_ALL,
				CAM_EPF_ALL, stderr);
		retval = 1;
		goto tagcontrol_bailout;
	}

	if (arglist & CAM_ARG_VERBOSE) {
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "dev_openings  %d\n", ccb->cgds.dev_openings);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "dev_active    %d\n", ccb->cgds.dev_active);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "devq_openings %d\n", ccb->cgds.devq_openings);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "devq_queued   %d\n", ccb->cgds.devq_queued);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "held          %d\n", ccb->cgds.held);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "mintags       %d\n", ccb->cgds.mintags);
		fprintf(stdout, "%s", pathstr);
		fprintf(stdout, "maxtags       %d\n", ccb->cgds.maxtags);
	} else {
		if (quiet == 0) {
			fprintf(stdout, "%s", pathstr);
			fprintf(stdout, "device openings: ");
		}
		fprintf(stdout, "%d\n", ccb->cgds.dev_openings +
			ccb->cgds.dev_active);
	}

tagcontrol_bailout:

	cam_freeccb(ccb);
	return(retval);
}

static void
cts_print(struct cam_device *device, struct ccb_trans_settings *cts)
{
	char pathstr[1024];

	cam_path_string(device, pathstr, sizeof(pathstr));

	if (cts->transport == XPORT_SPI) {
		struct ccb_trans_settings_spi *spi =
		    &cts->xport_specific.spi;

		if ((spi->valid & CTS_SPI_VALID_SYNC_RATE) != 0) {

			fprintf(stdout, "%ssync parameter: %d\n", pathstr,
				spi->sync_period);

			if (spi->sync_offset != 0) {
				u_int freq;

				freq = scsi_calc_syncsrate(spi->sync_period);
				fprintf(stdout, "%sfrequency: %d.%03dMHz\n",
					pathstr, freq / 1000, freq % 1000);
			}
		}

		if (spi->valid & CTS_SPI_VALID_SYNC_OFFSET) {
			fprintf(stdout, "%soffset: %d\n", pathstr,
			    spi->sync_offset);
		}

		if (spi->valid & CTS_SPI_VALID_BUS_WIDTH) {
			fprintf(stdout, "%sbus width: %d bits\n", pathstr,
				(0x01 << spi->bus_width) * 8);
		}

		if (spi->valid & CTS_SPI_VALID_DISC) {
			fprintf(stdout, "%sdisconnection is %s\n", pathstr,
				(spi->flags & CTS_SPI_FLAGS_DISC_ENB) ?
				"enabled" : "disabled");
		}
	}
	if (cts->transport == XPORT_ATA) {
		struct ccb_trans_settings_ata *ata =
		    &cts->xport_specific.ata;

		if ((ata->valid & CTS_ATA_VALID_MODE) != 0) {
			fprintf(stdout, "%sATA mode: %s\n", pathstr,
				ata_mode2string(ata->mode));
		}
		if ((ata->valid & CTS_ATA_VALID_ATAPI) != 0) {
			fprintf(stdout, "%sATAPI packet length: %d\n", pathstr,
				ata->atapi);
		}
		if ((ata->valid & CTS_ATA_VALID_BYTECOUNT) != 0) {
			fprintf(stdout, "%sPIO transaction length: %d\n",
				pathstr, ata->bytecount);
		}
	}
	if (cts->transport == XPORT_SATA) {
		struct ccb_trans_settings_sata *sata =
		    &cts->xport_specific.sata;

		if ((sata->valid & CTS_SATA_VALID_REVISION) != 0) {
			fprintf(stdout, "%sSATA revision: %d.x\n", pathstr,
				sata->revision);
		}
		if ((sata->valid & CTS_SATA_VALID_MODE) != 0) {
			fprintf(stdout, "%sATA mode: %s\n", pathstr,
				ata_mode2string(sata->mode));
		}
		if ((sata->valid & CTS_SATA_VALID_ATAPI) != 0) {
			fprintf(stdout, "%sATAPI packet length: %d\n", pathstr,
				sata->atapi);
		}
		if ((sata->valid & CTS_SATA_VALID_BYTECOUNT) != 0) {
			fprintf(stdout, "%sPIO transaction length: %d\n",
				pathstr, sata->bytecount);
		}
		if ((sata->valid & CTS_SATA_VALID_PM) != 0) {
			fprintf(stdout, "%sPMP presence: %d\n", pathstr,
				sata->pm_present);
		}
		if ((sata->valid & CTS_SATA_VALID_TAGS) != 0) {
			fprintf(stdout, "%sNumber of tags: %d\n", pathstr,
				sata->tags);
		}
		if ((sata->valid & CTS_SATA_VALID_CAPS) != 0) {
			fprintf(stdout, "%sSATA capabilities: %08x\n", pathstr,
				sata->caps);
		}
	}
	if (cts->protocol == PROTO_SCSI) {
		struct ccb_trans_settings_scsi *scsi=
		    &cts->proto_specific.scsi;

		if (scsi->valid & CTS_SCSI_VALID_TQ) {
			fprintf(stdout, "%stagged queueing is %s\n", pathstr,
				(scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) ?
				"enabled" : "disabled");
		}
	}

}

/*
 * Get a path inquiry CCB for the specified device.
 */
static int
get_cpi(struct cam_device *device, struct ccb_pathinq *cpi)
{
	union ccb *ccb;
	int retval = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("get_cpi: couldn't allocate CCB");
		return(1);
	}
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_pathinq) - sizeof(struct ccb_hdr));
	ccb->ccb_h.func_code = XPT_PATH_INQ;
	if (cam_send_ccb(device, ccb) < 0) {
		warn("get_cpi: error sending Path Inquiry CCB");
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_cpi_bailout;
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_cpi_bailout;
	}
	bcopy(&ccb->cpi, cpi, sizeof(struct ccb_pathinq));

get_cpi_bailout:
	cam_freeccb(ccb);
	return(retval);
}

/*
 * Get a get device CCB for the specified device.
 */
static int
get_cgd(struct cam_device *device, struct ccb_getdev *cgd)
{
	union ccb *ccb;
	int retval = 0;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("get_cgd: couldn't allocate CCB");
		return(1);
	}
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_pathinq) - sizeof(struct ccb_hdr));
	ccb->ccb_h.func_code = XPT_GDEV_TYPE;
	if (cam_send_ccb(device, ccb) < 0) {
		warn("get_cgd: error sending Path Inquiry CCB");
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_cgd_bailout;
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_cgd_bailout;
	}
	bcopy(&ccb->cgd, cgd, sizeof(struct ccb_getdev));

get_cgd_bailout:
	cam_freeccb(ccb);
	return(retval);
}

static void
cpi_print(struct ccb_pathinq *cpi)
{
	char adapter_str[1024];
	int i;

	snprintf(adapter_str, sizeof(adapter_str),
		 "%s%d:", cpi->dev_name, cpi->unit_number);

	fprintf(stdout, "%s SIM/HBA version: %d\n", adapter_str,
		cpi->version_num);

	for (i = 1; i < 0xff; i = i << 1) {
		const char *str;

		if ((i & cpi->hba_inquiry) == 0)
			continue;

		fprintf(stdout, "%s supports ", adapter_str);

		switch(i) {
		case PI_MDP_ABLE:
			str = "MDP message";
			break;
		case PI_WIDE_32:
			str = "32 bit wide SCSI";
			break;
		case PI_WIDE_16:
			str = "16 bit wide SCSI";
			break;
		case PI_SDTR_ABLE:
			str = "SDTR message";
			break;
		case PI_LINKED_CDB:
			str = "linked CDBs";
			break;
		case PI_TAG_ABLE:
			str = "tag queue messages";
			break;
		case PI_SOFT_RST:
			str = "soft reset alternative";
			break;
		case PI_SATAPM:
			str = "SATA Port Multiplier";
			break;
		default:
			str = "unknown PI bit set";
			break;
		}
		fprintf(stdout, "%s\n", str);
	}

	for (i = 1; i < 0xff; i = i << 1) {
		const char *str;

		if ((i & cpi->hba_misc) == 0)
			continue;

		fprintf(stdout, "%s ", adapter_str);

		switch(i) {
		case PIM_SCANHILO:
			str = "bus scans from high ID to low ID";
			break;
		case PIM_NOREMOVE:
			str = "removable devices not included in scan";
			break;
		case PIM_NOINITIATOR:
			str = "initiator role not supported";
			break;
		case PIM_NOBUSRESET:
			str = "user has disabled initial BUS RESET or"
			      " controller is in target/mixed mode";
			break;
		case PIM_NO_6_BYTE:
			str = "do not send 6-byte commands";
			break;
		case PIM_SEQSCAN:
			str = "scan bus sequentially";
			break;
		default:
			str = "unknown PIM bit set";
			break;
		}
		fprintf(stdout, "%s\n", str);
	}

	for (i = 1; i < 0xff; i = i << 1) {
		const char *str;

		if ((i & cpi->target_sprt) == 0)
			continue;

		fprintf(stdout, "%s supports ", adapter_str);
		switch(i) {
		case PIT_PROCESSOR:
			str = "target mode processor mode";
			break;
		case PIT_PHASE:
			str = "target mode phase cog. mode";
			break;
		case PIT_DISCONNECT:
			str = "disconnects in target mode";
			break;
		case PIT_TERM_IO:
			str = "terminate I/O message in target mode";
			break;
		case PIT_GRP_6:
			str = "group 6 commands in target mode";
			break;
		case PIT_GRP_7:
			str = "group 7 commands in target mode";
			break;
		default:
			str = "unknown PIT bit set";
			break;
		}

		fprintf(stdout, "%s\n", str);
	}
	fprintf(stdout, "%s HBA engine count: %d\n", adapter_str,
		cpi->hba_eng_cnt);
	fprintf(stdout, "%s maximum target: %d\n", adapter_str,
		cpi->max_target);
	fprintf(stdout, "%s maximum LUN: %d\n", adapter_str,
		cpi->max_lun);
	fprintf(stdout, "%s highest path ID in subsystem: %d\n",
		adapter_str, cpi->hpath_id);
	fprintf(stdout, "%s initiator ID: %d\n", adapter_str,
		cpi->initiator_id);
	fprintf(stdout, "%s SIM vendor: %s\n", adapter_str, cpi->sim_vid);
	fprintf(stdout, "%s HBA vendor: %s\n", adapter_str, cpi->hba_vid);
	fprintf(stdout, "%s HBA vendor ID: 0x%04x\n",
	    adapter_str, cpi->hba_vendor);
	fprintf(stdout, "%s HBA device ID: 0x%04x\n",
	    adapter_str, cpi->hba_device);
	fprintf(stdout, "%s HBA subvendor ID: 0x%04x\n",
	    adapter_str, cpi->hba_subvendor);
	fprintf(stdout, "%s HBA subdevice ID: 0x%04x\n",
	    adapter_str, cpi->hba_subdevice);
	fprintf(stdout, "%s bus ID: %d\n", adapter_str, cpi->bus_id);
	fprintf(stdout, "%s base transfer speed: ", adapter_str);
	if (cpi->base_transfer_speed > 1000)
		fprintf(stdout, "%d.%03dMB/sec\n",
			cpi->base_transfer_speed / 1000,
			cpi->base_transfer_speed % 1000);
	else
		fprintf(stdout, "%dKB/sec\n",
			(cpi->base_transfer_speed % 1000) * 1000);
	fprintf(stdout, "%s maximum transfer size: %u bytes\n",
	    adapter_str, cpi->maxio);
}

static int
get_print_cts(struct cam_device *device, int user_settings, int quiet,
	      struct ccb_trans_settings *cts)
{
	int retval;
	union ccb *ccb;

	retval = 0;
	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("get_print_cts: error allocating ccb");
		return(1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_trans_settings) - sizeof(struct ccb_hdr));

	ccb->ccb_h.func_code = XPT_GET_TRAN_SETTINGS;

	if (user_settings == 0)
		ccb->cts.type = CTS_TYPE_CURRENT_SETTINGS;
	else
		ccb->cts.type = CTS_TYPE_USER_SETTINGS;

	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending XPT_GET_TRAN_SETTINGS CCB");
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_print_cts_bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("XPT_GET_TRANS_SETTINGS CCB failed");
		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		retval = 1;
		goto get_print_cts_bailout;
	}

	if (quiet == 0)
		cts_print(device, &ccb->cts);

	if (cts != NULL)
		bcopy(&ccb->cts, cts, sizeof(struct ccb_trans_settings));

get_print_cts_bailout:

	cam_freeccb(ccb);

	return(retval);
}

static int
ratecontrol(struct cam_device *device, int retry_count, int timeout,
	    int argc, char **argv, char *combinedopt)
{
	int c;
	union ccb *ccb;
	int user_settings = 0;
	int retval = 0;
	int disc_enable = -1, tag_enable = -1;
	int mode = -1;
	int offset = -1;
	double syncrate = -1;
	int bus_width = -1;
	int quiet = 0;
	int change_settings = 0, send_tur = 0;
	struct ccb_pathinq cpi;

	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("ratecontrol: error allocating ccb");
		return(1);
	}
	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c){
		case 'a':
			send_tur = 1;
			break;
		case 'c':
			user_settings = 0;
			break;
		case 'D':
			if (strncasecmp(optarg, "enable", 6) == 0)
				disc_enable = 1;
			else if (strncasecmp(optarg, "disable", 7) == 0)
				disc_enable = 0;
			else {
				warnx("-D argument \"%s\" is unknown", optarg);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		case 'M':
			mode = ata_string2mode(optarg);
			if (mode < 0) {
				warnx("unknown mode '%s'", optarg);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		case 'O':
			offset = strtol(optarg, NULL, 0);
			if (offset < 0) {
				warnx("offset value %d is < 0", offset);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		case 'q':
			quiet++;
			break;
		case 'R':
			syncrate = atof(optarg);
			if (syncrate < 0) {
				warnx("sync rate %f is < 0", syncrate);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		case 'T':
			if (strncasecmp(optarg, "enable", 6) == 0)
				tag_enable = 1;
			else if (strncasecmp(optarg, "disable", 7) == 0)
				tag_enable = 0;
			else {
				warnx("-T argument \"%s\" is unknown", optarg);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		case 'U':
			user_settings = 1;
			break;
		case 'W':
			bus_width = strtol(optarg, NULL, 0);
			if (bus_width < 0) {
				warnx("bus width %d is < 0", bus_width);
				retval = 1;
				goto ratecontrol_bailout;
			}
			change_settings = 1;
			break;
		default:
			break;
		}
	}
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_pathinq) - sizeof(struct ccb_hdr));
	/*
	 * Grab path inquiry information, so we can determine whether
	 * or not the initiator is capable of the things that the user
	 * requests.
	 */
	ccb->ccb_h.func_code = XPT_PATH_INQ;
	if (cam_send_ccb(device, ccb) < 0) {
		perror("error sending XPT_PATH_INQ CCB");
		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		retval = 1;
		goto ratecontrol_bailout;
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("XPT_PATH_INQ CCB failed");
		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		retval = 1;
		goto ratecontrol_bailout;
	}
	bcopy(&ccb->cpi, &cpi, sizeof(struct ccb_pathinq));
	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_trans_settings) - sizeof(struct ccb_hdr));
	if (quiet == 0) {
		fprintf(stdout, "%s parameters:\n",
		    user_settings ? "User" : "Current");
	}
	retval = get_print_cts(device, user_settings, quiet, &ccb->cts);
	if (retval != 0)
		goto ratecontrol_bailout;

	if (arglist & CAM_ARG_VERBOSE)
		cpi_print(&cpi);

	if (change_settings) {
		int didsettings = 0;
		struct ccb_trans_settings_spi *spi = NULL;
		struct ccb_trans_settings_ata *ata = NULL;
		struct ccb_trans_settings_sata *sata = NULL;
		struct ccb_trans_settings_scsi *scsi = NULL;

		if (ccb->cts.transport == XPORT_SPI)
			spi = &ccb->cts.xport_specific.spi;
		if (ccb->cts.transport == XPORT_ATA)
			ata = &ccb->cts.xport_specific.ata;
		if (ccb->cts.transport == XPORT_SATA)
			sata = &ccb->cts.xport_specific.sata;
		if (ccb->cts.protocol == PROTO_SCSI)
			scsi = &ccb->cts.proto_specific.scsi;
		ccb->cts.xport_specific.valid = 0;
		ccb->cts.proto_specific.valid = 0;
		if (spi && disc_enable != -1) {
			spi->valid |= CTS_SPI_VALID_DISC;
			if (disc_enable == 0)
				spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;
			else
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
		}
		if (scsi && tag_enable != -1) {
			if ((cpi.hba_inquiry & PI_TAG_ABLE) == 0) {
				warnx("HBA does not support tagged queueing, "
				      "so you cannot modify tag settings");
				retval = 1;
				goto ratecontrol_bailout;
			}
			scsi->valid |= CTS_SCSI_VALID_TQ;
			if (tag_enable == 0)
				scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
			else
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
			didsettings++;
		}
		if (spi && offset != -1) {
			if ((cpi.hba_inquiry & PI_SDTR_ABLE) == 0) {
				warnx("HBA is not capable of changing offset");
				retval = 1;
				goto ratecontrol_bailout;
			}
			spi->valid |= CTS_SPI_VALID_SYNC_OFFSET;
			spi->sync_offset = offset;
			didsettings++;
		}
		if (spi && syncrate != -1) {
			int prelim_sync_period;
			u_int freq;

			if ((cpi.hba_inquiry & PI_SDTR_ABLE) == 0) {
				warnx("HBA is not capable of changing "
				      "transfer rates");
				retval = 1;
				goto ratecontrol_bailout;
			}
			spi->valid |= CTS_SPI_VALID_SYNC_RATE;
			/*
			 * The sync rate the user gives us is in MHz.
			 * We need to translate it into KHz for this
			 * calculation.
			 */
			syncrate *= 1000;
			/*
			 * Next, we calculate a "preliminary" sync period
			 * in tenths of a nanosecond.
			 */
			if (syncrate == 0)
				prelim_sync_period = 0;
			else
				prelim_sync_period = 10000000 / syncrate;
			spi->sync_period =
				scsi_calc_syncparam(prelim_sync_period);
			freq = scsi_calc_syncsrate(spi->sync_period);
			didsettings++;
		}
		if (sata && syncrate != -1) {
			if ((cpi.hba_inquiry & PI_SDTR_ABLE) == 0) {
				warnx("HBA is not capable of changing "
				      "transfer rates");
				retval = 1;
				goto ratecontrol_bailout;
			}
			sata->revision = ata_speed2revision(syncrate * 100);
			if (sata->revision < 0) {
				warnx("Invalid rate %f", syncrate);
				retval = 1;
				goto ratecontrol_bailout;
			}
			sata->valid |= CTS_SATA_VALID_REVISION;
			didsettings++;
		}
		if ((ata || sata) && mode != -1) {
			if ((cpi.hba_inquiry & PI_SDTR_ABLE) == 0) {
				warnx("HBA is not capable of changing "
				      "transfer rates");
				retval = 1;
				goto ratecontrol_bailout;
			}
			if (ata) {
				ata->mode = mode;
				ata->valid |= CTS_ATA_VALID_MODE;
			} else {
				sata->mode = mode;
				sata->valid |= CTS_SATA_VALID_MODE;
			}
			didsettings++;
		}
		/*
		 * The bus_width argument goes like this:
		 * 0 == 8 bit
		 * 1 == 16 bit
		 * 2 == 32 bit
		 * Therefore, if you shift the number of bits given on the
		 * command line right by 4, you should get the correct
		 * number.
		 */
		if (spi && bus_width != -1) {
			/*
			 * We might as well validate things here with a
			 * decipherable error message, rather than what
			 * will probably be an indecipherable error message
			 * by the time it gets back to us.
			 */
			if ((bus_width == 16)
			 && ((cpi.hba_inquiry & PI_WIDE_16) == 0)) {
				warnx("HBA does not support 16 bit bus width");
				retval = 1;
				goto ratecontrol_bailout;
			} else if ((bus_width == 32)
				&& ((cpi.hba_inquiry & PI_WIDE_32) == 0)) {
				warnx("HBA does not support 32 bit bus width");
				retval = 1;
				goto ratecontrol_bailout;
			} else if ((bus_width != 8)
				&& (bus_width != 16)
				&& (bus_width != 32)) {
				warnx("Invalid bus width %d", bus_width);
				retval = 1;
				goto ratecontrol_bailout;
			}
			spi->valid |= CTS_SPI_VALID_BUS_WIDTH;
			spi->bus_width = bus_width >> 4;
			didsettings++;
		}
		if  (didsettings == 0) {
			goto ratecontrol_bailout;
		}
		if  (!user_settings && (ata || sata)) {
			warnx("You can modify only user settings for ATA/SATA");
			retval = 1;
			goto ratecontrol_bailout;
		}
		ccb->ccb_h.func_code = XPT_SET_TRAN_SETTINGS;
		if (cam_send_ccb(device, ccb) < 0) {
			perror("error sending XPT_SET_TRAN_SETTINGS CCB");
			if (arglist & CAM_ARG_VERBOSE) {
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			}
			retval = 1;
			goto ratecontrol_bailout;
		}
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			warnx("XPT_SET_TRANS_SETTINGS CCB failed");
			if (arglist & CAM_ARG_VERBOSE) {
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			}
			retval = 1;
			goto ratecontrol_bailout;
		}
	}
	if (send_tur) {
		retval = testunitready(device, retry_count, timeout,
				       (arglist & CAM_ARG_VERBOSE) ? 0 : 1);
		/*
		 * If the TUR didn't succeed, just bail.
		 */
		if (retval != 0) {
			if (quiet == 0)
				fprintf(stderr, "Test Unit Ready failed\n");
			goto ratecontrol_bailout;
		}
		/*
		 * If the user wants things quiet, there's no sense in
		 * getting the transfer settings, if we're not going
		 * to print them.
		 */
		if (quiet != 0)
			goto ratecontrol_bailout;
		fprintf(stdout, "New parameters:\n");
		retval = get_print_cts(device, user_settings, 0, NULL);
	}

ratecontrol_bailout:
	cam_freeccb(ccb);
	return(retval);
}

static int
scsiformat(struct cam_device *device, int argc, char **argv,
	   char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	int c;
	int ycount = 0, quiet = 0;
	int error = 0, response = 0, retval = 0;
	int use_timeout = 10800 * 1000;
	int immediate = 1;
	struct format_defect_list_header fh;
	u_int8_t *data_ptr = NULL;
	u_int32_t dxfer_len = 0;
	u_int8_t byte2 = 0;
	int num_warnings = 0;
	int reportonly = 0;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("scsiformat: error allocating ccb");
		return(1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch(c) {
		case 'q':
			quiet++;
			break;
		case 'r':
			reportonly = 1;
			break;
		case 'w':
			immediate = 0;
			break;
		case 'y':
			ycount++;
			break;
		}
	}

	if (reportonly)
		goto doreport;

	if (quiet == 0) {
		fprintf(stdout, "You are about to REMOVE ALL DATA from the "
			"following device:\n");

		error = scsidoinquiry(device, argc, argv, combinedopt,
				      retry_count, timeout);

		if (error != 0) {
			warnx("scsiformat: error sending inquiry");
			goto scsiformat_bailout;
		}
	}

	if (ycount == 0) {

		do {
			char str[1024];

			fprintf(stdout, "Are you SURE you want to do "
				"this? (yes/no) ");

			if (fgets(str, sizeof(str), stdin) != NULL) {

				if (strncasecmp(str, "yes", 3) == 0)
					response = 1;
				else if (strncasecmp(str, "no", 2) == 0)
					response = -1;
				else {
					fprintf(stdout, "Please answer"
						" \"yes\" or \"no\"\n");
				}
			}
		} while (response == 0);

		if (response == -1) {
			error = 1;
			goto scsiformat_bailout;
		}
	}

	if (timeout != 0)
		use_timeout = timeout;

	if (quiet == 0) {
		fprintf(stdout, "Current format timeout is %d seconds\n",
			use_timeout / 1000);
	}

	/*
	 * If the user hasn't disabled questions and didn't specify a
	 * timeout on the command line, ask them if they want the current
	 * timeout.
	 */
	if ((ycount == 0)
	 && (timeout == 0)) {
		char str[1024];
		int new_timeout = 0;

		fprintf(stdout, "Enter new timeout in seconds or press\n"
			"return to keep the current timeout [%d] ",
			use_timeout / 1000);

		if (fgets(str, sizeof(str), stdin) != NULL) {
			if (str[0] != '\0')
				new_timeout = atoi(str);
		}

		if (new_timeout != 0) {
			use_timeout = new_timeout * 1000;
			fprintf(stdout, "Using new timeout value %d\n",
				use_timeout / 1000);
		}
	}

	/*
	 * Keep this outside the if block below to silence any unused
	 * variable warnings.
	 */
	bzero(&fh, sizeof(fh));

	/*
	 * If we're in immediate mode, we've got to include the format
	 * header
	 */
	if (immediate != 0) {
		fh.byte2 = FU_DLH_IMMED;
		data_ptr = (u_int8_t *)&fh;
		dxfer_len = sizeof(fh);
		byte2 = FU_FMT_DATA;
	} else if (quiet == 0) {
		fprintf(stdout, "Formatting...");
		fflush(stdout);
	}

	scsi_format_unit(&ccb->csio,
			 /* retries */ retry_count,
			 /* cbfcnp */ NULL,
			 /* tag_action */ MSG_SIMPLE_Q_TAG,
			 /* byte2 */ byte2,
			 /* ileave */ 0,
			 /* data_ptr */ data_ptr,
			 /* dxfer_len */ dxfer_len,
			 /* sense_len */ SSD_FULL_SIZE,
			 /* timeout */ use_timeout);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((immediate == 0)
	   && ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP))) {
		const char errstr[] = "error sending format command";

		if (retval < 0)
			warn(errstr);
		else
			warnx(errstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		error = 1;
		goto scsiformat_bailout;
	}

	/*
	 * If we ran in non-immediate mode, we already checked for errors
	 * above and printed out any necessary information.  If we're in
	 * immediate mode, we need to loop through and get status
	 * information periodically.
	 */
	if (immediate == 0) {
		if (quiet == 0) {
			fprintf(stdout, "Format Complete\n");
		}
		goto scsiformat_bailout;
	}

doreport:
	do {
		cam_status status;

		bzero(&(&ccb->ccb_h)[1],
		      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

		/*
		 * There's really no need to do error recovery or
		 * retries here, since we're just going to sit in a
		 * loop and wait for the device to finish formatting.
		 */
		scsi_test_unit_ready(&ccb->csio,
				     /* retries */ 0,
				     /* cbfcnp */ NULL,
				     /* tag_action */ MSG_SIMPLE_Q_TAG,
				     /* sense_len */ SSD_FULL_SIZE,
				     /* timeout */ 5000);

		/* Disable freezing the device queue */
		ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

		retval = cam_send_ccb(device, ccb);

		/*
		 * If we get an error from the ioctl, bail out.  SCSI
		 * errors are expected.
		 */
		if (retval < 0) {
			warn("error sending CAMIOCOMMAND ioctl");
			if (arglist & CAM_ARG_VERBOSE) {
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			}
			error = 1;
			goto scsiformat_bailout;
		}

		status = ccb->ccb_h.status & CAM_STATUS_MASK;

		if ((status != CAM_REQ_CMP)
		 && (status == CAM_SCSI_STATUS_ERROR)
		 && ((ccb->ccb_h.status & CAM_AUTOSNS_VALID) != 0)) {
			struct scsi_sense_data *sense;
			int error_code, sense_key, asc, ascq;

			sense = &ccb->csio.sense_data;
			scsi_extract_sense(sense, &error_code, &sense_key,
					   &asc, &ascq);

			/*
			 * According to the SCSI-2 and SCSI-3 specs, a
			 * drive that is in the middle of a format should
			 * return NOT READY with an ASC of "logical unit
			 * not ready, format in progress".  The sense key
			 * specific bytes will then be a progress indicator.
			 */
			if ((sense_key == SSD_KEY_NOT_READY)
			 && (asc == 0x04) && (ascq == 0x04)) {
				if ((sense->extra_len >= 10)
				 && ((sense->sense_key_spec[0] &
				      SSD_SCS_VALID) != 0)
				 && (quiet == 0)) {
					int val;
					u_int64_t percentage;

					val = scsi_2btoul(
						&sense->sense_key_spec[1]);
					percentage = 10000 * val;

					fprintf(stdout,
						"\rFormatting:  %ju.%02u %% "
						"(%d/%d) done",
						(uintmax_t)(percentage /
						(0x10000 * 100)),
						(unsigned)((percentage /
						0x10000) % 100),
						val, 0x10000);
					fflush(stdout);
				} else if ((quiet == 0)
					&& (++num_warnings <= 1)) {
					warnx("Unexpected SCSI Sense Key "
					      "Specific value returned "
					      "during format:");
					scsi_sense_print(device, &ccb->csio,
							 stderr);
					warnx("Unable to print status "
					      "information, but format will "
					      "proceed.");
					warnx("will exit when format is "
					      "complete");
				}
				sleep(1);
			} else {
				warnx("Unexpected SCSI error during format");
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
				error = 1;
				goto scsiformat_bailout;
			}

		} else if (status != CAM_REQ_CMP) {
			warnx("Unexpected CAM status %#x", status);
			if (arglist & CAM_ARG_VERBOSE)
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			error = 1;
			goto scsiformat_bailout;
		}

	} while((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP);

	if (quiet == 0)
		fprintf(stdout, "\nFormat Complete\n");

scsiformat_bailout:

	cam_freeccb(ccb);

	return(error);
}

static int
scsireportluns(struct cam_device *device, int argc, char **argv,
	       char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	int c, countonly, lunsonly;
	struct scsi_report_luns_data *lundata;
	int alloc_len;
	uint8_t report_type;
	uint32_t list_len, i, j;
	int retval;

	retval = 0;
	lundata = NULL;
	report_type = RPL_REPORT_DEFAULT;
	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("%s: error allocating ccb", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

	countonly = 0;
	lunsonly = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'c':
			countonly++;
			break;
		case 'l':
			lunsonly++;
			break;
		case 'r':
			if (strcasecmp(optarg, "default") == 0)
				report_type = RPL_REPORT_DEFAULT;
			else if (strcasecmp(optarg, "wellknown") == 0)
				report_type = RPL_REPORT_WELLKNOWN;
			else if (strcasecmp(optarg, "all") == 0)
				report_type = RPL_REPORT_ALL;
			else {
				warnx("%s: invalid report type \"%s\"",
				      __func__, optarg);
				retval = 1;
				goto bailout;
			}
			break;
		default:
			break;
		}
	}

	if ((countonly != 0)
	 && (lunsonly != 0)) {
		warnx("%s: you can only specify one of -c or -l", __func__);
		retval = 1;
		goto bailout;
	}
	/*
	 * According to SPC-4, the allocation length must be at least 16
	 * bytes -- enough for the header and one LUN.
	 */
	alloc_len = sizeof(*lundata) + 8;

retry:

	lundata = malloc(alloc_len);

	if (lundata == NULL) {
		warn("%s: error mallocing %d bytes", __func__, alloc_len);
		retval = 1;
		goto bailout;
	}

	scsi_report_luns(&ccb->csio,
			 /*retries*/ retry_count,
			 /*cbfcnp*/ NULL,
			 /*tag_action*/ MSG_SIMPLE_Q_TAG,
			 /*select_report*/ report_type,
			 /*rpl_buf*/ lundata,
			 /*alloc_len*/ alloc_len,
			 /*sense_len*/ SSD_FULL_SIZE,
			 /*timeout*/ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending REPORT LUNS command");

		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);

		retval = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
		retval = 1;
		goto bailout;
	}


	list_len = scsi_4btoul(lundata->length);

	/*
	 * If we need to list the LUNs, and our allocation
	 * length was too short, reallocate and retry.
	 */
	if ((countonly == 0)
	 && (list_len > (alloc_len - sizeof(*lundata)))) {
		alloc_len = list_len + sizeof(*lundata);
		free(lundata);
		goto retry;
	}

	if (lunsonly == 0)
		fprintf(stdout, "%u LUN%s found\n", list_len / 8,
			((list_len / 8) > 1) ? "s" : "");

	if (countonly != 0)
		goto bailout;

	for (i = 0; i < (list_len / 8); i++) {
		int no_more;

		no_more = 0;
		for (j = 0; j < sizeof(lundata->luns[i].lundata); j += 2) {
			if (j != 0)
				fprintf(stdout, ",");
			switch (lundata->luns[i].lundata[j] &
				RPL_LUNDATA_ATYP_MASK) {
			case RPL_LUNDATA_ATYP_PERIPH:
				if ((lundata->luns[i].lundata[j] &
				    RPL_LUNDATA_PERIPH_BUS_MASK) != 0)
					fprintf(stdout, "%d:",
						lundata->luns[i].lundata[j] &
						RPL_LUNDATA_PERIPH_BUS_MASK);
				else if ((j == 0)
				      && ((lundata->luns[i].lundata[j+2] &
					  RPL_LUNDATA_PERIPH_BUS_MASK) == 0))
					no_more = 1;

				fprintf(stdout, "%d",
					lundata->luns[i].lundata[j+1]);
				break;
			case RPL_LUNDATA_ATYP_FLAT: {
				uint8_t tmplun[2];
				tmplun[0] = lundata->luns[i].lundata[j] &
					RPL_LUNDATA_FLAT_LUN_MASK;
				tmplun[1] = lundata->luns[i].lundata[j+1];

				fprintf(stdout, "%d", scsi_2btoul(tmplun));
				no_more = 1;
				break;
			}
			case RPL_LUNDATA_ATYP_LUN:
				fprintf(stdout, "%d:%d:%d",
					(lundata->luns[i].lundata[j+1] &
					RPL_LUNDATA_LUN_BUS_MASK) >> 5,
					lundata->luns[i].lundata[j] &
					RPL_LUNDATA_LUN_TARG_MASK,
					lundata->luns[i].lundata[j+1] &
					RPL_LUNDATA_LUN_LUN_MASK);
				break;
			case RPL_LUNDATA_ATYP_EXTLUN: {
				int field_len, field_len_code, eam_code;

				eam_code = lundata->luns[i].lundata[j] &
					RPL_LUNDATA_EXT_EAM_MASK;
				field_len_code = (lundata->luns[i].lundata[j] &
					RPL_LUNDATA_EXT_LEN_MASK) >> 4;
				field_len = field_len_code * 2;

				if ((eam_code == RPL_LUNDATA_EXT_EAM_WK)
				 && (field_len_code == 0x00)) {
					fprintf(stdout, "%d",
						lundata->luns[i].lundata[j+1]);
				} else if ((eam_code ==
					    RPL_LUNDATA_EXT_EAM_NOT_SPEC)
					&& (field_len_code == 0x03)) {
					uint8_t tmp_lun[8];

					/*
					 * This format takes up all 8 bytes.
					 * If we aren't starting at offset 0,
					 * that's a bug.
					 */
					if (j != 0) {
						fprintf(stdout, "Invalid "
							"offset %d for "
							"Extended LUN not "
							"specified format", j);
						no_more = 1;
						break;
					}
					bzero(tmp_lun, sizeof(tmp_lun));
					bcopy(&lundata->luns[i].lundata[j+1],
					      &tmp_lun[1], sizeof(tmp_lun) - 1);
					fprintf(stdout, "%#jx",
					       (intmax_t)scsi_8btou64(tmp_lun));
					no_more = 1;
				} else {
					fprintf(stderr, "Unknown Extended LUN"
						"Address method %#x, length "
						"code %#x", eam_code,
						field_len_code);
					no_more = 1;
				}
				break;
			}
			default:
				fprintf(stderr, "Unknown LUN address method "
					"%#x\n", lundata->luns[i].lundata[0] &
					RPL_LUNDATA_ATYP_MASK);
				break;
			}
			/*
			 * For the flat addressing method, there are no
			 * other levels after it.
			 */
			if (no_more != 0)
				break;
		}
		fprintf(stdout, "\n");
	}

bailout:

	cam_freeccb(ccb);

	free(lundata);

	return (retval);
}

static int
scsireadcapacity(struct cam_device *device, int argc, char **argv,
		 char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	int blocksizeonly, humanize, numblocks, quiet, sizeonly, baseten;
	struct scsi_read_capacity_data rcap;
	struct scsi_read_capacity_data_long rcaplong;
	uint64_t maxsector;
	uint32_t block_len;
	int retval;
	int c;

	blocksizeonly = 0;
	humanize = 0;
	numblocks = 0;
	quiet = 0;
	sizeonly = 0;
	baseten = 0;
	retval = 0;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("%s: error allocating ccb", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'b':
			blocksizeonly++;
			break;
		case 'h':
			humanize++;
			baseten = 0;
			break;
		case 'H':
			humanize++;
			baseten++;
			break;
		case 'N':
			numblocks++;
			break;
		case 'q':
			quiet++;
			break;
		case 's':
			sizeonly++;
			break;
		default:
			break;
		}
	}

	if ((blocksizeonly != 0)
	 && (numblocks != 0)) {
		warnx("%s: you can only specify one of -b or -N", __func__);
		retval = 1;
		goto bailout;
	}

	if ((blocksizeonly != 0)
	 && (sizeonly != 0)) {
		warnx("%s: you can only specify one of -b or -s", __func__);
		retval = 1;
		goto bailout;
	}

	if ((humanize != 0)
	 && (quiet != 0)) {
		warnx("%s: you can only specify one of -h/-H or -q", __func__);
		retval = 1;
		goto bailout;
	}

	if ((humanize != 0)
	 && (blocksizeonly != 0)) {
		warnx("%s: you can only specify one of -h/-H or -b", __func__);
		retval = 1;
		goto bailout;
	}

	scsi_read_capacity(&ccb->csio,
			   /*retries*/ retry_count,
			   /*cbfcnp*/ NULL,
			   /*tag_action*/ MSG_SIMPLE_Q_TAG,
			   &rcap,
			   SSD_FULL_SIZE,
			   /*timeout*/ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending READ CAPACITY command");

		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);

		retval = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
		retval = 1;
		goto bailout;
	}

	maxsector = scsi_4btoul(rcap.addr);
	block_len = scsi_4btoul(rcap.length);

	/*
	 * A last block of 2^32-1 means that the true capacity is over 2TB,
	 * and we need to issue the long READ CAPACITY to get the real
	 * capacity.  Otherwise, we're all set.
	 */
	if (maxsector != 0xffffffff)
		goto do_print;

	scsi_read_capacity_16(&ccb->csio,
			      /*retries*/ retry_count,
			      /*cbfcnp*/ NULL,
			      /*tag_action*/ MSG_SIMPLE_Q_TAG,
			      /*lba*/ 0,
			      /*reladdr*/ 0,
			      /*pmi*/ 0,
			      &rcaplong,
			      /*sense_len*/ SSD_FULL_SIZE,
			      /*timeout*/ timeout ? timeout : 5000);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending READ CAPACITY (16) command");

		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);

		retval = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
		retval = 1;
		goto bailout;
	}

	maxsector = scsi_8btou64(rcaplong.addr);
	block_len = scsi_4btoul(rcaplong.length);

do_print:
	if (blocksizeonly == 0) {
		/*
		 * Humanize implies !quiet, and also implies numblocks.
		 */
		if (humanize != 0) {
			char tmpstr[6];
			int64_t tmpbytes;
			int ret;

			tmpbytes = (maxsector + 1) * block_len;
			ret = humanize_number(tmpstr, sizeof(tmpstr),
					      tmpbytes, "", HN_AUTOSCALE,
					      HN_B | HN_DECIMAL |
					      ((baseten != 0) ?
					      HN_DIVISOR_1000 : 0));
			if (ret == -1) {
				warnx("%s: humanize_number failed!", __func__);
				retval = 1;
				goto bailout;
			}
			fprintf(stdout, "Device Size: %s%s", tmpstr,
				(sizeonly == 0) ?  ", " : "\n");
		} else if (numblocks != 0) {
			fprintf(stdout, "%s%ju%s", (quiet == 0) ?
				"Blocks: " : "", (uintmax_t)maxsector + 1,
				(sizeonly == 0) ? ", " : "\n");
		} else {
			fprintf(stdout, "%s%ju%s", (quiet == 0) ?
				"Last Block: " : "", (uintmax_t)maxsector,
				(sizeonly == 0) ? ", " : "\n");
		}
	}
	if (sizeonly == 0)
		fprintf(stdout, "%s%u%s\n", (quiet == 0) ?
			"Block Length: " : "", block_len, (quiet == 0) ?
			" bytes" : "");
bailout:
	cam_freeccb(ccb);

	return (retval);
}

static int
smpcmd(struct cam_device *device, int argc, char **argv, char *combinedopt,
       int retry_count, int timeout)
{
	int c, error;
	union ccb *ccb;
	uint8_t *smp_request = NULL, *smp_response = NULL;
	int request_size = 0, response_size = 0;
	int fd_request = 0, fd_response = 0;
	char *datastr = NULL;
	struct get_hook hook;
	int retval;
	int flags = 0;

	/*
	 * Note that at the moment we don't support sending SMP CCBs to
	 * devices that aren't probed by CAM.
	 */
	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'R':
			arglist |= CAM_ARG_CMD_IN;
			response_size = strtol(optarg, NULL, 0);
			if (response_size <= 0) {
				warnx("invalid number of response bytes %d",
				      response_size);
				error = 1;
				goto smpcmd_bailout;
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
				fd_response = 1;

			smp_response = (u_int8_t *)malloc(response_size);
			if (smp_response == NULL) {
				warn("can't malloc memory for SMP response");
				error = 1;
				goto smpcmd_bailout;
			}
			break;
		case 'r':
			arglist |= CAM_ARG_CMD_OUT;
			request_size = strtol(optarg, NULL, 0);
			if (request_size <= 0) {
				warnx("invalid number of request bytes %d",
				      request_size);
				error = 1;
				goto smpcmd_bailout;
			}
			hook.argc = argc - optind;
			hook.argv = argv + optind;
			hook.got = 0;
			datastr = cget(&hook, NULL);
			smp_request = (u_int8_t *)malloc(request_size);
			if (smp_request == NULL) {
				warn("can't malloc memory for SMP request");
				error = 1;
				goto smpcmd_bailout;
			}
			bzero(smp_request, request_size);
			/*
			 * If the user supplied "-" instead of a format, he
			 * wants the data to be read from stdin.
			 */
			if ((datastr != NULL)
			 && (datastr[0] == '-'))
				fd_request = 1;
			else
				buff_encode_visit(smp_request, request_size,
						  datastr,
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
	if ((fd_request == 1) && (arglist & CAM_ARG_CMD_OUT)) {
		ssize_t amt_read;
		int amt_to_read = request_size;
		u_int8_t *buf_ptr = smp_request;

		for (amt_read = 0; amt_to_read > 0;
		     amt_read = read(STDIN_FILENO, buf_ptr, amt_to_read)) {
			if (amt_read == -1) {
				warn("error reading data from stdin");
				error = 1;
				goto smpcmd_bailout;
			}
			amt_to_read -= amt_read;
			buf_ptr += amt_read;
		}
	}

	if (((arglist & CAM_ARG_CMD_IN) == 0)
	 || ((arglist & CAM_ARG_CMD_OUT) == 0)) {
		warnx("%s: need both the request (-r) and response (-R) "
		      "arguments", __func__);
		error = 1;
		goto smpcmd_bailout;
	}

	flags |= CAM_DEV_QFRZDIS;

	cam_fill_smpio(&ccb->smpio,
		       /*retries*/ retry_count,
		       /*cbfcnp*/ NULL,
		       /*flags*/ flags,
		       /*smp_request*/ smp_request,
		       /*smp_request_len*/ request_size,
		       /*smp_response*/ smp_response,
		       /*smp_response_len*/ response_size,
		       /*timeout*/ timeout ? timeout : 5000);

	ccb->smpio.flags = SMP_FLAG_NONE;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
	}

	if (((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
	 && (response_size > 0)) {
		if (fd_response == 0) {
			buff_decode_visit(smp_response, response_size,
					  datastr, arg_put, NULL);
			fprintf(stdout, "\n");
		} else {
			ssize_t amt_written;
			int amt_to_write = response_size;
			u_int8_t *buf_ptr = smp_response;

			for (amt_written = 0; (amt_to_write > 0) &&
			     (amt_written = write(STDOUT_FILENO, buf_ptr,
						  amt_to_write)) > 0;){
				amt_to_write -= amt_written;
				buf_ptr += amt_written;
			}
			if (amt_written == -1) {
				warn("error writing data to stdout");
				error = 1;
				goto smpcmd_bailout;
			} else if ((amt_written == 0)
				&& (amt_to_write > 0)) {
				warnx("only wrote %u bytes out of %u",
				      response_size - amt_to_write, 
				      response_size);
			}
		}
	}
smpcmd_bailout:
	if (ccb != NULL)
		cam_freeccb(ccb);

	if (smp_request != NULL)
		free(smp_request);

	if (smp_response != NULL)
		free(smp_response);

	return (error);
}

static int
smpreportgeneral(struct cam_device *device, int argc, char **argv,
		 char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	struct smp_report_general_request *request = NULL;
	struct smp_report_general_response *response = NULL;
	struct sbuf *sb = NULL;
	int error = 0;
	int c, long_response = 0;
	int retval;

	/*
	 * Note that at the moment we don't support sending SMP CCBs to
	 * devices that aren't probed by CAM.
	 */
	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'l':
			long_response = 1;
			break;
		default:
			break;
		}
	}
	request = malloc(sizeof(*request));
	if (request == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*request));
		error = 1;
		goto bailout;
	}

	response = malloc(sizeof(*response));
	if (response == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*response));
		error = 1;
		goto bailout;
	}

try_long:
	smp_report_general(&ccb->smpio,
			   retry_count,
			   /*cbfcnp*/ NULL,
			   request,
			   /*request_len*/ sizeof(*request),
			   (uint8_t *)response,
			   /*response_len*/ sizeof(*response),
			   /*long_response*/ long_response,
			   timeout);

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		error = 1;
		goto bailout;
	}

	/*
	 * If the device supports the long response bit, try again and see
	 * if we can get all of the data.
	 */
	if ((response->long_response & SMP_RG_LONG_RESPONSE)
	 && (long_response == 0)) {
		ccb->ccb_h.status = CAM_REQ_INPROG;
		bzero(&(&ccb->ccb_h)[1],
		      sizeof(union ccb) - sizeof(struct ccb_hdr));
		long_response = 1;
		goto try_long;
	}

	/*
	 * XXX KDM detect and decode SMP errors here.
	 */
	sb = sbuf_new_auto();
	if (sb == NULL) {
		warnx("%s: error allocating sbuf", __func__);
		goto bailout;
	}

	smp_report_general_sbuf(response, sizeof(*response), sb);

	sbuf_finish(sb);

	printf("%s", sbuf_data(sb));

bailout:
	if (ccb != NULL)
		cam_freeccb(ccb);

	if (request != NULL)
		free(request);

	if (response != NULL)
		free(response);

	if (sb != NULL)
		sbuf_delete(sb);

	return (error);
}

struct camcontrol_opts phy_ops[] = {
	{"nop", SMP_PC_PHY_OP_NOP, CAM_ARG_NONE, NULL},
	{"linkreset", SMP_PC_PHY_OP_LINK_RESET, CAM_ARG_NONE, NULL},
	{"hardreset", SMP_PC_PHY_OP_HARD_RESET, CAM_ARG_NONE, NULL},
	{"disable", SMP_PC_PHY_OP_DISABLE, CAM_ARG_NONE, NULL},
	{"clearerrlog", SMP_PC_PHY_OP_CLEAR_ERR_LOG, CAM_ARG_NONE, NULL},
	{"clearaffiliation", SMP_PC_PHY_OP_CLEAR_AFFILIATON, CAM_ARG_NONE,NULL},
	{"sataportsel", SMP_PC_PHY_OP_TRANS_SATA_PSS, CAM_ARG_NONE, NULL},
	{"clearitnl", SMP_PC_PHY_OP_CLEAR_STP_ITN_LS, CAM_ARG_NONE, NULL},
	{"setdevname", SMP_PC_PHY_OP_SET_ATT_DEV_NAME, CAM_ARG_NONE, NULL},
	{NULL, 0, 0, NULL}
};

static int
smpphycontrol(struct cam_device *device, int argc, char **argv,
	      char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	struct smp_phy_control_request *request = NULL;
	struct smp_phy_control_response *response = NULL;
	int long_response = 0;
	int retval = 0;
	int phy = -1;
	uint32_t phy_operation = SMP_PC_PHY_OP_NOP;
	int phy_op_set = 0;
	uint64_t attached_dev_name = 0;
	int dev_name_set = 0;
	uint32_t min_plr = 0, max_plr = 0;
	uint32_t pp_timeout_val = 0;
	int slumber_partial = 0;
	int set_pp_timeout_val = 0;
	int c;

	/*
	 * Note that at the moment we don't support sending SMP CCBs to
	 * devices that aren't probed by CAM.
	 */
	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'a':
		case 'A':
		case 's':
		case 'S': {
			int enable = -1;

			if (strcasecmp(optarg, "enable") == 0)
				enable = 1;
			else if (strcasecmp(optarg, "disable") == 0)
				enable = 2;
			else {
				warnx("%s: Invalid argument %s", __func__,
				      optarg);
				retval = 1;
				goto bailout;
			}
			switch (c) {
			case 's':
				slumber_partial |= enable <<
						   SMP_PC_SAS_SLUMBER_SHIFT;
				break;
			case 'S':
				slumber_partial |= enable <<
						   SMP_PC_SAS_PARTIAL_SHIFT;
				break;
			case 'a':
				slumber_partial |= enable <<
						   SMP_PC_SATA_SLUMBER_SHIFT;
				break;
			case 'A':
				slumber_partial |= enable <<
						   SMP_PC_SATA_PARTIAL_SHIFT;
				break;
			default:
				warnx("%s: programmer error", __func__);
				retval = 1;
				goto bailout;
				break; /*NOTREACHED*/
			}
			break;
		}
		case 'd':
			attached_dev_name = (uintmax_t)strtoumax(optarg,
								 NULL,0);
			dev_name_set = 1;
			break;
		case 'l':
			long_response = 1;
			break;
		case 'm':
			/*
			 * We don't do extensive checking here, so this
			 * will continue to work when new speeds come out.
			 */
			min_plr = strtoul(optarg, NULL, 0);
			if ((min_plr == 0)
			 || (min_plr > 0xf)) {
				warnx("%s: invalid link rate %x",
				      __func__, min_plr);
				retval = 1;
				goto bailout;
			}
			break;
		case 'M':
			/*
			 * We don't do extensive checking here, so this
			 * will continue to work when new speeds come out.
			 */
			max_plr = strtoul(optarg, NULL, 0);
			if ((max_plr == 0)
			 || (max_plr > 0xf)) {
				warnx("%s: invalid link rate %x",
				      __func__, max_plr);
				retval = 1;
				goto bailout;
			}
			break;
		case 'o': {
			camcontrol_optret optreturn;
			cam_argmask argnums;
			const char *subopt;

			if (phy_op_set != 0) {
				warnx("%s: only one phy operation argument "
				      "(-o) allowed", __func__);
				retval = 1;
				goto bailout;
			}

			phy_op_set = 1;

			/*
			 * Allow the user to specify the phy operation
			 * numerically, as well as with a name.  This will
			 * future-proof it a bit, so options that are added
			 * in future specs can be used.
			 */
			if (isdigit(optarg[0])) {
				phy_operation = strtoul(optarg, NULL, 0);
				if ((phy_operation == 0)
				 || (phy_operation > 0xff)) {
					warnx("%s: invalid phy operation %#x",
					      __func__, phy_operation);
					retval = 1;
					goto bailout;
				}
				break;
			}
			optreturn = getoption(phy_ops, optarg, &phy_operation,
					      &argnums, &subopt);

			if (optreturn == CC_OR_AMBIGUOUS) {
				warnx("%s: ambiguous option %s", __func__,
				      optarg);
				usage(0);
				retval = 1;
				goto bailout;
			} else if (optreturn == CC_OR_NOT_FOUND) {
				warnx("%s: option %s not found", __func__,
				      optarg);
				usage(0);
				retval = 1;
				goto bailout;
			}
			break;
		}
		case 'p':
			phy = atoi(optarg);
			break;
		case 'T':
			pp_timeout_val = strtoul(optarg, NULL, 0);
			if (pp_timeout_val > 15) {
				warnx("%s: invalid partial pathway timeout "
				      "value %u, need a value less than 16",
				      __func__, pp_timeout_val);
				retval = 1;
				goto bailout;
			}
			set_pp_timeout_val = 1;
			break;
		default:
			break;
		}
	}

	if (phy == -1) {
		warnx("%s: a PHY (-p phy) argument is required",__func__);
		retval = 1;
		goto bailout;
	}

	if (((dev_name_set != 0)
	  && (phy_operation != SMP_PC_PHY_OP_SET_ATT_DEV_NAME))
	 || ((phy_operation == SMP_PC_PHY_OP_SET_ATT_DEV_NAME)
	  && (dev_name_set == 0))) {
		warnx("%s: -d name and -o setdevname arguments both "
		      "required to set device name", __func__);
		retval = 1;
		goto bailout;
	}

	request = malloc(sizeof(*request));
	if (request == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*request));
		retval = 1;
		goto bailout;
	}

	response = malloc(sizeof(*response));
	if (response == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*request));
		retval = 1;
		goto bailout;
	}

	smp_phy_control(&ccb->smpio,
			retry_count,
			/*cbfcnp*/ NULL,
			request,
			sizeof(*request),
			(uint8_t *)response,
			sizeof(*response),
			long_response,
			/*expected_exp_change_count*/ 0,
			phy,
			phy_operation,
			(set_pp_timeout_val != 0) ? 1 : 0,
			attached_dev_name,
			min_plr,
			max_plr,
			slumber_partial,
			pp_timeout_val,
			timeout);

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			/*
			 * Use CAM_EPF_NORMAL so we only get one line of
			 * SMP command decoding.
			 */
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_NORMAL, stderr);
		}
		retval = 1;
		goto bailout;
	}

	/* XXX KDM print out something here for success? */
bailout:
	if (ccb != NULL)
		cam_freeccb(ccb);

	if (request != NULL)
		free(request);

	if (response != NULL)
		free(response);

	return (retval);
}

static int
smpmaninfo(struct cam_device *device, int argc, char **argv,
	   char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	struct smp_report_manuf_info_request request;
	struct smp_report_manuf_info_response response;
	struct sbuf *sb = NULL;
	int long_response = 0;
	int retval = 0;
	int c;

	/*
	 * Note that at the moment we don't support sending SMP CCBs to
	 * devices that aren't probed by CAM.
	 */
	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'l':
			long_response = 1;
			break;
		default:
			break;
		}
	}
	bzero(&request, sizeof(request));
	bzero(&response, sizeof(response));

	smp_report_manuf_info(&ccb->smpio,
			      retry_count,
			      /*cbfcnp*/ NULL,
			      &request,
			      sizeof(request),
			      (uint8_t *)&response,
			      sizeof(response),
			      long_response,
			      timeout);

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		retval = 1;
		goto bailout;
	}

	sb = sbuf_new_auto();
	if (sb == NULL) {
		warnx("%s: error allocating sbuf", __func__);
		goto bailout;
	}

	smp_report_manuf_info_sbuf(&response, sizeof(response), sb);

	sbuf_finish(sb);

	printf("%s", sbuf_data(sb));

bailout:

	if (ccb != NULL)
		cam_freeccb(ccb);

	if (sb != NULL)
		sbuf_delete(sb);

	return (retval);
}

static int
getdevid(struct cam_devitem *item)
{
	int retval = 0;
	union ccb *ccb = NULL;

	struct cam_device *dev;

	dev = cam_open_btl(item->dev_match.path_id,
			   item->dev_match.target_id,
			   item->dev_match.target_lun, O_RDWR, NULL);

	if (dev == NULL) {
		warnx("%s", cam_errbuf);
		retval = 1;
		goto bailout;
	}

	item->device_id_len = 0;

	ccb = cam_getccb(dev);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		retval = 1;
		goto bailout;
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	/*
	 * On the first try, we just probe for the size of the data, and
	 * then allocate that much memory and try again.
	 */
retry:
	ccb->ccb_h.func_code = XPT_GDEV_ADVINFO;
	ccb->ccb_h.flags = CAM_DIR_IN;
	ccb->cgdai.flags = CGDAI_FLAG_PROTO;
	ccb->cgdai.buftype = CGDAI_TYPE_SCSI_DEVID;
	ccb->cgdai.bufsiz = item->device_id_len;
	if (item->device_id_len != 0)
		ccb->cgdai.buf = (uint8_t *)item->device_id;

	if (cam_send_ccb(dev, ccb) < 0) {
		warn("%s: error sending XPT_GDEV_ADVINFO CCB", __func__);
		retval = 1;
		goto bailout;
	}

	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		warnx("%s: CAM status %#x", __func__, ccb->ccb_h.status);
		retval = 1;
		goto bailout;
	}

	if (item->device_id_len == 0) {
		/*
		 * This is our first time through.  Allocate the buffer,
		 * and then go back to get the data.
		 */
		if (ccb->cgdai.provsiz == 0) {
			warnx("%s: invalid .provsiz field returned with "
			     "XPT_GDEV_ADVINFO CCB", __func__);
			retval = 1;
			goto bailout;
		}
		item->device_id_len = ccb->cgdai.provsiz;
		item->device_id = malloc(item->device_id_len);
		if (item->device_id == NULL) {
			warn("%s: unable to allocate %d bytes", __func__,
			     item->device_id_len);
			retval = 1;
			goto bailout;
		}
		ccb->ccb_h.status = CAM_REQ_INPROG;
		goto retry;
	}

bailout:
	if (dev != NULL)
		cam_close_device(dev);

	if (ccb != NULL)
		cam_freeccb(ccb);

	return (retval);
}

/*
 * XXX KDM merge this code with getdevtree()?
 */
static int
buildbusdevlist(struct cam_devlist *devlist)
{
	union ccb ccb;
	int bufsize, fd = -1;
	struct dev_match_pattern *patterns;
	struct cam_devitem *item = NULL;
	int skip_device = 0;
	int retval = 0;

	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		warn("couldn't open %s", XPT_DEVICE);
		return(1);
	}

	bzero(&ccb, sizeof(union ccb));

	ccb.ccb_h.path_id = CAM_XPT_PATH_ID;
	ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
	ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

	ccb.ccb_h.func_code = XPT_DEV_MATCH;
	bufsize = sizeof(struct dev_match_result) * 100;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
	if (ccb.cdm.matches == NULL) {
		warnx("can't malloc memory for matches");
		close(fd);
		return(1);
	}
	ccb.cdm.num_matches = 0;
	ccb.cdm.num_patterns = 2;
	ccb.cdm.pattern_buf_len = sizeof(struct dev_match_pattern) *
		ccb.cdm.num_patterns;

	patterns = (struct dev_match_pattern *)malloc(ccb.cdm.pattern_buf_len);
	if (patterns == NULL) {
		warnx("can't malloc memory for patterns");
		retval = 1;
		goto bailout;
	}

	ccb.cdm.patterns = patterns;
	bzero(patterns, ccb.cdm.pattern_buf_len);

	patterns[0].type = DEV_MATCH_DEVICE;
	patterns[0].pattern.device_pattern.flags = DEV_MATCH_PATH;
	patterns[0].pattern.device_pattern.path_id = devlist->path_id;
	patterns[1].type = DEV_MATCH_PERIPH;
	patterns[1].pattern.periph_pattern.flags = PERIPH_MATCH_PATH;
	patterns[1].pattern.periph_pattern.path_id = devlist->path_id;

	/*
	 * We do the ioctl multiple times if necessary, in case there are
	 * more than 100 nodes in the EDT.
	 */
	do {
		unsigned int i;

		if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
			warn("error sending CAMIOCOMMAND ioctl");
			retval = 1;
			goto bailout;
		}

		if ((ccb.ccb_h.status != CAM_REQ_CMP)
		 || ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
		    && (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
			warnx("got CAM error %#x, CDM error %d\n",
			      ccb.ccb_h.status, ccb.cdm.status);
			retval = 1;
			goto bailout;
		}

		for (i = 0; i < ccb.cdm.num_matches; i++) {
			switch (ccb.cdm.matches[i].type) {
			case DEV_MATCH_DEVICE: {
				struct device_match_result *dev_result;

				dev_result = 
				     &ccb.cdm.matches[i].result.device_result;

				if (dev_result->flags &
				    DEV_RESULT_UNCONFIGURED) {
					skip_device = 1;
					break;
				} else
					skip_device = 0;

				item = malloc(sizeof(*item));
				if (item == NULL) {
					warn("%s: unable to allocate %zd bytes",
					     __func__, sizeof(*item));
					retval = 1;
					goto bailout;
				}
				bzero(item, sizeof(*item));
				bcopy(dev_result, &item->dev_match,
				      sizeof(*dev_result));
				STAILQ_INSERT_TAIL(&devlist->dev_queue, item,
						   links);

				if (getdevid(item) != 0) {
					retval = 1;
					goto bailout;
				}
				break;
			}
			case DEV_MATCH_PERIPH: {
				struct periph_match_result *periph_result;

				periph_result =
				      &ccb.cdm.matches[i].result.periph_result;

				if (skip_device != 0)
					break;
				item->num_periphs++;
				item->periph_matches = realloc(
					item->periph_matches,
					item->num_periphs *
					sizeof(struct periph_match_result));
				if (item->periph_matches == NULL) {
					warn("%s: error allocating periph "
					     "list", __func__);
					retval = 1;
					goto bailout;
				}
				bcopy(periph_result, &item->periph_matches[
				      item->num_periphs - 1],
				      sizeof(*periph_result));
				break;
			}
			default:
				fprintf(stderr, "%s: unexpected match "
					"type %d\n", __func__,
					ccb.cdm.matches[i].type);
				retval = 1;
				goto bailout;
				break; /*NOTREACHED*/
			}
		}
	} while ((ccb.ccb_h.status == CAM_REQ_CMP)
		&& (ccb.cdm.status == CAM_DEV_MATCH_MORE));
bailout:

	if (fd != -1)
		close(fd);

	free(patterns);

	free(ccb.cdm.matches);

	if (retval != 0)
		freebusdevlist(devlist);

	return (retval);
}

static void
freebusdevlist(struct cam_devlist *devlist)
{
	struct cam_devitem *item, *item2;

	STAILQ_FOREACH_SAFE(item, &devlist->dev_queue, links, item2) {
		STAILQ_REMOVE(&devlist->dev_queue, item, cam_devitem,
			      links);
		free(item->device_id);
		free(item->periph_matches);
		free(item);
	}
}

static struct cam_devitem *
findsasdevice(struct cam_devlist *devlist, uint64_t sasaddr)
{
	struct cam_devitem *item;

	STAILQ_FOREACH(item, &devlist->dev_queue, links) {
		uint8_t *item_addr;

		/*
		 * XXX KDM look for LUN IDs as well?
		 */
		item_addr = scsi_get_sas_addr(item->device_id,
					      item->device_id_len);
		if (item_addr == NULL)
			continue;

		if (scsi_8btou64(item_addr) == sasaddr)
			return (item);
	}

	return (NULL);
}

static int
smpphylist(struct cam_device *device, int argc, char **argv,
	   char *combinedopt, int retry_count, int timeout)
{
	struct smp_report_general_request *rgrequest = NULL;
	struct smp_report_general_response *rgresponse = NULL;
	struct smp_discover_request *disrequest = NULL;
	struct smp_discover_response *disresponse = NULL;
	struct cam_devlist devlist;
	union ccb *ccb;
	int long_response = 0;
	int num_phys = 0;
	int quiet = 0;
	int retval;
	int i, c;

	/*
	 * Note that at the moment we don't support sending SMP CCBs to
	 * devices that aren't probed by CAM.
	 */
	ccb = cam_getccb(device);
	if (ccb == NULL) {
		warnx("%s: error allocating CCB", __func__);
		return (1);
	}

	bzero(&(&ccb->ccb_h)[1],
	      sizeof(union ccb) - sizeof(struct ccb_hdr));

	rgrequest = malloc(sizeof(*rgrequest));
	if (rgrequest == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*rgrequest));
		retval = 1;
		goto bailout;
	}

	rgresponse = malloc(sizeof(*rgresponse));
	if (rgresponse == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*rgresponse));
		retval = 1;
		goto bailout;
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 'l':
			long_response = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			break;
		}
	}

	smp_report_general(&ccb->smpio,
			   retry_count,
			   /*cbfcnp*/ NULL,
			   rgrequest,
			   /*request_len*/ sizeof(*rgrequest),
			   (uint8_t *)rgresponse,
			   /*response_len*/ sizeof(*rgresponse),
			   /*long_response*/ long_response,
			   timeout);

	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (((retval = cam_send_ccb(device, ccb)) < 0)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)) {
		const char *warnstr = "error sending command";

		if (retval < 0)
			warn(warnstr);
		else
			warnx(warnstr);

		if (arglist & CAM_ARG_VERBOSE) {
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);
		}
		retval = 1;
		goto bailout;
	}

	num_phys = rgresponse->num_phys;

	if (num_phys == 0) {
		if (quiet == 0)
			fprintf(stdout, "%s: No Phys reported\n", __func__);
		retval = 1;
		goto bailout;
	}

	STAILQ_INIT(&devlist.dev_queue);
	devlist.path_id = device->path_id;

	retval = buildbusdevlist(&devlist);
	if (retval != 0)
		goto bailout;

	if (quiet == 0) {
		fprintf(stdout, "%d PHYs:\n", num_phys);
		fprintf(stdout, "PHY  Attached SAS Address\n");
	}

	disrequest = malloc(sizeof(*disrequest));
	if (disrequest == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*disrequest));
		retval = 1;
		goto bailout;
	}

	disresponse = malloc(sizeof(*disresponse));
	if (disresponse == NULL) {
		warn("%s: unable to allocate %zd bytes", __func__,
		     sizeof(*disresponse));
		retval = 1;
		goto bailout;
	}

	for (i = 0; i < num_phys; i++) {
		struct cam_devitem *item;
		struct device_match_result *dev_match;
		char vendor[16], product[48], revision[16];
		char tmpstr[256];
		int j;

		bzero(&(&ccb->ccb_h)[1],
		      sizeof(union ccb) - sizeof(struct ccb_hdr));

		ccb->ccb_h.status = CAM_REQ_INPROG;
		ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

		smp_discover(&ccb->smpio,
			     retry_count,
			     /*cbfcnp*/ NULL,
			     disrequest,
			     sizeof(*disrequest),
			     (uint8_t *)disresponse,
			     sizeof(*disresponse),
			     long_response,
			     /*ignore_zone_group*/ 0,
			     /*phy*/ i,
			     timeout);

		if (((retval = cam_send_ccb(device, ccb)) < 0)
		 || (((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
		  && (disresponse->function_result != SMP_FR_PHY_VACANT))) {
			const char *warnstr = "error sending command";

			if (retval < 0)
				warn(warnstr);
			else
				warnx(warnstr);

			if (arglist & CAM_ARG_VERBOSE) {
				cam_error_print(device, ccb, CAM_ESF_ALL,
						CAM_EPF_ALL, stderr);
			}
			retval = 1;
			goto bailout;
		}

		if (disresponse->function_result == SMP_FR_PHY_VACANT) {
			if (quiet == 0)
				fprintf(stdout, "%3d  <vacant>\n", i);
			continue;
		}

		item = findsasdevice(&devlist,
			scsi_8btou64(disresponse->attached_sas_address));

		if ((quiet == 0)
		 || (item != NULL)) {
			fprintf(stdout, "%3d  0x%016jx", i,
				(uintmax_t)scsi_8btou64(
				disresponse->attached_sas_address));
			if (item == NULL) {
				fprintf(stdout, "\n");
				continue;
			}
		} else if (quiet != 0)
			continue;

		dev_match = &item->dev_match;

		if (dev_match->protocol == PROTO_SCSI) {
			cam_strvis(vendor, dev_match->inq_data.vendor,
				   sizeof(dev_match->inq_data.vendor),
				   sizeof(vendor));
			cam_strvis(product, dev_match->inq_data.product,
				   sizeof(dev_match->inq_data.product),
				   sizeof(product));
			cam_strvis(revision, dev_match->inq_data.revision,
				   sizeof(dev_match->inq_data.revision),
				   sizeof(revision));
			sprintf(tmpstr, "<%s %s %s>", vendor, product,
				revision);
		} else if ((dev_match->protocol == PROTO_ATA)
			|| (dev_match->protocol == PROTO_SATAPM)) {
			cam_strvis(product, dev_match->ident_data.model,
				   sizeof(dev_match->ident_data.model),
				   sizeof(product));
			cam_strvis(revision, dev_match->ident_data.revision,
				   sizeof(dev_match->ident_data.revision),
				   sizeof(revision));
			sprintf(tmpstr, "<%s %s>", product, revision);
		} else {
			sprintf(tmpstr, "<>");
		}
		fprintf(stdout, "   %-33s ", tmpstr);

		/*
		 * If we have 0 periphs, that's a bug...
		 */
		if (item->num_periphs == 0) {
			fprintf(stdout, "\n");
			continue;
		}

		fprintf(stdout, "(");
		for (j = 0; j < item->num_periphs; j++) {
			if (j > 0)
				fprintf(stdout, ",");

			fprintf(stdout, "%s%d",
				item->periph_matches[j].periph_name,
				item->periph_matches[j].unit_number);
				
		}
		fprintf(stdout, ")\n");
	}
bailout:
	if (ccb != NULL)
		cam_freeccb(ccb);

	free(rgrequest);

	free(rgresponse);

	free(disrequest);

	free(disresponse);

	freebusdevlist(&devlist);

	return (retval);
}

static int
atapm(struct cam_device *device, int argc, char **argv,
		 char *combinedopt, int retry_count, int timeout)
{
	union ccb *ccb;
	int retval = 0;
	int t = -1;
	int c;
	u_char cmd, sc;

	ccb = cam_getccb(device);

	if (ccb == NULL) {
		warnx("%s: error allocating ccb", __func__);
		return (1);
	}

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 't':
			t = atoi(optarg);
			break;
		default:
			break;
		}
	}
	if (strcmp(argv[1], "idle") == 0) {
		if (t == -1)
			cmd = ATA_IDLE_IMMEDIATE;
		else
			cmd = ATA_IDLE_CMD;
	} else if (strcmp(argv[1], "standby") == 0) {
		if (t == -1)
			cmd = ATA_STANDBY_IMMEDIATE;
		else
			cmd = ATA_STANDBY_CMD;
	} else {
		cmd = ATA_SLEEP;
		t = -1;
	}

	if (t < 0)
		sc = 0;
	else if (t <= (240 * 5))
		sc = (t + 4) / 5;
	else if (t <= (252 * 5))
		/* special encoding for 21 minutes */
		sc = 252;
	else if (t <= (11 * 30 * 60))
		sc = (t - 1) / (30 * 60) + 241;
	else
		sc = 253;

	cam_fill_ataio(&ccb->ataio,
		      retry_count,
		      NULL,
		      /*flags*/CAM_DIR_NONE,
		      MSG_SIMPLE_Q_TAG,
		      /*data_ptr*/NULL,
		      /*dxfer_len*/0,
		      timeout ? timeout : 30 * 1000);
	ata_28bit_cmd(&ccb->ataio, cmd, 0, 0, sc);

	/* Disable freezing the device queue */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (arglist & CAM_ARG_ERR_RECOVER)
		ccb->ccb_h.flags |= CAM_PASS_ERR_RECOVER;

	if (cam_send_ccb(device, ccb) < 0) {
		warn("error sending command");

		if (arglist & CAM_ARG_VERBOSE)
			cam_error_print(device, ccb, CAM_ESF_ALL,
					CAM_EPF_ALL, stderr);

		retval = 1;
		goto bailout;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		cam_error_print(device, ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
		retval = 1;
		goto bailout;
	}
bailout:
	cam_freeccb(ccb);
	return (retval);
}

#endif /* MINIMALISTIC */

void
usage(int verbose)
{
	fprintf(verbose ? stdout : stderr,
"usage:  camcontrol <command>  [device id][generic args][command args]\n"
"        camcontrol devlist    [-v]\n"
#ifndef MINIMALISTIC
"        camcontrol periphlist [dev_id][-n dev_name] [-u unit]\n"
"        camcontrol tur        [dev_id][generic args]\n"
"        camcontrol inquiry    [dev_id][generic args] [-D] [-S] [-R]\n"
"        camcontrol identify   [dev_id][generic args] [-v]\n"
"        camcontrol reportluns [dev_id][generic args] [-c] [-l] [-r report]\n"
"        camcontrol readcap    [dev_id][generic args] [-b] [-h] [-H] [-N]\n"
"                              [-q] [-s]\n"
"        camcontrol start      [dev_id][generic args]\n"
"        camcontrol stop       [dev_id][generic args]\n"
"        camcontrol load       [dev_id][generic args]\n"
"        camcontrol eject      [dev_id][generic args]\n"
#endif /* MINIMALISTIC */
"        camcontrol rescan     <all | bus[:target:lun]>\n"
"        camcontrol reset      <all | bus[:target:lun]>\n"
#ifndef MINIMALISTIC
"        camcontrol defects    [dev_id][generic args] <-f format> [-P][-G]\n"
"        camcontrol modepage   [dev_id][generic args] <-m page | -l>\n"
"                              [-P pagectl][-e | -b][-d]\n"
"        camcontrol cmd        [dev_id][generic args]\n"
"                              <-a cmd [args] | -c cmd [args]>\n"
"                              [-d] [-f] [-i len fmt|-o len fmt [args]] [-r fmt]\n"
"        camcontrol smpcmd     [dev_id][generic args]\n"
"                              <-r len fmt [args]> <-R len fmt [args]>\n"
"        camcontrol smprg      [dev_id][generic args][-l]\n"
"        camcontrol smppc      [dev_id][generic args] <-p phy> [-l]\n"
"                              [-o operation][-d name][-m rate][-M rate]\n"
"                              [-T pp_timeout][-a enable|disable]\n"
"                              [-A enable|disable][-s enable|disable]\n"
"                              [-S enable|disable]\n"
"        camcontrol smpphylist [dev_id][generic args][-l][-q]\n"
"        camcontrol smpmaninfo [dev_id][generic args][-l]\n"
"        camcontrol debug      [-I][-P][-T][-S][-X][-c]\n"
"                              <all|bus[:target[:lun]]|off>\n"
"        camcontrol tags       [dev_id][generic args] [-N tags] [-q] [-v]\n"
"        camcontrol negotiate  [dev_id][generic args] [-a][-c]\n"
"                              [-D <enable|disable>][-M mode][-O offset]\n"
"                              [-q][-R syncrate][-v][-T <enable|disable>]\n"
"                              [-U][-W bus_width]\n"
"        camcontrol format     [dev_id][generic args][-q][-r][-w][-y]\n"
"        camcontrol idle       [dev_id][generic args][-t time]\n"
"        camcontrol standby    [dev_id][generic args][-t time]\n"
"        camcontrol sleep      [dev_id][generic args]\n"
#endif /* MINIMALISTIC */
"        camcontrol help\n");
	if (!verbose)
		return;
#ifndef MINIMALISTIC
	fprintf(stdout,
"Specify one of the following options:\n"
"devlist     list all CAM devices\n"
"periphlist  list all CAM peripheral drivers attached to a device\n"
"tur         send a test unit ready to the named device\n"
"inquiry     send a SCSI inquiry command to the named device\n"
"identify    send a ATA identify command to the named device\n"
"reportluns  send a SCSI report luns command to the device\n"
"readcap     send a SCSI read capacity command to the device\n"
"start       send a Start Unit command to the device\n"
"stop        send a Stop Unit command to the device\n"
"load        send a Start Unit command to the device with the load bit set\n"
"eject       send a Stop Unit command to the device with the eject bit set\n"
"rescan      rescan all busses, the given bus, or bus:target:lun\n"
"reset       reset all busses, the given bus, or bus:target:lun\n"
"defects     read the defect list of the specified device\n"
"modepage    display or edit (-e) the given mode page\n"
"cmd         send the given SCSI command, may need -i or -o as well\n"
"smpcmd      send the given SMP command, requires -o and -i\n"
"smprg       send the SMP Report General command\n"
"smppc       send the SMP PHY Control command, requires -p\n"
"smpphylist  display phys attached to a SAS expander\n"
"smpmaninfo  send the SMP Report Manufacturer Info command\n"
"debug       turn debugging on/off for a bus, target, or lun, or all devices\n"
"tags        report or set the number of transaction slots for a device\n"
"negotiate   report or set device negotiation parameters\n"
"format      send the SCSI FORMAT UNIT command to the named device\n"
"idle        send the ATA IDLE command to the named device\n"
"standby     send the ATA STANDBY command to the named device\n"
"sleep       send the ATA SLEEP command to the named device\n"
"help        this message\n"
"Device Identifiers:\n"
"bus:target        specify the bus and target, lun defaults to 0\n"
"bus:target:lun    specify the bus, target and lun\n"
"deviceUNIT        specify the device name, like \"da4\" or \"cd2\"\n"
"Generic arguments:\n"
"-v                be verbose, print out sense information\n"
"-t timeout        command timeout in seconds, overrides default timeout\n"
"-n dev_name       specify device name, e.g. \"da\", \"cd\"\n"
"-u unit           specify unit number, e.g. \"0\", \"5\"\n"
"-E                have the kernel attempt to perform SCSI error recovery\n"
"-C count          specify the SCSI command retry count (needs -E to work)\n"
"modepage arguments:\n"
"-l                list all available mode pages\n"
"-m page           specify the mode page to view or edit\n"
"-e                edit the specified mode page\n"
"-b                force view to binary mode\n"
"-d                disable block descriptors for mode sense\n"
"-P pgctl          page control field 0-3\n"
"defects arguments:\n"
"-f format         specify defect list format (block, bfi or phys)\n"
"-G                get the grown defect list\n"
"-P                get the permanant defect list\n"
"inquiry arguments:\n"
"-D                get the standard inquiry data\n"
"-S                get the serial number\n"
"-R                get the transfer rate, etc.\n"
"reportluns arguments:\n"
"-c                only report a count of available LUNs\n"
"-l                only print out luns, and not a count\n"
"-r <reporttype>   specify \"default\", \"wellknown\" or \"all\"\n"
"readcap arguments\n"
"-b                only report the blocksize\n"
"-h                human readable device size, base 2\n"
"-H                human readable device size, base 10\n"
"-N                print the number of blocks instead of last block\n"
"-q                quiet, print numbers only\n"
"-s                only report the last block/device size\n"
"cmd arguments:\n"
"-c cdb [args]     specify the SCSI CDB\n"
"-i len fmt        specify input data and input data format\n"
"-o len fmt [args] specify output data and output data fmt\n"
"smpcmd arguments:\n"
"-r len fmt [args] specify the SMP command to be sent\n"
"-R len fmt [args] specify SMP response format\n"
"smprg arguments:\n"
"-l                specify the long response format\n"
"smppc arguments:\n"
"-p phy            specify the PHY to operate on\n"
"-l                specify the long request/response format\n"
"-o operation      specify the phy control operation\n"
"-d name           set the attached device name\n"
"-m rate           set the minimum physical link rate\n"
"-M rate           set the maximum physical link rate\n"
"-T pp_timeout     set the partial pathway timeout value\n"
"-a enable|disable enable or disable SATA slumber\n"
"-A enable|disable enable or disable SATA partial phy power\n"
"-s enable|disable enable or disable SAS slumber\n"
"-S enable|disable enable or disable SAS partial phy power\n"
"smpphylist arguments:\n"
"-l                specify the long response format\n"
"-q                only print phys with attached devices\n"
"smpmaninfo arguments:\n"
"-l                specify the long response format\n"
"debug arguments:\n"
"-I                CAM_DEBUG_INFO -- scsi commands, errors, data\n"
"-T                CAM_DEBUG_TRACE -- routine flow tracking\n"
"-S                CAM_DEBUG_SUBTRACE -- internal routine command flow\n"
"-c                CAM_DEBUG_CDB -- print out SCSI CDBs only\n"
"tags arguments:\n"
"-N tags           specify the number of tags to use for this device\n"
"-q                be quiet, don't report the number of tags\n"
"-v                report a number of tag-related parameters\n"
"negotiate arguments:\n"
"-a                send a test unit ready after negotiation\n"
"-c                report/set current negotiation settings\n"
"-D <arg>          \"enable\" or \"disable\" disconnection\n"
"-M mode           set ATA mode\n"
"-O offset         set command delay offset\n"
"-q                be quiet, don't report anything\n"
"-R syncrate       synchronization rate in MHz\n"
"-T <arg>          \"enable\" or \"disable\" tagged queueing\n"
"-U                report/set user negotiation settings\n"
"-W bus_width      set the bus width in bits (8, 16 or 32)\n"
"-v                also print a Path Inquiry CCB for the controller\n"
"format arguments:\n"
"-q                be quiet, don't print status messages\n"
"-r                run in report only mode\n"
"-w                don't send immediate format command\n"
"-y                don't ask any questions\n"
"idle/standby arguments:\n"
"-t <arg>          number of seconds before respective state.\n");
#endif /* MINIMALISTIC */
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
	const char *mainopt = "C:En:t:u:v";
	const char *subopt = NULL;
	char combinedopt[256];
	int error = 0, optstart = 2;
	int devopen = 1;
#ifndef MINIMALISTIC
	int bus, target, lun;
#endif /* MINIMALISTIC */

	cmdlist = CAM_CMD_NONE;
	arglist = CAM_ARG_NONE;

	if (argc < 2) {
		usage(0);
		exit(1);
	}

	/*
	 * Get the base option.
	 */
	optreturn = getoption(option_table,argv[1], &cmdlist, &arglist,&subopt);

	if (optreturn == CC_OR_AMBIGUOUS) {
		warnx("ambiguous option %s", argv[1]);
		usage(0);
		exit(1);
	} else if (optreturn == CC_OR_NOT_FOUND) {
		warnx("option %s not found", argv[1]);
		usage(0);
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
	 * For these options we do not parse optional device arguments and
	 * we do not open a passthrough device.
	 */
	if ((cmdlist == CAM_CMD_RESCAN)
	 || (cmdlist == CAM_CMD_RESET)
	 || (cmdlist == CAM_CMD_DEVTREE)
	 || (cmdlist == CAM_CMD_USAGE)
	 || (cmdlist == CAM_CMD_DEBUG))
		devopen = 0;

#ifndef MINIMALISTIC
	if ((devopen == 1)
	 && (argc > 2 && argv[2][0] != '-')) {
		char name[30];
		int rv;

		if (isdigit(argv[2][0])) {
			/* device specified as bus:target[:lun] */
			rv = parse_btl(argv[2], &bus, &target, &lun, &arglist);
			if (rv < 2)
				errx(1, "numeric device specification must "
				     "be either bus:target, or "
				     "bus:target:lun");
			/* default to 0 if lun was not specified */
			if ((arglist & CAM_ARG_LUN) == 0) {
				lun = 0;
				arglist |= CAM_ARG_LUN;
			}
			optstart++;
		} else {
			if (cam_get_device(argv[2], name, sizeof name, &unit)
			    == -1)
				errx(1, "%s", cam_errbuf);
			device = strdup(name);
			arglist |= CAM_ARG_DEVICE | CAM_ARG_UNIT;
			optstart++;
		}
	}
#endif /* MINIMALISTIC */
	/*
	 * Start getopt processing at argv[2/3], since we've already
	 * accepted argv[1..2] as the command name, and as a possible
	 * device name.
	 */
	optind = optstart;

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

#ifndef MINIMALISTIC
	/*
	 * For most commands we'll want to open the passthrough device
	 * associated with the specified device.  In the case of the rescan
	 * commands, we don't use a passthrough device at all, just the
	 * transport layer device.
	 */
	if (devopen == 1) {
		if (((arglist & (CAM_ARG_BUS|CAM_ARG_TARGET)) == 0)
		 && (((arglist & CAM_ARG_DEVICE) == 0)
		  || ((arglist & CAM_ARG_UNIT) == 0))) {
			errx(1, "subcommand \"%s\" requires a valid device "
			     "identifier", argv[1]);
		}

		if ((cam_dev = ((arglist & (CAM_ARG_BUS | CAM_ARG_TARGET))?
				cam_open_btl(bus, target, lun, O_RDWR, NULL) :
				cam_open_spec_device(device,unit,O_RDWR,NULL)))
		     == NULL)
			errx(1,"%s", cam_errbuf);
	}
#endif /* MINIMALISTIC */

	/*
	 * Reset optind to 2, and reset getopt, so these routines can parse
	 * the arguments again.
	 */
	optind = optstart;
	optreset = 1;

	switch(cmdlist) {
#ifndef MINIMALISTIC
		case CAM_CMD_DEVLIST:
			error = getdevlist(cam_dev);
			break;
#endif /* MINIMALISTIC */
		case CAM_CMD_DEVTREE:
			error = getdevtree();
			break;
#ifndef MINIMALISTIC
		case CAM_CMD_TUR:
			error = testunitready(cam_dev, retry_count, timeout, 0);
			break;
		case CAM_CMD_INQUIRY:
			error = scsidoinquiry(cam_dev, argc, argv, combinedopt,
					      retry_count, timeout);
			break;
		case CAM_CMD_IDENTIFY:
			error = ataidentify(cam_dev, retry_count, timeout);
			break;
		case CAM_CMD_STARTSTOP:
			error = scsistart(cam_dev, arglist & CAM_ARG_START_UNIT,
					  arglist & CAM_ARG_EJECT, retry_count,
					  timeout);
			break;
#endif /* MINIMALISTIC */
		case CAM_CMD_RESCAN:
			error = dorescan_or_reset(argc, argv, 1);
			break;
		case CAM_CMD_RESET:
			error = dorescan_or_reset(argc, argv, 0);
			break;
#ifndef MINIMALISTIC
		case CAM_CMD_READ_DEFECTS:
			error = readdefects(cam_dev, argc, argv, combinedopt,
					    retry_count, timeout);
			break;
		case CAM_CMD_MODE_PAGE:
			modepage(cam_dev, argc, argv, combinedopt,
				 retry_count, timeout);
			break;
		case CAM_CMD_SCSI_CMD:
			error = scsicmd(cam_dev, argc, argv, combinedopt,
					retry_count, timeout);
			break;
		case CAM_CMD_SMP_CMD:
			error = smpcmd(cam_dev, argc, argv, combinedopt,
				       retry_count, timeout);
			break;
		case CAM_CMD_SMP_RG:
			error = smpreportgeneral(cam_dev, argc, argv,
						 combinedopt, retry_count,
						 timeout);
			break;
		case CAM_CMD_SMP_PC:
			error = smpphycontrol(cam_dev, argc, argv, combinedopt, 
					      retry_count, timeout);
			break;
		case CAM_CMD_SMP_PHYLIST:
			error = smpphylist(cam_dev, argc, argv, combinedopt,
					   retry_count, timeout);
			break;
		case CAM_CMD_SMP_MANINFO:
			error = smpmaninfo(cam_dev, argc, argv, combinedopt,
					   retry_count, timeout);
			break;
		case CAM_CMD_DEBUG:
			error = camdebug(argc, argv, combinedopt);
			break;
		case CAM_CMD_TAG:
			error = tagcontrol(cam_dev, argc, argv, combinedopt);
			break;
		case CAM_CMD_RATE:
			error = ratecontrol(cam_dev, retry_count, timeout,
					    argc, argv, combinedopt);
			break;
		case CAM_CMD_FORMAT:
			error = scsiformat(cam_dev, argc, argv,
					   combinedopt, retry_count, timeout);
			break;
		case CAM_CMD_REPORTLUNS:
			error = scsireportluns(cam_dev, argc, argv,
					       combinedopt, retry_count,
					       timeout);
			break;
		case CAM_CMD_READCAP:
			error = scsireadcapacity(cam_dev, argc, argv,
						 combinedopt, retry_count,
						 timeout);
			break;
		case CAM_CMD_IDLE:
		case CAM_CMD_STANDBY:
		case CAM_CMD_SLEEP:
			error = atapm(cam_dev, argc, argv,
						 combinedopt, retry_count,
						 timeout);
			break;
#endif /* MINIMALISTIC */
		case CAM_CMD_USAGE:
			usage(1);
			break;
		default:
			usage(0);
			error = 1;
			break;
	}

	if (cam_dev != NULL)
		cam_close_device(cam_dev);

	exit(error);
}
