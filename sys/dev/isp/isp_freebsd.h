/* $FreeBSD$ */
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

#define	ISP_PLATFORM_VERSION_MAJOR	2
#define	ISP_PLATFORM_VERSION_MINOR	0

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
#define	ISP2100_SCRLEN		0x100
#endif

#ifdef	SCSI_ISP_SCCLUN
#define	ISP2100_SCCLUN	1
#endif


#define	ISP_SCSI_XFER_T		struct scsi_xfer
struct isposinfo {
	char			name[8];
	int			unit, blocked;
	union {
		int	 	_seed;
		u_int16_t	_discovered[2];
	} un;
#define	seed		un._seed
#define	discovered	un._discovered
	struct scsi_link	_link;
	struct scsi_link	_link_b;
	struct scsi_xfer	*wqf, *wqt;
};
#define	MAXISPREQUEST		64

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#define	IDPRINTF(lev, x)	if (isp->isp_dblev >= lev) printf x
#define	PRINTF			printf

#define	MEMZERO			bzero
#define	MEMCPY(dst, src, amt)	bcopy((src), (dst), (amt))
#define	MemoryBarrier()

#define	DMA_MSW(x)	(((x) >> 16) & 0xffff)
#define	DMA_LSW(x)	(((x) & 0xffff))

#if	defined(SCSIDEBUG)
#define	DFLT_DBLEVEL		3
#define	CFGPRINTF		printf
#elif	defined(DIAGNOSTIC) || defined(DEBUG)
#define	DFLT_DBLEVEL		1
#define	CFGPRINTF		printf
#else
#define	DFLT_DBLEVEL		0
#define	CFGPRINTF		if (0)	printf
#endif

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
#define	XS_KINDOF_TAG(xs)	REQFLAG_OTAG


/*
 * These get turned into NetBSD midlayer codes
 */
#define	CMD_COMPLETE		100
#define	CMD_EAGAIN		101
#define	CMD_QUEUED		102
#define	CMD_RQLATER		103

#define	isp_name	isp_osinfo.name
#define	isp_unit	isp_osinfo.unit

#define	SCSI_QFULL	0x28

#define	SYS_DELAY(x)	DELAY(x)

#define	WATCH_INTERVAL	30

#define	FC_FW_READY_DELAY	(12 * 1000000)
#define	DEFAULT_LOOPID(x)	109
#define	DEFAULT_WWN(x)		(0x1000feeb00000000LL + (x)->isp_osinfo.seed)

extern void isp_attach(struct ispsoftc *);
extern void isp_uninit(struct ispsoftc *);

#define ISP_UNSWIZZLE_AND_COPY_PDBP(isp, dest, src)	\
        bcopy(src, dest, sizeof (isp_pdb_t))
#define ISP_SWIZZLE_ICB(a, b)
#ifdef	__sparc__
#define ISP_SWIZZLE_REQUEST(a, b)			\
	ISP_SBUSIFY_ISPHDR(a, &(b)->req_header);	\
        ISP_SBUSIFY_ISPREQ(a, b)
#define ISP_UNSWIZZLE_RESPONSE(a, b)			\
	ISP_SBUSIFY_ISPHDR(a, &(b)->req_header)
#else
#define ISP_SWIZZLE_REQUEST(a, b)
#define ISP_UNSWIZZLE_RESPONSE(a, b)
#endif
#define	ISP_SWIZZLE_SNS_REQ(a, b)
#define	ISP_UNSWIZZLE_SNS_RSP(a, b, c)

#define	INLINE	__inline
#include <dev/isp/isp_inline.h>

#endif	/* _ISP_FREEBSD_H */
