/* $FreeBSD: src/sys/dev/isp/ispvar.h,v 1.25.2.3 2000/09/21 21:19:08 mjacob Exp $ */
/*
 * Soft Definitions for for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 1998, 1999, 2000 by Matthew Jacob
 * NASA/Ames Research Center
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
 *
 */

#ifndef	_ISPVAR_H
#define	_ISPVAR_H

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/ispmbox.h>
#ifdef	ISP_TARGET_MODE
#include <dev/ic/isp_target.h>
#include <dev/ic/isp_tpublic.h>
#endif
#endif
#ifdef	__FreeBSD__
#include <dev/isp/ispmbox.h>
#ifdef	ISP_TARGET_MODE
#include <dev/isp/isp_target.h>
#include <dev/isp/isp_tpublic.h>
#endif
#endif
#ifdef	__linux__
#include "ispmbox.h"
#ifdef	ISP_TARGET_MODE
#include "isp_target.h"
#include "isp_tpublic.h"
#endif
#endif

#define	ISP_CORE_VERSION_MAJOR	2
#define	ISP_CORE_VERSION_MINOR	0

/*
 * Vector for bus specific code to provide specific services.
 */
struct ispsoftc;
struct ispmdvec {
	u_int16_t	(*dv_rd_reg) __P((struct ispsoftc *, int));
	void		(*dv_wr_reg) __P((struct ispsoftc *, int, u_int16_t));
	int		(*dv_mbxdma) __P((struct ispsoftc *));
	int		(*dv_dmaset) __P((struct ispsoftc *,
		XS_T *, ispreq_t *, u_int16_t *, u_int16_t));
	void		(*dv_dmaclr)
		__P((struct ispsoftc *, XS_T *, u_int32_t));
	void		(*dv_reset0) __P((struct ispsoftc *));
	void		(*dv_reset1) __P((struct ispsoftc *));
	void		(*dv_dregs) __P((struct ispsoftc *, const char *));
	const u_int16_t	*dv_ispfw;	/* ptr to f/w */
	u_int16_t	dv_conf1;
	u_int16_t	dv_clock;	/* clock frequency */
};

/*
 * Overall parameters
 */
#define	MAX_TARGETS	16
#ifdef	ISP2100_FABRIC
#define	MAX_FC_TARG	256
#else
#define	MAX_FC_TARG	126
#endif

#define	ISP_MAX_TARGETS(isp)	(IS_FC(isp)? MAX_FC_TARG : MAX_TARGETS)
#define	ISP_MAX_LUNS(isp)	(isp)->isp_maxluns


/*
 * Macros to access ISP registers through bus specific layers-
 * mostly wrappers to vector through the mdvec structure.
 */

#define	ISP_READ(isp, reg)	\
	(*(isp)->isp_mdvec->dv_rd_reg)((isp), (reg))

#define	ISP_WRITE(isp, reg, val)	\
	(*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), (val))

#define	ISP_MBOXDMASETUP(isp)	\
	(*(isp)->isp_mdvec->dv_mbxdma)((isp))

#define	ISP_DMASETUP(isp, xs, req, iptrp, optr)	\
	(*(isp)->isp_mdvec->dv_dmaset)((isp), (xs), (req), (iptrp), (optr))

#define	ISP_DMAFREE(isp, xs, hndl)	\
	if ((isp)->isp_mdvec->dv_dmaclr) \
	    (*(isp)->isp_mdvec->dv_dmaclr)((isp), (xs), (hndl))

#define	ISP_RESET0(isp)	\
	if ((isp)->isp_mdvec->dv_reset0) (*(isp)->isp_mdvec->dv_reset0)((isp))
#define	ISP_RESET1(isp)	\
	if ((isp)->isp_mdvec->dv_reset1) (*(isp)->isp_mdvec->dv_reset1)((isp))
#define	ISP_DUMPREGS(isp, m)	\
	if ((isp)->isp_mdvec->dv_dregs) (*(isp)->isp_mdvec->dv_dregs)((isp),(m))

#define	ISP_SETBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) | (val))

#define	ISP_CLRBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) & ~(val))

/*
 * The MEMORYBARRIER macro is defined per platform (to provide synchronization
 * on Request and Response Queues, Scratch DMA areas, and Registers)
 *
 * Defined Memory Barrier Synchronization Types
 */
#define	SYNC_REQUEST	0	/* request queue synchronization */
#define	SYNC_RESULT	1	/* result queue synchronization */
#define	SYNC_SFORDEV	2	/* scratch, sync for ISP */
#define	SYNC_SFORCPU	3	/* scratch, sync for CPU */
#define	SYNC_REG	4	/* for registers */

/*
 * Request/Response Queue defines and macros.
 * The maximum is defined per platform (and can be based on board type).
 */
/* This is the size of a queue entry (request and response) */
#define	QENTRY_LEN			64
/* Both request and result queue length must be a power of two */
#define	RQUEST_QUEUE_LEN(x)		MAXISPREQUEST(x)
#define	RESULT_QUEUE_LEN(x)		\
	(((MAXISPREQUEST(x) >> 2) < 64)? 64 : MAXISPREQUEST(x) >> 2)
#define	ISP_QUEUE_ENTRY(q, idx)		((q) + ((idx) * QENTRY_LEN))
#define	ISP_QUEUE_SIZE(n)		((n) * QENTRY_LEN)
#define	ISP_NXT_QENTRY(idx, qlen)	(((idx) + 1) & ((qlen)-1))
#define	ISP_QFREE(in, out, qlen)	\
	((in == out)? (qlen - 1) : ((in > out)? \
	((qlen - 1) - (in - out)) : (out - in - 1)))
#define	ISP_QAVAIL(isp)	\
	ISP_QFREE(isp->isp_reqidx, isp->isp_reqodx, RQUEST_QUEUE_LEN(isp))

#define	ISP_ADD_REQUEST(isp, iptr)	\
	MEMORYBARRIER(isp, SYNC_REQUEST, iptr, QENTRY_LEN); \
	ISP_WRITE(isp, INMAILBOX4, iptr); \
	isp->isp_reqidx = iptr

/*
 * SCSI Specific Host Adapter Parameters- per bus, per target
 */

typedef struct {
	u_int		isp_gotdparms		: 1,
			isp_req_ack_active_neg	: 1,
			isp_data_line_active_neg: 1,
			isp_cmd_dma_burst_enable: 1,
			isp_data_dma_burst_enabl: 1,
			isp_fifo_threshold	: 3,
			isp_ultramode		: 1,
			isp_diffmode		: 1,
			isp_lvdmode		: 1,
			isp_fast_mttr		: 1,	/* fast sram */
			isp_initiator_id	: 4,
			isp_async_data_setup	: 4;
	u_int16_t	isp_selection_timeout;
	u_int16_t	isp_max_queue_depth;
	u_int8_t	isp_tag_aging;
	u_int8_t	isp_bus_reset_delay;
	u_int8_t	isp_retry_count;
	u_int8_t	isp_retry_delay;
	struct {
		u_int	dev_enable	:	1,	/* ignored */
					:	1,
			dev_update	:	1,
			dev_refresh	:	1,
			exc_throttle	:	8,
			cur_offset	:	4,
			sync_offset	:	4;
		u_int8_t	cur_period;	/* current sync period */
		u_int8_t	sync_period;	/* goal sync period */
		u_int16_t	dev_flags;	/* goal device flags */
		u_int16_t	cur_dflags;	/* current device flags */
	} isp_devparam[MAX_TARGETS];
} sdparam;

/*
 * Device Flags
 */
#define	DPARM_DISC	0x8000
#define	DPARM_PARITY	0x4000
#define	DPARM_WIDE	0x2000
#define	DPARM_SYNC	0x1000
#define	DPARM_TQING	0x0800
#define	DPARM_ARQ	0x0400
#define	DPARM_QFRZ	0x0200
#define	DPARM_RENEG	0x0100
#define	DPARM_NARROW	0x0080
#define	DPARM_ASYNC	0x0040
#define	DPARM_PPR	0x0020
#define	DPARM_DEFAULT	(0xFF00 & ~DPARM_QFRZ)
#define	DPARM_SAFE_DFLT	(DPARM_DEFAULT & ~(DPARM_WIDE|DPARM_SYNC|DPARM_TQING))


/* technically, not really correct, as they need to be rated based upon clock */
#define	ISP_80M_SYNCPARMS	0x0c09
#define	ISP_40M_SYNCPARMS	0x0c0a
#define	ISP_20M_SYNCPARMS	0x0c0c
#define	ISP_20M_SYNCPARMS_1040	0x080c
#define	ISP_10M_SYNCPARMS	0x0c19
#define	ISP_08M_SYNCPARMS	0x0c25
#define	ISP_05M_SYNCPARMS	0x0c32
#define	ISP_04M_SYNCPARMS	0x0c41

/*
 * Fibre Channel Specifics
 */
#define	FL_PORT_ID		0x7e	/* FL_Port Special ID */
#define	FC_PORT_ID		0x7f	/* Fabric Controller Special ID */
#define	FC_SNS_ID		0x80	/* SNS Server Special ID */

typedef struct {
	u_int32_t		isp_fwoptions	: 16,
						: 4,
				loop_seen_once	: 1,
				isp_loopstate	: 3,	/* Current Loop State */
				isp_fwstate	: 3,	/* ISP F/W state */
				isp_gotdparms	: 1,
				isp_topo	: 3,
				isp_onfabric	: 1;
	u_int8_t		isp_loopid;	/* hard loop id */
	u_int8_t		isp_alpa;	/* ALPA */
	volatile u_int16_t	isp_lipseq;	/* LIP sequence # */
	u_int32_t		isp_portid;
	u_int8_t		isp_execthrottle;
	u_int8_t		isp_retry_delay;
	u_int8_t		isp_retry_count;
	u_int8_t		isp_reserved;
	u_int16_t		isp_maxalloc;
	u_int16_t		isp_maxfrmlen;
	u_int64_t		isp_nodewwn;
	u_int64_t		isp_portwwn;
	/*
	 * Port Data Base. This is indexed by 'target', which is invariate.
	 * However, elements within can move around due to loop changes,
	 * so the actual loop ID passed to the F/W is in this structure.
	 * The first time the loop is seen up, loopid will match the index
	 * (except for fabric nodes which are above mapped above FC_SNS_ID
	 * and are completely virtual), but subsequent LIPs can cause things
	 * to move around.
	 */
	struct lportdb {
		u_int
					loopid		: 8,
							: 4,
					loggedin	: 1,
					roles		: 2,
					valid		: 1;
		u_int32_t		portid;
		u_int64_t		node_wwn;
		u_int64_t		port_wwn;
	} portdb[MAX_FC_TARG], tport[FL_PORT_ID];

	/*
	 * Scratch DMA mapped in area to fetch Port Database stuff, etc.
	 */
	caddr_t			isp_scratch;
	u_int32_t		isp_scdma;
} fcparam;

#define	FW_CONFIG_WAIT		0
#define	FW_WAIT_AL_PA		1
#define	FW_WAIT_LOGIN		2
#define	FW_READY		3
#define	FW_LOSS_OF_SYNC		4
#define	FW_ERROR		5
#define	FW_REINIT		6
#define	FW_NON_PART		7

#define	LOOP_NIL		0
#define	LOOP_LIP_RCVD		1
#define	LOOP_PDB_RCVD		2
#define	LOOP_READY		7

#define	TOPO_NL_PORT		0
#define	TOPO_FL_PORT		1
#define	TOPO_N_PORT		2
#define	TOPO_F_PORT		3
#define	TOPO_PTP_STUB		4

/*
 * Soft Structure per host adapter
 */
typedef struct ispsoftc {
	/*
	 * Platform (OS) specific data
	 */
	struct isposinfo	isp_osinfo;

	/*
	 * Pointer to bus specific functions and data
	 */
	struct ispmdvec *	isp_mdvec;

	/*
	 * (Mostly) nonvolatile state. Board specific parameters
	 * may contain some volatile state (e.g., current loop state).
	 */

	void * 			isp_param;	/* type specific */
	u_int16_t		isp_fwrev[3];	/* Loaded F/W revision */
	u_int16_t		isp_romfw_rev[3]; /* PROM F/W revision */
	u_int16_t		isp_maxcmds;	/* max possible I/O cmds */
	u_int8_t		isp_type;	/* HBA Chip Type */
	u_int8_t		isp_revision;	/* HBA Chip H/W Revision */
	u_int32_t		isp_maxluns;	/* maximum luns supported */

	u_int32_t
				isp_touched	: 1,	/* board ever seen? */
						: 1,
				isp_bustype	: 1,	/* SBus or PCI */
				isp_loaded_fw	: 1,	/* loaded firmware */
				isp_dblev	: 12,	/* debug log mask */
				isp_clock	: 8,	/* input clock */
				isp_confopts	: 8;	/* config options */

	/*
	 * Volatile state
	 */

	volatile u_int32_t
		isp_mboxbsy	:	8,	/* mailbox command active */
				:	1,
		isp_state	:	3,
		isp_sendmarker	:	2,	/* send a marker entry */
		isp_update	:	2,	/* update parameters */
		isp_nactive	:	16;	/* how many commands active */
	volatile u_int16_t	isp_reqodx;	/* index of last ISP pickup */
	volatile u_int16_t	isp_reqidx;	/* index of next request */
	volatile u_int16_t	isp_residx;	/* index of next result */
	volatile u_int16_t	isp_lasthdls;	/* last handle seed */
	volatile u_int16_t	isp_mboxtmp[MAX_MAILBOX];

	/*
	 * Active commands are stored here, indexed by handle functions.
	 */
	XS_T **isp_xflist;

	/*
	 * request/result queue pointers and dma handles for them.
	 */
	caddr_t			isp_rquest;
	caddr_t			isp_result;
	u_int32_t		isp_rquest_dma;
	u_int32_t		isp_result_dma;
} ispsoftc_t;

#define	SDPARAM(isp)	((sdparam *) (isp)->isp_param)
#define	FCPARAM(isp)	((fcparam *) (isp)->isp_param)

/*
 * ISP Driver Run States
 */
#define	ISP_NILSTATE	0
#define	ISP_RESETSTATE	1
#define	ISP_INITSTATE	2
#define	ISP_RUNSTATE	3

/*
 * ISP Configuration Options
 */
#define	ISP_CFG_NORELOAD	0x80	/* don't download f/w */
#define	ISP_CFG_NONVRAM		0x40	/* ignore NVRAM */
#define	ISP_CFG_FULL_DUPLEX	0x01	/* Full Duplex (Fibre Channel only) */
#define	ISP_CFG_OWNWWN		0x02	/* override NVRAM wwn */
#define	ISP_CFG_NPORT		0x04	/* try to force N- instead of L-Port */

/*
 * Firmware related defines
 */
#define	ISP_CODE_ORG		0x1000	/* default f/w code start */
#define	ISP_FW_REV(maj, min, mic)	((maj << 24) | (min << 16) | mic)
#define	ISP_FW_REVX(xp)	((xp[0]<<24) | (xp[1] << 16) | xp[2])

/*
 * Bus (implementation) types
 */
#define	ISP_BT_PCI		0	/* PCI Implementations */
#define	ISP_BT_SBUS		1	/* SBus Implementations */

/*
 * Chip Types
 */
#define	ISP_HA_SCSI		0xf
#define	ISP_HA_SCSI_UNKNOWN	0x1
#define	ISP_HA_SCSI_1020	0x2
#define	ISP_HA_SCSI_1020A	0x3
#define	ISP_HA_SCSI_1040	0x4
#define	ISP_HA_SCSI_1040A	0x5
#define	ISP_HA_SCSI_1040B	0x6
#define	ISP_HA_SCSI_1040C	0x7
#define	ISP_HA_SCSI_1240	0x8
#define	ISP_HA_SCSI_1080	0x9
#define	ISP_HA_SCSI_1280	0xa
#define	ISP_HA_SCSI_12160	0xb
#define	ISP_HA_FC		0xf0
#define	ISP_HA_FC_2100		0x10
#define	ISP_HA_FC_2200		0x20

#define	IS_SCSI(isp)	(isp->isp_type & ISP_HA_SCSI)
#define	IS_1240(isp)	(isp->isp_type == ISP_HA_SCSI_1240)
#define	IS_1080(isp)	(isp->isp_type == ISP_HA_SCSI_1080)
#define	IS_1280(isp)	(isp->isp_type == ISP_HA_SCSI_1280)
#define	IS_12160(isp)	(isp->isp_type == ISP_HA_SCSI_12160)

#define	IS_12X0(isp)	(IS_1240(isp) || IS_1280(isp))
#define	IS_DUALBUS(isp)	(IS_12X0(isp) || IS_12160(isp))
#define	IS_ULTRA2(isp)	(IS_1080(isp) || IS_1280(isp) || IS_12160(isp))
#define	IS_ULTRA3(isp)	(IS_12160(isp))

#define	IS_FC(isp)	(isp->isp_type & ISP_HA_FC)
#define	IS_2100(isp)	(isp->isp_type == ISP_HA_FC_2100)
#define	IS_2200(isp)	(isp->isp_type == ISP_HA_FC_2200)

/*
 * DMA cookie macros
 */
#define	DMA_MSW(x)	(((x) >> 16) & 0xffff)
#define	DMA_LSW(x)	(((x) & 0xffff))

/*
 * Core System Function Prototypes
 */

/*
 * Reset Hardware. Totally. Assumes that you'll follow this with
 * a call to isp_init.
 */
void isp_reset __P((struct ispsoftc *));

/*
 * Initialize Hardware to known state
 */
void isp_init __P((struct ispsoftc *));

/*
 * Reset the ISP and call completion for any orphaned commands.
 */
void isp_reinit __P((struct ispsoftc *));

/*
 * Interrupt Service Routine
 */
int isp_intr __P((void *));

/*
 * Command Entry Point- Platform Dependent layers call into this
 */
int isp_start __P((XS_T *));
/* these values are what isp_start returns */
#define	CMD_COMPLETE	101	/* command completed */
#define	CMD_EAGAIN	102	/* busy- maybe retry later */
#define	CMD_QUEUED	103	/* command has been queued for execution */
#define	CMD_RQLATER 	104	/* requeue this command later */

/*
 * Command Completion Point- Core layers call out from this with completed cmds
 */
void isp_done __P((XS_T *));

/*
 * Platform Dependent to External to Internal Control Function
 *
 * Assumes all locks are held and that no reentrancy issues need be dealt with.
 *
 */
typedef enum {
	ISPCTL_RESET_BUS,		/* Reset Bus */
	ISPCTL_RESET_DEV,		/* Reset Device */
	ISPCTL_ABORT_CMD,		/* Abort Command */
	ISPCTL_UPDATE_PARAMS,		/* Update Operating Parameters */
	ISPCTL_FCLINK_TEST,		/* Test FC Link Status */
	ISPCTL_PDB_SYNC,		/* Synchronize Port Database */
	ISPCTL_TOGGLE_TMODE		/* toggle target mode */
} ispctl_t;
int isp_control __P((struct ispsoftc *, ispctl_t, void *));


/*
 * Platform Dependent to Internal to External Control Function
 * (each platform must provide such a function)
 *
 * Assumes all locks are held and that no reentrancy issues need be dealt with.
 *
 */

typedef enum {
	ISPASYNC_NEW_TGT_PARAMS,
	ISPASYNC_BUS_RESET,		/* Bus Was Reset */
	ISPASYNC_LOOP_DOWN,		/* FC Loop Down */
	ISPASYNC_LOOP_UP,		/* FC Loop Up */
	ISPASYNC_PDB_CHANGED,		/* FC Port Data Base Changed */
	ISPASYNC_CHANGE_NOTIFY,		/* FC SNS Change Notification */
	ISPASYNC_FABRIC_DEV,		/* FC New Fabric Device */
	ISPASYNC_TARGET_MESSAGE,	/* target message */
	ISPASYNC_TARGET_EVENT,		/* target asynchronous event */
	ISPASYNC_TARGET_ACTION		/* other target command action */
} ispasync_t;
int isp_async __P((struct ispsoftc *, ispasync_t, void *));

/*
 * Platform Dependent Error and Debug Printout
 */
void isp_prt __P((struct ispsoftc *, int level, const char *, ...));
#define	ISP_LOGALL	0x0	/* log always */
#define	ISP_LOGCONFIG	0x1	/* log configuration messages */
#define	ISP_LOGINFO	0x2	/* log informational messages */
#define	ISP_LOGWARN	0x4	/* log warning messages */
#define	ISP_LOGERR	0x8	/* log error messages */
#define	ISP_LOGDEBUG0	0x10	/* log simple debug messages */
#define	ISP_LOGDEBUG1	0x20	/* log intermediate debug messages */
#define	ISP_LOGDEBUG2	0x40	/* log most debug messages */
#define	ISP_LOGDEBUG3	0x100	/* log high frequency debug messages */
#define	ISP_LOGTDEBUG0	0x200	/* log simple debug messages (target mode) */
#define	ISP_LOGTDEBUG1	0x400	/* log intermediate debug messages (target) */
#define	ISP_LOGTDEBUG2	0x800	/* log all debug messages (target) */

/*
 * Each Platform provides it's own isposinfo substructure of the ispsoftc
 * defined above.
 *
 * Each platform must also provide the following macros/defines:
 *
 *
 *	INLINE		-	platform specific define for 'inline' functions
 *
 *	ISP2100_FABRIC	-	defines whether FABRIC support is enabled
 *	ISP2100_SCRLEN	-	length for the Fibre Channel scratch DMA area
 *
 *	MEMZERO(dst, src)			platform zeroing function
 *	MEMCPY(dst, src, count)			platform copying function
 *	SNPRINTF(buf, bufsize, fmt, ...)	snprintf
 *	STRNCAT(dstbuf, size, srcbuf)		strncat
 *	USEC_DELAY(usecs)			microsecond spindelay function
 *
 *	NANOTIME_T				nanosecond time type
 *
 *	GET_NANOTIME(NANOTIME_T *)		get current nanotime.
 *
 *	GET_NANOSEC(NANOTIME_T *)		get u_int64_t from NANOTIME_T
 *
 *	NANOTIME_SUB(NANOTIME_T *, NANOTIME_T *)
 *						subtract two NANOTIME_T values
 *
 *
 *	MAXISPREQUEST(struct ispsoftc *)	maximum request queue size
 *						for this particular board type
 *
 *	MEMORYBARRIER(struct ispsoftc *, barrier_type, offset, size)
 *
 *		Function/Macro the provides memory synchronization on
 *		various objects so that the ISP's and the system's view
 *		of the same object is consistent.
 *
 *	MBOX_ACQUIRE(struct ispsoftc *)		acquire lock on mailbox regs
 *	MBOX_WAIT_COMPLETE(struct ispsoftc *)	wait for mailbox cmd to be done
 *	MBOX_NOTIFY_COMPLETE(struct ispsoftc *)	notification of mbox cmd donee
 *	MBOX_RELEASE(struct ispsoftc *)		release lock on mailbox regs
 * 
 *
 *	SCSI_GOOD	SCSI 'Good' Status
 *	SCSI_CHECK	SCSI 'Check Condition' Status
 *	SCSI_BUSY	SCSI 'Busy' Status
 *	SCSI_QFULL	SCSI 'Queue Full' Status
 *
 *	XS_T		Platform SCSI transaction type (i.e., command for HBA)
 *	XS_ISP(xs)	gets an instance out of an XS_T
 *	XS_CHANNEL(xs)	gets the channel (bus # for DUALBUS cards) ""
 *	XS_TGT(xs)	gets the target ""
 *	XS_LUN(xs)	gets the lun ""
 *	XS_CDBP(xs)	gets a pointer to the scsi CDB ""
 *	XS_CDBLEN(xs)	gets the CDB's length ""
 *	XS_XFRLEN(xs)	gets the associated data transfer length ""
 *	XS_TIME(xs)	gets the time (in milliseconds) for this command
 *	XS_RESID(xs)	gets the current residual count
 *	XS_STSP(xs)	gets a pointer to the SCSI status byte ""
 *	XS_SNSP(xs)	gets a pointer to the associate sense data
 *	XS_SNSLEN(xs)	gets the length of sense data storage
 *	XS_SNSKEY(xs)	dereferences XS_SNSP to get the current stored Sense Key
 *	XS_TAG_P(xs)	predicate of whether this command should be tagged
 *	XS_TAG_TYPE(xs)	which type of tag to use
 *	XS_SETERR(xs)	set error state
 *
 *		HBA_NOERROR	command has no erros
 *		HBA_BOTCH	hba botched something
 *		HBA_CMDTIMEOUT	command timed out
 *		HBA_SELTIMEOUT	selection timed out (also port logouts for FC)
 *		HBA_TGTBSY	target returned a BUSY status
 *		HBA_BUSRESET	bus reset destroyed command
 *		HBA_ABORTED	command was aborted (by request)
 *		HBA_DATAOVR	a data overrun was detected
 *		HBA_ARQFAIL	Automatic Request Sense failed
 *
 *	XS_ERR(xs)	return current error state
 *	XS_NOERR(xs)	there is no error currently set
 *	XS_INITERR(xs)	initialize error state
 *
 *	XS_SAVE_SENSE(xs, sp)		save sense data
 *
 *	XS_SET_STATE_STAT(isp, sp, xs)	platform dependent interpreter of
 *					response queue entry status bits
 *
 *
 *	DEFAULT_IID(struct ispsoftc *)		Default SCSI initiator ID
 *
 *	DEFAULT_LOOPID(struct ispsoftc *)	Default FC Loop ID
 *	DEFAULT_NODEWWN(struct ispsoftc *)	Default FC Node WWN
 *	DEFAULT_PORTWWN(struct ispsoftc *)	Default FC Port WWN
 *
 *	PORT_FROM_NODE_WWN(struct ispsoftc *, u_int64_t nwwn)
 *
 *		Node to Port WWN generator- this needs to be platform
 *		specific so that given a NAA=2 WWN dragged from the Qlogic
 *		card's NVRAM, a Port WWN can be generated that has the
 *		appropriate bits set in bits 48..60 that are likely to be
 *		based on the device instance number.
 *
 *	(XXX these do endian specific transformations- in transition XXX)
 *	ISP_SWIZZLE_ICB
 *	ISP_UNSWIZZLE_AND_COPY_PDBP
 *	ISP_SWIZZLE_CONTINUATION
 *	ISP_SWIZZLE_REQUEST
 *	ISP_UNSWIZZLE_RESPONSE
 *	ISP_SWIZZLE_SNS_REQ
 *	ISP_UNSWIZZLE_SNS_RSP
 *	ISP_SWIZZLE_NVRAM_WORD
 *
 *
 */
#endif	/* _ISPVAR_H */
