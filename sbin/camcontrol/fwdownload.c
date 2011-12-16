/*-
 * Copyright (c) 2011 Sandvine Incorporated. All rights reserved.
 * Copyright (c) 2002-2011 Andre Albsmeier <andre@albsmeier.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution. 
 *    
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT  
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This software is derived from Andre Albsmeier's fwprog.c which contained
 * the following note:
 *
 * Many thanks goes to Marc Frajola <marc@terasolutions.com> from
 * TeraSolutions for the initial idea and his programme for upgrading
 * the firmware of I*M DDYS drives.
 */

/*
 * BEWARE:
 *
 * The fact that you see your favorite vendor listed below does not
 * imply that your equipment won't break when you use this software
 * with it. It only means that the firmware of at least one device type
 * of each vendor listed has been programmed successfully using this code.
 *
 * The -s option simulates a download but does nothing apart from that.
 * It can be used to check what chunk sizes would have been used with the
 * specified device.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <camlib.h>

#include "camcontrol.h"

#define	CMD_TIMEOUT 50000	/* 50 seconds */

typedef enum {
	VENDOR_HITACHI,
	VENDOR_HP,
	VENDOR_IBM,
	VENDOR_PLEXTOR,
	VENDOR_QUANTUM,
	VENDOR_SEAGATE,
	VENDOR_UNKNOWN
} fw_vendor_t;

struct fw_vendor {
	fw_vendor_t type;
	const char *pattern;
	int max_pkt_size;
	u_int8_t cdb_byte2;
	u_int8_t cdb_byte2_last;
	int inc_cdb_buffer_id;
	int inc_cdb_offset;
};

static const struct fw_vendor vendors_list[] = {
	{VENDOR_HITACHI,	"HITACHI",	0x8000, 0x05, 0x05, 1, 0},
	{VENDOR_HP,		"HP",		0x8000, 0x07, 0x07, 0, 1},
	{VENDOR_IBM,		"IBM",		0x8000, 0x05, 0x05, 1, 0},
	{VENDOR_PLEXTOR,	"PLEXTOR",	0x2000, 0x04, 0x05, 0, 1},
	{VENDOR_QUANTUM,	"QUANTUM",	0x2000, 0x04, 0x05, 0, 1},
	{VENDOR_SEAGATE,	"SEAGATE",	0x8000, 0x07, 0x07, 0, 1},
	{VENDOR_UNKNOWN,	NULL,		0x0000, 0x00, 0x00, 0, 0}
};

static const struct fw_vendor *fw_get_vendor(struct cam_device *cam_dev);
static char	*fw_read_img(const char *fw_img_path,
		    const struct fw_vendor *vp, int *num_bytes);
static int	 fw_download_img(struct cam_device *cam_dev,
		    const struct fw_vendor *vp, char *buf, int img_size,
		    int sim_mode, int verbose, int retry_count, int timeout);

/*
 * Find entry in vendors list that belongs to
 * the vendor of given cam device.
 */
static const struct fw_vendor *
fw_get_vendor(struct cam_device *cam_dev)
{
	char vendor[SID_VENDOR_SIZE + 1];
	const struct fw_vendor *vp;

	if (cam_dev == NULL)
		return (NULL);
	cam_strvis((u_char *)vendor, (u_char *)cam_dev->inq_data.vendor,
	    sizeof(cam_dev->inq_data.vendor), sizeof(vendor));
	for (vp = vendors_list; vp->pattern != NULL; vp++) {
		if (!cam_strmatch((const u_char *)vendor,
		    (const u_char *)vp->pattern, strlen(vendor)))
			break;
	}
	return (vp);
}

/*
 * Allocate a buffer and read fw image file into it
 * from given path. Number of bytes read is stored
 * in num_bytes.
 */
static char *
fw_read_img(const char *fw_img_path, const struct fw_vendor *vp, int *num_bytes)
{
	int fd;
	struct stat stbuf;
	char *buf;
	off_t img_size;
	int skip_bytes = 0;

	if ((fd = open(fw_img_path, O_RDONLY)) < 0) {
		warn("Could not open image file %s", fw_img_path);
		return (NULL);
	}
	if (fstat(fd, &stbuf) < 0) {
		warn("Could not stat image file %s", fw_img_path);
		goto bailout1;
	}
	if ((img_size = stbuf.st_size) == 0) {
		warnx("Zero length image file %s", fw_img_path);
		goto bailout1;
	}
	if ((buf = malloc(img_size)) == NULL) {
		warnx("Could not allocate buffer to read image file %s",
		    fw_img_path);
		goto bailout1;
	}
	/* Skip headers if applicable. */
	switch (vp->type) {
	case VENDOR_SEAGATE:
		if (read(fd, buf, 16) != 16) {
			warn("Could not read image file %s", fw_img_path);
			goto bailout;
		}
		if (lseek(fd, 0, SEEK_SET) == -1) {
			warn("Unable to lseek");
			goto bailout;
		}
		if ((strncmp(buf, "SEAGATE,SEAGATE ", 16) == 0) ||
		    (img_size % 512 == 80))
			skip_bytes = 80;
		break;
	default:
		break;
	}
	if (skip_bytes != 0) {
		fprintf(stdout, "Skipping %d byte header.\n", skip_bytes);
		if (lseek(fd, skip_bytes, SEEK_SET) == -1) {
			warn("Could not lseek");
			goto bailout;
		}
		img_size -= skip_bytes;
	}
	/* Read image into a buffer. */
	if (read(fd, buf, img_size) != img_size) {
		warn("Could not read image file %s", fw_img_path);
		goto bailout;
	}
	*num_bytes = img_size;
	return (buf);
bailout:
	free(buf);
bailout1:
	close(fd);
	*num_bytes = 0;
	return (NULL);
}

/* 
 * Download firmware stored in buf to cam_dev. If simulation mode
 * is enabled, only show what packet sizes would be sent to the 
 * device but do not sent any actual packets
 */
static int
fw_download_img(struct cam_device *cam_dev, const struct fw_vendor *vp,
    char *buf, int img_size, int sim_mode, int verbose, int retry_count,
    int timeout)
{
	struct scsi_write_buffer cdb;
	union ccb *ccb;
	int pkt_count = 0;
	u_int32_t pkt_size = 0;
	char *pkt_ptr = buf;
	u_int32_t offset;
	int last_pkt = 0;

	if ((ccb = cam_getccb(cam_dev)) == NULL) {
		warnx("Could not allocate CCB");
		return (1);
	}
	scsi_test_unit_ready(&ccb->csio, 0, NULL, MSG_SIMPLE_Q_TAG,
	    SSD_FULL_SIZE, 5000);
	/* Disable freezing the device queue. */
	ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;
	if (cam_send_ccb(cam_dev, ccb) < 0) {
		warnx("Error sending test unit ready");
		if (verbose)
			cam_error_print(cam_dev, ccb, CAM_ESF_ALL,
			    CAM_EPF_ALL, stderr);
		cam_freeccb(ccb);
		return(1);
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		warnx("Device is not ready");
		if (verbose)
			cam_error_print(cam_dev, ccb, CAM_ESF_ALL,
			    CAM_EPF_ALL, stderr);
		cam_freeccb(ccb);
		return (1);
	}
	pkt_size = vp->max_pkt_size;
	if (verbose || sim_mode) {
		fprintf(stdout,
		    "--------------------------------------------------\n");
		fprintf(stdout,
		    "PktNo.	PktSize	       BytesRemaining	LastPkt\n");
		fprintf(stdout,
		    "--------------------------------------------------\n");
	}
	/* Download single fw packets. */
	do {
		if (img_size <= vp->max_pkt_size) {
			last_pkt = 1;
			pkt_size = img_size;
		}
		if (verbose || sim_mode)
			fprintf(stdout, "%3u   %5u (0x%05X)   %7u (0x%06X)   "
			    "%d\n", pkt_count, pkt_size, pkt_size,
			    img_size - pkt_size, img_size - pkt_size,
			    last_pkt);
		bzero(&cdb, sizeof(cdb));
		cdb.opcode  = WRITE_BUFFER;
		cdb.control = 0;
		/* Parameter list length. */
		scsi_ulto3b(pkt_size, &cdb.length[0]);
		offset = vp->inc_cdb_offset ? (pkt_ptr - buf) : 0;
		scsi_ulto3b(offset, &cdb.offset[0]);
		cdb.byte2 = last_pkt ? vp->cdb_byte2_last : vp->cdb_byte2;
		cdb.buffer_id = vp->inc_cdb_buffer_id ? pkt_count : 0;
		/* Zero out payload of ccb union after ccb header. */
		bzero((u_char *)ccb + sizeof(struct ccb_hdr),
		    sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));
		/* Copy previously constructed cdb into ccb_scsiio struct. */
		bcopy(&cdb, &ccb->csio.cdb_io.cdb_bytes[0],
		    sizeof(struct scsi_write_buffer));
		/* Fill rest of ccb_scsiio struct. */
		if (!sim_mode) {
			cam_fill_csio(&ccb->csio,		/* ccb_scsiio	*/
			    retry_count,			/* retries	*/
			    NULL,				/* cbfcnp	*/
			    CAM_DIR_OUT | CAM_DEV_QFRZDIS,	/* flags	*/
			    CAM_TAG_ACTION_NONE,		/* tag_action	*/
			    (u_char *)pkt_ptr,			/* data_ptr	*/
			    pkt_size,				/* dxfer_len	*/
			    SSD_FULL_SIZE,			/* sense_len	*/
			    sizeof(struct scsi_write_buffer),	/* cdb_len	*/
			    timeout ? timeout : CMD_TIMEOUT);	/* timeout	*/
			/* Execute the command. */
			if (cam_send_ccb(cam_dev, ccb) < 0) {
				warnx("Error writing image to device");
				if (verbose)
					cam_error_print(cam_dev, ccb, CAM_ESF_ALL,
					    CAM_EPF_ALL, stderr);
				goto bailout;
			}
		}
		/* Prepare next round. */
		pkt_count++;
		pkt_ptr += pkt_size;
		img_size -= pkt_size;
	} while(!last_pkt);
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (verbose)
			cam_error_print(cam_dev, ccb, CAM_ESF_ALL,
			    CAM_EPF_ALL, stderr);
		goto bailout;
	}
	cam_freeccb(ccb);
	return (0);
bailout:
	cam_freeccb(ccb);
	return (1);
}

int
fwdownload(struct cam_device *device, int argc, char **argv,
    char *combinedopt, int verbose, int retry_count, int timeout)
{
	const struct fw_vendor *vp;
	char *fw_img_path = NULL;
	char *buf;
	int img_size;
	int c;
	int sim_mode = 0;
	int confirmed = 0;

	while ((c = getopt(argc, argv, combinedopt)) != -1) {
		switch (c) {
		case 's':
			sim_mode = 1;
			confirmed = 1;
			break;
		case 'f':
			fw_img_path = optarg;
			break;
		case 'y':
			confirmed = 1;
			break;
		default:
			break;
		}
	}

	if (fw_img_path == NULL)
		errx(1,
		    "you must specify a firmware image file using -f option");

	vp = fw_get_vendor(device);
	if (vp == NULL || vp->type == VENDOR_UNKNOWN)
		errx(1, "Unsupported device");

	buf = fw_read_img(fw_img_path, vp, &img_size);
	if (buf == NULL)
		goto fail;

	if (!confirmed) {
		fprintf(stdout, "You are about to download firmware image (%s)"
		    " into the following device:\n",
		    fw_img_path);
		if (scsidoinquiry(device, argc, argv, combinedopt, 0,
		    5000) != 0) {
			warnx("Error sending inquiry");
			goto fail;
		}
		fprintf(stdout, "\nIt may damage your drive. ");
		if (!get_confirmation())
			goto fail;
	}
	if (sim_mode)
		fprintf(stdout, "Running in simulation mode\n");

	if (fw_download_img(device, vp, buf, img_size, sim_mode, verbose,
	    retry_count, timeout) != 0) {
		fprintf(stderr, "Firmware download failed\n");
		goto fail;
	} else 
		fprintf(stdout, "Firmware download successful\n");

	free(buf);
	return (0);
fail:
	if (buf != NULL)
		free(buf);
	return (1);
}

