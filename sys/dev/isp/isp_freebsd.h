/* $FreeBSD$ */
/* $Id: isp_freebsd.h,v 1.9 1998/04/17 17:09:29 mjacob Exp $ */
/*
 * Qlogic ISP SCSI Host Adapter FreeBSD Wrapper Definitions (non CAM version)
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
#ifndef	_ISP_FREEBSD_H
#define	_ISP_FREEBSD_H

#include <pci.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsiconf.h>
#include <scsi/scsi_debug.h>

#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <sys/kernel.h>

#include <osreldate.h>

#define	ISP_SCSI_XFER_T		struct scsi_xfer
struct isposinfo {
	char			name[8];
	int			unit;
	struct scsi_link	_link;
#if	__FreeBSD_version >=	300001
	struct callout_handle	watchid;
#endif
};
/*
 * XXXX: UNTIL WE PUT CODE IN THAT CHECKS RETURNS FROM MALLOC
 * XXXX: FOR CONTIGOUS PAGES, WE LIMIT TO PAGE_SIZE THE SIZE
 * XXXX: OF MAILBOXES.
 */
#define	MAXISPREQUEST	64

#define	ISP_VERSION_STRING	\
	"Version 0.92 Alpha Fri Apr 17 09:34:03 PDT 1998"

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#define	PRINTF			printf
#define	IDPRINTF(lev, x)	if (isp->isp_dblev >= lev) printf x
#ifdef	SCSIDEBUG
#define	DFLT_DBLEVEL		2
#else
#define	DFLT_DBLEVEL		1
#endif

#define	ISP_LOCKVAL_DECL	int isp_spl_save
#define	ISP_UNLOCK		(void) splx(isp_spl_save)
#define	ISP_LOCK		isp_spl_save = splbio()
#define	IMASK			bio_imask

#define	XS_NULL(xs)		xs == NULL || xs->sc_link == NULL
#define	XS_ISP(xs)		(xs)->sc_link->adapter_softc
#define	XS_LUN(xs)		(xs)->sc_link->lun
#define	XS_TGT(xs)		(xs)->sc_link->target
#define	XS_RESID(xs)		(xs)->resid
#define	XS_XFRLEN(xs)		(xs)->datalen
#define	XS_CDBLEN(xs)		(xs)->cmdlen
#define	XS_CDBP(xs)		(xs)->cmd
#define	XS_STS(xs)		(xs)->status
#define	XS_TIME(xs)		(xs)->timeout
#define	XS_SNSP(xs)		(&(xs)->sense)
#define	XS_SNSLEN(xs)		(sizeof (xs)->sense)
#define	XS_SNSKEY(xs)		((xs)->sense.ext.extended.flags)

#define	HBA_NOERROR		XS_NOERROR
#define	HBA_BOTCH		XS_DRIVER_STUFFUP
#define	HBA_CMDTIMEOUT		XS_TIMEOUT
#define	HBA_SELTIMEOUT		XS_SELTIMEOUT
#define	HBA_TGTBSY		XS_BUSY
#define	HBA_BUSRESET		XS_DRIVER_STUFFUP
#define	HBA_ABORTED		XS_DRIVER_STUFFUP

#define	XS_SNS_IS_VALID(xs)	(xs)->error = XS_SENSE
#define	XS_IS_SNS_VALID(xs)	((xs)->error == XS_SENSE)

#define	XS_INITERR(xs)		(xs)->error = 0
#define	XS_SETERR(xs, v)	(xs)->error = v
#define	XS_ERR(xs)		(xs)->error
#define	XS_NOERR(xs)		(xs)->error == XS_NOERROR

#define	XS_CMD_DONE(xs)		(xs)->flags |= ITSDONE, scsi_done(xs)
#define	XS_IS_CMD_DONE(xs)	(((xs)->flags & ITSDONE) != 0)

#define	XS_POLLDCMD(xs)		((xs)->flags & SCSI_NOMASK)

#define	CMD_COMPLETE		COMPLETE
#define	CMD_EAGAIN		TRY_AGAIN_LATER
#define	CMD_QUEUED		SUCCESSFULLY_QUEUED



#define	isp_name	isp_osinfo.name


#define	SYS_DELAY(x)	DELAY(x)

#define	WATCH_INTERVAL		30
#if	__FreeBSD_version >=	300001
#define	START_WATCHDOG(f, s)	\
	(s)->isp_osinfo.watchid = timeout(f, s, WATCH_INTERVAL * hz), \
	s->isp_dogactive = 1
#define	STOP_WATCHDOG(f, s)	untimeout(f, s, (s)->isp_osinfo.watchid),\
	(s)->isp_dogactive = 0
#else
#define	START_WATCHDOG(f, s)	\
	timeout(f, s, WATCH_INTERVAL * hz), s->isp_dogactive = 1
#define	STOP_WATCHDOG(f, s)	untimeout(f, s), (s)->isp_dogactive = 0
#endif

#define	RESTART_WATCHDOG(f, s)	START_WATCHDOG(f, s)
extern void isp_attach __P((struct ispsoftc *));

#endif	/* _ISP_FREEBSD_H */
