/*	$FreeBSD$	*/
/*	$NecBSD: scsi_low.h,v 1.24 1999/07/23 21:00:05 honda Exp $	*/
/*	$NetBSD$	*/

#define	SCSI_LOW_DIAGNOSTIC

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SCSI_LOW_H_
#define	_SCSI_LOW_H_

#ifdef __NetBSD__
#include <i386/Cbus/dev/scsi_dvcfg.h>
#endif
#ifdef __FreeBSD__
#include <sys/device_port.h>
#define CAM
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_dvcfg.h>
#endif

/* user configuration flags defs */
#define	SCSI_LOW_SYNC		DVF_SCSI_SYNC
#define	SCSI_LOW_DISC		DVF_SCSI_DISC
#define	SCSI_LOW_WAIT		DVF_SCSI_WAIT
#define	SCSI_LOW_LINK		DVF_SCSI_LINK
#define	SCSI_LOW_QTAG		DVF_SCSI_QTAG
#define	SCSI_LOW_NOPARITY	DVF_SCSI_NOPARITY
#define	SCSI_LOW_SAVESP		DVF_SCSI_SAVESP
#define	SCSI_LOW_DEFCFG		DVF_SCSI_DEFCFG
#define	SCSI_LOW_BITS		DVF_SCSI_BITS

#define	SCSI_LOW_PERIOD(n)	DVF_SCSI_PERIOD(n)
#define	SCSI_LOW_OFFSET(n)	DVF_SCSI_OFFSET(n)

/* host scsi id and targets macro */
#ifndef	SCSI_LOW_NTARGETS
#define	SCSI_LOW_NTARGETS			8
#endif	/* SCSI_LOW_NTARGETS */
#define	SCSI_LOW_NCCB				32

#define	SCSI_LOW_MAX_MSGLEN			16
#define	SCSI_LOW_MAX_RETRY			3

/* timeout control macro */
#define SCSI_LOW_MIN_TOUT			24
#define SCSI_LOW_TIMEOUT_CHECK_INTERVAL 	4
#define	SCSI_LOW_POWDOWN_TC			15
#define	SCSI_LOW_MAX_PHCHANGES			256

/* max synch period */
#ifndef	SCSI_LOW_MAX_SYNCH_SPEED
#define	SCSI_LOW_MAX_SYNCH_SPEED		(100)	/* 10.0M */
#endif	/* !SCSI_LOW_MAX_SYNCH_SPEED */

/*************************************************
 * Scsi Data Pointer
 *************************************************/
/* scsi pointer */
struct sc_p {
	u_int8_t *scp_data;
	int scp_datalen;

	u_int8_t *scp_cmd;
	int scp_cmdlen;

	u_int scp_direction;
#define	SCSI_LOW_RWUNK	(-1)
#define	SCSI_LOW_WRITE	0
#define	SCSI_LOW_READ	1
};

#define	SCSI_LOW_SETUP_PHASE(ti, phase)			\
{							\
	if ((ti)->ti_phase != (phase))			\
	{						\
		(ti)->ti_ophase = ti->ti_phase;		\
		(ti)->ti_phase = (phase);		\
	}						\
}

#define	SCSI_LOW_SETUP_MSGPHASE(slp, PHASE)		\
{							\
	(slp)->sl_msgphase = (PHASE);			\
}

#define	SCSI_LOW_TARGET_ASSERT_ATN(slp)			\
{							\
	(ti)->ti_tflags |= TARG_ASSERT_ATN;		\
}

/*************************************************
 * Command Control Block Structure
 *************************************************/
typedef int scsi_low_tag_t;			
struct targ_info;

#define	SCSI_LOW_UNKLUN	((u_int) -1)
#define	SCSI_LOW_UNKTAG	((scsi_low_tag_t) -1)

struct slccb {
	TAILQ_ENTRY(slccb) ccb_chain;

#ifdef CAM
	union ccb *ccb;
	struct buf *bp;
#else
	struct scsipi_xfer *xs;		/* scsi upper */
#endif
	struct targ_info *ti;		/* targ_info */
	struct lun_info *li;		/* lun info */
	scsi_low_tag_t ccb_tag;		/* tag */

	/*****************************************
	 * Scsi data pointers (original and saved)
	 *****************************************/
	struct sc_p ccb_scp;		/* given */
	struct sc_p ccb_sscp;		/* saved scsi data pointer */

#ifdef	SCSI_LOW_SUPPORT_USER_MSGOUT
	u_int8_t msgout[SCSI_LOW_MAX_MSGLEN];	/* scsi msgout */
	u_int msgoutlen;
#endif	/* SCSI_LOW_SUPPORT_USER_MSGOUT */

	/*****************************************
	 * Error or Timeout counters
	 *****************************************/
	u_int ccb_flags;
#define	CCB_SENSE	0x01
	u_int ccb_error;

	int ccb_rcnt;			/* retry counter */
	int ccb_tc;			/* timer counter */
	int ccb_tcmax;			/* max timeout */

	/*****************************************
	 * Sense data buffer
	 *****************************************/
#ifdef __NetBSD__
	struct scsipi_sense ccb_sense_cmd;
	struct scsipi_sense_data ccb_sense;
#endif
#ifdef __FreeBSD__
	struct scsi_sense ccb_sense_cmd;
	struct scsi_sense_data ccb_sense;
#endif
};

/* ccb assert */
#ifdef __NetBSD__
#include <dev/isa/ccbque.h>
#endif
#ifdef __FreeBSD__
#include <i386/isa/ccbque.h>
#endif
GENERIC_CCB_ASSERT(scsi_low, slccb)

/*************************************************
 * Target structures
 *************************************************/
struct scsi_low_softc;
TAILQ_HEAD(targ_info_tab, targ_info);
LIST_HEAD(lun_info_tab, lun_info);

struct lun_info {
	int li_lun;
	struct targ_info *li_ti;		/* my target */

	LIST_ENTRY(lun_info) lun_chain;		/* targ_info link */

	int li_disc;				/* num disconnects */
	int li_maxnexus;

	/*
	 * lun state
	 */
#define	UNIT_SLEEP	0x00
#define	UNIT_START	0x01
#define	UNIT_SYNCH	0x02
#define	UNIT_WIDE	0x03
#define	UNIT_OK		0x04
#define	UNIT_NEGSTART	UNIT_SYNCH
	u_int li_state;				/* target lun state */
	u_int li_maxstate;			/* max state */

	/*
	 * lun control flags
 	 */
	u_int li_flags;				/* real control flags */
	u_int li_cfgflags;			/* given target cfgflags */
	u_int li_quirks;			/* given target quirk */

	/*
	 * lun synch and wide data
 	 */
	struct synch {
		u_int8_t offset;
		u_int8_t period;
	} li_maxsynch;		/* synch data */

	u_int li_width;
};

struct targ_info {
	TAILQ_ENTRY(targ_info) ti_chain;	/* targ_info link */

	struct scsi_low_softc *ti_sc;		/* our softc */
	u_int ti_id;				/* scsi id */

	/*
	 * Lun chain
	 */
	struct lun_info_tab ti_litab;		/* lun chain */

	/*
	 * Nexus
	 */
	struct slccb *ti_nexus;			/* current nexus */
	struct lun_info *ti_li;			/* current nexus lun_info */

	/*
	 * Target status
 	 */
#define	TARG_ASSERT_ATN	0x01
	u_int ti_tflags;			/* target state I */

	/*
	 * Scsi phase control
 	 */
	struct slccbtab ti_discq;			/* disconnect queue */

#define	PH_NULL		0x00
#define	PH_ARBSTART	0x01
#define	PH_SELSTART	0x02
#define	PH_SELECTED	0x03
#define	PH_CMD		0x04
#define	PH_DATA		0x05
#define	PH_MSGIN	0x06
#define	PH_MSGOUT	0x07
#define	PH_STAT		0x08
#define	PH_DISC		0x09
#define	PH_RESEL	0x0a
	u_int ti_phase;				/* scsi phase */
	u_int ti_ophase;			/* old scsi phase */

	/*
	 * Status in
 	 */
	u_int8_t ti_status;			/* status in */

	/*
	 * Msg in
 	 */
	u_int ti_msginptr;			/* msgin ptr */
	u_int ti_msginlen;			/* expected msg length */
	u_int8_t ti_msgin[SCSI_LOW_MAX_MSGLEN];	/* msgin buffer */
	u_int ti_sphase;

#ifdef	SCSI_LOW_DIAGNOSTIC
#define	MSGIN_HISTORY_LEN	5
	u_int8_t ti_msgin_history[MSGIN_HISTORY_LEN];
	int ti_msgin_hist_pointer;
#endif	/* SCSI_LOW_DIAGNOSTIC */

	/*
	 * Msg out
 	 */
	u_int ti_msgflags;			/* msgs to be asserted */
	u_int ti_omsgflags;			/* msgs asserted */
	u_int ti_emsgflags;			/* a msg currently asserted */
#define	SCSI_LOW_MSG_RESET	0x00000001
#define	SCSI_LOW_MSG_ABORT	0x00000002
#define	SCSI_LOW_MSG_REJECT	0x00000004
#define	SCSI_LOW_MSG_PARITY	0x00000008
#define	SCSI_LOW_MSG_ERROR	0x00000010
#define	SCSI_LOW_MSG_IDENTIFY	0x00000020
#define	SCSI_LOW_MSG_SYNCH	0x00000040
#define	SCSI_LOW_MSG_WIDE	0x00000080
#define	SCSI_LOW_MSG_USER	0x00000100
#define	SCSI_LOW_MSG_NOOP	0x00000200
#define	SCSI_LOW_MSG_ALL	0xffffffff

	/* msgout buffer */
	u_int8_t ti_msgoutstr[SCSI_LOW_MAX_MSGLEN];	/* scsi msgout */
	u_int ti_msgoutlen;			/* msgout strlen */

	/*
	 * lun info size.
 	 */
	int ti_lunsize;	
};

/*************************************************
 * COMMON HEADER STRUCTURE
 *************************************************/
struct scsi_low_softc;
typedef struct scsi_low_softc *sc_low_t;

#define	SCSI_LOW_START_OK	0
#define	SCSI_LOW_START_FAIL	1

#define	SC_LOW_INIT_T (int (*) __P((sc_low_t, int)))
#define	SC_LOW_BUSRST_T (void (*) __P((sc_low_t)))
#define	SC_LOW_LUN_INIT_T (int (*) __P((sc_low_t, struct targ_info *, struct lun_info *)))
#define	SC_LOW_SELECT_T (int (*) __P((sc_low_t, struct slccb *)))
#define	SC_LOW_ATTEN_T (void (*) __P((sc_low_t)))
#define	SC_LOW_NEXUS_T (int (*) __P((sc_low_t, struct targ_info *)))
#define	SC_LOW_MSG_T (int (*) __P((sc_low_t, struct targ_info *, u_int)))
#define	SC_LOW_POLL_T (int (*) __P((void *)))
#define	SC_LOW_POWER_T (int (*) __P((sc_low_t, u_int)))

struct scsi_low_funcs {
	int (*scsi_low_init) __P((sc_low_t, int));
	void (*scsi_low_bus_reset) __P((sc_low_t));
	int (*scsi_low_lun_init) __P((sc_low_t, struct targ_info *, struct lun_info *));

	int (*scsi_low_start_bus) __P((sc_low_t, struct slccb *));
	int (*scsi_low_establish_nexus) __P((sc_low_t, struct targ_info *));

	void (*scsi_low_attention) __P((sc_low_t));
	int (*scsi_low_msg) __P((sc_low_t, struct targ_info *, u_int));

	int (*scsi_low_poll) __P((void *));

#define	SCSI_LOW_POWDOWN	1
#define	SCSI_LOW_ENGAGE		2
	int (*scsi_low_power) __P((sc_low_t, u_int));
};

/*************************************************
 * SCSI LOW SOFTC
 *************************************************/
struct scsi_low_softc {
	DEVPORT_DEVICE sl_dev;
	u_char sl_xname[16];
				
	/* upper interface */
#ifdef CAM
	struct cam_sim *sim;
	struct cam_path *path;
#else
	struct scsipi_link sl_link;
#endif

	/* my targets */
	struct targ_info *sl_ti[SCSI_LOW_NTARGETS];
	struct targ_info_tab sl_titab;

	/* current active nexus */
	int sl_nexus_call;
	struct targ_info *sl_nexus;

	/* ccb start queue */
	struct slccbtab sl_start;	

	/* retry limit and phase change counter */
	int sl_max_retry;
	int sl_ph_count;

	/* selection & total num disconnect targets */
	int sl_disc;
	struct targ_info *sl_selid;

	/* scsi phased suggested by scsi msg */
	u_int sl_msgphase;	
#define	MSGPH_NULL	0x00		/* no msg */
#define	MSGPH_DISC	0x01		/* disconnect msg */
#define	MSGPH_CMDC	0x02		/* cmd complete msg */

	/* error */
#define	FATALIO		0x01		/* generic io error & retry io */
#define	ABORTIO		0x02		/* generic io error & terminate io */
#define	TIMEOUTIO	0x04		/* watch dog timeout */
#define	SELTIMEOUTIO	0x08		/* selection timeout */
#define	PDMAERR		0x10		/* dma xfer error */
#define	MSGERR		0x20		/* msgsys error */
#define	PARITYERR	0x40		/* parity error */
#define	BUSYERR		0x80		/* target busy error */
#define	CMDREJECT	0x100		/* cmd reject error */
#define	SCSI_LOW_ERRORBITS "\020\009cmdrej\008busy\007parity\006msgerr\005pdmaerr\004seltimeout\003timeout\002abort\001fatal"
	u_int sl_error;				/* error flags */

	/* current scsi data pointer */
	struct sc_p sl_scp;

	/* power control */
	u_int sl_active;		/* host is busy state */
	int sl_powc;			/* power down timer counter */
	u_int sl_rstep;			/* resume step */

	/* configuration flags */
	u_int sl_flags;		
#define	HW_POWDOWN	0x01
#define	HW_RESUME	0x02
#define	HW_PDMASTART	0x04
#define	HW_INACTIVE	0x08
#define	HW_POWERCTRL	0x10

	u_int sl_cfgflags;
#define	CFG_NODISC	0x01
#define	CFG_NOPARITY	0x02
#define	CFG_NOATTEN	0x04
#define	CFG_ASYNC	0x08
#define	CFG_MSGUNIFY	0x10

	/* host informations */
	u_int sl_hostid;
	int sl_nluns;
	int sl_ntargs;
	int sl_openings;

	/* interface functions */
	struct scsi_low_funcs *sl_funcs;

#if	defined(i386)
	u_int sl_irq;		/* XXX */
#endif	/* i386 */
#ifdef __FreeBSD__
	struct callout_handle engage_ch;
	struct callout_handle timeout_ch;
#ifdef	SCSI_LOW_POWFUNC
	struct callout_handle recover_ch;
#endif
#endif /* __FreeBSD__ */
};

/*************************************************
 * SCSI LOW service functions
 *************************************************/
/* 
 * Scsi low attachment function.
 */
int scsi_low_attach __P((struct scsi_low_softc *, int, int, int, int));
int scsi_low_dettach __P((struct scsi_low_softc *));

/* 
 * Scsi phase "bus service" functions.
 * These functions are corresponding to each scsi bus phaeses.
 */
/* nexus abort (selection failed) */
void scsi_low_clear_nexus __P((struct scsi_low_softc *, struct targ_info *));
/* msgout phase */
int scsi_low_msgout __P((struct scsi_low_softc *, struct targ_info *));
/* msgin phase */
void scsi_low_msgin __P((struct scsi_low_softc *, struct targ_info *, u_int8_t));
/* data phase */
int scsi_low_data __P((struct scsi_low_softc *, struct targ_info *, struct buf **, int));
/* cmd phase */
int scsi_low_cmd __P((struct scsi_low_softc *, struct targ_info *));

/* reselection phase */
struct targ_info *scsi_low_reselected __P((struct scsi_low_softc *, u_int));
/* disconnection phase */
int scsi_low_disconnected __P((struct scsi_low_softc *, struct targ_info *));

/* 
 * Scsi bus restart function.
 * Canncel all established nexuses => scsi system initialized => restart jobs.
 */
#define	SCSI_LOW_RESTART_HARD	1
#define	SCSI_LOW_RESTART_SOFT	0
int scsi_low_restart __P((struct scsi_low_softc *, int, u_char *));

/* 
 * Scsi utility fucntions
 */
/* print current status */
void scsi_low_print __P((struct scsi_low_softc *, struct targ_info *));
/* timeout utility (only in used scsi_low_pisa) */
void scsi_low_timeout __P((void *));
#define	SCSI2_RESET_DELAY	5000000
#define	TWIDDLEWAIT	10000
/* bus reset utility */
void scsi_low_bus_reset __P((struct scsi_low_softc *));

/*************************************************
 * Inline utility
 *************************************************/
static __inline u_int8_t scsi_low_identify __P((struct targ_info *ti));
static __inline void scsi_low_attention __P((struct scsi_low_softc *, struct targ_info *));
static __inline int scsi_low_is_msgout_continue __P((struct targ_info *));
static __inline int scsi_low_assert_msg __P((struct scsi_low_softc *, struct targ_info *, u_int, int));
static __inline void scsi_low_arbit_win __P((struct scsi_low_softc *, struct targ_info *));

static __inline int
scsi_low_is_msgout_continue(ti)
	struct targ_info *ti;
{
	
	return (ti->ti_msgflags != 0);
}

static __inline u_int8_t
scsi_low_identify(ti)
	struct targ_info *ti;
{
	u_int8_t msg;
	struct lun_info *li = ti->ti_li;

	msg = (li->li_flags & SCSI_LOW_DISC) ? 0xc0 : 0x80;
	msg |= li->li_lun;
	return msg;
}

#define	ID_MSG_SETUP(ti) (scsi_low_identify(ti))

static __inline void
scsi_low_attention(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	(*slp->sl_funcs->scsi_low_attention) (slp);
	SCSI_LOW_TARGET_ASSERT_ATN(slp);
}

static __inline int
scsi_low_assert_msg(slp, ti, msg, now)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int msg;
	int now;
{

	ti->ti_msgflags |= msg;
	if (now != 0)
		scsi_low_attention(slp, ti);
	return 0;
}

static __inline void
scsi_low_arbit_win(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	slp->sl_selid = NULL;
}

/*************************************************
 * Message out defs
 *************************************************/
/* XXX: use scsi_message.h */
#define	ST_GOOD		0x00
#define	ST_CHKCOND	0x02
#define	ST_MET		0x04
#define	ST_BUSY		0x08
#define	ST_INTERGOOD	0x10
#define	ST_INTERMET	0x14
#define	ST_CONFLICT	0x18
#define	ST_QUEFULL	0x28

#define	MSG_COMP	0x00
#define	MSG_EXTEND	0x01

#define	MKMSG_EXTEND(XLEN, XCODE) ((((u_int)(XLEN)) << NBBY) | ((u_int)(XCODE)))
#define	MSG_EXTEND_MDPCODE	0x00
#define	MSG_EXTEND_MDPLEN	0x05
#define	MSG_EXTEND_SYNCHCODE	0x01
#define	MSG_EXTEND_SYNCHLEN	0x03
#define	MSG_EXTEND_WIDECODE	0x03
#define	MSG_EXTEND_WIDELEN	0x02

#define	MSG_SAVESP	0x02
#define	MSG_RESTORESP	0x03
#define	MSG_DISCON	0x04
#define	MSG_I_ERROR	0x05
#define	MSG_ABORT	0x06
#define	MSG_REJECT	0x07
#define	MSG_NOOP	0x08
#define	MSG_PARITY	0x09
#define	MSG_LCOMP	0x0a
#define	MSG_LCOMP_F	0x0b
#define	MSG_RESET	0x0c
#ifdef __FreeBSD__
#undef	MSG_IDENTIFY
#endif
#define	MSG_IDENTIFY	0x80

#define	OS_DEPEND(s)	(s)
#endif	/* !_SCSI_LOW_H_ */
