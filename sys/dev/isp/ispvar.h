/* $FreeBSD: src/sys/dev/isp/ispvar.h,v 1.85 2007/07/02 20:08:20 mjacob Exp $ */
/*-
 *  Copyright (c) 1997-2007 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
/*
 * Soft Definitions for for Qlogic ISP SCSI adapters.
 */

#ifndef	_ISPVAR_H
#define	_ISPVAR_H

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/isp_stds.h>
#include <dev/ic/ispmbox.h>
#endif
#ifdef	__FreeBSD__
#include <dev/isp/isp_stds.h>
#include <dev/isp/ispmbox.h>
#endif
#ifdef	__linux__
#include "isp_stds.h"
#include "ispmbox.h"
#endif
#ifdef	__svr4__
#include "isp_stds.h"
#include "ispmbox.h"
#endif

#define	ISP_CORE_VERSION_MAJOR	3
#define	ISP_CORE_VERSION_MINOR	0

/*
 * Vector for bus specific code to provide specific services.
 */
typedef struct ispsoftc ispsoftc_t;
struct ispmdvec {
	int		(*dv_rd_isr)
	    (ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
	uint32_t	(*dv_rd_reg) (ispsoftc_t *, int);
	void		(*dv_wr_reg) (ispsoftc_t *, int, uint32_t);
	int		(*dv_mbxdma) (ispsoftc_t *);
	int		(*dv_dmaset)
	    (ispsoftc_t *, XS_T *, ispreq_t *, uint32_t *, uint32_t);
	void		(*dv_dmaclr) (ispsoftc_t *, XS_T *, uint32_t);
	void		(*dv_reset0) (ispsoftc_t *);
	void		(*dv_reset1) (ispsoftc_t *);
	void		(*dv_dregs) (ispsoftc_t *, const char *);
	const void *	dv_ispfw;	/* ptr to f/w */
	uint16_t	dv_conf1;
	uint16_t	dv_clock;	/* clock frequency */
};

/*
 * Overall parameters
 */
#define	MAX_TARGETS		16
#define	MAX_FC_TARG		512
#define	ISP_MAX_TARGETS(isp)	(IS_FC(isp)? MAX_FC_TARG : MAX_TARGETS)
#define	ISP_MAX_LUNS(isp)	(isp)->isp_maxluns

/*
 * Macros to access ISP registers through bus specific layers-
 * mostly wrappers to vector through the mdvec structure.
 */
#define	ISP_READ_ISR(isp, isrp, semap, mbox0p)	\
	(*(isp)->isp_mdvec->dv_rd_isr)(isp, isrp, semap, mbox0p)

#define	ISP_READ(isp, reg)	\
	(*(isp)->isp_mdvec->dv_rd_reg)((isp), (reg))

#define	ISP_WRITE(isp, reg, val)	\
	(*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), (val))

#define	ISP_MBOXDMASETUP(isp)	\
	(*(isp)->isp_mdvec->dv_mbxdma)((isp))

#define	ISP_DMASETUP(isp, xs, req, iptrp, optr)	\
	(*(isp)->isp_mdvec->dv_dmaset)((isp), (xs), (req), (iptrp), (optr))

#define	ISP_DMAFREE(isp, xs, hndl)		\
	if ((isp)->isp_mdvec->dv_dmaclr)	\
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
#define	SYNC_ATIOQ	5	/* atio result queue (24xx) */

/*
 * Request/Response Queue defines and macros.
 * The maximum is defined per platform (and can be based on board type).
 */
/* This is the size of a queue entry (request and response) */
#define	QENTRY_LEN			64
/* Both request and result queue length must be a power of two */
#define	RQUEST_QUEUE_LEN(x)		MAXISPREQUEST(x)
#ifdef	ISP_TARGET_MODE
#define	RESULT_QUEUE_LEN(x)		MAXISPREQUEST(x)
#else
#define	RESULT_QUEUE_LEN(x)		\
	(((MAXISPREQUEST(x) >> 2) < 64)? 64 : MAXISPREQUEST(x) >> 2)
#endif
#define	ISP_QUEUE_ENTRY(q, idx)		(((uint8_t *)q) + ((idx) * QENTRY_LEN))
#define	ISP_QUEUE_SIZE(n)		((n) * QENTRY_LEN)
#define	ISP_NXT_QENTRY(idx, qlen)	(((idx) + 1) & ((qlen)-1))
#define	ISP_QFREE(in, out, qlen)	\
	((in == out)? (qlen - 1) : ((in > out)? \
	((qlen - 1) - (in - out)) : (out - in - 1)))
#define	ISP_QAVAIL(isp)	\
	ISP_QFREE(isp->isp_reqidx, isp->isp_reqodx, RQUEST_QUEUE_LEN(isp))

#define	ISP_ADD_REQUEST(isp, nxti)					\
	MEMORYBARRIER(isp, SYNC_REQUEST, isp->isp_reqidx, QENTRY_LEN);	\
	ISP_WRITE(isp, isp->isp_rqstinrp, nxti);			\
	isp->isp_reqidx = nxti

/*
 * SCSI Specific Host Adapter Parameters- per bus, per target
 */
typedef struct {
	uint32_t 				: 10,
			isp_bad_nvram		: 1,
			isp_gotdparms		: 1,
			isp_req_ack_active_neg	: 1,
			isp_data_line_active_neg: 1,
			isp_cmd_dma_burst_enable: 1,
			isp_data_dma_burst_enabl: 1,
			isp_fifo_threshold	: 3,
			isp_ptisp		: 1,
			isp_ultramode		: 1,
			isp_diffmode		: 1,
			isp_lvdmode		: 1,
			isp_fast_mttr		: 1,	/* fast sram */
			isp_initiator_id	: 4,
			isp_async_data_setup	: 4;
	uint16_t	isp_selection_timeout;
	uint16_t	isp_max_queue_depth;
	uint8_t		isp_tag_aging;
	uint8_t		isp_bus_reset_delay;
	uint8_t		isp_retry_count;
	uint8_t		isp_retry_delay;
	struct {
		uint32_t	
			exc_throttle	:	8,
					:	1,
			dev_enable	:	1,	/* ignored */
			dev_update	:	1,
			dev_refresh	:	1,
			actv_offset	:	4,
			goal_offset	:	4,
			nvrm_offset	:	4;
		uint8_t		actv_period;	/* current sync period */
		uint8_t		goal_period;	/* goal sync period */
		uint8_t		nvrm_period;	/* nvram sync period */
		uint16_t	actv_flags;	/* current device flags */
		uint16_t	goal_flags;	/* goal device flags */
		uint16_t	nvrm_flags;	/* nvram device flags */
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
/* These are for non-2K Login Firmware cards */
#define	FL_ID			0x7e	/* FL_Port Special ID */
#define	SNS_ID			0x80	/* SNS Server Special ID */
#define	NPH_MAX			0xfe

/* These are for 2K Login Firmware cards */
#define	NPH_RESERVED		0x7F0	/* begin of reserved N-port handles */
#define	NPH_MGT_ID		0x7FA	/* Management Server Special ID */
#define	NPH_SNS_ID		0x7FC	/* SNS Server Special ID */
#define	NPH_FL_ID		0x7FE	/* FL Port Special ID */
#define	NPH_MAX_2K		0x800

/*
 * "Unassigned" handle to be used internally
 */
#define	NIL_HANDLE		0xffff

/*
 * Limit for devices on an arbitrated loop.
 */
#define	LOCAL_LOOP_LIM		126

/*
 * Special Port IDs
 */
#define	MANAGEMENT_PORT_ID	0xFFFFFA
#define	SNS_PORT_ID		0xFFFFFC
#define	FABRIC_PORT_ID		0xFFFFFE


/*
 * FC Port Database entry.
 *
 * It has a handle that the f/w uses to address commands to a device.
 * This handle's value may be assigned by the firmware (e.g., for local loop
 * devices) or by the driver (e.g., for fabric devices).
 *
 * It has a state. If the state if VALID, that means that we've logged into
 * the device. We also *may* have a initiator map index entry. This is a value
 * from 0..MAX_FC_TARG that is used to index into the isp_ini_map array. If
 * the value therein is non-zero, then that value minus one is used to index
 * into the Port Database to find the handle for forming commands. There is
 * back-index minus one value within to Port Database entry that tells us 
 * which entry in isp_ini_map points to us (to avoid searching).
 *
 * Local loop devices the firmware automatically performs PLOGI on for us
 * (which is why that handle is imposed upon us). Fabric devices we assign
 * a handle to and perform the PLOGI on.
 *
 * When a PORT DATABASE CHANGED asynchronous event occurs, we mark all VALID
 * entries as PROBATIONAL. This allows us, if policy says to, just keep track
 * of devices whose handles change but are otherwise the same device (and
 * thus keep 'target' constant).
 *
 * In any case, we search all possible local loop handles. For each one that
 * has a port database entity returned, we search for any PROBATIONAL entry
 * that matches it and update as appropriate. Otherwise, as a new entry, we
 * find room for it in the Port Database. We *try* and use the handle as the
 * index to put it into the Database, but that's just an optimization. We mark
 * the entry VALID and make sure that the target index is updated and correct.
 *
 * When we get done searching the local loop, we then search similarily for
 * a list of devices we've gotten from the fabric name controller (if we're
 * on a fabric). VALID marking is also done similarily.
 *
 * When all of this is done, we can march through the database and clean up
 * any entry that is still PROBATIONAL (these represent devices which have
 * departed). Then we're done and can resume normal operations.
 *
 * Negative invariants that we try and test for are:
 *
 *  + There can never be two non-NIL entries with the same { Port, Node } WWN
 *    duples.
 *
 *  + There can never be two non-NIL entries with the same handle.
 *
 *  + There can never be two non-NIL entries which have the same ini_map_idx
 *    value.
 */
typedef struct {
	/*
	 * This is the handle that the firmware needs in order for us to
	 * send commands to the device. For pre-24XX cards, this would be
	 * the 'loopid'.
	 */
	uint16_t	handle;
	/*
	 * The ini_map_idx, if nonzero, is the system virtual target ID (+1)
	 * as a cross-reference with the isp_ini_map.
	 *
	 * A device is 'autologin' if the firmware automatically logs into
	 * it (re-logins as needed). Basically, local private loop devices.
	 *
	 * The state is the current state of thsi entry.
	 *
	 * Role is Initiator, Target, Both
	 *
	 * Portid is obvious, as or node && port WWNs. The new_role and
	 * new_portid is for when we are pending a change.
	 */
	uint16_t	ini_map_idx	: 12,
			autologin	: 1,	/* F/W does PLOGI/PLOGO */
			state		: 3;
	uint32_t	reserved	: 6,
			roles		: 2,
			portid		: 24;
	uint32_t	new_reserved	: 6,
			new_roles	: 2,
			new_portid	: 24;
	uint64_t	node_wwn;
	uint64_t	port_wwn;
} fcportdb_t;

#define	FC_PORTDB_STATE_NIL		0
#define	FC_PORTDB_STATE_PROBATIONAL	1
#define	FC_PORTDB_STATE_DEAD		2
#define	FC_PORTDB_STATE_CHANGED		3
#define	FC_PORTDB_STATE_NEW		4
#define	FC_PORTDB_STATE_PENDING_VALID	5
#define	FC_PORTDB_STATE_ZOMBIE		6
#define	FC_PORTDB_STATE_VALID		7

/*
 * FC card specific information
 */
typedef struct {
	uint32_t				: 10,
				isp_tmode	: 1,
				isp_2klogin	: 1,
				isp_sccfw	: 1,
				isp_gbspeed	: 3,
						: 1,
						: 1,
				isp_gotdparms	: 1,
				isp_bad_nvram	: 1,
				isp_loopstate	: 4,	/* Current Loop State */
				isp_fwstate	: 4,	/* ISP F/W state */
				isp_topo	: 3,
				loop_seen_once	: 1;
	uint32_t				: 8,
				isp_portid	: 24;	/* S_ID */
	uint16_t		isp_fwoptions;
	uint16_t		isp_xfwoptions;
	uint16_t		isp_zfwoptions;
	uint16_t		isp_loopid;	/* hard loop id */
	uint16_t		isp_fwattr;	/* firmware attributes */
	uint16_t		isp_execthrottle;
	uint8_t			isp_retry_delay;
	uint8_t			isp_retry_count;
	uint8_t			isp_reserved;
	uint16_t		isp_maxalloc;
	uint16_t		isp_maxfrmlen;
	uint64_t		isp_wwnn_nvram;
	uint64_t		isp_wwpn_nvram;

	/*
	 * Our Port Data Base
	 */
	fcportdb_t		portdb[MAX_FC_TARG];

	/*
	 * This maps system virtual 'target' id to a portdb entry.
	 *
	 * The mapping function is to take any non-zero entry and
	 * subtract one to get the portdb index. This means that
	 * entries which are zero are unmapped (i.e., don't exist).
	 */
	uint16_t		isp_ini_map[MAX_FC_TARG];

	/*
	 * Scratch DMA mapped in area to fetch Port Database stuff, etc.
	 */
	void *			isp_scratch;
	XS_DMA_ADDR_T		isp_scdma;
#ifdef	ISP_FW_CRASH_DUMP
	uint16_t *		isp_dump_data;
#endif
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
#define	LOOP_SCANNING_LOOP	3
#define	LOOP_LSCAN_DONE		4
#define	LOOP_SCANNING_FABRIC	5
#define	LOOP_FSCAN_DONE		6
#define	LOOP_SYNCING_PDB	7
#define	LOOP_READY		8

#define	TOPO_NL_PORT		0
#define	TOPO_FL_PORT		1
#define	TOPO_N_PORT		2
#define	TOPO_F_PORT		3
#define	TOPO_PTP_STUB		4

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
	uint16_t		isp_fwrev[3];	/* Loaded F/W revision */
	uint16_t		isp_romfw_rev[3]; /* PROM F/W revision */
	uint16_t		isp_maxcmds;	/* max possible I/O cmds */
	uint8_t			isp_type;	/* HBA Chip Type */
	uint8_t			isp_revision;	/* HBA Chip H/W Revision */
	uint32_t		isp_maxluns;	/* maximum luns supported */

	uint32_t		isp_clock	: 8,	/* input clock */
						: 4,
				isp_port	: 1,	/* 23XX/24XX only */
				isp_failed	: 1,	/* board failed */
				isp_open	: 1,	/* opened (ioctl) */
				isp_touched	: 1,	/* board ever seen? */
				isp_bustype	: 1,	/* SBus or PCI */
				isp_loaded_fw	: 1,	/* loaded firmware */
				isp_role	: 2,	/* roles supported */
				isp_dblev	: 12;	/* debug log mask */

	uint32_t		isp_confopts;		/* config options */

	uint32_t		isp_rqstinrp;	/* register for REQINP */
	uint32_t		isp_rqstoutrp;	/* register for REQOUTP */
	uint32_t		isp_respinrp;	/* register for RESINP */
	uint32_t		isp_respoutrp;	/* register for RESOUTP */
	uint32_t		isp_atioinrp;	/* register for ATIOINP */
	uint32_t		isp_atiooutrp;	/* register for ATIOOUTP */

	/*
	 * Instrumentation
	 */
	uint64_t		isp_intcnt;		/* total int count */
	uint64_t		isp_intbogus;		/* spurious int count */
	uint64_t		isp_intmboxc;		/* mbox completions */
	uint64_t		isp_intoasync;		/* other async */
	uint64_t		isp_rsltccmplt;		/* CMDs on result q */
	uint64_t		isp_fphccmplt;		/* CMDs via fastpost */
	uint16_t		isp_rscchiwater;
	uint16_t		isp_fpcchiwater;

	/*
	 * Volatile state
	 */

	volatile uint32_t	:	8,
		isp_mboxbsy	:	1,	/* mailbox command active */
		isp_state	:	3,
		isp_sendmarker	:	2,	/* send a marker entry */
		isp_update	:	2,	/* update parameters */
		isp_nactive	:	16;	/* how many commands active */
	volatile uint32_t	isp_reqodx;	/* index of last ISP pickup */
	volatile uint32_t	isp_reqidx;	/* index of next request */
	volatile uint32_t	isp_residx;	/* index of next result */
	volatile uint32_t	isp_resodx;	/* index of next result */
	volatile uint32_t	isp_rspbsy;
	volatile uint32_t	isp_lasthdls;	/* last handle seed */
	volatile uint32_t	isp_obits;	/* mailbox command output */
	volatile uint16_t	isp_mboxtmp[MAILBOX_STORAGE];
	volatile uint16_t	isp_lastmbxcmd;	/* last mbox command sent */
	volatile uint16_t	isp_mbxwrk0;
	volatile uint16_t	isp_mbxwrk1;
	volatile uint16_t	isp_mbxwrk2;
	volatile uint16_t	isp_mbxwrk8;
	void *			isp_mbxworkp;

	/*
	 * Active commands are stored here, indexed by handle functions.
	 */
	XS_T **isp_xflist;

#ifdef	ISP_TARGET_MODE
	/*
	 * Active target commands are stored here, indexed by handle function.
	 */
	void **isp_tgtlist;
#endif

	/*
	 * request/result queue pointers and DMA handles for them.
	 */
	void *			isp_rquest;
	void *			isp_result;
	XS_DMA_ADDR_T		isp_rquest_dma;
	XS_DMA_ADDR_T		isp_result_dma;
#ifdef	ISP_TARGET_MODE
	/* for 24XX only */
	void *			isp_atioq;
	XS_DMA_ADDR_T		isp_atioq_dma;
#endif
};

#define	SDPARAM(isp)	((sdparam *) (isp)->isp_param)
#define	FCPARAM(isp)	((fcparam *) (isp)->isp_param)

/*
 * ISP Driver Run States
 */
#define	ISP_NILSTATE	0
#define	ISP_CRASHED	1
#define	ISP_RESETSTATE	2
#define	ISP_INITSTATE	3
#define	ISP_RUNSTATE	4

/*
 * ISP Configuration Options
 */
#define	ISP_CFG_NORELOAD	0x80	/* don't download f/w */
#define	ISP_CFG_NONVRAM		0x40	/* ignore NVRAM */
#define	ISP_CFG_TWOGB		0x20	/* force 2GB connection (23XX only) */
#define	ISP_CFG_ONEGB		0x10	/* force 1GB connection (23XX only) */
#define	ISP_CFG_FULL_DUPLEX	0x01	/* Full Duplex (Fibre Channel only) */
#define	ISP_CFG_PORT_PREF	0x0C	/* Mask for Port Prefs (2200 only) */
#define	ISP_CFG_LPORT		0x00	/* prefer {N/F}L-Port connection */
#define	ISP_CFG_NPORT		0x04	/* prefer {N/F}-Port connection */
#define	ISP_CFG_NPORT_ONLY	0x08	/* insist on {N/F}-Port connection */
#define	ISP_CFG_LPORT_ONLY	0x0C	/* insist on {N/F}L-Port connection */
#define	ISP_CFG_OWNWWPN		0x100	/* override NVRAM wwpn */
#define	ISP_CFG_OWNWWNN		0x200	/* override NVRAM wwnn */
#define	ISP_CFG_OWNFSZ		0x400	/* override NVRAM frame size */
#define	ISP_CFG_OWNLOOPID	0x800	/* override NVRAM loopid */
#define	ISP_CFG_OWNEXCTHROTTLE	0x1000	/* override NVRAM execution throttle */
#define	ISP_CFG_FOURGB		0x2000	/* force 4GB connection (24XX only) */

/*
 * Prior to calling isp_reset for the first time, the outer layer
 * should set isp_role to one of NONE, INITIATOR, TARGET, BOTH.
 *
 * If you set ISP_ROLE_NONE, the cards will be reset, new firmware loaded,
 * NVRAM read, and defaults set, but any further initialization (e.g.
 * INITIALIZE CONTROL BLOCK commands for 2X00 cards) won't be done.
 *
 * If INITIATOR MODE isn't set, attempts to run commands will be stopped
 * at isp_start and completed with the moral equivalent of SELECTION TIMEOUT.
 *
 * If TARGET MODE is set, it doesn't mean that the rest of target mode support
 * needs to be enabled, or will even work. What happens with the 2X00 cards
 * here is that if you have enabled it with TARGET MODE as part of the ICB
 * options, but you haven't given the f/w any ram resources for ATIOs or
 * Immediate Notifies, the f/w just handles what it can and you never see
 * anything. Basically, it sends a single byte of data (the first byte,
 * which you can set as part of the INITIALIZE CONTROL BLOCK command) for
 * INQUIRY, and sends back QUEUE FULL status for any other command.
 *
 */
#define	ISP_ROLE_NONE		0x0
#define	ISP_ROLE_TARGET		0x1
#define	ISP_ROLE_INITIATOR	0x2
#define	ISP_ROLE_BOTH		(ISP_ROLE_TARGET|ISP_ROLE_INITIATOR)
#define	ISP_ROLE_EITHER		ISP_ROLE_BOTH
#ifndef	ISP_DEFAULT_ROLES
#define	ISP_DEFAULT_ROLES	ISP_ROLE_INITIATOR
#endif


/*
 * Firmware related defines
 */
#define	ISP_CODE_ORG			0x1000	/* default f/w code start */
#define	ISP_CODE_ORG_2300		0x0800	/* ..except for 2300s */
#define	ISP_CODE_ORG_2400		0x100000 /* ..and 2400s */
#define	ISP_FW_REV(maj, min, mic)	((maj << 24) | (min << 16) | mic)
#define	ISP_FW_MAJOR(code)		((code >> 24) & 0xff)
#define	ISP_FW_MINOR(code)		((code >> 16) & 0xff)
#define	ISP_FW_MICRO(code)		((code >>  8) & 0xff)
#define	ISP_FW_REVX(xp)			((xp[0]<<24) | (xp[1] << 16) | xp[2])
#define	ISP_FW_MAJORX(xp)		(xp[0])
#define	ISP_FW_MINORX(xp)		(xp[1])
#define	ISP_FW_MICROX(xp)		(xp[2])
#define	ISP_FW_NEWER_THAN(i, major, minor, micro)		\
 (ISP_FW_REVX((i)->isp_fwrev) > ISP_FW_REV(major, minor, micro))
#define	ISP_FW_OLDER_THAN(i, major, minor, micro)		\
 (ISP_FW_REVX((i)->isp_fwrev) < ISP_FW_REV(major, minor, micro))

/*
 * Bus (implementation) types
 */
#define	ISP_BT_PCI		0	/* PCI Implementations */
#define	ISP_BT_SBUS		1	/* SBus Implementations */

/*
 * If we have not otherwise defined SBus support away make sure
 * it is defined here such that the code is included as default
 */
#ifndef	ISP_SBUS_SUPPORTED
#define	ISP_SBUS_SUPPORTED	1
#endif

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
#define	ISP_HA_SCSI_10160	0xb
#define	ISP_HA_SCSI_12160	0xc
#define	ISP_HA_FC		0xf0
#define	ISP_HA_FC_2100		0x10
#define	ISP_HA_FC_2200		0x20
#define	ISP_HA_FC_2300		0x30
#define	ISP_HA_FC_2312		0x40
#define	ISP_HA_FC_2322		0x50
#define	ISP_HA_FC_2400		0x60

#define	IS_SCSI(isp)	(isp->isp_type & ISP_HA_SCSI)
#define	IS_1240(isp)	(isp->isp_type == ISP_HA_SCSI_1240)
#define	IS_1080(isp)	(isp->isp_type == ISP_HA_SCSI_1080)
#define	IS_1280(isp)	(isp->isp_type == ISP_HA_SCSI_1280)
#define	IS_10160(isp)	(isp->isp_type == ISP_HA_SCSI_10160)
#define	IS_12160(isp)	(isp->isp_type == ISP_HA_SCSI_12160)

#define	IS_12X0(isp)	(IS_1240(isp) || IS_1280(isp))
#define	IS_1X160(isp)	(IS_10160(isp) || IS_12160(isp))
#define	IS_DUALBUS(isp)	(IS_12X0(isp) || IS_12160(isp))
#define	IS_ULTRA2(isp)	(IS_1080(isp) || IS_1280(isp) || IS_1X160(isp))
#define	IS_ULTRA3(isp)	(IS_1X160(isp))

#define	IS_FC(isp)	((isp)->isp_type & ISP_HA_FC)
#define	IS_2100(isp)	((isp)->isp_type == ISP_HA_FC_2100)
#define	IS_2200(isp)	((isp)->isp_type == ISP_HA_FC_2200)
#define	IS_23XX(isp)	((isp)->isp_type >= ISP_HA_FC_2300 && \
				(isp)->isp_type < ISP_HA_FC_2400)
#define	IS_2300(isp)	((isp)->isp_type == ISP_HA_FC_2300)
#define	IS_2312(isp)	((isp)->isp_type == ISP_HA_FC_2312)
#define	IS_2322(isp)	((isp)->isp_type == ISP_HA_FC_2322)
#define	IS_24XX(isp)	((isp)->isp_type >= ISP_HA_FC_2400)

/*
 * DMA related macros
 */
#define	DMA_WD3(x)	(((uint16_t)(((uint64_t)x) >> 48)) & 0xffff)
#define	DMA_WD2(x)	(((uint16_t)(((uint64_t)x) >> 32)) & 0xffff)
#define	DMA_WD1(x)	((uint16_t)((x) >> 16) & 0xffff)
#define	DMA_WD0(x)	((uint16_t)((x) & 0xffff))

#define	DMA_LO32(x)	((uint32_t) (x))
#define	DMA_HI32(x)	((uint32_t)(((uint64_t)x) >> 32))

/*
 * Core System Function Prototypes
 */

/*
 * Reset Hardware. Totally. Assumes that you'll follow this with
 * a call to isp_init.
 */
void isp_reset(ispsoftc_t *);

/*
 * Initialize Hardware to known state
 */
void isp_init(ispsoftc_t *);

/*
 * Reset the ISP and call completion for any orphaned commands.
 */
void isp_reinit(ispsoftc_t *);

#ifdef	ISP_FW_CRASH_DUMP
/*
 * Dump firmware entry point.
 */
void isp_fw_dump(ispsoftc_t *isp);
#endif

/*
 * Internal Interrupt Service Routine
 *
 * The outer layers do the spade work to get the appropriate status register,
 * semaphore register and first mailbox register (if appropriate). This also
 * means that most spurious/bogus interrupts not for us can be filtered first.
 */
void isp_intr(ispsoftc_t *, uint32_t, uint16_t, uint16_t);


/*
 * Command Entry Point- Platform Dependent layers call into this
 */
int isp_start(XS_T *);

/* these values are what isp_start returns */
#define	CMD_COMPLETE	101	/* command completed */
#define	CMD_EAGAIN	102	/* busy- maybe retry later */
#define	CMD_QUEUED	103	/* command has been queued for execution */
#define	CMD_RQLATER 	104	/* requeue this command later */

/*
 * Command Completion Point- Core layers call out from this with completed cmds
 */
void isp_done(XS_T *);

/*
 * Platform Dependent to External to Internal Control Function
 *
 * Assumes locks are held on entry. You should note that with many of
 * these commands and locks may be released while this is occurring.
 *
 * A few notes about some of these functions:
 *
 * ISPCTL_FCLINK_TEST tests to make sure we have good fibre channel link.
 * The argument is a pointer to an integer which is the time, in microseconds,
 * we should wait to see whether we have good link. This test, if successful,
 * lets us know our connection topology and our Loop ID/AL_PA and so on.
 * You can't get anywhere without this.
 *
 * ISPCTL_SCAN_FABRIC queries the name server (if we're on a fabric) for
 * all entities using the FC Generic Services subcommand GET ALL NEXT.
 * For each found entity, an ISPASYNC_FABRICDEV event is generated (see
 * below).
 *
 * ISPCTL_SCAN_LOOP does a local loop scan. This is only done if the connection
 * topology is NL or FL port (private or public loop). Since the Qlogic f/w
 * 'automatically' manages local loop connections, this function essentially
 * notes the arrival, departure, and possible shuffling around of local loop
 * entities. Thus for each arrival and departure this generates an isp_async
 * event of ISPASYNC_PROMENADE (see below).
 *
 * ISPCTL_PDB_SYNC is somewhat misnamed. It actually is the final step, in
 * order, of ISPCTL_FCLINK_TEST, ISPCTL_SCAN_FABRIC, and ISPCTL_SCAN_LOOP.
 * The main purpose of ISPCTL_PDB_SYNC is to complete management of logging
 * and logging out of fabric devices (if one is on a fabric) and then marking
 * the 'loop state' as being ready to now be used for sending commands to
 * devices. Originally fabric name server and local loop scanning were
 * part of this function. It's now been separated to allow for finer control.
 */
typedef enum {
	ISPCTL_RESET_BUS,		/* Reset Bus */
	ISPCTL_RESET_DEV,		/* Reset Device */
	ISPCTL_ABORT_CMD,		/* Abort Command */
	ISPCTL_UPDATE_PARAMS,		/* Update Operating Parameters (SCSI) */
	ISPCTL_FCLINK_TEST,		/* Test FC Link Status */
	ISPCTL_SCAN_FABRIC,		/* (Re)scan Fabric Name Server */
	ISPCTL_SCAN_LOOP,		/* (Re)scan Local Loop */
	ISPCTL_PDB_SYNC,		/* Synchronize Port Database */
	ISPCTL_SEND_LIP,		/* Send a LIP */
	ISPCTL_GET_PORTNAME,		/* get portname from an N-port handle */
	ISPCTL_RUN_MBOXCMD,		/* run a mailbox command */
	ISPCTL_TOGGLE_TMODE,		/* toggle target mode */
	ISPCTL_GET_PDB,			/* get a single port database entry */
	ISPCTL_PLOGX			/* do a port login/logout */
} ispctl_t;
int isp_control(ispsoftc_t *, ispctl_t, void *);


/*
 * Platform Dependent to Internal to External Control Function
 * (each platform must provide such a function)
 *
 * Assumes locks are held.
 *
 * A few notes about some of these functions:
 *
 * ISPASYNC_CHANGE_NOTIFY notifies the outer layer that a change has
 * occurred that invalidates the list of fabric devices known and/or
 * the list of known loop devices. The argument passed is a pointer
 * whose values are defined below  (local loop change, name server
 * change, other). 'Other' may simply be a LIP, or a change in
 * connection topology.
 *
 * ISPASYNC_FABRIC_DEV announces the next element in a list of
 * fabric device names we're getting out of the name server. The
 * argument points to a GET ALL NEXT response structure. The list
 * is known to terminate with an entry that refers to ourselves.
 * One of the main purposes of this function is to allow outer
 * layers, which are OS dependent, to set policy as to which fabric
 * devices might actually be logged into (and made visible) later
 * at ISPCTL_PDB_SYNC time. Since there's a finite number of fabric
 * devices that we can log into (256 less 3 'reserved' for F-port
 * topologies), and fabrics can grow up to 8 million or so entries
 * (24 bits of Port Address, less a wad of reserved spaces), clearly
 * we had better let the OS determine login policy.
 *
 * ISPASYNC_PROMENADE has an argument that is a pointer to an integer which
 * is an index into the portdb in the softc ('target'). Whether that entry's
 * valid tag is set or not says whether something has arrived or departed.
 * The name refers to a favorite pastime of many city dwellers- watching
 * people come and go, talking of Michaelangelo, and so on..
 *
 * ISPASYNC_UNHANDLED_RESPONSE gives outer layers a chance to parse a
 * response queue entry not otherwise handled. The outer layer should
 * return non-zero if it handled it. The 'arg' points to an unmassaged
 * response queue entry.
 */

typedef enum {
	ISPASYNC_NEW_TGT_PARAMS,	/* New Target Parameters Negotiated */
	ISPASYNC_BUS_RESET,		/* Bus Was Reset */
	ISPASYNC_LOOP_DOWN,		/* FC Loop Down */
	ISPASYNC_LOOP_UP,		/* FC Loop Up */
	ISPASYNC_LIP,			/* LIP Received */
	ISPASYNC_LOOP_RESET,		/* Loop Reset Received */
	ISPASYNC_CHANGE_NOTIFY,		/* FC Change Notification */
	ISPASYNC_DEV_ARRIVED,		/* FC Device Arrival */
	ISPASYNC_DEV_CHANGED,		/* FC Device Change */
	ISPASYNC_DEV_STAYED,		/* FC Device Stayed the Same */
	ISPASYNC_DEV_GONE,		/* FC Device Depart */
	ISPASYNC_TARGET_NOTIFY,		/* target asynchronous notification event */
	ISPASYNC_TARGET_ACTION,		/* target action requested */
	ISPASYNC_CONF_CHANGE,		/* Platform Configuration Change */
	ISPASYNC_UNHANDLED_RESPONSE,	/* Unhandled Response Entry */
	ISPASYNC_FW_CRASH,		/* Firmware has crashed */
	ISPASYNC_FW_DUMPED,		/* Firmware crashdump taken */
	ISPASYNC_FW_RESTARTED		/* Firmware has been restarted */
} ispasync_t;
int isp_async(ispsoftc_t *, ispasync_t, void *);

#define	ISPASYNC_CHANGE_PDB	((void *) 0)
#define	ISPASYNC_CHANGE_SNS	((void *) 1)
#define	ISPASYNC_CHANGE_OTHER	((void *) 2)

/*
 * Platform Dependent Error and Debug Printout
 *
 * Generally this is:
 *
 *    void isp_prt(ispsoftc_t *, int level, const char *, ...)
 *
 * but due to compiler differences on different platforms this won't be
 * formally done here. Instead, it goes in each platform definition file.
 */

#define	ISP_LOGALL	0x0	/* log always */
#define	ISP_LOGCONFIG	0x1	/* log configuration messages */
#define	ISP_LOGINFO	0x2	/* log informational messages */
#define	ISP_LOGWARN	0x4	/* log warning messages */
#define	ISP_LOGERR	0x8	/* log error messages */
#define	ISP_LOGDEBUG0	0x10	/* log simple debug messages */
#define	ISP_LOGDEBUG1	0x20	/* log intermediate debug messages */
#define	ISP_LOGDEBUG2	0x40	/* log most debug messages */
#define	ISP_LOGDEBUG3	0x80	/* log high frequency debug messages */
#define	ISP_LOGSANCFG	0x100	/* log SAN configuration */
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
 *	ISP2100_SCRLEN	-	length for the Fibre Channel scratch DMA area
 *
 *	MEMZERO(dst, src)			platform zeroing function
 *	MEMCPY(dst, src, count)			platform copying function
 *	SNPRINTF(buf, bufsize, fmt, ...)	snprintf
 *	USEC_DELAY(usecs)			microsecond spindelay function
 *	USEC_SLEEP(isp, usecs)			microsecond sleep function
 *
 *	NANOTIME_T				nanosecond time type
 *
 *	GET_NANOTIME(NANOTIME_T *)		get current nanotime.
 *
 *	GET_NANOSEC(NANOTIME_T *)		get uint64_t from NANOTIME_T
 *
 *	NANOTIME_SUB(NANOTIME_T *, NANOTIME_T *)
 *						subtract two NANOTIME_T values
 *
 *
 *	MAXISPREQUEST(ispsoftc_t *)	maximum request queue size
 *						for this particular board type
 *
 *	MEMORYBARRIER(ispsoftc_t *, barrier_type, offset, size)
 *
 *		Function/Macro the provides memory synchronization on
 *		various objects so that the ISP's and the system's view
 *		of the same object is consistent.
 *
 *	MBOX_ACQUIRE(ispsoftc_t *)		acquire lock on mailbox regs
 *	MBOX_WAIT_COMPLETE(ispsoftc_t *, mbreg_t *) wait for cmd to be done
 *	MBOX_NOTIFY_COMPLETE(ispsoftc_t *)	notification of mbox cmd donee
 *	MBOX_RELEASE(ispsoftc_t *)		release lock on mailbox regs
 *
 *	FC_SCRATCH_ACQUIRE(ispsoftc_t *)	acquire lock on FC scratch area
 *	FC_SCRATCH_RELEASE(ispsoftc_t *)	acquire lock on FC scratch area
 *
 *	SCSI_GOOD	SCSI 'Good' Status
 *	SCSI_CHECK	SCSI 'Check Condition' Status
 *	SCSI_BUSY	SCSI 'Busy' Status
 *	SCSI_QFULL	SCSI 'Queue Full' Status
 *
 *	XS_T		Platform SCSI transaction type (i.e., command for HBA)
 *	XS_DMA_ADDR_T	Platform PCI DMA Address Type
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
 *	XS_SAVE_SENSE(xs, sp, len)	save sense data
 *
 *	XS_SET_STATE_STAT(isp, sp, xs)	platform dependent interpreter of
 *					response queue entry status bits
 *
 *
 *	DEFAULT_IID(ispsoftc_t *)		Default SCSI initiator ID
 *	DEFAULT_LOOPID(ispsoftc_t *)		Default FC Loop ID
 *	DEFAULT_NODEWWN(ispsoftc_t *)		Default Node WWN
 *	DEFAULT_PORTWWN(ispsoftc_t *)		Default Port WWN
 *	DEFAULT_FRAMESIZE(ispsoftc_t *)		Default Frame Size
 *	DEFAULT_EXEC_THROTTLE(ispsoftc_t *) Default Execution Throttle
 *		These establish reasonable defaults for each platform.
 * 		These must be available independent of card NVRAM and are
 *		to be used should NVRAM not be readable.
 *
 *	ISP_NODEWWN(ispsoftc_t *)		FC Node WWN to use
 *	ISP_PORTWWN(ispsoftc_t *)		FC Port WWN to use
 *
 *		These are to be used after NVRAM is read. The tags
 *		in fcparam.isp_ww{n,p}n_nvram reflect the values
 *		read from NVRAM (possibly corrected for card botches).
 *		Each platform can take that information and override
 *		it or ignore and return the Node and Port WWNs to be
 * 		used when sending the Qlogic f/w the Initialization
 *		Control Block.
 *
 *	(XXX these do endian specific transformations- in transition XXX)
 *
 *	ISP_IOXPUT_8(ispsoftc_t *, uint8_t srcval, uint8_t *dstptr)
 *	ISP_IOXPUT_16(ispsoftc_t *, uint16_t srcval, uint16_t *dstptr)
 *	ISP_IOXPUT_32(ispsoftc_t *, uint32_t srcval, uint32_t *dstptr)
 *
 *	ISP_IOXGET_8(ispsoftc_t *, uint8_t *srcptr, uint8_t dstrval)
 *	ISP_IOXGET_16(ispsoftc_t *, uint16_t *srcptr, uint16_t dstrval)
 *	ISP_IOXGET_32(ispsoftc_t *, uint32_t *srcptr, uint32_t dstrval)
 *
 *	ISP_SWIZZLE_NVRAM_WORD(ispsoftc_t *, uint16_t *)
 *	ISP_SWIZZLE_NVRAM_LONG(ispsoftc_t *, uint32_t *)
 *	ISP_SWAP16(ispsoftc_t *, uint16_t srcval)
 *	ISP_SWAP32(ispsoftc_t *, uint32_t srcval)
 */

#endif	/* _ISPVAR_H */
