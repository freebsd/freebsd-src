/* $FreeBSD: src/sys/dev/isp/isp_freebsd.h,v 1.26 2000/02/11 19:37:56 mjacob Exp $ */
/*
 * Qlogic ISP SCSI Host Adapter FreeBSD Wrapper Definitions (CAM version)
 *---------------------------------------
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
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
#ifndef	_ISP_FREEBSD_H
#define	_ISP_FREEBSD_H

#define	ISP_PLATFORM_VERSION_MAJOR	4
#define	ISP_PLATFORM_VERSION_MINOR	2


#include <sys/param.h>
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

#include "opt_ddb.h"
#include "opt_isp.h"
#ifdef	SCSI_ISP_FABRIC
#define	ISP2100_FABRIC		1
#define	ISP2100_SCRLEN		0x400
#else
#define	ISP2100_SCRLEN		0x100
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
#ifndef	SCSI_QFULL
#define	SCSI_QFULL	SCSI_STATUS_QUEUE_FULL
#endif

#define	ISP_SCSI_XFER_T		struct ccb_scsiio

#ifdef	ISP_TARGET_MODE
typedef struct tstate {
	struct tstate *next;
	struct cam_path *owner;
	struct ccb_hdr_slist atios;
	struct ccb_hdr_slist inots;
	lun_id_t lun;
	u_int32_t hold;
} tstate_t;

/*
 * This should work very well for 100% of parallel SCSI cases, 100%
 * of non-SCCLUN FC cases, and hopefully some larger fraction of the
 * SCCLUN FC cases. Basically, we index by the low 5 bits of lun and
 * then linear search. This has to be reasonably zippy, but not crucially
 * so.
 */
#define	LUN_HASH_SIZE		32
#define	LUN_HASH_FUNC(lun)	((lun) & 0x1f)

#endif

struct isposinfo {
	struct ispsoftc *	next;
	u_int64_t		default_wwn;
	char			name[8];
	int			unit;
	struct cam_sim		*sim;
	struct cam_path		*path;
	struct cam_sim		*sim2;
	struct cam_path		*path2;
	volatile char		simqfrozen;
#ifdef	ISP_TARGET_MODE
#define	TM_WANTED		0x01
#define	TM_BUSY			0x02
#define	TM_TMODE_ENABLED	0x80
	u_int8_t		tmflags;
	u_int8_t		rstatus;
	u_int16_t		rollinfo;
	tstate_t		tsdflt;
	tstate_t		*lun_hash[LUN_HASH_SIZE];
#endif
};
#define	SIMQFRZ_RESOURCE	0x1
#define	SIMQFRZ_LOOPDOWN	0x2
#define	SIMQFRZ_TIMED		0x4

#define	isp_sim		isp_osinfo.sim
#define	isp_path	isp_osinfo.path
#define	isp_sim2	isp_osinfo.sim2
#define	isp_path2	isp_osinfo.path2
#define	isp_unit	isp_osinfo.unit
#define	isp_name	isp_osinfo.name

#define	MAXISPREQUEST		256

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#define	DFLT_DBLEVEL		isp_debug
extern int isp_debug;
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
#define	XS_CHANNEL(ccb)		cam_sim_bus(xpt_path_sim((ccb)->ccb_h.path))
#define	XS_RESID(ccb)		(ccb)->resid
#define	XS_XFRLEN(ccb)		(ccb)->dxfer_len
#define	XS_CDBLEN(ccb)		(ccb)->cdb_len
#define	XS_CDBP(ccb)		(((ccb)->ccb_h.flags & CAM_CDB_POINTER)? \
	(ccb)->cdb_io.cdb_ptr : (ccb)->cdb_io.cdb_bytes)
#define	XS_STS(ccb)		(ccb)->scsi_status
#define	XS_TIME(ccb)		(ccb)->ccb_h.timeout
#define	XS_SNSP(ccb)		(&(ccb)->sense_data)
#define	XS_SNSLEN(ccb)		\
	imin((sizeof((ccb)->sense_data)), ccb->sense_len)
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
#define	CMD_RQLATER		3
#define	STOP_WATCHDOG(f, s)

extern void isp_attach(struct ispsoftc *);
extern void isp_uninit(struct ispsoftc *);

#define	MEMZERO			bzero
#define	MEMCPY(dst, src, amt)	bcopy((src), (dst), (amt))
#ifdef	__alpha__
#define	MemoryBarrier	alpha_mb
#else
#define	MemoryBarrier()
#endif


#define	DMA_MSW(x)	(((x) >> 16) & 0xffff)
#define	DMA_LSW(x)	(((x) & 0xffff))

#define	ISP_UNSWIZZLE_AND_COPY_PDBP(isp, dest, src)	\
	bcopy(src, dest, sizeof (isp_pdb_t))
#define	ISP_SWIZZLE_ICB(a, b)
#define	ISP_SWIZZLE_REQUEST(a, b)
#define	ISP_UNSWIZZLE_RESPONSE(a, b)
#define	ISP_SWIZZLE_SNS_REQ(a, b)
#define	ISP_UNSWIZZLE_SNS_RSP(a, b, c)

#define	IDPRINTF(lev, x)	if (isp->isp_dblev >= (u_int8_t) lev) printf x
#define	PRINTF			printf
#define	CFGPRINTF		if (bootverbose || DFLT_DBLEVEL > 1) printf

#define	SYS_DELAY(x)	DELAY(x)

#define	FC_FW_READY_DELAY	(5 * 1000000)
#define	DEFAULT_LOOPID(x)	109
#define	DEFAULT_WWN(x)		(x)->isp_osinfo.default_wwn

#define	INLINE	__inline
#include <dev/isp/isp_inline.h>
#endif	/* _ISP_FREEBSD_H */
