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


#if __FreeBSD_version < 500000  
#define	ISP_IFLAGS	INTR_TYPE_CAM
#elif __FreeBSD_version < 700037
#define	ISP_IFLAGS	INTR_TYPE_CAM | INTR_ENTROPY
#else
#define	ISP_IFLAGS	INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE
#endif

#if __FreeBSD_version < 700000
typedef void ispfwfunc(int, int, int, const void **);
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

/*
 * Per command info.
 */
struct isp_pcmd {
	struct isp_pcmd *	next;
	bus_dmamap_t 		dmap;	/* dma map for this command */
	struct ispsoftc *	isp;	/* containing isp */
	struct callout		wdog;	/* watchdog timer */
};
#define	ISP_PCMD(ccb)		(ccb)->ccb_h.spriv_ptr1
#define	PISP_PCMD(ccb)		((struct isp_pcmd *)ISP_PCMD(ccb))

struct isposinfo {
	struct ispsoftc *	next;
	bus_space_tag_t		bus_tag;
	bus_space_handle_t	bus_handle;
	bus_dma_tag_t		dmat;
	uint64_t		default_port_wwn;
	uint64_t		default_node_wwn;
	uint32_t		default_id;
	device_t		dev;
	struct cam_sim		*sim;
	struct cam_path		*path;
	struct cam_sim		*sim2;
	struct cam_path		*path2;
	struct intr_config_hook	ehook;
	uint32_t		loop_down_time;
	uint32_t		loop_down_limit;
	uint32_t		gone_device_time;
	uint32_t		: 5,
		simqfrozen	: 3,
		hysteresis	: 8,
		gdt_running	: 1,
		ldt_running	: 1,
		disabled	: 1,
		fcbsy		: 1,
		mbox_sleeping	: 1,
		mbox_sleep_ok	: 1,
		mboxcmd_done	: 1,
		mboxbsy		: 1;
	struct callout		ldt;	/* loop down timer */
	struct callout		gdt;	/* gone device timer */
#if __FreeBSD_version < 500000  
	uint32_t		splcount;
	uint32_t		splsaved;
#else
	struct mtx		lock;
	const struct firmware *	fw;
	union {
		struct {
			char wwnn[19];
			char wwpn[19];
		} fc;
	} sysctl_info;
#endif
	struct proc		*kproc;
	bus_dma_tag_t		cdmat;
	bus_dmamap_t		cdmap;
#define	isp_cdmat		isp_osinfo.cdmat
#define	isp_cdmap		isp_osinfo.cdmap
	/*
	 * Per command information.
	 */
	struct isp_pcmd *	pcmd_pool;
	struct isp_pcmd *	pcmd_free;

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
#define	ISP_KT_WCHAN(isp)	(&(isp)->isp_osinfo.kproc)

#define	isp_lock	isp_osinfo.lock
#define	isp_bus_tag	isp_osinfo.bus_tag
#define	isp_bus_handle	isp_osinfo.bus_handle

/*
 * Locking macros...
 */
#if __FreeBSD_version < 500000
#define	ISP_LOCK(isp)						\
	if (isp->isp_osinfo.splcount++ == 0) {			\
		 isp->isp_osinfo.splsaved = splcam();		\
	}
#define	ISP_UNLOCK(isp)						\
	if (isp->isp_osinfo.splcount > 1) {			\
		isp->isp_osinfo.splcount--;			\
	} else {						\
		isp->isp_osinfo.splcount = 0;			\
		splx(isp->isp_osinfo.splsaved);			\
	}
#elif	__FreeBSD_version < 700037
#define	ISP_LOCK(isp)	do {} while (0)
#define	ISP_UNLOCK(isp)	do {} while (0)
#else
#define	ISP_LOCK(isp)	mtx_lock(&isp->isp_osinfo.lock)
#define	ISP_UNLOCK(isp)	mtx_unlock(&isp->isp_osinfo.lock)
#endif

/*
 * Required Macros/Defines
 */

#define	ISP2100_SCRLEN		0x1000

#define	MEMZERO(a, b)		memset(a, 0, b)
#define	MEMCPY			memcpy
#define	SNPRINTF		snprintf
#define	USEC_DELAY		DELAY
#define	USEC_SLEEP(isp, x)	DELAY(x)

#define	NANOTIME_T		struct timespec
#define	GET_NANOTIME		nanotime
#define	GET_NANOSEC(x)		((x)->tv_sec * 1000000000 + (x)->tv_nsec)
#define	NANOTIME_SUB		isp_nanotime_sub

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
case SYNC_REG:							\
	bus_space_barrier(isp->isp_bus_tag,			\
	    isp->isp_bus_handle, offset, size, 			\
	    BUS_SPACE_BARRIER_READ);				\
	break;							\
default:							\
	break;							\
}

#define	MBOX_ACQUIRE			isp_mbox_acquire
#define	MBOX_WAIT_COMPLETE		isp_mbox_wait_complete
#define	MBOX_NOTIFY_COMPLETE		isp_mbox_notify_done
#define	MBOX_RELEASE			isp_mbox_release

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
#define	XS_ISP(ccb)		cam_sim_softc(xpt_path_sim((ccb)->ccb_h.path))
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

#define	XS_SAVE_SENSE(xs, sense_ptr, sense_len)		\
	(xs)->ccb_h.status |= CAM_AUTOSNS_VALID;	\
	memcpy(&(xs)->sense_data, sense_ptr, imin(XS_SNSLEN(xs), sense_len))

#define	XS_SET_STATE_STAT(a, b, c)

#define	DEFAULT_IID(x)		(isp)->isp_osinfo.default_id
#define	DEFAULT_LOOPID(x)	(isp)->isp_osinfo.default_id
#define	DEFAULT_NODEWWN(isp)	(isp)->isp_osinfo.default_node_wwn
#define	DEFAULT_PORTWWN(isp)	(isp)->isp_osinfo.default_port_wwn
#define	ISP_NODEWWN(isp)	FCPARAM(isp)->isp_wwnn_nvram
#define	ISP_PORTWWN(isp)	FCPARAM(isp)->isp_wwpn_nvram


#if __FreeBSD_version < 500000  
#if _BYTE_ORDER == _LITTLE_ENDIAN
#define	bswap16		htobe16
#define	bswap32		htobe32
#else
#define	bswap16		htole16
#define	bswap32		htole32
#endif
#endif

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

#else	/* ISP_SBUS_SUPPORTED */
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = bswap16(s)
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = bswap32(s)
#define	ISP_IOXGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOXGET_16(isp, s, d)	d = bswap16(*((uint16_t *)s))
#define	ISP_IOXGET_32(isp, s, d)	d = bswap32(*((uint32_t *)s))
#endif
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)	*rp = bswap16(*rp)
#define	ISP_SWIZZLE_NVRAM_LONG(isp, rp)	*rp = bswap32(*rp)

#define	ISP_IOZGET_8(isp, s, d)		d = (*((uint8_t *)s))
#define	ISP_IOZGET_16(isp, s, d)	d = (*((uint16_t *)s))
#define	ISP_IOZGET_32(isp, s, d)	d = (*((uint32_t *)s))
#define	ISP_IOZPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOZPUT_16(isp, s, d)	*(d) = s
#define	ISP_IOZPUT_32(isp, s, d)	*(d) = s


#else
#define	ISP_IOXPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOXPUT_16(isp, s, d)	*(d) = s
#define	ISP_IOXPUT_32(isp, s, d)	*(d) = s
#define	ISP_IOXGET_8(isp, s, d)		d = *(s)
#define	ISP_IOXGET_16(isp, s, d)	d = *(s)
#define	ISP_IOXGET_32(isp, s, d)	d = *(s)
#define	ISP_SWIZZLE_NVRAM_WORD(isp, rp)
#define	ISP_SWIZZLE_NVRAM_LONG(isp, rp)

#define	ISP_IOZPUT_8(isp, s, d)		*(d) = s
#define	ISP_IOZPUT_16(isp, s, d)	*(d) = bswap16(s)
#define	ISP_IOZPUT_32(isp, s, d)	*(d) = bswap32(s)

#define	ISP_IOZGET_8(isp, s, d)		d = (*((uint8_t *)(s)))
#define	ISP_IOZGET_16(isp, s, d)	d = bswap16(*((uint16_t *)(s)))
#define	ISP_IOZGET_32(isp, s, d)	d = bswap32(*((uint32_t *)(s)))

#endif

#define	ISP_SWAP16(isp, s)	bswap16(s)
#define	ISP_SWAP32(isp, s)	bswap32(s)

/*
 * Includes of common header files
 */

#include <dev/isp/ispreg.h>
#include <dev/isp/ispvar.h>
#include <dev/isp/ispmbox.h>

#ifdef	ISP_TARGET_MODE
#include <dev/isp/isp_tpublic.h>
#endif

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
extern int isp_fabric_hysteresis;
extern int isp_loop_down_limit;
extern int isp_gone_device_time;
extern int isp_quickboot_time;

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
 * Platform Library Functions
 */
void isp_prt(ispsoftc_t *, int level, const char *, ...) __printflike(3, 4);
uint64_t isp_nanotime_sub(struct timespec *, struct timespec *);
int isp_mbox_acquire(ispsoftc_t *);
void isp_mbox_wait_complete(ispsoftc_t *, mbreg_t *);
void isp_mbox_notify_done(ispsoftc_t *);
void isp_mbox_release(ispsoftc_t *);
int isp_mstohz(int);
void isp_platform_intr(void *);
void isp_common_dmateardown(ispsoftc_t *, struct ccb_scsiio *, uint32_t);

/*
 * Platform Version specific defines
 */
#if __FreeBSD_version < 500000  
#define	BUS_DMA_ROOTARG(x)	NULL
#define	isp_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)	\
	bus_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)
#elif	__FreeBSD_version < 700020
#define	BUS_DMA_ROOTARG(x)	NULL
#define	isp_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)	\
	bus_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, \
	busdma_lock_mutex, &Giant, z)
#elif __FreeBSD_version < 700037
#define	BUS_DMA_ROOTARG(x)	bus_get_dma_tag(x)
#define	isp_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)	\
	bus_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, \
	busdma_lock_mutex, &Giant, z)
#else
#define	BUS_DMA_ROOTARG(x)	bus_get_dma_tag(x)
#define	isp_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)	\
	bus_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, \
	busdma_lock_mutex, &isp->isp_osinfo.lock, z)
#endif

#if __FreeBSD_version < 700031
#define	isp_setup_intr(d, i, f, U, if, ifa, hp)	\
	bus_setup_intr(d, i, f, if, ifa, hp)
#else
#define	isp_setup_intr	bus_setup_intr
#endif

#if __FreeBSD_version < 500000
#define	isp_sim_alloc	cam_sim_alloc
#define	isp_callout_init(x)	callout_init(x)
#elif	__FreeBSD_version < 700037
#define	isp_callout_init(x)	callout_init(x, 0)
#define	isp_sim_alloc		cam_sim_alloc
#else
#define	isp_callout_init(x)	callout_init(x, 1)
#define	isp_sim_alloc(a, b, c, d, e, f, g, h)	\
	cam_sim_alloc(a, b, c, d, e, &(d)->isp_osinfo.lock, f, g, h)
#endif

/* Should be BUS_SPACE_MAXSIZE, but MAXPHYS is larger than BUS_SPACE_MAXSIZE */
#define ISP_NSEGS ((MAXPHYS / PAGE_SIZE) + 1)  

/*
 * Platform specific inline functions
 */
static __inline int isp_get_pcmd(ispsoftc_t *, union ccb *);
static __inline void isp_free_pcmd(ispsoftc_t *, union ccb *);

static __inline int
isp_get_pcmd(ispsoftc_t *isp, union ccb *ccb)
{
	ISP_PCMD(ccb) = isp->isp_osinfo.pcmd_free;
	if (ISP_PCMD(ccb) == NULL) {
		return (-1);
	}
	isp->isp_osinfo.pcmd_free = ((struct isp_pcmd *)ISP_PCMD(ccb))->next;
	return (0);
}

static __inline void
isp_free_pcmd(ispsoftc_t *isp, union ccb *ccb)
{
	((struct isp_pcmd *)ISP_PCMD(ccb))->next = isp->isp_osinfo.pcmd_free;
	isp->isp_osinfo.pcmd_free = ISP_PCMD(ccb);
	ISP_PCMD(ccb) = NULL;
}


/*
 * ISP General Library functions
 */

#include <dev/isp/isp_library.h>

#endif	/* _ISP_FREEBSD_H */
