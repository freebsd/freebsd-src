/* $FreeBSD$ */
/*
 * Soft Definitions for for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
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
 *
 */

#ifndef	_ISPVAR_H
#define	_ISPVAR_H

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/ispmbox.h>
#ifdef	ISP_TARGET_MODE
#include <dev/ic/isp_target.h>
#endif
#endif
#ifdef	__FreeBSD__
#include <dev/isp/ispmbox.h>
#ifdef	ISP_TARGET_MODE
#include <dev/isp/isp_target.h>
#endif
#endif
#ifdef	__linux__
#include "ispmbox.h"
#ifdef	ISP_TARGET_MODE
#include "isp_target.h"
#endif
#endif

#define	ISP_CORE_VERSION_MAJOR	1
#define	ISP_CORE_VERSION_MINOR	11

/*
 * Vector for bus specific code to provide specific services.
 */
struct ispsoftc;
struct ispmdvec {
	u_int16_t	(*dv_rd_reg) __P((struct ispsoftc *, int));
	void		(*dv_wr_reg) __P((struct ispsoftc *, int, u_int16_t));
	int		(*dv_mbxdma) __P((struct ispsoftc *));
	int		(*dv_dmaset) __P((struct ispsoftc *,
		ISP_SCSI_XFER_T *, ispreq_t *, u_int16_t *, u_int16_t));
	void		(*dv_dmaclr)
		__P((struct ispsoftc *, ISP_SCSI_XFER_T *, u_int32_t));
	void		(*dv_reset0) __P((struct ispsoftc *));
	void		(*dv_reset1) __P((struct ispsoftc *));
	void		(*dv_dregs) __P((struct ispsoftc *));
	const u_int16_t *dv_ispfw;	/* ptr to f/w */
	u_int16_t 	dv_fwlen;	/* length of f/w */
	u_int16_t	dv_codeorg;	/* code ORG for f/w */
	u_int32_t	dv_fwrev;	/* f/w revision */
	/*
	 * Initial values for conf1 register
	 */
	u_int16_t	dv_conf1;
	u_int16_t	dv_clock;	/* clock frequency */
};

#define	MAX_TARGETS	16
#ifdef	ISP2100_FABRIC
#define	MAX_FC_TARG	256
#else
#define	MAX_FC_TARG	126
#endif

#define	ISP_MAX_TARGETS(isp)	(IS_FC(isp)? MAX_FC_TARG : MAX_TARGETS)
#ifdef	ISP2100_SCCLUN
#define	_ISP_FC_LUN(isp)	65536
#else
#define	_ISP_FC_LUN(isp)	16
#endif
#define	_ISP_SCSI_LUN(isp)	\
	((ISP_FW_REVX(isp->isp_fwrev) >= ISP_FW_REV(7, 55, 0))? 32 : 8)
#define	ISP_MAX_LUNS(isp)	\
	(IS_FC(isp)? _ISP_FC_LUN(isp) : _ISP_SCSI_LUN(isp))


/*
 * Macros to read, write ISP registers through bus specific code.
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
#define	ISP_DUMPREGS(isp)	\
	if ((isp)->isp_mdvec->dv_dregs) (*(isp)->isp_mdvec->dv_dregs)((isp))

#define	ISP_SETBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) | (val))

#define	ISP_CLRBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) & ~(val))

/* this is the size of a queue entry (request and response) */
#define	QENTRY_LEN			64
/* both request and result queue length must be a power of two */
#define	RQUEST_QUEUE_LEN		MAXISPREQUEST
/* I've seen wierdnesses with the result queue < 64 */
#if	MAXISPREQUEST > 64
#define	RESULT_QUEUE_LEN		(MAXISPREQUEST/2)
#else
#define	RESULT_QUEUE_LEN		MAXISPREQUEST
#endif
#define	ISP_QUEUE_ENTRY(q, idx)		((q) + ((idx) * QENTRY_LEN))
#define	ISP_QUEUE_SIZE(n)		((n) * QENTRY_LEN)
#define	ISP_NXT_QENTRY(idx, qlen)	(((idx) + 1) & ((qlen)-1))
#define	ISP_QAVAIL(in, out, qlen)	\
	((in == out)? (qlen - 1) : ((in > out)? \
	((qlen - 1) - (in - out)) : (out - in - 1)))

#define	ISP_ADD_REQUEST(isp, iptr)	\
	ISP_WRITE(isp, INMAILBOX4, iptr), isp->isp_reqidx = iptr

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
						: 1,
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
#define	DPARM_NARROW	0x0080	/* Possibly only available with >= 7.55 fw */
#define	DPARM_ASYNC	0x0040	/* Possibly only available with >= 7.55 fw */
#define	DPARM_DEFAULT	(0xFF00 & ~DPARM_QFRZ)
#define	DPARM_SAFE_DFLT	(DPARM_DEFAULT & ~(DPARM_WIDE|DPARM_SYNC|DPARM_TQING))


/* technically, not really correct, as they need to be rated based upon clock */
#define	ISP_40M_SYNCPARMS	0x080a
#define	ISP_20M_SYNCPARMS	0x080c
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
						: 7,
				loop_seen_once	: 1,
				isp_loopstate	: 3,	/* Current Loop State */
				isp_fwstate	: 3,	/* ISP F/W state */
				isp_gotdparms	: 1,
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
					loopid	: 8,
						: 4,
					fabdev	: 1,
					roles	: 2,
					valid	: 1;
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

/*
 * Soft Structure per host adapter
 */
struct ispsoftc {
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

	u_int32_t				: 4,
				isp_touched	: 1,	/* board ever seen? */
				isp_fast_mttr	: 1,	/* fast sram */
				isp_bustype	: 1,	/* SBus or PCI */
				isp_dogactive	: 1,	/* watchdog running */
				isp_dblev	: 8,	/* debug level */
				isp_clock	: 8,	/* input clock */
				isp_confopts	: 8;	/* config options */

	/*
	 * Volatile state
	 */

	volatile u_int32_t	:	9,
		isp_state	:	3,
		isp_sendmarker	:	2,	/* send a marker entry */
		isp_update	:	2,	/* update parameters */
		isp_nactive	:	16;	/* how many commands active */

	volatile u_int16_t	isp_reqodx;	/* index of last ISP pickup */
	volatile u_int16_t	isp_reqidx;	/* index of next request */
	volatile u_int16_t	isp_residx;	/* index of next result */
	volatile u_int16_t	isp_lasthdls;	/* last handle seed */

	/*
	 * Active commands are stored here, indexed by handle functions.
	 */
	ISP_SCSI_XFER_T **isp_xflist;

	/*
	 * request/result queue pointers and dma handles for them.
	 */
	caddr_t			isp_rquest;
	caddr_t			isp_result;
	u_int32_t		isp_rquest_dma;
	u_int32_t		isp_result_dma;
};

#define	SDPARAM(isp)	((sdparam *) (isp)->isp_param)
#define	FCPARAM(isp)	((fcparam *) (isp)->isp_param)

/*
 * ISP States
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
#define	ISP_HA_SCSI_1080	0xd
#define	ISP_HA_SCSI_12X0	0xe
#define	ISP_HA_FC		0xf0
#define	ISP_HA_FC_2100		0x10
#define	ISP_HA_FC_2200		0x20

#define	IS_SCSI(isp)	(isp->isp_type & ISP_HA_SCSI)
#define	IS_1080(isp)	(isp->isp_type == ISP_HA_SCSI_1080)
#define	IS_12X0(isp)	(isp->isp_type == ISP_HA_SCSI_12X0)
#define	IS_FC(isp)	(isp->isp_type & ISP_HA_FC)

/*
 * Function Prototypes
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
void isp_restart __P((struct ispsoftc *));

/*
 * Interrupt Service Routine
 */
int isp_intr __P((void *));

/*
 * Command Entry Point
 */
int32_t ispscsicmd __P((ISP_SCSI_XFER_T *));

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
#ifdef	ISP_TARGET_MODE
	ISPCTL_ENABLE_LUN,		/* enable a LUN */
	ISPCTL_MODIFY_LUN,		/* enable a LUN */
#endif
	ISPCTL_FCLINK_TEST		/* Test FC Link Status */
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
	ISPASYNC_TARGET_CMD,		/* New target command */
	ISPASYNC_TARGET_EVENT		/* New target event */
} ispasync_t;
int isp_async __P((struct ispsoftc *, ispasync_t, void *));

/*
 * lost command routine (XXXX IN TRANSITION XXXX)
 */
void isp_lostcmd __P((struct ispsoftc *, ISP_SCSI_XFER_T *));

#endif	/* _ISPVAR_H */
