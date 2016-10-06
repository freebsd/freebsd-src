/*-
 * CAM ioctl compatibility shims
 *
 * Copyright (c) 2013 Scott Long
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_xpt.h>
#include <cam/cam_compat.h>

#include <cam/scsi/scsi_pass.h>

#include "opt_cam.h"

static int cam_compat_handle_0x17(struct cdev *dev, u_long cmd, caddr_t addr,
    int flag, struct thread *td, d_ioctl_t *cbfnp);

int
cam_compat_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td, d_ioctl_t *cbfnp)
{
	int error;

	switch (cmd) {
	case CAMIOCOMMAND_0x16:
	{
		struct ccb_hdr_0x17 *hdr17;

		hdr17 = (struct ccb_hdr_0x17 *)addr;
		if (hdr17->flags & CAM_SG_LIST_PHYS_0x16) {
			hdr17->flags &= ~CAM_SG_LIST_PHYS_0x16;
			hdr17->flags |= CAM_DATA_SG_PADDR;
		}
		if (hdr17->flags & CAM_DATA_PHYS_0x16) {
			hdr17->flags &= ~CAM_DATA_PHYS_0x16;
			hdr17->flags |= CAM_DATA_PADDR;
		}
		if (hdr17->flags & CAM_SCATTER_VALID_0x16) {
			hdr17->flags &= CAM_SCATTER_VALID_0x16;
			hdr17->flags |= CAM_DATA_SG;
		}
		cmd = CAMIOCOMMAND;
		error = cam_compat_handle_0x17(dev, cmd, addr, flag, td, cbfnp);
		break;
	}
	case CAMGETPASSTHRU_0x16:
		cmd = CAMGETPASSTHRU;
		error = cam_compat_handle_0x17(dev, cmd, addr, flag, td, cbfnp);
		break;
	case CAMIOCOMMAND_0x17:
		cmd = CAMIOCOMMAND;
		error = cam_compat_handle_0x17(dev, cmd, addr, flag, td, cbfnp);
		break;
	case CAMGETPASSTHRU_0x17:
		cmd = CAMGETPASSTHRU;
		error = cam_compat_handle_0x17(dev, cmd, addr, flag, td, cbfnp);
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

static int
cam_compat_handle_0x17(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td, d_ioctl_t *cbfnp)
{
	union ccb		*ccb;
	struct ccb_hdr		*hdr;
	struct ccb_hdr_0x17	*hdr17;
	uint8_t			*ccbb, *ccbb17;
	u_int			error;

	hdr17 = (struct ccb_hdr_0x17 *)addr;
	ccb = xpt_alloc_ccb();
	hdr = &ccb->ccb_h;

	hdr->pinfo = hdr17->pinfo;
	hdr->xpt_links = hdr17->xpt_links;
	hdr->sim_links = hdr17->sim_links;
	hdr->periph_links = hdr17->periph_links;
	hdr->retry_count = hdr17->retry_count;
	hdr->cbfcnp = hdr17->cbfcnp;
	hdr->func_code = hdr17->func_code;
	hdr->status = hdr17->status;
	hdr->path = hdr17->path;
	hdr->path_id = hdr17->path_id;
	hdr->target_id = hdr17->target_id;
	hdr->target_lun = hdr17->target_lun;
	hdr->ext_lun.lun64 = 0;
	hdr->flags = hdr17->flags;
	hdr->xflags = 0;
	hdr->periph_priv = hdr17->periph_priv;
	hdr->sim_priv = hdr17->sim_priv;
	hdr->timeout = hdr17->timeout;
	hdr->softtimeout.tv_sec = 0;
	hdr->softtimeout.tv_usec = 0;

	ccbb = (uint8_t *)&hdr[1];
	ccbb17 = (uint8_t *)&hdr17[1];
	bcopy(ccbb17, ccbb, CAM_0X17_DATA_LEN);

	error = (cbfnp)(dev, cmd, (caddr_t)ccb, flag, td);

	hdr17->pinfo = hdr->pinfo;
	hdr17->xpt_links = hdr->xpt_links;
	hdr17->sim_links = hdr->sim_links;
	hdr17->periph_links = hdr->periph_links;
	hdr17->retry_count = hdr->retry_count;
	hdr17->cbfcnp = hdr->cbfcnp;
	hdr17->func_code = hdr->func_code;
	hdr17->status = hdr->status;
	hdr17->path = hdr->path;
	hdr17->path_id = hdr->path_id;
	hdr17->target_id = hdr->target_id;
	hdr17->target_lun = hdr->target_lun;
	hdr17->flags = hdr->flags;
	hdr17->periph_priv = hdr->periph_priv;
	hdr17->sim_priv = hdr->sim_priv;
	hdr17->timeout = hdr->timeout;

	/* The PATH_INQ only needs special handling on the way out */
	if (ccb->ccb_h.func_code != XPT_PATH_INQ) {
		bcopy(ccbb, ccbb17, CAM_0X17_DATA_LEN);
	} else {
		struct ccb_pathinq	*cpi;
		struct ccb_pathinq_0x17 *cpi17;

		cpi = &ccb->cpi;
		cpi17 = (struct ccb_pathinq_0x17 *)hdr17;
		cpi17->version_num = cpi->version_num;
		cpi17->hba_inquiry = cpi->hba_inquiry;
		cpi17->target_sprt = (u_int8_t)cpi->target_sprt;
		cpi17->hba_misc = (u_int8_t)cpi->hba_misc;
		cpi17->hba_eng_cnt = cpi->hba_eng_cnt;
		bcopy(&cpi->vuhba_flags[0], &cpi17->vuhba_flags[0], VUHBALEN);
		cpi17->max_target = cpi->max_target;
		cpi17->max_lun = cpi->max_lun;
		cpi17->async_flags = cpi->async_flags;
		cpi17->hpath_id = cpi->hpath_id;
		cpi17->initiator_id = cpi->initiator_id;
		bcopy(&cpi->sim_vid[0], &cpi17->sim_vid[0], SIM_IDLEN);
		bcopy(&cpi->hba_vid[0], &cpi17->hba_vid[0], HBA_IDLEN);
		bcopy(&cpi->dev_name[0], &cpi17->dev_name[0], DEV_IDLEN);
		cpi17->unit_number = cpi->unit_number;
		cpi17->bus_id = cpi->bus_id;
		cpi17->base_transfer_speed = cpi->base_transfer_speed;
		cpi17->protocol = cpi->protocol;
		cpi17->protocol_version = cpi->protocol_version;
		cpi17->transport = cpi->transport;
		cpi17->transport_version = cpi->transport_version;
		bcopy(&cpi->xport_specific, &cpi17->xport_specific,
		    PATHINQ_SETTINGS_SIZE);
		cpi17->maxio = cpi->maxio;
		cpi17->hba_vendor = cpi->hba_vendor;
		cpi17->hba_device = cpi->hba_device;
		cpi17->hba_subvendor = cpi->hba_subvendor;
		cpi17->hba_subdevice = cpi->hba_subdevice;
	}

	xpt_free_ccb(ccb);

	return (error);
}
