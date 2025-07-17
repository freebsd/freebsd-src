/*-
 * Copyright (c) 2010 by Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * A VHBA device to test REPORT LUN functionality.
 */
#include "vhba.h"

#define	MAX_TGT		1
#define	MAX_LUN		1024

#define	DISK_SIZE	32
#define	DISK_SHIFT	9
#define	DISK_NBLKS	((DISK_SIZE << 20) >> DISK_SHIFT)
#define	PSEUDO_SPT	64
#define	PSEUDO_HDS	64
#define	PSEUDO_SPC	(PSEUDO_SPT * PSEUDO_HDS)

typedef struct {
	vhba_softc_t *	vhba;
	uint8_t *	disk;
	size_t		disk_size;
	struct task	qt;
	uint8_t 	rpbitmap[MAX_LUN >> 3];
} vhbarptluns_t;

static void vhba_task(void *, int);
static void vhbarptluns_act(vhbarptluns_t *, struct ccb_scsiio *);

void
vhba_init(vhba_softc_t *vhba)
{
	static vhbarptluns_t vhbas;
	struct timeval now;
	int i;

	vhbas.vhba = vhba;
	vhbas.disk_size = DISK_SIZE << 20;
	vhbas.disk = malloc(vhbas.disk_size, M_DEVBUF, M_WAITOK|M_ZERO);
	vhba->private = &vhbas;
	printf("setting luns");
	getmicrotime(&now);
	if (now.tv_usec & 0x1) {
		vhbas.rpbitmap[0] |= 1;
	}
	for (i = 1; i < 8; i++) {
		if (arc4random() & 1) {
			printf(" %d", i);
			vhbas.rpbitmap[0] |= (1 << i);
		}
	}
	for (i = 8; i < MAX_LUN; i++) {
		if ((arc4random() % i) == 0) {
			vhbas.rpbitmap[i >> 3] |= (1 << (i & 0x7));
			printf(" %d", i);
		}
	}
	printf("\n");
	TASK_INIT(&vhbas.qt, 0, vhba_task, &vhbas);
}

void
vhba_fini(vhba_softc_t *vhba)
{
	vhbarptluns_t *vhbas = vhba->private;
	vhba->private = NULL;
	free(vhbas->disk, M_DEVBUF);
}

void
vhba_kick(vhba_softc_t *vhba)
{
	vhbarptluns_t *vhbas = vhba->private;
	taskqueue_enqueue(taskqueue_swi, &vhbas->qt);
}

static void
vhba_task(void *arg, int pending)
{
	vhbarptluns_t *vhbas = arg;
	struct ccb_hdr *ccbh;

	mtx_lock(&vhbas->vhba->lock);
	while ((ccbh = TAILQ_FIRST(&vhbas->vhba->actv)) != NULL) {
		TAILQ_REMOVE(&vhbas->vhba->actv, ccbh, sim_links.tqe);
                vhbarptluns_act(vhbas, (struct ccb_scsiio *)ccbh);
	}
	while ((ccbh = TAILQ_FIRST(&vhbas->vhba->done)) != NULL) {
		TAILQ_REMOVE(&vhbas->vhba->done, ccbh, sim_links.tqe);
		xpt_done((union ccb *)ccbh);
	}
	mtx_unlock(&vhbas->vhba->lock);
}

static void
vhbarptluns_act(vhbarptluns_t *vhbas, struct ccb_scsiio *csio)
{
	char junk[128];
	uint8_t *cdb, *ptr, status;
	uint32_t data_len;
	uint64_t off;
	int i, attached_lun = 0;
	    
	data_len = 0;
	status = SCSI_STATUS_OK;

	memset(&csio->sense_data, 0, sizeof (csio->sense_data));
	cdb = csio->cdb_io.cdb_bytes;

	if (csio->ccb_h.target_id >= MAX_TGT) {
		csio->ccb_h.status = CAM_SEL_TIMEOUT;
		TAILQ_INSERT_TAIL(&vhbas->vhba->done, &csio->ccb_h, sim_links.tqe);
		return;
	}

	if (csio->ccb_h.target_lun < MAX_LUN) {
		i = csio->ccb_h.target_lun & 0x7;
		if (vhbas->rpbitmap[csio->ccb_h.target_lun >> 3] & (1 << i)) {
			attached_lun = 1;
		}
	}
	if (attached_lun == 0 && cdb[0] != INQUIRY && cdb[0] != REPORT_LUNS && cdb[0] != REQUEST_SENSE) {
		vhba_fill_sense(csio, SSD_KEY_ILLEGAL_REQUEST, 0x25, 0x0);
		TAILQ_INSERT_TAIL(&vhbas->vhba->done, &csio->ccb_h, sim_links.tqe);
		return;
	}

	switch (cdb[0]) {
	case MODE_SENSE:
	case MODE_SENSE_10:
	{
		unsigned int nbyte;
		uint8_t page = cdb[2] & SMS_PAGE_CODE;
		uint8_t pgctl = cdb[2] & SMS_PAGE_CTRL_MASK;

		switch (page) {
		case SMS_FORMAT_DEVICE_PAGE:
		case SMS_GEOMETRY_PAGE:
		case SMS_CACHE_PAGE:
		case SMS_CONTROL_MODE_PAGE:
		case SMS_ALL_PAGES_PAGE:
			break;
		default:
			vhba_fill_sense(csio, SSD_KEY_ILLEGAL_REQUEST, 0x24, 0x0);
			TAILQ_INSERT_TAIL(&vhbas->vhba->done, &csio->ccb_h, sim_links.tqe);
			return;
		}
		memset(junk, 0, sizeof (junk));
		if (cdb[1] & SMS_DBD) {
			ptr = &junk[4];
		} else {
			ptr = junk;
			ptr[3] = 8;
			ptr[4] = ((1 << DISK_SHIFT) >> 24) & 0xff;
			ptr[5] = ((1 << DISK_SHIFT) >> 16) & 0xff;
			ptr[6] = ((1 << DISK_SHIFT) >>  8) & 0xff;
			ptr[7] = ((1 << DISK_SHIFT)) & 0xff;

			ptr[8] = (DISK_NBLKS >> 24) & 0xff;
			ptr[9] = (DISK_NBLKS >> 16) & 0xff;
			ptr[10] = (DISK_NBLKS >> 8) & 0xff;
			ptr[11] = DISK_NBLKS & 0xff;
			ptr += 12;
		}

		if (page == SMS_ALL_PAGES_PAGE || page == SMS_FORMAT_DEVICE_PAGE) {
			ptr[0] = SMS_FORMAT_DEVICE_PAGE;
			ptr[1] = 24;
			if (pgctl != SMS_PAGE_CTRL_CHANGEABLE) {
				/* tracks per zone */
				/* ptr[2] = 0; */
				/* ptr[3] = 0; */
				/* alternate sectors per zone */
				/* ptr[4] = 0; */
				/* ptr[5] = 0; */
				/* alternate tracks per zone */
				/* ptr[6] = 0; */
				/* ptr[7] = 0; */
				/* alternate tracks per logical unit */
				/* ptr[8] = 0; */
				/* ptr[9] = 0; */
				/* sectors per track */
				ptr[10] = (PSEUDO_SPT >> 8) & 0xff;
				ptr[11] = PSEUDO_SPT & 0xff;
				/* data bytes per physical sector */
				ptr[12] = ((1 << DISK_SHIFT) >> 8) & 0xff;
				ptr[13] = (1 << DISK_SHIFT) & 0xff;
				/* interleave */
				/* ptr[14] = 0; */
				/* ptr[15] = 1; */
				/* track skew factor */
				/* ptr[16] = 0; */
				/* ptr[17] = 0; */
				/* cylinder skew factor */
				/* ptr[18] = 0; */
				/* ptr[19] = 0; */
				/* SSRC, HSEC, RMB, SURF */
			}
			ptr += 26;
		}

		if (page == SMS_ALL_PAGES_PAGE || page == SMS_GEOMETRY_PAGE) {
			ptr[0] = SMS_GEOMETRY_PAGE;
			ptr[1] = 24;
			if (pgctl != SMS_PAGE_CTRL_CHANGEABLE) {
				uint32_t cyl = (DISK_NBLKS + ((PSEUDO_SPC - 1))) / PSEUDO_SPC;
				/* number of cylinders */
				ptr[2] = (cyl >> 24) & 0xff;
				ptr[3] = (cyl >> 16) & 0xff;
				ptr[4] = cyl & 0xff;
				/* number of heads */
				ptr[5] = PSEUDO_HDS;
				/* starting cylinder- write precompensation */
				/* ptr[6] = 0; */
				/* ptr[7] = 0; */
				/* ptr[8] = 0; */
				/* starting cylinder- reduced write current */
				/* ptr[9] = 0; */
				/* ptr[10] = 0; */
				/* ptr[11] = 0; */
				/* drive step rate */
				/* ptr[12] = 0; */
				/* ptr[13] = 0; */
				/* landing zone cylinder */
				/* ptr[14] = 0; */
				/* ptr[15] = 0; */
				/* ptr[16] = 0; */
				/* RPL */
				/* ptr[17] = 0; */
				/* rotational offset */
				/* ptr[18] = 0; */
				/* medium rotation rate -  7200 RPM */
				ptr[20] = 0x1c;
				ptr[21] = 0x20;
			}
			ptr += 26;
		}

		if (page == SMS_ALL_PAGES_PAGE || page == SMS_CACHE_PAGE) {
			ptr[0] = SMS_CACHE_PAGE;
			ptr[1] = 18;
			ptr[2] = 1 << 2;
			ptr += 20;
		}

		if (page == SMS_ALL_PAGES_PAGE || page == SMS_CONTROL_MODE_PAGE) {
			ptr[0] = SMS_CONTROL_MODE_PAGE;
			ptr[1] = 10;
			if (pgctl != SMS_PAGE_CTRL_CHANGEABLE) {
				ptr[3] = 1 << 4; /* unrestricted reordering allowed */
				ptr[8] = 0x75;   /* 30000 ms */
				ptr[9] = 0x30;
			}
			ptr += 12;
		}
		nbyte = (char *)ptr - &junk[0];
		ptr[0] = nbyte - 4;

		if (cdb[0] == MODE_SENSE) {
			data_len = min(cdb[4], csio->dxfer_len);
		} else {
			uint16_t tw = (cdb[7] << 8) | cdb[8];
			data_len = min(tw, csio->dxfer_len);
		}
		data_len = min(data_len, nbyte);
		if (data_len) {
			memcpy(csio->data_ptr, junk, data_len);
		}
		csio->resid = csio->dxfer_len - data_len;
		break;
	}
	case READ_6:
	case READ_10:
	case READ_12:
	case READ_16:
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
	case WRITE_16:
		if (vhba_rwparm(cdb, &off, &data_len, DISK_NBLKS, DISK_SHIFT)) {
			vhba_fill_sense(csio, SSD_KEY_ILLEGAL_REQUEST, 0x24, 0x0);
			break;
		}
		if (data_len) {
			if ((cdb[0] & 0xf) == 8) {
				memcpy(csio->data_ptr, &vhbas->disk[off], data_len);
			} else {
				memcpy(&vhbas->disk[off], csio->data_ptr, data_len);
			}
			csio->resid = csio->dxfer_len - data_len;
		} else {
			csio->resid = csio->dxfer_len;
		}
		break;
		break;

	case READ_CAPACITY:
		if (cdb[2] || cdb[3] || cdb[4] || cdb[5]) {
			vhba_fill_sense(csio, SSD_KEY_UNIT_ATTENTION, 0x24, 0x0);
			break;
		}
		if (cdb[8] & 0x1) { /* PMI */
			csio->data_ptr[0] = 0xff;
			csio->data_ptr[1] = 0xff;
			csio->data_ptr[2] = 0xff;
			csio->data_ptr[3] = 0xff;
		} else {
			uint64_t last_blk = DISK_NBLKS - 1;
			if (last_blk < 0xffffffffULL) {
			    csio->data_ptr[0] = (last_blk >> 24) & 0xff;
			    csio->data_ptr[1] = (last_blk >> 16) & 0xff;
			    csio->data_ptr[2] = (last_blk >>  8) & 0xff;
			    csio->data_ptr[3] = (last_blk) & 0xff;
			} else {
			    csio->data_ptr[0] = 0xff;
			    csio->data_ptr[1] = 0xff;
			    csio->data_ptr[2] = 0xff;
			    csio->data_ptr[3] = 0xff;
			}
		}
		csio->data_ptr[4] = ((1 << DISK_SHIFT) >> 24) & 0xff;
		csio->data_ptr[5] = ((1 << DISK_SHIFT) >> 16) & 0xff;
		csio->data_ptr[6] = ((1 << DISK_SHIFT) >>  8) & 0xff;
		csio->data_ptr[7] = ((1 << DISK_SHIFT)) & 0xff;
		break;

	default:
		vhba_default_cmd(csio, MAX_LUN, vhbas->rpbitmap);
		break;
	}
	csio->ccb_h.status &= ~CAM_STATUS_MASK;
	if (csio->scsi_status != SCSI_STATUS_OK) {
		csio->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
		if (csio->scsi_status == SCSI_STATUS_CHECK_COND) {
			csio->ccb_h.status |= CAM_AUTOSNS_VALID;
		}
	} else {
		csio->scsi_status = SCSI_STATUS_OK;
		csio->ccb_h.status |= CAM_REQ_CMP;
	}
	TAILQ_INSERT_TAIL(&vhbas->vhba->done, &csio->ccb_h, sim_links.tqe);
}
DEV_MODULE(vhba_rtpluns, vhba_modprobe, NULL);
