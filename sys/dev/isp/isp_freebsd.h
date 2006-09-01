/* $FreeBSD$ */
/*-
 * Qlogic ISP SCSI Host Adapter FreeBSD Wrapper Definitions
 *
 * Copyright (c) 1997-2006 by Matthew Jacob
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
#ifndef	_ISP_FREEBSD_H
#define	_ISP_FREEBSD_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#if __FreeBSD_version < 500000
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#else
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#endif

#include <sys/proc.h>
#include <sys/bus.h>

#include <machine/bus.h>
#if __FreeBSD_version < 500000
#include <machine/clock.h>
#endif
#include <machine/cpu.h>

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

#if __FreeBSD_version < 500000
#define	ISP_PLATFORM_VERSION_MAJOR	4
#define	ISP_PLATFORM_VERSION_MINOR	17
#else
#define	ISP_PLATFORM_VERSION_MAJOR	5
#define	ISP_PLATFORM_VERSION_MINOR	9
#endif

/*
 * Efficiency- get rid of SBus code && tests unless we need them.
 */
#ifdef __sparc64__
#define	ISP_SBUS_SUPPORTED	1
#else
#define	ISP_SBUS_SUPPORTED	0
#endif

#define	HANDLE_LOOPSTATE_IN_OUTER_LAYERS	1
/* #define	ISP_SMPLOCK			1 */

#if __FreeBSD_version < 500000  
#define	ISP_IFLAGS	INTR_TYPE_CAM
#else
#ifdef	ISP_SMPLOCK
#define	ISP_IFLAGS	INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE
#else
#define	ISP_IFLAGS	INTR_TYPE_CAM | INTR_ENTROPY
#endif
#endif

#if __FreeBSD_version < 700000
typedef void ispfwfunc(int, int, int, uint16_t **);
#endif

#ifdef	ISP_TARGET_MODE
#define	ISP_TARGET_FUNCTIONS	1
#define	ATPDPSIZE	256
typedef struct {
	uint32_t	orig_datalen;
	uint32_t	bytes_xfered;
	uint32_t	last_xframt;
	uint32_t	tag	: 16,
			lun	: 13,	/* not enough */
			state	: 3;
} atio_private_data_t;
#define	ATPD_STATE_FREE			0
#define	ATPD_STATE_ATIO			1
#define	ATPD_STATE_CAM			2
#define	ATPD_STATE_CTIO			3
#define	ATPD_STATE_LAST_CTIO		4
#define	ATPD_STATE_PDON			5

typedef struct tstate {
	struct tstate *next;
	struct cam_path *owner;
	struct ccb_hdr_slist atios;
	struct ccb_hdr_slist inots;
	lun_id_t lun;
	int bus;
	uint32_t hold;
	int atio_count;
	int inot_count;
} tstate_t;

#define	LUN_HASH_SIZE			32
#define	LUN_HASH_FUNC(isp, port, lun)					\
	((IS_DUALBUS(isp)) ?						\
		(((lun) & ((LUN_HASH_SIZE >> 1) - 1)) << (port)) :	\
		((lun) & (LUN_HASH_SIZE - 1)))
#endif

struct isposinfo {
	struct ispsoftc *	next;
	uint64_t		default_port_wwn;
	uint64_t		default_node_wwn;
	uint32_t		default_id;
	device_t		dev;
	struct cam_sim		*sim;
	struct cam_path		*path;
	struct cam_sim		*sim2;
	struct cam_path		*path2;
	struct intr_config_hook	ehook;
	uint8_t
		disabled	: 1,
		fcbsy		: 1,
		ktmature	: 1,
		mboxwaiting	: 1,
		intsok		: 1,
		simqfrozen	: 3;
#if __FreeBSD_version >= 500000  
	struct firmware *	fw;
	struct mtx		lock;
	struct cv		kthread_cv;
#endif
	struct proc		*kproc;
	bus_dma_tag_t		cdmat;
	bus_dmamap_t		cdmap;
#define	isp_cdmat		isp_osinfo.cdmat
#define	isp_cdmap		isp_osinfo.cdmap
#ifdef	ISP_TARGET_MODE
#define	TM_WILDCARD_ENABLED	0x02
#define	TM_TMODE_ENABLED	0x01
	uint8_t			tmflags[2];	/* two busses */
#define	NLEACT	4
	union ccb *		leact[NLEACT];
	tstate_t		tsdflt[2];	/* two busses */
	tstate_t		*lun_hash[LUN_HASH_SIZE];
	atio_private_data_t	atpdp[ATPDPSIZE];
#endif
};

#define	isp_lock	isp_osinfo.lock

/*
 * Locking macros...
 */

#ifdef	ISP_SMPLOCK
#define	ISP_LOCK(x)		mtx_lock(&(x)->isp_lock)
#define	ISP_UNLOCK(x)		mtx_unlock(&(x)->isp_lock)
#define	ISPLOCK_2_CAMLOCK(isp)	\
	mtx_unlock(&(isp)->isp_lock); mtx_lock(&Giant)
#define	CAMLOCK_2_ISPLOCK(isp)	\
	mtx_unlock(&Giant); mtx_lock(&(isp)->isp_lock)
#else
#define	ISP_LOCK(x)		do { } while (0)
#define	ISP_UNLOCK(x)		do { } while (0)
#define	ISPLOCK_2_CAMLOCK(isp)	do { } while (0)
#define	CAMLOCK_2_ISPLOCK(isp)	do { } while (0)
#endif

/*
 * Required Macros/Defines
 */

#define	ISP2100_SCRLEN		0x800

#define	MEMZERO(a, b)		memset(a, 0, b)
#define	MEMCPY			memcpy
#define	SNPRINTF		snprintf
#define	USEC_DELAY		DELAY
#define	USEC_SLEEP(isp, x)		\
	if (isp->isp_osinfo.intsok)	\
		ISP_UNLOCK(isp);	\
	DELAY(x);			\
	if (isp->isp_osinfo.intsok)	\
		ISP_LOCK(isp)

#define	NANOTIME_T		struct timespec
#define	GET_NANOTIME		nanotime
#define	GET_NANOSEC(x)		((x)->tv_sec * 1000000000 + (x)->tv_nsec)
#define	NANOTIME_SUB		nanotime_sub

#define	MAXISPREQUEST(isp)	((IS_FC(isp) || IS_ULTRA2(isp))? 1024 : 256)

#define	MEMORYBARRIER(isp, type, offset, size)			\
switch (type) {							\
case SYNC_SFORDEV:						\
case SYNC_REQUEST:						\
	bus_dmamap_sync(isp->isp_cdmat, isp->isp_cdmap, 	\
	   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);		\
	break;							\
case SYNC_SFORCPU:						\
case SYNC_RESULT:						\
	bus_dmamap_sync(isp->isp_cdmat, isp->isp_cdmap,		\
	   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);	\
	break;							\
default:							\
	break;							\
}

#define	MBOX_ACQUIRE(isp)
#define	MBOX_WAIT_COMPLETE		isp_mbox_wait_complete
#define	MBOX_NOTIFY_COMPLETE(isp)	\
	if (isp->isp_osinfo.mboxwaiting) { \
		isp->isp_osinfo.mboxwaiting = 0; \
		wakeup(&isp->isp_mbxworkp); \
	} \
	isp->isp_mboxbsy = 0
#define	MBOX_RELEASE(isp)

#define	FC_SCRATCH_ACQUIRE(isp)						\
	if (isp->isp_osinfo.fcbsy) {					\
		isp_prt(isp, ISP_LOGWARN,				\
		    "FC scratch area busy (line %d)!", __LINE__);	\
	} else								\
		isp->isp_osinfo.fcbsy = 1
#define	FC_SCRATCH_RELEASE(isp)		 isp->isp_osinfo.fcbsy = 0

#ifndef	SCSI_GOOD
#define	SCSI_GOOD	SCSI_STATUS_OK
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

#define	XS_T			struct ccb_scsiio
#define	XS_DMA_ADDR_T		bus_addr_t
#define	XS_ISP(ccb)		((ispsoftc_t *) (ccb)->ccb_h.spriv_ptr1)
#define	XS_CHANNEL(ccb)		cam_sim_bus(xpt_path_sim((ccb)->ccb_h.path))
#define	XS_TGT(ccb)		(ccb)->ccb_h.target_id
#define	XS_LUN(ccb)		(ccb)->ccb_h.target_lun

#define	XS_CDBP(ccb)	\
	(((ccb)->ccb_h.flags & CAM_CDB_POINTER)? \
	 (ccb)->cdb_io.cdb_ptr : (ccb)->cdb_io.cdb_bytes)

#define	XS_CDBLEN(ccb)		(ccb)->cdb_len
#define	XS_XFRLEN(ccb)		(ccb)->dxfer_len
#define	XS_TIME(ccb)		(ccb)->ccb_h.timeout
#define	XS_RESID(ccb)		(ccb)->resid
#define	XS_STSP(ccb)		(&(ccb)->scsi_status)
#define	XS_SNSP(ccb)		(&(ccb)->sense_data)

#define	XS_SNSLEN(ccb)		\
	imin((sizeof((ccb)->sense_data)), ccb->sense_len)

#define	XS_SNSKEY(ccb)		((ccb)->sense_data.flags & 0xf)
#define	XS_TAG_P(ccb)	\
	(((ccb)->ccb_h.flags & CAM_TAG_ACTION_VALID) && \
	 (ccb)->tag_action != CAM_TAG_ACTION_NONE)

#define	XS_TAG_TYPE(ccb)	\
	((ccb->tag_action == MSG_SIMPLE_Q_TAG)? REQFLAG_STAG : \
	 ((ccb->tag_action == MSG_HEAD_OF_Q_TAG)? REQFLAG_HTAG : REQFLAG_OTAG))
		

#define	XS_SETERR(ccb, v)	(ccb)->ccb_h.status &= ~CAM_STATUS_MASK, \
				(ccb)->ccb_h.status |= v, \
				(ccb)->ccb_h.spriv_field0 |= ISP_SPRIV_ERRSET

#	define	HBA_NOERROR		CAM_REQ_INPROG
#	define	HBA_BOTCH		CAM_UNREC_HBA_ERROR
#	define	HBA_CMDTIMEOUT		CAM_CMD_TIMEOUT
#	define	HBA_SELTIMEOUT		CAM_SEL_TIMEOUT
#	define	HBA_TGTBSY		CAM_SCSI_STATUS_ERROR
#	define	HBA_BUSRESET		CAM_SCSI_BUS_RESET
#	define	HBA_ABORTED		CAM_REQ_ABORTED
#	define	HBA_DATAOVR		CAM_DATA_RUN_ERR
#	define	HBA_ARQFAIL		CAM_AUTOSENSE_FAIL


#define	XS_ERR(ccb)		((ccb)->ccb_h.status & CAM_STATUS_MASK)

#define	XS_NOERR(ccb)		\
	(((ccb)->ccb_h.spriv_field0 & ISP_SPRIV_ERRSET) == 0 || \
	 ((ccb)->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG)

#define	XS_INITERR(ccb)		\
	XS_SETERR(ccb, CAM_REQ_INPROG), (ccb)->ccb_h.spriv_field0 = 0

#define	XS_SAVE_SENSE(xs, sp)				\
	(xs)->ccb_h.status |= CAM_AUTOSNS_VALID,	\
	memcpy(&(xs)->sense_data, sp->req_sense_data,	\
	    imin(XS_SNSLEN(xs), sp->req_sense_len))

#define	XS_SET_STATE_STAT(a, b, c)

#define	DEFAULT_IID(x)		(isp)->isp_osinfo.default_id
#define	DEFAULT_LOOPID(x)	(isp)->isp_osinfo.default_id
#define	DEFAULT_NODEWWN(isp)	(isp)->isp_osinfo.default_node_wwn
#define	DEFAULT_PORTWWN(isp)	(isp)->isp_osinfo.default_port_wwn
#define	ISP_NODEWWN(isp)	FCPARAM(isp)->isp_nodewwn
#define	ISP_PORTWWN(isp)	FCPARAM(isp)->isp_portwwn

#if	BYTE_ORDER == BIG_ENDIAN
#ifdef	ISP_SBUS_SUPPORTED
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)				\
	*(d) = (isp->isp_bustype == ISP_BT_SBUS)? s : bswap16(s)
#define	ISP_IOXPUT_32(isp, s, d)				\
	*(d) = (isp->isp_bustype == ISP_BT_SBUS)? s : bswap32(s)
#define	ISP_IOXGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOXGET_16(isp, s, d)				\
	d = (isp->isp_bustype == ISP_BT_SBUS)?			\
	*((uint16_t *)s) : bswap16(*((uint16_t *)s))
#define	ISP_IOXGET_32(isp, s, d)				\
	d = (isp->isp_bustype == ISP_BT_SBUS)?			\
	*((uint32_t *)s) : bswap32(*((uint32_t *)s))
#else
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = bswap16(s)
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = bswap32(s)
#define	ISP_IOXGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOXGET_16(isp, s, d)	d = bswap16(*((uint16_t *)s))
#define	ISP_IOXGET_32(isp, s, d)	d = bswap32(*((uint32_t *)s))
#endif
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)	*rp = bswap16(*rp)
#else
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = s
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = s
#define	ISP_IOXGET_8(isp, s, d)		d = *(s)
#define	ISP_IOXGET_16(isp, s, d)	d = *(s)
#define	ISP_IOXGET_32(isp, s, d)	d = *(s)
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)
#endif

/*
 * Includes of common header files
 */

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#ifdef	ISP_TARGET_MODE
#include <dev/isp/isp_tpublic.h>
#endif

void isp_prt(ispsoftc_t *, int level, const char *, ...)
	__printflike(3, 4);
/*
 * isp_osinfo definiitions && shorthand
 */
#define	SIMQFRZ_RESOURCE	0x1
#define	SIMQFRZ_LOOPDOWN	0x2
#define	SIMQFRZ_TIMED		0x4

#define	isp_sim		isp_osinfo.sim
#define	isp_path	isp_osinfo.path
#define	isp_sim2	isp_osinfo.sim2
#define	isp_path2	isp_osinfo.path2
#define	isp_dev		isp_osinfo.dev

/*
 * prototypes for isp_pci && isp_freebsd to share
 */
extern void isp_attach(ispsoftc_t *);
extern void isp_uninit(ispsoftc_t *);

/*
 * driver global data
 */
extern int isp_announced;

/*
 * Platform private flags
 */
#define	ISP_SPRIV_ERRSET	0x1
#define	ISP_SPRIV_INWDOG	0x2
#define	ISP_SPRIV_GRACE		0x4
#define	ISP_SPRIV_DONE		0x8

#define	XS_CMD_S_WDOG(sccb)	(sccb)->ccb_h.spriv_field0 |= ISP_SPRIV_INWDOG
#define	XS_CMD_C_WDOG(sccb)	(sccb)->ccb_h.spriv_field0 &= ~ISP_SPRIV_INWDOG
#define	XS_CMD_WDOG_P(sccb)	((sccb)->ccb_h.spriv_field0 & ISP_SPRIV_INWDOG)

#define	XS_CMD_S_GRACE(sccb)	(sccb)->ccb_h.spriv_field0 |= ISP_SPRIV_GRACE
#define	XS_CMD_C_GRACE(sccb)	(sccb)->ccb_h.spriv_field0 &= ~ISP_SPRIV_GRACE
#define	XS_CMD_GRACE_P(sccb)	((sccb)->ccb_h.spriv_field0 & ISP_SPRIV_GRACE)

#define	XS_CMD_S_DONE(sccb)	(sccb)->ccb_h.spriv_field0 |= ISP_SPRIV_DONE
#define	XS_CMD_C_DONE(sccb)	(sccb)->ccb_h.spriv_field0 &= ~ISP_SPRIV_DONE
#define	XS_CMD_DONE_P(sccb)	((sccb)->ccb_h.spriv_field0 & ISP_SPRIV_DONE)

#define	XS_CMD_S_CLEAR(sccb)	(sccb)->ccb_h.spriv_field0 = 0

/*
 * Platform specific inline functions
 */

static __inline void isp_mbox_wait_complete(ispsoftc_t *);
static __inline void
isp_mbox_wait_complete(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.intsok) {
		int lim = ((isp->isp_mbxwrk0)? 120 : 20) * hz;
		isp->isp_osinfo.mboxwaiting = 1;
#ifdef	ISP_SMPLOCK
		(void) msleep(&isp->isp_mbxworkp,
		    &isp->isp_lock, PRIBIO, "isp_mboxwaiting", lim);
#else
		(void) tsleep(&isp->isp_mbxworkp,
		    PRIBIO, "isp_mboxwaiting", lim);
#endif
		if (isp->isp_mboxbsy != 0) {
			isp_prt(isp, ISP_LOGWARN,
			    "Interrupting Mailbox Command (0x%x) Timeout",
			    isp->isp_lastmbxcmd);
			isp->isp_mboxbsy = 0;
		}
		isp->isp_osinfo.mboxwaiting = 0;
	} else {
		int lim = ((isp->isp_mbxwrk0)? 240 : 60) * 10000;
		int j;
		for (j = 0; j < lim; j++) {
			uint16_t isr, sema, mbox;
			if (isp->isp_mboxbsy == 0) {
				break;
			}
			if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
				isp_intr(isp, isr, sema, mbox);
				if (isp->isp_mboxbsy == 0) {
					break;
				}
			}
			USEC_DELAY(500);
		}
		if (isp->isp_mboxbsy != 0) {
			isp_prt(isp, ISP_LOGWARN,
			    "Polled Mailbox Command (0x%x) Timeout",
			    isp->isp_lastmbxcmd);
		}
	}
}

static __inline uint64_t nanotime_sub(struct timespec *, struct timespec *);
static __inline uint64_t
nanotime_sub(struct timespec *b, struct timespec *a)
{
	uint64_t elapsed;
	struct timespec x = *b;
	timespecsub(&x, a);
	elapsed = GET_NANOSEC(&x);
	if (elapsed == 0)
		elapsed++;
	return (elapsed);
}

static __inline char *strncat(char *, const char *, size_t);
static __inline char *
strncat(char *d, const char *s, size_t c)
{
        char *t = d;

        if (c) {
                while (*d)
                        d++;
                while ((*d++ = *s++)) {
                        if (--c == 0) {
                                *d = '\0';
                                break;
                        }
                }
        }
        return (t);
}

/*
 * ISP Library functions
 */

#include <dev/isp/isp_library.h>

#endif	/* _ISP_FREEBSD_H */
