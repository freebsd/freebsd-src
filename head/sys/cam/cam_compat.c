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
#include <cam/cam_compat.h>

#include <cam/scsi/scsi_pass.h>

#include "opt_cam.h"

int
cam_compat_ioctl(struct cdev *dev, u_long *cmd, caddr_t *addr, int *flag, struct thread *td)
{
	int error;

	switch (*cmd) {
	case CAMIOCOMMAND_0x16:
	{
		union ccb *ccb;

		ccb = (union ccb *)*addr;
		if (ccb->ccb_h.flags & CAM_SG_LIST_PHYS_0x16) {
			ccb->ccb_h.flags &= ~CAM_SG_LIST_PHYS_0x16;
			ccb->ccb_h.flags |= CAM_DATA_SG_PADDR;
		}
		if (ccb->ccb_h.flags & CAM_DATA_PHYS_0x16) {
			ccb->ccb_h.flags &= ~CAM_DATA_PHYS_0x16;
			ccb->ccb_h.flags |= CAM_DATA_PADDR;
		}
		if (ccb->ccb_h.flags & CAM_SCATTER_VALID_0x16) {
			ccb->ccb_h.flags &= CAM_SCATTER_VALID_0x16;
			ccb->ccb_h.flags |= CAM_DATA_SG;
		}
		*cmd = CAMIOCOMMAND;
		error = EAGAIN;
		break;
	}
	case CAMGETPASSTHRU_0x16:
		*cmd = CAMGETPASSTHRU;
		error = EAGAIN;
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}
