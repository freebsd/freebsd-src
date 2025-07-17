/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *  Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
 *  Copyright (c) 1997-2009 by Matthew Jacob
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
 *
 */
/*
 * Soft Definitions for Qlogic ISP SCSI adapters.
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

#define	ISP_CORE_VERSION_MAJOR	7
#define	ISP_CORE_VERSION_MINOR	0

/*
 * Vector for bus specific code to provide specific services.
 */
typedef struct ispsoftc ispsoftc_t;
struct ispmdvec {
	void		(*dv_run_isr) (ispsoftc_t *);
	uint32_t	(*dv_rd_reg) (ispsoftc_t *, int);
	void		(*dv_wr_reg) (ispsoftc_t *, int, uint32_t);
	int		(*dv_mbxdma) (ispsoftc_t *);
	int		(*dv_send_cmd) (ispsoftc_t *, void *, void *, uint32_t);
	int		(*dv_irqsetup) (ispsoftc_t *);
	void		(*dv_dregs) (ispsoftc_t *, const char *);
	const void *	dv_ispfw;	/* ptr to f/w of ispfw(4)*/
};

/*
 * Overall parameters
 */
#define	MAX_TARGETS		16
#ifndef	MAX_FC_TARG
#define	MAX_FC_TARG		1024
#endif
#define	ISP_MAX_TARGETS(isp)	MAX_FC_TARG
#define	ISP_MAX_IRQS		3

/*
 * Macros to access ISP registers through bus specific layers-
 * mostly wrappers to vector through the mdvec structure.
 */
#define	ISP_RUN_ISR(isp)	\
	(*(isp)->isp_mdvec->dv_run_isr)(isp)

#define	ISP_READ(isp, reg)	\
	(*(isp)->isp_mdvec->dv_rd_reg)((isp), (reg))

#define	ISP_WRITE(isp, reg, val)	\
	(*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), (val))

#define	ISP_MBOXDMASETUP(isp)	\
	(*(isp)->isp_mdvec->dv_mbxdma)((isp))

#define	ISP_SEND_CMD(isp, qe, segp, nseg)	\
	(*(isp)->isp_mdvec->dv_send_cmd)((isp), (qe), (segp), (nseg))

#define	ISP_IRQSETUP(isp)	\
	(((isp)->isp_mdvec->dv_irqsetup) ? (*(isp)->isp_mdvec->dv_irqsetup)(isp) : 0)
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
#define	SYNC_IFORDEV	6	/* synchrounous IOCB, sync for ISP */
#define	SYNC_IFORCPU	7	/* synchrounous IOCB, sync for CPU */

/*
 * Request/Response Queue defines and macros.
 */
/* This is the size of a queue entry (request and response) */
#define	QENTRY_LEN			64
/*
 * Hardware requires queue lengths of at least 8 elements.  Driver requires
 * lengths to be a power of two, and request queue of at least 256 elements.
 */
#define	RQUEST_QUEUE_LEN(x)		8192
#define	RESULT_QUEUE_LEN(x)		1024
#define	ATIO_QUEUE_LEN(x)		1024
#define	ISP_QUEUE_ENTRY(q, idx)		(((uint8_t *)q) + ((size_t)(idx) * QENTRY_LEN))
#define	ISP_QUEUE_SIZE(n)		((size_t)(n) * QENTRY_LEN)
#define	ISP_NXT_QENTRY(idx, qlen)	(((idx) + 1) & ((qlen)-1))
#define	ISP_QFREE(in, out, qlen)	((out - in - 1) & ((qlen) - 1))
#define	ISP_QAVAIL(isp)	\
	ISP_QFREE(isp->isp_reqidx, isp->isp_reqodx, RQUEST_QUEUE_LEN(isp))

#define	ISP_ADD_REQUEST(isp, nxti)						\
	MEMORYBARRIER(isp, SYNC_REQUEST, isp->isp_reqidx, QENTRY_LEN, -1);	\
	ISP_WRITE(isp, BIU2400_REQINP, nxti);					\
	isp->isp_reqidx = nxti

#define	ISP_SYNC_REQUEST(isp)								\
	MEMORYBARRIER(isp, SYNC_REQUEST, isp->isp_reqidx, QENTRY_LEN, -1);		\
	isp->isp_reqidx = ISP_NXT_QENTRY(isp->isp_reqidx, RQUEST_QUEUE_LEN(isp));	\
	ISP_WRITE(isp, BIU2400_REQINP, isp->isp_reqidx)

/*
 * Fibre Channel Specifics
 */
#define	NPH_RESERVED		0x7F0	/* begin of reserved N-port handles */
#define	NPH_MGT_ID		0x7FA	/* Management Server Special ID */
#define	NPH_SNS_ID		0x7FC	/* SNS Server Special ID */
#define	NPH_FABRIC_CTLR		0x7FD	/* Fabric Controller (0xFFFFFD) */
#define	NPH_FL_ID		0x7FE	/* F Port Special ID (0xFFFFFE) */
#define	NPH_IP_BCST		0x7FF	/* IP Broadcast Special ID (0xFFFFFF) */
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
 * Limit for (2K login) N-port handle amounts
 */
#define	MAX_NPORT_HANDLE	2048

/*
 * Special Constants
 */
#define	INI_NONE    		((uint64_t) 0)
#define	ISP_NOCHAN		0xff

/*
 * Special Port IDs
 */
#define	MANAGEMENT_PORT_ID	0xFFFFFA
#define	SNS_PORT_ID		0xFFFFFC
#define	FABRIC_PORT_ID		0xFFFFFE
#define	PORT_ANY		0xFFFFFF
#define	PORT_NONE		0
#define	VALID_PORT(port)	(port != PORT_NONE && port != PORT_ANY)
#define	DOMAIN_CONTROLLER_BASE	0xFFFC00
#define	DOMAIN_CONTROLLER_END	0xFFFCFF

/*
 * Command Handles
 *
 * Most QLogic initiator or target have 32 bit handles associated with them.
 * We want to have a quick way to index back and forth between a local SCSI
 * command context and what the firmware is passing back to us. We also
 * want to avoid working on stale information. This structure handles both
 * at the expense of some local memory.
 *
 * The handle is architected thusly:
 *
 *	0 means "free handle"
 *	bits  0..12 index commands
 *	bits 13..15 bits index usage
 *	bits 16..31 contain a rolling sequence
 *
 * 
 */
typedef struct {
	void *		cmd;	/* associated command context */
	uint32_t	handle;	/* handle associated with this command */
} isp_hdl_t;
#define	ISP_HANDLE_FREE		0x00000000
#define	ISP_HANDLE_CMD_MASK	0x00003fff
#define	ISP_HANDLE_USAGE_MASK	0x0000c000
#define	ISP_HANDLE_USAGE_SHIFT	14
#define	ISP_H2HT(hdl)	((hdl & ISP_HANDLE_USAGE_MASK) >> ISP_HANDLE_USAGE_SHIFT)
#	define	ISP_HANDLE_NONE		0
#	define	ISP_HANDLE_INITIATOR	1
#	define	ISP_HANDLE_TARGET	2
#	define	ISP_HANDLE_CTRL		3
#define	ISP_HANDLE_SEQ_MASK	0xffff0000
#define	ISP_HANDLE_SEQ_SHIFT	16
#define	ISP_H2SEQ(hdl)	((hdl & ISP_HANDLE_SEQ_MASK) >> ISP_HANDLE_SEQ_SHIFT)
#define	ISP_HANDLE_MAX		(ISP_HANDLE_CMD_MASK + 1)
#define	ISP_HANDLE_RESERVE	256
#define	ISP_HANDLE_NUM(isp)	((isp)->isp_maxcmds + ISP_HANDLE_RESERVE)
#define	ISP_VALID_HANDLE(isp, hdl)	\
	((ISP_H2HT(hdl) == ISP_HANDLE_INITIATOR || \
	  ISP_H2HT(hdl) == ISP_HANDLE_TARGET || \
	  ISP_H2HT(hdl) == ISP_HANDLE_CTRL) && \
	 ((hdl) & ISP_HANDLE_CMD_MASK) < ISP_HANDLE_NUM(isp) && \
	 (hdl) == ((isp)->isp_xflist[(hdl) & ISP_HANDLE_CMD_MASK].handle))


/*
 * FC Port Database entry.
 *
 * It has a handle that the f/w uses to address commands to a device.
 * This handle's value may be assigned by the firmware (e.g., for local loop
 * devices) or by the driver (e.g., for fabric devices).
 *
 * It has a state. If the state if VALID, that means that we've logged into
 * the device.
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
 * When we get done searching the local loop, we then search similarly for
 * a list of devices we've gotten from the fabric name controller (if we're
 * on a fabric). VALID marking is also done similarly.
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
 */
typedef struct {
	/*
	 * This is the handle that the firmware needs in order for us to
	 * send commands to the device. For pre-24XX cards, this would be
	 * the 'loopid'.
	 */
	uint16_t	handle;

	/*
	 * PRLI word 0 contains the Establish Image Pair bit, which is
	 * important for knowing when to reset the CRN.
	 *
	 * PRLI word 3 parameters contains role as well as other things.
	 *
	 * The state is the current state of this entry.
	 *
	 * The is_target is the current state of target on this port.
	 *
	 * The is_initiator is the current state of initiator on this port.
	 *
	 * Portid is obvious, as are node && port WWNs. The new_role and
	 * new_portid is for when we are pending a change.
	 */
	uint16_t	prli_word0;		/* PRLI parameters */
	uint16_t	prli_word3;		/* PRLI parameters */
	uint16_t	new_prli_word0;		/* Incoming new PRLI parameters */
	uint16_t	new_prli_word3;		/* Incoming new PRLI parameters */
	uint16_t			: 12,
			probational	: 1,
			state		: 3;
	uint32_t			: 6,
			is_target	: 1,
			is_initiator	: 1,
			portid		: 24;
	uint32_t
					: 8,
			new_portid	: 24;
	uint64_t	node_wwn;
	uint64_t	port_wwn;
	uint32_t	gone_timer;
} fcportdb_t;

#define	FC_PORTDB_STATE_NIL		0	/* Empty DB slot */
#define	FC_PORTDB_STATE_DEAD		1	/* Was valid, but no more. */
#define	FC_PORTDB_STATE_CHANGED		2	/* Was valid, but changed. */
#define	FC_PORTDB_STATE_NEW		3	/* Logged in, not announced. */
#define	FC_PORTDB_STATE_ZOMBIE		4	/* Invalid, but announced. */
#define	FC_PORTDB_STATE_VALID		5	/* Valid */

#define	FC_PORTDB_TGT(isp, bus, pdb)		(int)(lp - FCPARAM(isp, bus)->portdb)

/*
 * FC card specific information
 *
 * This structure is replicated across multiple channels for multi-id
 * capapble chipsets, with some entities different on a per-channel basis.
 */

typedef struct {
	int			isp_gbspeed;		/* Connection speed */
	int			isp_linkstate;		/* Link state */
	int			isp_fwstate;		/* ISP F/W state */
	int			isp_loopstate;		/* Loop State */
	int			isp_topo;		/* Connection Type */

	uint32_t				: 4,
				fctape_enabled	: 1,
				sendmarker	: 1,
				role		: 2,
				isp_portid	: 24;	/* S_ID */

	uint16_t		isp_fwoptions;
	uint16_t		isp_xfwoptions;
	uint16_t		isp_zfwoptions;
	uint16_t		isp_loopid;		/* hard loop id */
	uint16_t		isp_sns_hdl;		/* N-port handle for SNS */
	uint16_t		isp_lasthdl;		/* only valid for channel 0 */
	uint16_t		isp_fabric_params;
	uint16_t		isp_login_hdl;		/* Logging in handle */
	uint8_t			isp_retry_delay;
	uint8_t			isp_retry_count;
	int			isp_use_gft_id;		/* Use GFT_ID */
	int			isp_use_gff_id;		/* Use GFF_ID */

	uint32_t		flash_data_addr;
	uint32_t		fw_flashrev[4]; /* Flash F/W revision */
	uint32_t		fw_ispfwrev[4]; /* ispfw(4) F/W revision */
	char			fw_version_flash[12];
	char			fw_version_ispfw[12];
	char			fw_version_run[12];
	uint32_t		fw_ability_mask;
	uint16_t		max_supported_speed;

	/*
	 * FLT
	 */
	uint16_t		flt_length;
	uint32_t		flt_region_entries;
	uint32_t		flt_region_flt;
	uint32_t		flt_region_fdt;
	uint32_t		flt_region_boot;
	uint32_t		flt_region_boot_sec;
	uint32_t		flt_region_fw;
	uint32_t		flt_region_fw_sec;
	uint32_t		flt_region_vpd_nvram;
	uint32_t		flt_region_vpd_nvram_sec;
	uint32_t		flt_region_vpd;
	uint32_t		flt_region_vpd_sec;
	uint32_t		flt_region_nvram;
	uint32_t		flt_region_nvram_sec;
	uint32_t		flt_region_npiv_conf;
	uint32_t		flt_region_gold_fw;
	uint32_t		flt_region_fcp_prio;
	uint32_t		flt_region_bootload;
	uint32_t		flt_region_img_status_pri;
	uint32_t		flt_region_img_status_sec;
	uint32_t		flt_region_aux_img_status_pri;
	uint32_t		flt_region_aux_img_status_sec;

	/*
	 * Current active WWNN/WWPN
	 */
	uint64_t		isp_wwnn;
	uint64_t		isp_wwpn;

	/*
	 * NVRAM WWNN/WWPN
	 */
	uint64_t		isp_wwnn_nvram;
	uint64_t		isp_wwpn_nvram;

	/*
	 * Our Port Data Base
	 */
	fcportdb_t		portdb[MAX_FC_TARG];

	/*
	 * Scratch DMA mapped in area to fetch Port Database stuff, etc.
	 */
	void *			isp_scratch;
	XS_DMA_ADDR_T		isp_scdma;

	uint8_t			isp_scanscratch[ISP_FC_SCRLEN];
} fcparam;

/*
 * Image status
 */
struct isp_image_status{
	uint8_t		image_status_mask;
	uint16_t	generation;
	uint8_t		ver_major;
	uint8_t		ver_minor;
	uint8_t		bitmap;		/* 28xx only */
	uint8_t		reserved[2];
	uint32_t	checksum;
	uint32_t	signature;
} __packed;

/* 28xx aux image status bitmap values */
#define ISP28XX_AUX_IMG_BOARD_CONFIG		0x1
#define ISP28XX_AUX_IMG_VPD_NVRAM		0x2
#define ISP28XX_AUX_IMG_NPIV_CONFIG_0_1		0x4
#define ISP28XX_AUX_IMG_NPIV_CONFIG_2_3		0x8
#define ISP28XX_AUX_IMG_NVME_PARAMS		0x10

/*
 * Active regions
 */
struct active_regions {
	uint8_t global;
	struct {
		uint8_t	board_config;
		uint8_t	vpd_nvram;
		uint8_t	npiv_config_0_1;
		uint8_t	npiv_config_2_3;
		uint8_t	nvme_params;
	} aux;
};

#define	FW_CONFIG_WAIT		0
#define	FW_WAIT_LINK		1
#define	FW_WAIT_LOGIN		2
#define	FW_READY		3
#define	FW_LOSS_OF_SYNC		4
#define	FW_ERROR		5
#define	FW_REINIT		6
#define	FW_NON_PART		7

#define	LOOP_NIL		0
#define	LOOP_HAVE_LINK		1
#define	LOOP_HAVE_ADDR		2
#define	LOOP_TESTING_LINK	3
#define	LOOP_LTEST_DONE		4
#define	LOOP_SCANNING_LOOP	5
#define	LOOP_LSCAN_DONE		6
#define	LOOP_SCANNING_FABRIC	7
#define	LOOP_FSCAN_DONE		8
#define	LOOP_SYNCING_PDB	9
#define	LOOP_READY		10

#define	TOPO_NL_PORT		0
#define	TOPO_FL_PORT		1
#define	TOPO_N_PORT		2
#define	TOPO_F_PORT		3
#define	TOPO_PTP_STUB		4

#define TOPO_IS_FABRIC(x)	((x) == TOPO_FL_PORT || (x) == TOPO_F_PORT)

#define FCP_AL_DA_ALL		0xFF
#define FCP_AL_PA(fcp) ((uint8_t)(fcp->isp_portid))
#define FCP_IS_DEST_ALPD(fcp, alpd) (FCP_AL_PA((fcp)) == FCP_AL_DA_ALL || FCP_AL_PA((fcp)) == alpd)

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

	fcparam			*isp_param;	/* Per-channel storage. */
	uint16_t		isp_fwattr;	/* firmware attributes */
	uint16_t		isp_fwattr_h;	/* firmware attributes */
	uint16_t		isp_fwattr_ext[2]; /* firmware attributes */
	uint16_t		isp_fwrev[3];	/* Loaded F/W revision */
	uint16_t		isp_maxcmds;	/* max possible I/O cmds */
	uint16_t		isp_nchan;	/* number of channels */
	uint16_t		isp_dblev;	/* debug log mask */
	uint32_t		isp_did;	/* DID */
	uint8_t			isp_type;	/* HBA Chip Type */
	uint8_t			isp_revision;	/* HBA Chip H/W Revision */
	uint8_t			isp_nirq;	/* number of IRQs */
	uint8_t			isp_port;	/* physical port on a card */
	uint32_t		isp_confopts;	/* config options */

	/*
	 * Volatile state
	 */
	volatile u_int		isp_mboxbsy;	/* mailbox command active */
	volatile u_int		isp_state;
	volatile uint32_t	isp_reqodx;	/* index of last ISP pickup */
	volatile uint32_t	isp_reqidx;	/* index of next request */
	volatile uint32_t	isp_resodx;	/* index of next result */
	volatile uint32_t	isp_atioodx;	/* index of next ATIO */
	volatile uint32_t	isp_obits;	/* mailbox command output */
	volatile uint32_t	isp_serno;	/* rolling serial number */
	volatile uint16_t	isp_mboxtmp[MAX_MAILBOX];
	volatile uint16_t	isp_seqno;	/* running sequence number */
	u_int			isp_rqovf;	/* request queue overflow */

	/*
	 * Active commands are stored here, indexed by handle functions.
	 */
	isp_hdl_t		*isp_xflist;
	isp_hdl_t		*isp_xffree;

	/*
	 * DMA mapped in area for synchronous IOCB requests.
	 */
	void *			isp_iocb;
	XS_DMA_ADDR_T		isp_iocb_dma;

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

#define	FCPARAM(isp, chan)	(&(isp)->isp_param[(chan)])

#define	ISP_SET_SENDMARKER(isp, chan, val)	\
    FCPARAM(isp, chan)->sendmarker = val	\

#define	ISP_TST_SENDMARKER(isp, chan)		\
    (FCPARAM(isp, chan)->sendmarker != 0)

/*
 * ISP Driver Run States
 */
#define	ISP_NILSTATE	0
#define	ISP_CRASHED	1
#define	ISP_RESETSTATE	2
#define	ISP_INITSTATE	3
#define	ISP_RUNSTATE	4

/*
 * ISP Runtime Configuration Options
 */
#define	ISP_CFG_FULL_DUPLEX	0x01	/* Full Duplex (Fibre Channel only) */
#define	ISP_CFG_PORT_PREF	0x0e	/* Mask for Port Prefs (all FC except 2100) */
#define	ISP_CFG_PORT_DEF	0x00	/* prefer connection type from NVRAM */
#define	ISP_CFG_LPORT_ONLY	0x02	/* insist on {N/F}L-Port connection */
#define	ISP_CFG_NPORT_ONLY	0x04	/* insist on {N/F}-Port connection */
#define	ISP_CFG_LPORT		0x06	/* prefer {N/F}L-Port connection */
#define	ISP_CFG_NPORT		0x08	/* prefer {N/F}-Port connection */
#define	ISP_CFG_1GB		0x10	/* force 1Gb connection (23XX only) */
#define	ISP_CFG_2GB		0x20	/* force 2Gb connection (23XX only) */
#define	ISP_CFG_NONVRAM		0x40	/* ignore NVRAM */
#define	ISP_CFG_NORELOAD	0x80	/* don't download f/w */
#define	ISP_CFG_NOFCTAPE	0x100	/* disable FC-Tape */
#define	ISP_CFG_FCTAPE		0x200	/* enable FC-Tape */
#define	ISP_CFG_OWNFSZ		0x400	/* override NVRAM frame size */
#define	ISP_CFG_OWNLOOPID	0x800	/* override NVRAM loopid */
#define	ISP_CFG_4GB		0x2000	/* force 4Gb connection (24XX only) */
#define	ISP_CFG_8GB		0x4000	/* force 8Gb connection (25XX only) */
#define	ISP_CFG_16GB		0x8000	/* force 16Gb connection (26XX only) */
#define	ISP_CFG_32GB		0x10000	/* force 32Gb connection (27XX only) */
#define	ISP_CFG_64GB		0x20000	/* force 64Gb connection (28XX only) */
#define	ISP_CFG_FWLOAD_FORCE	0x40000 /* Prefer ispfw(4) even if older */

/*
 * For each channel, the outer layers should know what role that channel
 * will take: ISP_ROLE_NONE, ISP_ROLE_INITIATOR, ISP_ROLE_TARGET,
 * ISP_ROLE_BOTH.
 *
 * If you set ISP_ROLE_NONE, the cards will be reset, new firmware loaded,
 * NVRAM read, and defaults set, but any further initialization (e.g.
 * INITIALIZE CONTROL BLOCK commands for 2X00 cards) won't be done.
 *
 * If INITIATOR MODE isn't set, attempts to run commands will be stopped
 * at isp_start and completed with the equivalent of SELECTION TIMEOUT.
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
/*
 * Counterintuitively, we prefer to default to role 'none'
 * if we are enable target mode support. This gives us the
 * maximum flexibility as to which port will do what.
 */
#ifdef	ISP_TARGET_MODE
#define	ISP_DEFAULT_ROLES	ISP_ROLE_NONE
#else
#define	ISP_DEFAULT_ROLES	ISP_ROLE_INITIATOR
#endif
#endif


/*
 * Firmware related defines
 */
#define	ISP_CODE_ORG			0x1000	/* default f/w code start */
#define	ISP_CODE_ORG_2300		0x0800	/* ..except for 2300s */
#define	ISP_CODE_ORG_2400		0x100000 /* ..and 2400s */
#define	ISP_FW_REV(maj, min, mic)	(((maj) << 16) | ((min) << 8) | (mic))
#define	ISP_FW_MAJOR(code)		(((code) >> 16) & 0xff)
#define	ISP_FW_MINOR(code)		(((code) >> 8) & 0xff)
#define	ISP_FW_MICRO(code)		((code) & 0xff)
#define	ISP_FW_REVX(xp)			(((xp)[0] << 16) | ((xp)[1] << 8) | (xp)[2])
#define	ISP_FW_MAJORX(xp)		(xp[0])
#define	ISP_FW_MINORX(xp)		(xp[1])
#define	ISP_FW_MICROX(xp)		(xp[2])
#define	ISP_FW_NEWER_THAN(i, major, minor, micro)		\
	(ISP_FW_REVX(i) > ISP_FW_REV(major, minor, micro))
#define	ISP_FW_OLDER_THAN(i, major, minor, micro)		\
	(ISP_FW_REVX(i) < ISP_FW_REV(major, minor, micro))
#define	ISP_FW_NEWER_THANX(i, j)		\
	(ISP_FW_REVX(i) > ISP_FW_REVX(j))
#define	ISP_FW_OLDER_THANX(i, j)		\
	(ISP_FW_REVX(i) < ISP_FW_REVX(j))

/*
 * Chip Types
 */
#define	ISP_HA_FC_2400		0x04
#define	ISP_HA_FC_2500		0x05
#define	ISP_HA_FC_2600		0x06
#define	ISP_HA_FC_2700		0x07
#define	ISP_HA_FC_2800		0x08

#define	IS_25XX(isp)	((isp)->isp_type >= ISP_HA_FC_2500)
#define	IS_26XX(isp)	((isp)->isp_type >= ISP_HA_FC_2600)
#define	IS_27XX(isp)	((isp)->isp_type >= ISP_HA_FC_2700)
#define	IS_28XX(isp)	((isp)->isp_type >= ISP_HA_FC_2800)

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
 * function return status codes
 */
#define MBS_MASK		0x3fff

#define ISP_SUCCESS		(MBOX_COMMAND_COMPLETE & MBS_MASK)
#define ISP_INVALID_COMMAND	(MBOX_INVALID_COMMAND & MBS_MASK)
#define ISP_INTERFACE_ERROR	(MBOX_HOST_INTERFACE_ERROR & MBS_MASK)
#define ISP_TEST_FAILED		(MBOX_TEST_FAILED & MBS_MASK)
#define ISP_COMMAND_ERROR	(MBOX_COMMAND_ERROR & MBS_MASK)
#define ISP_PARAMETER_ERROR	(MBOX_COMMAND_PARAMETER_ERROR & MBS_MASK)
#define ISP_PORT_ID_USED	(MBOX_PORT_ID_USED & MBS_MASK)
#define ISP_LOOP_ID_USED	(MBOX_LOOP_ID_USED & MBS_MASK)
#define ISP_ALL_IDS_IN_USE	(MBOX_ALL_IDS_IN_USE & MBS_MASK)
#define ISP_NOT_LOGGED_IN	(MBOX_NOT_LOGGED_IN & MBS_MASK)

#define ISP_FUNCTION_TIMEOUT		0x100
#define ISP_FUNCTION_PARAMETER_ERROR	0x101
#define ISP_FUNCTION_FAILED		0x102
#define ISP_MEMORY_ALLOC_FAILED		0x103
#define ISP_LOCK_TIMEOUT		0x104
#define ISP_ABORTED			0x105
#define ISP_SUSPENDED			0x106
#define ISP_BUSY			0x107
#define ISP_ALREADY_REGISTERED		0x109
#define ISP_OS_TIMER_EXPIRED		0x10a
#define ISP_ERR_NO_QPAIR		0x10b
#define ISP_ERR_NOT_FOUND		0x10c
#define ISP_ERR_FROM_FW			0x10d

/*
 * Core System Function Prototypes
 */

/*
 * Reset Hardware. Totally. Assumes that you'll follow this with a call to isp_init.
 */
void isp_reset(ispsoftc_t *, int);

/*
 * Initialize Hardware to known state
 */
void isp_init(ispsoftc_t *);

/*
 * Reset the ISP and call completion for any orphaned commands.
 */
int isp_reinit(ispsoftc_t *, int);

/*
 * Shutdown hardware after use.
 */
void isp_shutdown(ispsoftc_t *);

/*
 * Internal Interrupt Service Routine
 */
#ifdef	ISP_TARGET_MODE
void isp_intr_atioq(ispsoftc_t *);
#endif
void isp_intr_async(ispsoftc_t *, uint16_t event);
void isp_intr_mbox(ispsoftc_t *, uint16_t mbox0);
void isp_intr_respq(ispsoftc_t *);


/*
 * Command Entry Point- Platform Dependent layers call into this
 */
int isp_start(XS_T *);

/* these values are what isp_start returns */
#define	CMD_COMPLETE	101	/* command completed */
#define	CMD_EAGAIN	102	/* busy- maybe retry later */
#define	CMD_RQLATER	103	/* requeue this command later */

/*
 * Command Completion Point- Core layers call out from this with completed cmds
 */
void isp_done(XS_T *);

/*
 * Platform Dependent to External to Internal Control Function
 *
 * Assumes locks are held on entry. You should note that with many of
 * these commands locks may be released while this function is called.
 *
 * ... ISPCTL_RESET_BUS, int channel);
 *        Reset BUS on this channel
 * ... ISPCTL_RESET_DEV, int channel, int target);
 *        Reset Device on this channel at this target.
 * ... ISPCTL_ABORT_CMD, XS_T *xs);
 *        Abort active transaction described by xs.
 * ... IPCTL_UPDATE_PARAMS);
 *        Update any operating parameters (speed, etc.)
 * ... ISPCTL_FCLINK_TEST, int channel);
 *        Test FC link status on this channel
 * ... ISPCTL_SCAN_LOOP, int channel);
 *        Scan local loop on this channel
 * ... ISPCTL_SCAN_FABRIC, int channel);
 *        Scan fabric on this channel
 * ... ISPCTL_PDB_SYNC, int channel);
 *        Synchronize port database on this channel
 * ... ISPCTL_SEND_LIP, int channel);
 *        Send a LIP on this channel
 * ... ISPCTL_GET_NAMES, int channel, int np, uint64_t *wwnn, uint64_t *wwpn)
 *        Get a WWNN/WWPN for this N-port handle on this channel
 * ... ISPCTL_RUN_MBOXCMD, mbreg_t *mbp)
 *        Run this mailbox command
 * ... ISPCTL_GET_PDB, int channel, int nphandle, isp_pdb_t *pdb)
 *        Get PDB on this channel for this N-port handle
 * ... ISPCTL_PLOGX, isp_plcmd_t *)
 *        Performa a port login/logout
 * ... ISPCTL_CHANGE_ROLE, int channel, int role);
 *        Change role of specified channel
 *
 * ISPCTL_PDB_SYNC is somewhat misnamed. It actually is the final step, in
 * order, of ISPCTL_FCLINK_TEST, ISPCTL_SCAN_LOOP, and ISPCTL_SCAN_FABRIC.
 * The main purpose of ISPCTL_PDB_SYNC is to complete management of logging
 * and logging out of fabric devices (if one is on a fabric) and then marking
 * the 'loop state' as being ready to now be used for sending commands to
 * devices.
 */
typedef enum {
	ISPCTL_RESET_BUS,
	ISPCTL_RESET_DEV,
	ISPCTL_ABORT_CMD,
	ISPCTL_UPDATE_PARAMS,
	ISPCTL_FCLINK_TEST,
	ISPCTL_SCAN_FABRIC,
	ISPCTL_SCAN_LOOP,
	ISPCTL_PDB_SYNC,
	ISPCTL_SEND_LIP,
	ISPCTL_GET_NAMES,
	ISPCTL_RUN_MBOXCMD,
	ISPCTL_GET_PDB,
	ISPCTL_PLOGX,
	ISPCTL_CHANGE_ROLE
} ispctl_t;
int isp_control(ispsoftc_t *, ispctl_t, ...);

/*
 * Platform Dependent to Internal to External Control Function
 */

typedef enum {
	ISPASYNC_LOOP_DOWN,		/* FC Loop Down */
	ISPASYNC_LOOP_UP,		/* FC Loop Up */
	ISPASYNC_LIP,			/* FC LIP Received */
	ISPASYNC_LOOP_RESET,		/* FC Loop Reset Received */
	ISPASYNC_CHANGE_NOTIFY,		/* FC Change Notification */
	ISPASYNC_DEV_ARRIVED,		/* FC Device Arrived */
	ISPASYNC_DEV_CHANGED,		/* FC Device Changed */
	ISPASYNC_DEV_STAYED,		/* FC Device Stayed */
	ISPASYNC_DEV_GONE,		/* FC Device Departure */
	ISPASYNC_TARGET_NOTIFY,		/* All target async notification */
	ISPASYNC_TARGET_NOTIFY_ACK,	/* All target notify ack required */
	ISPASYNC_TARGET_ACTION,		/* All target action requested */
	ISPASYNC_FW_CRASH,		/* All Firmware has crashed */
	ISPASYNC_FW_RESTARTED		/* All Firmware has been restarted */
} ispasync_t;
void isp_async(ispsoftc_t *, ispasync_t, ...);

#define	ISPASYNC_CHANGE_PDB	0
#define	ISPASYNC_CHANGE_SNS	1
#define	ISPASYNC_CHANGE_OTHER	2

/*
 * Platform Dependent Error and Debug Printout
 *
 * Two required functions for each platform must be provided:
 *
 *    void isp_prt(ispsoftc_t *, int level, const char *, ...)
 *    void isp_xs_prt(ispsoftc_t *, XS_T *, int level, const char *, ...)
 *
 * but due to compiler differences on different platforms this won't be
 * formally defined here. Instead, they go in each platform definition file.
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
#define	ISP_LOG_SANCFG	0x100	/* log SAN configuration */
#define	ISP_LOG_CWARN	0x200	/* log SCSI command "warnings" (e.g., check conditions) */
#define	ISP_LOG_WARN1	0x400	/* log WARNS we might be interested at some time */
#define	ISP_LOGTINFO	0x1000	/* log informational messages (target mode) */
#define	ISP_LOGTDEBUG0	0x2000	/* log simple debug messages (target mode) */
#define	ISP_LOGTDEBUG1	0x4000	/* log intermediate debug messages (target) */
#define	ISP_LOGTDEBUG2	0x8000	/* log all debug messages (target) */

/*
 * Each Platform provides it's own isposinfo substructure of the ispsoftc
 * defined above.
 *
 * Each platform must also provide the following macros/defines:
 *
 *
 *	ISP_FC_SCRLEN				FC scratch area DMA length
 *
 *	ISP_MEMZERO(dst, src)			platform zeroing function
 *	ISP_MEMCPY(dst, src, count)		platform copying function
 *	ISP_SNPRINTF(buf, bufsize, fmt, ...)	snprintf
 *	ISP_DELAY(usecs)			microsecond spindelay function
 *	ISP_SLEEP(isp, usecs)			microsecond sleep function
 *
 *	ISP_INLINE				___inline or not- depending on how
 *						good your debugger is
 *	ISP_MIN					shorthand for ((a) < (b))? (a) : (b)
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
 *	MAXISPREQUEST(ispsoftc_t *)		maximum request queue size
 *						for this particular board type
 *
 *	MEMORYBARRIER(ispsoftc_t *, barrier_type, offset, size, chan)
 *
 *		Function/Macro the provides memory synchronization on
 *		various objects so that the ISP's and the system's view
 *		of the same object is consistent.
 *
 *	FC_SCRATCH_ACQUIRE(ispsoftc_t *, chan)	acquire lock on FC scratch area
 *						return -1 if you cannot
 *	FC_SCRATCH_RELEASE(ispsoftc_t *, chan)	acquire lock on FC scratch area
 *
 *	FCP_NEXT_CRN(ispsoftc_t *, XS_T *, rslt, channel, target, lun)	generate the next command reference number. XS_T * may be null.
 *
 *	SCSI_GOOD	SCSI 'Good' Status
 *	SCSI_CHECK	SCSI 'Check Condition' Status
 *	SCSI_BUSY	SCSI 'Busy' Status
 *	SCSI_QFULL	SCSI 'Queue Full' Status
 *
 *	XS_T			Platform SCSI transaction type (i.e., command for HBA)
 *	XS_DMA_ADDR_T		Platform PCI DMA Address Type
 *	XS_GET_DMA64_SEG(..)	Get 64 bit dma segment list value
 *	XS_ISP(xs)		gets an instance out of an XS_T
 *	XS_CHANNEL(xs)		gets the channel (bus # for DUALBUS cards) ""
 *	XS_TGT(xs)		gets the target ""
 *	XS_LUN(xs)		gets the lun ""
 *	XS_CDBP(xs)		gets a pointer to the scsi CDB ""
 *	XS_CDBLEN(xs)		gets the CDB's length ""
 *	XS_XFRLEN(xs)		gets the associated data transfer length ""
 *	XS_XFRIN(xs)		gets IN direction
 *	XS_XFROUT(xs)		gets OUT direction
 *	XS_TIME(xs)		gets the time (in seconds) for this command
 *	XS_GET_RESID(xs)	gets the current residual count
 *	XS_GET_RESID(xs, resid)	sets the current residual count
 *	XS_STSP(xs)		gets a pointer to the SCSI status byte ""
 *	XS_SNSP(xs)		gets a pointer to the associate sense data
 *	XS_TOT_SNSLEN(xs)	gets the total length of sense data storage
 *	XS_CUR_SNSLEN(xs)	gets the currently used length of sense data storage
 *	XS_SNSKEY(xs)		dereferences XS_SNSP to get the current stored Sense Key
 *	XS_SNSASC(xs)		dereferences XS_SNSP to get the current stored Additional Sense Code
 *	XS_SNSASCQ(xs)		dereferences XS_SNSP to get the current stored Additional Sense Code Qualifier
 *	XS_TAG_P(xs)		predicate of whether this command should be tagged
 *	XS_TAG_TYPE(xs)		which type of tag to use
 *	XS_PRIORITY(xs)		command priority for SIMPLE tag
 *	XS_SETERR(xs)		set error state
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
 *	XS_APPEND_SENSE(xs, sp, len)	append more sense data
 *
 *	XS_SENSE_VALID(xs)		indicates whether sense is valid
 *
 *	DEFAULT_FRAMESIZE(ispsoftc_t *)		Default Frame Size
 *
 *	DEFAULT_ROLE(ispsoftc_t *, int)		Get Default Role for a channel
 *	DEFAULT_LOOPID(ispsoftc_t *, int)	Default FC Loop ID
 *
 *		These establish reasonable defaults for each platform.
 * 		These must be available independent of card NVRAM and are
 *		to be used should NVRAM not be readable.
 *
 *	DEFAULT_NODEWWN(ispsoftc_t *, chan)	Default FC Node WWN to use
 *	DEFAULT_PORTWWN(ispsoftc_t *, chan)	Default FC Port WWN to use
 *
 *		These defines are hooks to allow the setting of node and
 *		port WWNs when NVRAM cannot be read or is to be overridden.
 *
 *	ACTIVE_NODEWWN(ispsoftc_t *, chan)	FC Node WWN to use
 *	ACTIVE_PORTWWN(ispsoftc_t *, chan)	FC Port WWN to use
 *
 *		After NVRAM is read, these will be invoked to get the
 *		node and port WWNs that will actually be used for this
 *		channel.
 *
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

#ifdef	ISP_TARGET_MODE
/*
 * The functions below are for the publicly available
 * target mode functions that are internal to the Qlogic driver.
 */

/*
 * This function handles new response queue entry appropriate for target mode.
 */
int isp_target_notify(ispsoftc_t *, void *, uint32_t *, uint16_t);

/*
 * This function externalizes the ability to acknowledge an Immediate Notify request.
 */
int isp_notify_ack(ispsoftc_t *, void *);

/*
 * This function externalized acknowledging (success/fail) an ABTS frame
 */
int isp_acknak_abts(ispsoftc_t *, void *, int);

/*
 * General routine to send a final CTIO for a command- used mostly for
 * local responses.
 */
int isp_endcmd(ispsoftc_t *, ...);
#define	ECMD_SVALID	0x100
#define	ECMD_RVALID	0x200
#define	ECMD_TERMINATE	0x400

/*
 * Handle an asynchronous event
 */
void isp_target_async(ispsoftc_t *, int, int);
#endif
#endif	/* _ISPVAR_H */
