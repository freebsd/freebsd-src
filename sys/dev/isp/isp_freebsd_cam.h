/* $Id: isp_freebsd_cam.h,v 1.15 1999/03/17 05:04:39 mjacob Exp $ */
/* release_03_25_99 */
/*
 * Qlogic ISP SCSI Host Adapter FreeBSD Wrapper Definitions (CAM version)
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
#ifndef	_ISP_FREEBSD_CAM_H
#define	_ISP_FREEBSD_CAM_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>

#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include "opt_isp.h"
#ifdef	SCSI_ISP_FABRIC
#define	ISP2100_FABRIC	1
#endif
#ifdef	SCSI_ISP_SCCLUN
#define	ISP2100_SCCLUN	1
#endif

#ifndef	SCSI_CHECK
#define	SCSI_CHECK	SCSI_STATUS_CHECK_COND
#endif
#ifndef	SCSI_BUSY
#define	SCSI_BUSY	SCSI_STATUS_BUSY
#endif

#define	ISP_SCSI_XFER_T		struct ccb_scsiio
struct isposinfo {
	char			name[8];
	int			unit;
	struct cam_sim		*sim;
	struct cam_path		*path;
	volatile char		simqfrozen;
};
#define	SIMQFRZ_RESOURCE	0x1
#define	SIMQFRZ_LOOPDOWN	0x2

#define	isp_sim		isp_osinfo.sim
#define	isp_path	isp_osinfo.path
#define	isp_unit	isp_osinfo.unit
#define	isp_name	isp_osinfo.name


#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#define	PVS			"Qlogic ISP Driver, FreeBSD CAM"
#ifdef	CAMDEBUG
#define	DFLT_DBLEVEL		2
#else
#define	DFLT_DBLEVEL		1
#endif
#define	ISP_LOCKVAL_DECL	int isp_spl_save
#define	ISP_ILOCKVAL_DECL	ISP_LOCKVAL_DECL
#define	ISP_UNLOCK(isp)		(void) splx(isp_spl_save)
#define	ISP_LOCK(isp)		isp_spl_save = splcam()
#define	ISP_ILOCK(isp)		ISP_LOCK(isp)
#define	ISP_IUNLOCK(isp)	ISP_UNLOCK(isp)
#define	IMASK			cam_imask

#define	XS_NULL(ccb)		ccb == NULL
#define	XS_ISP(ccb)		((struct ispsoftc *) (ccb)->ccb_h.spriv_ptr1)

#define	XS_LUN(ccb)		(ccb)->ccb_h.target_lun
#define	XS_TGT(ccb)		(ccb)->ccb_h.target_id
#define	XS_RESID(ccb)		(ccb)->resid
#define	XS_XFRLEN(ccb)		(ccb)->dxfer_len
#define	XS_CDBLEN(ccb)		(ccb)->cdb_len
#define	XS_CDBP(ccb)		(((ccb)->ccb_h.flags & CAM_CDB_POINTER)? \
	(ccb)->cdb_io.cdb_ptr : (ccb)->cdb_io.cdb_bytes)
#define	XS_STS(ccb)		(ccb)->scsi_status
#define	XS_TIME(ccb)		(ccb)->ccb_h.timeout
#define	XS_SNSP(ccb)		(&(ccb)->sense_data)
#define	XS_SNSLEN(ccb)		imin((sizeof((ccb)->sense_data)), ccb->sense_len)
#define	XS_SNSKEY(ccb)		((ccb)->sense_data.flags & 0xf)

/*
 * A little tricky- HBA_NOERROR is "in progress" so
 * that XS_CMD_DONE can transition this to CAM_REQ_CMP.
 */
#define	HBA_NOERROR		CAM_REQ_INPROG
#define	HBA_BOTCH		CAM_UNREC_HBA_ERROR
#define	HBA_CMDTIMEOUT		CAM_CMD_TIMEOUT
#define	HBA_SELTIMEOUT		CAM_SEL_TIMEOUT
#define	HBA_TGTBSY		CAM_SCSI_STATUS_ERROR
#define	HBA_BUSRESET		CAM_SCSI_BUS_RESET
#define	HBA_ABORTED		CAM_REQ_ABORTED
#define	HBA_DATAOVR		CAM_DATA_RUN_ERR
#define	HBA_ARQFAIL		CAM_AUTOSENSE_FAIL

#define	XS_SNS_IS_VALID(ccb) ((ccb)->ccb_h.status |= CAM_AUTOSNS_VALID)
#define	XS_IS_SNS_VALID(ccb) (((ccb)->ccb_h.status & CAM_AUTOSNS_VALID) != 0)

#define	XS_INITERR(ccb)		\
	(ccb)->ccb_h.status &= ~CAM_STATUS_MASK, \
	(ccb)->ccb_h.status |= CAM_REQ_INPROG, \
	(ccb)->ccb_h.spriv_field0 = CAM_REQ_INPROG
#define	XS_SETERR(ccb, v)	(ccb)->ccb_h.spriv_field0 = v
#define	XS_ERR(ccb)		(ccb)->ccb_h.spriv_field0
#define	XS_NOERR(ccb)		\
	((ccb)->ccb_h.spriv_field0 == CAM_REQ_INPROG)

extern void isp_done(struct ccb_scsiio *);
#define	XS_CMD_DONE(sccb)	isp_done(sccb)

#define	XS_IS_CMD_DONE(ccb)	\
	(((ccb)->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG)

/*
 * Can we tag?
 */
#define	XS_CANTAG(ccb)		(((ccb)->ccb_h.flags & CAM_TAG_ACTION_VALID) \
				  && (ccb)->tag_action != CAM_TAG_ACTION_NONE)
/*
 * And our favorite tag is....
 */
#define	XS_KINDOF_TAG(ccb)	\
	((ccb->tag_action == MSG_SIMPLE_Q_TAG)? REQFLAG_STAG : \
	  ((ccb->tag_action == MSG_HEAD_OF_Q_TAG)? REQFLAG_HTAG : REQFLAG_OTAG))
		


#define	CMD_COMPLETE		0
#define	CMD_EAGAIN		1
#define	CMD_QUEUED		2
#define	STOP_WATCHDOG(f, s)

#endif	/* _ISP_FREEBSD_CAM_H */
