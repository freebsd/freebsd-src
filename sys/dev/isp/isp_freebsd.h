/* $Id: $ */
/* release_6_2_99 */
/*
 * Qlogic ISP SCSI Host Adapter FreeBSD 2.X Wrapper Definitions
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

#define	ISP_PLATFORM_VERSION_MAJOR	0
#define	ISP_PLATFORM_VERSION_MINOR	992

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsiconf.h>
#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/kernel.h>

#include "opt_isp.h"

#ifndef	SCSI_ISP_PREFER_MEM_MAP
#define	SCSI_ISP_PREFER_MEM_MAP	0
#endif


#ifdef	SCSI_ISP_FABRIC
#define	ISP2100_FABRIC		1
#define	ISP2100_SCRLEN		0x400
#else
#define	ISP2100_SCRLEN		0x1000
#endif

#ifdef	SCSI_ISP_SCCLUN
#define	ISP2100_SCCLUN	1
#endif


#define	ISP_SCSI_XFER_T		struct scsi_xfer
struct isposinfo {
	char			name[8];
	int			unit;
	int			seed;
	struct scsi_link	_link;
	int8_t			delay_throttle_count;
};
#define	MAXISPREQUEST		64

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#define	PVS			"Qlogic ISP Driver, FreeBSD Non-Cam"
#define	DFLT_DBLEVEL		1
#define	ISP_LOCKVAL_DECL	int isp_spl_save
#define	ISP_ILOCKVAL_DECL	ISP_LOCKVAL_DECL
#define	ISP_UNLOCK(isp)		(void) splx(isp_spl_save)
#define	ISP_LOCK(isp)		isp_spl_save = splbio()
#define	ISP_ILOCK(isp)		ISP_LOCK(isp)
#define	ISP_IUNLOCK(isp)	ISP_UNLOCK(isp)
#define	IMASK			bio_imask

#define	XS_NULL(xs)		xs == NULL || xs->sc_link == NULL
#define	XS_ISP(xs)		\
	((struct ispsoftc *) (xs)->sc_link->adapter_softc)
#define	XS_LUN(xs)		((int) (xs)->sc_link->lun)
#define	XS_TGT(xs)		((int) (xs)->sc_link->target)
#define	XS_CHANNEL(xs)		((int) (xs)->sc_link->adapter_bus)
#define	XS_RESID(xs)		(xs)->resid
#define	XS_XFRLEN(xs)		(xs)->datalen
#define	XS_CDBLEN(xs)		(xs)->cmdlen
#define	XS_CDBP(xs)		(xs)->cmd
#define	XS_STS(xs)		(xs)->status
#define	XS_TIME(xs)		(xs)->timeout
#define	XS_SNSP(xs)		(&(xs)->sense)
#define	XS_SNSLEN(xs)		(sizeof((xs)->sense))
#define	XS_SNSKEY(xs)		((xs)->sense.ext.extended.flags)

#define	HBA_NOERROR		XS_NOERROR
#define	HBA_BOTCH		XS_DRIVER_STUFFUP
#define	HBA_CMDTIMEOUT		XS_TIMEOUT
#define	HBA_SELTIMEOUT		XS_SELTIMEOUT
#define	HBA_TGTBSY		XS_BUSY
#define	HBA_BUSRESET		XS_DRIVER_STUFFUP
#define	HBA_ABORTED		XS_DRIVER_STUFFUP
#define	HBA_DATAOVR		XS_DRIVER_STUFFUP
#define	HBA_ARQFAIL		XS_DRIVER_STUFFUP

#define	XS_SNS_IS_VALID(xs)	(xs)->error = XS_SENSE
#define	XS_IS_SNS_VALID(xs)	((xs)->error == XS_SENSE)

#define	XS_INITERR(xs)		(xs)->error = 0
#define	XS_SETERR(xs, v)	(xs)->error = v
#define	XS_ERR(xs)		(xs)->error
#define	XS_NOERR(xs)		(xs)->error == XS_NOERROR

#define	XS_CMD_DONE(xs)		(xs)->flags |= ITSDONE, scsi_done(xs)
#define	XS_IS_CMD_DONE(xs)	(((xs)->flags & ITSDONE) != 0)

/*
 * We decide whether to use tags based upon whether we're polling.
 */
#define	XS_CANTAG(xs)		(((xs)->flags & SCSI_NOMASK) != 0)

/*
 * Our default tag
 */
#define	XS_KINDOF_TAG(xs)	REQFLAG_STAG


#define	CMD_COMPLETE		COMPLETE
#define	CMD_EAGAIN		TRY_AGAIN_LATER
#define	CMD_QUEUED		SUCCESSFULLY_QUEUED

#define	isp_name	isp_osinfo.name
#define	isp_unit	isp_osinfo.unit

#define	SCSI_QFULL	0x28

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

#define	IDPRINTF(lev, x)	if (isp->isp_dblev >= lev) printf x
#define	PRINTF			printf

#define	SYS_DELAY(x)	DELAY(x)

#define	FC_FW_READY_DELAY	(5 * 1000000)
#define	DEFAULT_LOOPID(x)	109
#define	DEFAULT_WWN(x)		(0x1000feeb00000000LL + (x)->isp_osinfo.seed)

static __inline void isp_prtstst(ispstatusreq_t *sp);
static __inline const char *isp2100_fw_statename(int state);
static __inline const char *isp2100_pdb_statename(int pdb_state);

static __inline void isp_prtstst(ispstatusreq_t *sp)
{
	char buf[128];
	sprintf(buf, "states->");
	if (sp->req_state_flags & RQSF_GOT_BUS)
		sprintf(buf, "%s%s", buf, "GOT_BUS ");
	if (sp->req_state_flags & RQSF_GOT_TARGET)
		sprintf(buf, "%s%s", buf, "GOT_TGT ");
	if (sp->req_state_flags & RQSF_SENT_CDB)
		sprintf(buf, "%s%s", buf, "SENT_CDB ");
	if (sp->req_state_flags & RQSF_XFRD_DATA)
		sprintf(buf, "%s%s", buf, "XFRD_DATA ");
	if (sp->req_state_flags & RQSF_GOT_STATUS)
		sprintf(buf, "%s%s", buf, "GOT_STS ");
	if (sp->req_state_flags & RQSF_GOT_SENSE)
		sprintf(buf, "%s%s", buf, "GOT_SNS ");
	if (sp->req_state_flags & RQSF_XFER_COMPLETE)
		sprintf(buf, "%s%s", buf, "XFR_CMPLT ");
	sprintf(buf, "%s%s", buf, "\n");
	sprintf(buf, "%s%s", buf, "status->");
	if (sp->req_status_flags & RQSTF_DISCONNECT)
		sprintf(buf, "%s%s", buf, "Disconnect ");
	if (sp->req_status_flags & RQSTF_SYNCHRONOUS)
		sprintf(buf, "%s%s", buf, "Sync_xfr ");
	if (sp->req_status_flags & RQSTF_PARITY_ERROR)
		sprintf(buf, "%s%s", buf, "Parity ");
	if (sp->req_status_flags & RQSTF_BUS_RESET)
		sprintf(buf, "%s%s", buf, "Bus_Reset ");
	if (sp->req_status_flags & RQSTF_DEVICE_RESET)
		sprintf(buf, "%s%s", buf, "Device_Reset ");
	if (sp->req_status_flags & RQSTF_ABORTED)
		sprintf(buf, "%s%s", buf, "Aborted ");
	if (sp->req_status_flags & RQSTF_TIMEOUT)
		sprintf(buf, "%s%s", buf, "Timeout ");
	if (sp->req_status_flags & RQSTF_NEGOTIATION)
		sprintf(buf, "%s%s", buf, "Negotiation ");
	printf(buf, "%s\n", buf);
}

static __inline const char *isp2100_fw_statename(int state)
{
	static char buf[16];
	switch(state) {
	case FW_CONFIG_WAIT:	return "Config Wait";
	case FW_WAIT_AL_PA:	return "Waiting for AL_PA";
	case FW_WAIT_LOGIN:	return "Wait Login";
	case FW_READY:		return "Ready";
	case FW_LOSS_OF_SYNC:	return "Loss Of Sync";
	case FW_ERROR:		return "Error";
	case FW_REINIT:		return "Re-Init";
	case FW_NON_PART:	return "Nonparticipating";
	default:
		sprintf(buf, "0x%x", state);
		return buf;
	}
}

static __inline const char *isp2100_pdb_statename(int pdb_state)
{
	static char buf[16];
	switch(pdb_state) {
	case PDB_STATE_DISCOVERY:	return "Port Discovery";
	case PDB_STATE_WDISC_ACK:	return "Waiting Port Discovery ACK";
	case PDB_STATE_PLOGI:		return "Port Login";
	case PDB_STATE_PLOGI_ACK:	return "Wait Port Login ACK";
	case PDB_STATE_PRLI:		return "Process Login";
	case PDB_STATE_PRLI_ACK:	return "Wait Process Login ACK";
	case PDB_STATE_LOGGED_IN:	return "Logged In";
	case PDB_STATE_PORT_UNAVAIL:	return "Port Unavailable";
	case PDB_STATE_PRLO:		return "Process Logout";
	case PDB_STATE_PRLO_ACK:	return "Wait Process Logout ACK";
	case PDB_STATE_PLOGO:		return "Port Logout";
	case PDB_STATE_PLOG_ACK:	return "Wait Port Logout ACK";
	default:
		sprintf(buf, "0x%x", pdb_state);
		return buf;
	}
}

#endif	/* _ISP_FREEBSD_H */
