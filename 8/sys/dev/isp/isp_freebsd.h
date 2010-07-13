/* $FreeBSD$ */
/*-
 * Qlogic ISP SCSI Host Adapter FreeBSD Wrapper Definitions
 *
 * Copyright (c) 1997-2008 by Matthew Jacob
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
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <sys/proc.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/stdarg.h>

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

#define	ISP_PLATFORM_VERSION_MAJOR	7
#define	ISP_PLATFORM_VERSION_MINOR	0

/*
 * Efficiency- get rid of SBus code && tests unless we need them.
 */
#ifdef __sparc64__
#define	ISP_SBUS_SUPPORTED	1
#else
#define	ISP_SBUS_SUPPORTED	0
#endif

#define	ISP_IFLAGS	INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE

#ifdef	ISP_TARGET_MODE
#define	ISP_TARGET_FUNCTIONS	1
#define	ATPDPSIZE	4096

#include <dev/isp/isp_target.h>

typedef struct {
	void *		next;
	uint32_t	orig_datalen;
	uint32_t	bytes_xfered;
	uint32_t	last_xframt;
	uint32_t	tag;
	uint32_t	lun;
	uint32_t	nphdl;
	uint32_t	sid;
	uint32_t	portid;
	uint32_t
			oxid	: 16,
			cdb0	: 8,
				: 1,
			dead	: 1,
			tattr	: 3,
			state	: 3;
} atio_private_data_t;
#define	ATPD_STATE_FREE			0
#define	ATPD_STATE_ATIO			1
#define	ATPD_STATE_CAM			2
#define	ATPD_STATE_CTIO			3
#define	ATPD_STATE_LAST_CTIO		4
#define	ATPD_STATE_PDON			5

typedef union inot_private_data inot_private_data_t;
union inot_private_data {
	inot_private_data_t *next;
	struct {
		isp_notify_t nt;	/* must be first! */
		uint8_t data[64];	/* sb QENTRY_LEN, but order of definitions is wrong */
		uint32_t tag_id, seq_id;
	} rd;
};

typedef struct tstate {
	SLIST_ENTRY(tstate) next;
	struct cam_path *owner;
	struct ccb_hdr_slist atios;
	struct ccb_hdr_slist inots;
	uint32_t hold;
	int atio_count;
	int inot_count;
	inot_private_data_t *	restart_queue;
	inot_private_data_t *	ntfree;
	inot_private_data_t	ntpool[ATPDPSIZE];
	atio_private_data_t *	atfree;
	atio_private_data_t	atpool[ATPDPSIZE];
} tstate_t;

#define	LUN_HASH_SIZE		32
#define	LUN_HASH_FUNC(lun)	((lun) & (LUN_HASH_SIZE - 1))

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

/*
 * Per channel information
 */
SLIST_HEAD(tslist, tstate);

struct isp_fc {
	struct cam_sim *sim;
	struct cam_path *path;
	struct ispsoftc *isp;
	struct proc *kproc;
	bus_dma_tag_t tdmat;
	bus_dmamap_t tdmap;
	uint64_t def_wwpn;
	uint64_t def_wwnn;
	uint32_t loop_down_time;
	uint32_t loop_down_limit;
	uint32_t gone_device_time;
	uint32_t
#ifdef	ISP_TARGET_MODE
#ifdef	ISP_INTERNAL_TARGET
		proc_active	: 1,
#endif
		tm_luns_enabled	: 1,
		tm_enable_defer	: 1,
		tm_enabled	: 1,
#endif
		simqfrozen	: 3,
		default_id	: 8,
		hysteresis	: 8,
		def_role	: 2,	/* default role */
		gdt_running	: 1,
		loop_dead	: 1,
		fcbsy		: 1,
		ready		: 1;
	struct callout ldt;	/* loop down timer */
	struct callout gdt;	/* gone device timer */
#ifdef	ISP_TARGET_MODE
	struct tslist lun_hash[LUN_HASH_SIZE];
#ifdef	ISP_INTERNAL_TARGET
	struct proc *		target_proc;
#endif
#endif
};

struct isp_spi {
	struct cam_sim *sim;
	struct cam_path *path;
	uint32_t
#ifdef	ISP_TARGET_MODE
#ifdef	ISP_INTERNAL_TARGET
		proc_active	: 1,
#endif
		tm_luns_enabled	: 1,
		tm_enable_defer	: 1,
		tm_enabled	: 1,
#endif
		simqfrozen	: 3,
		def_role	: 2,
		iid		: 4;
#ifdef	ISP_TARGET_MODE
	struct tslist lun_hash[LUN_HASH_SIZE];
#ifdef	ISP_INTERNAL_TARGET
	struct proc *		target_proc;
#endif
#endif
};

struct isposinfo {
	/*
	 * Linkage, locking, and identity
	 */
	struct mtx		lock;
	device_t		dev;
	struct cdev *		cdev;
	struct intr_config_hook	ehook;
	struct cam_devq *	devq;

	/*
	 * Firmware pointer
	 */
	const struct firmware *	fw;

	/*
	 * DMA related sdtuff
	 */
	bus_space_tag_t		bus_tag;
	bus_dma_tag_t		dmat;
	bus_space_handle_t	bus_handle;
	bus_dma_tag_t		cdmat;
	bus_dmamap_t		cdmap;

	/*
	 * Command and transaction related related stuff
	 */
	struct isp_pcmd *	pcmd_pool;
	struct isp_pcmd *	pcmd_free;

	uint32_t
#ifdef	ISP_TARGET_MODE
		tmwanted	: 1,
		tmbusy		: 1,
#else
				: 2,
#endif
		forcemulti	: 1,
		timer_active	: 1,
		autoconf	: 1,
		ehook_active	: 1,
		disabled	: 1,
		mbox_sleeping	: 1,
		mbox_sleep_ok	: 1,
		mboxcmd_done	: 1,
		mboxbsy		: 1;

	struct callout		tmo;	/* general timer */

	/*
	 * misc- needs to be sorted better XXXXXX
	 */
	int			framesize;
	int			exec_throttle;
	int			cont_max;

#ifdef	ISP_TARGET_MODE
	cam_status *		rptr;
#endif

	/*
	 * Per-type private storage...
	 */
	union {
		struct isp_fc *fc;
		struct isp_spi *spi;
		void *ptr;
	} pc;
};
#define	ISP_FC_PC(isp, chan)	(&(isp)->isp_osinfo.pc.fc[(chan)])
#define	ISP_SPI_PC(isp, chan)	(&(isp)->isp_osinfo.pc.spi[(chan)])
#define	ISP_GET_PC(isp, chan, tag, rslt)		\
	if (IS_SCSI(isp)) {				\
		rslt = ISP_SPI_PC(isp, chan)-> tag;	\
	} else {					\
		rslt = ISP_FC_PC(isp, chan)-> tag;	\
	}
#define	ISP_GET_PC_ADDR(isp, chan, tag, rp)		\
	if (IS_SCSI(isp)) {				\
		rp = &ISP_SPI_PC(isp, chan)-> tag;	\
	} else {					\
		rp = &ISP_FC_PC(isp, chan)-> tag;	\
	}
#define	ISP_SET_PC(isp, chan, tag, val)			\
	if (IS_SCSI(isp)) {				\
		ISP_SPI_PC(isp, chan)-> tag = val;	\
	} else {					\
		ISP_FC_PC(isp, chan)-> tag = val;	\
	}

#define	isp_lock	isp_osinfo.lock
#define	isp_bus_tag	isp_osinfo.bus_tag
#define	isp_bus_handle	isp_osinfo.bus_handle

/*
 * Locking macros...
 */
#define	ISP_LOCK(isp)	mtx_lock(&isp->isp_osinfo.lock)
#define	ISP_UNLOCK(isp)	mtx_unlock(&isp->isp_osinfo.lock)

/*
 * Required Macros/Defines
 */

#define	ISP_FC_SCRLEN		0x1000

#define	ISP_MEMZERO(a, b)	memset(a, 0, b)
#define	ISP_MEMCPY		memcpy
#define	ISP_SNPRINTF		snprintf
#define	ISP_DELAY		DELAY
#define	ISP_SLEEP(isp, x)	DELAY(x)

#ifndef	DIAGNOSTIC
#define	ISP_INLINE		__inline
#else
#define	ISP_INLINE
#endif

#define	NANOTIME_T		struct timespec
#define	GET_NANOTIME		nanotime
#define	GET_NANOSEC(x)		((x)->tv_sec * 1000000000 + (x)->tv_nsec)
#define	NANOTIME_SUB		isp_nanotime_sub

#define	MAXISPREQUEST(isp)	((IS_FC(isp) || IS_ULTRA2(isp))? 1024 : 256)

#define	MEMORYBARRIER(isp, type, offset, size)			\
switch (type) {							\
case SYNC_SFORDEV:						\
case SYNC_REQUEST:						\
	bus_dmamap_sync(isp->isp_osinfo.cdmat,			\
	   isp->isp_osinfo.cdmap, 				\
	   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);		\
	break;							\
case SYNC_SFORCPU:						\
case SYNC_RESULT:						\
	bus_dmamap_sync(isp->isp_osinfo.cdmat, 			\
	   isp->isp_osinfo.cdmap,				\
	   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);	\
	break;							\
case SYNC_REG:							\
	bus_space_barrier(isp->isp_osinfo.bus_tag,		\
	    isp->isp_osinfo.bus_handle, offset, size,		\
	    BUS_SPACE_BARRIER_READ);				\
	break;							\
default:							\
	break;							\
}

#define	MBOX_ACQUIRE			isp_mbox_acquire
#define	MBOX_WAIT_COMPLETE		isp_mbox_wait_complete
#define	MBOX_NOTIFY_COMPLETE		isp_mbox_notify_done
#define	MBOX_RELEASE			isp_mbox_release

#define	FC_SCRATCH_ACQUIRE		isp_fc_scratch_acquire
#define	FC_SCRATCH_RELEASE(isp, chan)	isp->isp_osinfo.pc.fc[chan].fcbsy = 0

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
#define XS_GET_DMA64_SEG(a, b, c)		\
{						\
	ispds64_t *d = a;			\
	bus_dma_segment_t *e = b;		\
	uint32_t f = c;				\
	e += f;					\
        d->ds_base = DMA_LO32(e->ds_addr);	\
        d->ds_basehi = DMA_HI32(e->ds_addr);	\
        d->ds_count = e->ds_len;		\
}
#define XS_GET_DMA_SEG(a, b, c)			\
{						\
	ispds_t *d = a;				\
	bus_dma_segment_t *e = b;		\
	uint32_t f = c;				\
	e += f;					\
        d->ds_base = DMA_LO32(e->ds_addr);	\
        d->ds_count = e->ds_len;		\
}
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
#define	XS_GET_RESID(ccb)	(ccb)->resid
#define	XS_SET_RESID(ccb, r)	(ccb)->resid = r
#define	XS_STSP(ccb)		(&(ccb)->scsi_status)
#define	XS_SNSP(ccb)		(&(ccb)->sense_data)

#define	XS_SNSLEN(ccb)		\
	imin((sizeof((ccb)->sense_data)), ccb->sense_len)

#define	XS_SNSKEY(ccb)		((ccb)->sense_data.flags & 0xf)
#define	XS_SNSASC(ccb)		((ccb)->sense_data.add_sense_code)
#define	XS_SNSASCQ(ccb)		((ccb)->sense_data.add_sense_code_qual)
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

#define	XS_SENSE_VALID(xs)	(((xs)->ccb_h.status & CAM_AUTOSNS_VALID) != 0)

#define	DEFAULT_FRAMESIZE(isp)		isp->isp_osinfo.framesize
#define	DEFAULT_EXEC_THROTTLE(isp)	isp->isp_osinfo.exec_throttle

#define	GET_DEFAULT_ROLE(isp, chan)	\
	(IS_FC(isp)? ISP_FC_PC(isp, chan)->def_role : ISP_SPI_PC(isp, chan)->def_role)
#define	SET_DEFAULT_ROLE(isp, chan, val)		\
	if (IS_FC(isp)) { 				\
		ISP_FC_PC(isp, chan)->def_role = val;	\
	} else {					\
		ISP_SPI_PC(isp, chan)->def_role = val;	\
	}

#define	DEFAULT_IID(isp, chan)		isp->isp_osinfo.pc.spi[chan].iid

#define	DEFAULT_LOOPID(x, chan)		isp->isp_osinfo.pc.fc[chan].default_id

#define DEFAULT_NODEWWN(isp, chan)  	isp_default_wwn(isp, chan, 0, 1)
#define DEFAULT_PORTWWN(isp, chan)  	isp_default_wwn(isp, chan, 0, 0)
#define ACTIVE_NODEWWN(isp, chan)   	isp_default_wwn(isp, chan, 1, 1)
#define ACTIVE_PORTWWN(isp, chan)   	isp_default_wwn(isp, chan, 1, 0)


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

/*
 * isp_osinfo definiitions && shorthand
 */
#define	SIMQFRZ_RESOURCE	0x1
#define	SIMQFRZ_LOOPDOWN	0x2
#define	SIMQFRZ_TIMED		0x4

#define	isp_dev		isp_osinfo.dev

/*
 * prototypes for isp_pci && isp_freebsd to share
 */
extern int isp_attach(ispsoftc_t *);
extern void isp_detach(ispsoftc_t *);
extern void isp_uninit(ispsoftc_t *);
extern uint64_t isp_default_wwn(ispsoftc_t *, int, int, int);

/*
 * driver global data
 */
extern int isp_announced;
extern int isp_fabric_hysteresis;
extern int isp_loop_down_limit;
extern int isp_gone_device_time;
extern int isp_quickboot_time;
extern int isp_autoconfig;

/*
 * Platform private flags
 */
#define	ISP_SPRIV_ERRSET	0x1
#define	ISP_SPRIV_DONE		0x8

#define	XS_CMD_S_DONE(sccb)	(sccb)->ccb_h.spriv_field0 |= ISP_SPRIV_DONE
#define	XS_CMD_C_DONE(sccb)	(sccb)->ccb_h.spriv_field0 &= ~ISP_SPRIV_DONE
#define	XS_CMD_DONE_P(sccb)	((sccb)->ccb_h.spriv_field0 & ISP_SPRIV_DONE)

#define	XS_CMD_S_CLEAR(sccb)	(sccb)->ccb_h.spriv_field0 = 0

/*
 * Platform Library Functions
 */
void isp_prt(ispsoftc_t *, int level, const char *, ...) __printflike(3, 4);
void isp_xs_prt(ispsoftc_t *, XS_T *, int level, const char *, ...) __printflike(4, 5);
uint64_t isp_nanotime_sub(struct timespec *, struct timespec *);
int isp_mbox_acquire(ispsoftc_t *);
void isp_mbox_wait_complete(ispsoftc_t *, mbreg_t *);
void isp_mbox_notify_done(ispsoftc_t *);
void isp_mbox_release(ispsoftc_t *);
int isp_fc_scratch_acquire(ispsoftc_t *, int);
int isp_mstohz(int);
void isp_platform_intr(void *);
void isp_common_dmateardown(ispsoftc_t *, struct ccb_scsiio *, uint32_t);

/*
 * Platform Version specific defines
 */
#define	BUS_DMA_ROOTARG(x)	bus_get_dma_tag(x)
#define	isp_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, z)	\
	bus_dma_tag_create(a, b, c, d, e, f, g, h, i, j, k, \
	busdma_lock_mutex, &isp->isp_osinfo.lock, z)

#define	isp_setup_intr	bus_setup_intr

#define	isp_sim_alloc(a, b, c, d, e, f, g, h)	\
	cam_sim_alloc(a, b, c, d, e, &(d)->isp_osinfo.lock, f, g, h)

/* Should be BUS_SPACE_MAXSIZE, but MAXPHYS is larger than BUS_SPACE_MAXSIZE */
#define ISP_NSEGS ((MAXPHYS / PAGE_SIZE) + 1)  

#define	ISP_PATH_PRT(i, l, p, ...)					\
	if ((l) == ISP_LOGALL || ((l)& (i)->isp_dblev) != 0) {		\
                xpt_print(p, __VA_ARGS__);				\
        }

/*
 * Platform specific inline functions
 */

/*
 * ISP General Library functions
 */

#include <dev/isp/isp_library.h>

#endif	/* _ISP_FREEBSD_H */
