/*	$NecBSD: bsvar.h,v 1.2 1997/10/31 17:43:41 honda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
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
 *
 * $FreeBSD: src/sys/i386/isa/bs/bsvar.h,v 1.6.6.1 2000/03/22 03:36:45 nyan Exp $
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

#ifdef __FreeBSD__
#define	BS_INLINE	__inline
#else
#define	BS_INLINE	inline
#endif

/**************************************************
 *	CONTROL FLAGS  (cf_flags)
 *************************************************/
#define	BSC_FASTACK		0x01
#define	BSC_SMITSAT_DISEN	0x02
#define	BSC_CHIP_CLOCK(dvcfg)	(((dvcfg) >> 4) & 0x03)

#define	BS_SCSI_SYNC		DVF_SCSI_SYNC
#define	BS_SCSI_DISC		DVF_SCSI_DISC
#define	BS_SCSI_WAIT		DVF_SCSI_WAIT
#define	BS_SCSI_LINK		DVF_SCSI_LINK
#define	BS_SCSI_QTAG		DVF_SCSI_QTAG
#define	BS_SCSI_NOSAT		DVF_SCSI_SP0
#define	BS_SCSI_NOPARITY	DVF_SCSI_NOPARITY
#define	BS_SCSI_SAVESP		DVF_SCSI_SAVESP
#define	BS_SCSI_NOSMIT		DVF_SCSI_SP1
#define	BS_SCSI_PERIOD(XXX)	DVF_SCSI_PERIOD(XXX)
#define	BS_SCSI_OFFSET(XXX)	DVF_SCSI_OFFSET(XXX)
#define BS_SCSI_SYNCMASK	DVF_SCSI_SYNCMASK
#define	BS_SCSI_BITS		DVF_SCSI_BITS

#define	BS_SCSI_DEFCFG		(BS_SCSI_NOSAT | DVF_SCSI_DEFCFG)

#define	BS_SCSI_POSITIVE (BS_SCSI_SYNC | BS_SCSI_DISC | BS_SCSI_LINK)
#define	BS_SCSI_NEGATIVE (BS_SCSI_WAIT | BS_SCSI_NOSAT | BS_SCSI_NOPARITY |\
			  BS_SCSI_SAVESP | BS_SCSI_NOSMIT)
/*******************************************
 * CONFIG SECTION
 ******************************************/
/* Enable timeout watch dog */
#define	BS_DEFAULT_TIMEOUT_SECOND	16	/* default 16 sec */
#define	BS_SYNC_TIMEOUT			16
#define	BS_STARTUP_TIMEOUT		60
#define	BS_MOTOR_TIMEOUT		120
#define	BS_TIMEOUT_CHECK_INTERVAL	4	/* check each 4 sec */

/* If you use memory over 16M */
#ifdef	SCSI_BOUNCE_SIZE
#define	BS_BOUNCE_SIZE	SCSI_BOUNCE_SIZE
#else	/* !SCSI_BOUNCE_SIZE */
#define	BS_BOUNCE_SIZE	0
#endif	/* !SCSI_BOUNCE_SIZE */

/* debug */
#define	BS_STATICS		1
#define	BS_DIAG			1
#define	BS_DEBUG_ROUTINE	1
#define	BS_DEBUG		1
/* #define	SHOW_PORT	1 */

/**************************************************
 *	PARAMETER
 **************************************************/
#define	NTARGETS	8
#define	RETRIES		1	/* number of retries before giving up */
#define	HARDRETRIES	3
#define	XSMAX		4
#define	BSDMABUFSIZ	0x10000
#define BS_MAX_CCB	(XSMAX * (NTARGETS - 1))

#define	BSCMDSTART	0
#define	BSCMDRESTART	0x01
#define	BSCMDFORCE	0x02

#define	BS_TIMEOUT_INTERVAL	(hz * BS_TIMEOUT_CHECK_INTERVAL)

/**************************************************
 *	SCSI PHASE
 **************************************************/
enum scsi_phase {
	FREE = 0,
	HOSTQUEUE,
	DISCONNECTED,
	IOCOMPLETED,
	ATTENTIONASSERT,
	DISCONNECTASSERT,
	SELECTASSERT,
	SELECTED,
	RESELECTED,
	MSGIN,
	MSGOUT,
	STATUSIN,
	CMDPHASE,
	DATAPHASE,
	SATSEL,
	SATRESEL,
	SATSDP,
	SATCOMPSEQ,
	UNDEF,
};

/**************************************************
 *	SCSI PHASE CONTROL MACRO
 **************************************************/
#define	BS_HOST_START					\
{							\
	bsc->sc_nexus = ti;				\
}

#define	BS_HOST_TERMINATE				\
{							\
	bsc->sc_selwait = NULL;				\
	bsc->sc_nexus = NULL;				\
}

#define	BS_SELECTION_START				\
{							\
	bsc->sc_selwait = ti;				\
}

#define	BS_SELECTION_TERMINATE				\
{							\
	bsc->sc_selwait = NULL;				\
}

#define	BS_SETUP_PHASE(PHASE)				\
{							\
	ti->ti_ophase = ti->ti_phase;			\
	ti->ti_phase = (PHASE);				\
}

#define	BS_SETUP_MSGPHASE(PHASE)			\
{							\
	bsc->sc_msgphase = (PHASE);			\
}

#define	BS_SETUP_SYNCSTATE(STATE)			\
{							\
	ti->ti_syncnow.state = (STATE);			\
}

#define	BS_SETUP_TARGSTATE(STATE)			\
{							\
	ti->ti_state = (STATE);				\
}

#define	BS_LOAD_SDP					\
{							\
	bsc->sc_p.data = ti->ti_scsp.data;		\
	bsc->sc_p.datalen = ti->ti_scsp.datalen;	\
	bsc->sc_p.seglen = ti->ti_scsp.seglen;		\
}

#define	BS_RESTORE_SDP					\
{							\
	bsc->sc_p = ti->ti_scsp;			\
}

#define	BS_SAVE_SDP					\
{							\
	ti->ti_scsp = bsc->sc_p;			\
}

/**************************************************
 * STRUCTURES
 **************************************************/
struct msgbase {
#define	MAXMSGLEN	8
	u_int8_t msg[MAXMSGLEN];
	u_int msglen;

	u_int flag;
};

struct syncdata {
	u_int8_t period;
	u_int8_t offset;

#define	BS_SYNCMSG_NULL		0x00
#define	BS_SYNCMSG_ASSERT	0x01
#define	BS_SYNCMSG_REJECT	0x02
#define	BS_SYNCMSG_ACCEPT	0x03
#define	BS_SYNCMSG_REQUESTED	0x04
	u_int state;
};

struct sc_p {
	u_int8_t *data;
	int datalen;

	u_int8_t *segaddr;
	int seglen;

	u_int8_t *bufp;
};

/* targ_info error flags */
#define	BSDMAABNORMAL	0x01
#define	BSCMDABNORMAL	0x02
#define	BSPARITY	0x04
#define	BSSTATUSERROR	0x08
#define	BSTIMEOUT	0x10
#define	BSREQSENSE	0x20
#define	BSSELTIMEOUT	0x40
#define	BSFATALIO	0x80
#define	BSMSGERROR	0x100
#define	BSTRYRECOV	0x200
#define	BSABNORMAL	0x400
#define	BSTARGETBUSY	0x800

#define	BSERRORBITS	"\020\014busy\013abnormal\012retry\011msgerr\010fatal\007seltimeout\006sense\005timeout\004statuserr\003parity\002cmderr\001dmaerr"

/* bsccb bsccb_flags & targ_info flags & cmd flags*/
#define	BSREAD		0x0001
#define	BSSAT		0x0002
#define	BSLINK		0x0004
#define	BSERROROK	0x0008
#define	BSSMIT		0x0010
#define	BSDISC		0x1000
#define	BSFORCEIOPOLL	0x2000

#define	BSCASTAT	0x01000000
#define	BSSENSECCB	0x02000000
#define	BSQUEUED	0x04000000
#define	BSALTBUF	0x08000000
#define	BSITSDONE	0x10000000
#define	BSNEXUS		0x20000000

#define	BSCFLAGSMASK	(0xffff)

struct bsccb {
	TAILQ_ENTRY(bsccb) ccb_chain;

	union ccb *ccb;			/* upper drivers info */

	u_int lun;			/* lun */

	u_int bsccb_flags;		/* control flags */

	int rcnt;			/* retry counter of this ccb */

	u_int error;			/* recorded error */

	/*****************************************
	 * scsi cmd & data
	 *****************************************/
	u_int8_t *cmd;			/* scsi cmd */
	int cmdlen;

	u_int8_t *data;			/* scsi data */
	int datalen;

	u_int8_t msgout[MAXMSGLEN];	/* scsi msgout */
	u_int msgoutlen;

	/*****************************************
	 * timeout counter
	 *****************************************/
	int tc;
	int tcmax;
};

GENERIC_CCB_ASSERT(bs, bsccb)

/* target info */
struct targ_info {
/*0*/	TAILQ_ENTRY(targ_info) ti_tchain;	/* targ_info link */

/*4*/	TAILQ_ENTRY(targ_info) ti_wchain;	/* wait link */

/*8*/	struct bs_softc *ti_bsc;		/* our controller */
/*c*/	u_int ti_id;				/* scsi id */
/*10*/	u_int ti_lun;				/* current lun */

/*14*/	struct bsccbtab ti_ctab, ti_bctab;	/* ccb */

#define	BS_TARG_NULL	0
#define	BS_TARG_CTRL	1
#define	BS_TARG_START	2
#define	BS_TARG_SYNCH	3
#define BS_TARG_RDY	4
/*24*/	int ti_state;				/* target state */

/*28*/	u_int ti_cfgflags;			/* target cfg flags */

/*2c*/	u_int ti_flags;				/* flags */
/*30*/	u_int ti_mflags;			/* flags masks */

/*34*/	u_int ti_error;				/* error flags */
/*38*/	u_int ti_herrcnt;			/* hardware err retry counter */

	/*****************************************
	 * scsi phase data
	 *****************************************/
/*3c*/	struct sc_p ti_scsp;			/* saved scsi data pointer */

/*50*/	enum scsi_phase ti_phase;		/* scsi phase */
/*54*/	enum scsi_phase ti_ophase;		/* previous scsi phase */

/*58*/	u_int8_t ti_status;			/* status in */

/*59*/	u_int8_t ti_msgin[MAXMSGLEN];		/* msgin buffer */
/*64*/	int ti_msginptr;

/*68*/	u_int8_t ti_msgout;			/* last msgout byte */
/*69*/	u_int8_t ti_emsgout;			/* last msgout byte */
/*6c*/	u_int ti_omsgoutlen;			/* for retry msgout */

/*70*/	struct syncdata ti_syncmax;		/* synch data (scsi) */
/*72*/	struct syncdata ti_syncnow;
/*74*/	u_int8_t ti_sync;			/* synch val (chip) */

	/*****************************************
	 * bounce buffer & smit data pointer
	 *****************************************/
/*75*/	u_int8_t *bounce_phys;
/*76*/	u_int8_t *bounce_addr;
/*78*/	u_int bounce_size;

/*7c*/	u_long sm_offset;

	/*****************************************
	 * target inq data
	 *****************************************/
/*79*/	u_int8_t targ_type;
/*7a*/	u_int8_t targ_support;

	/*****************************************
	 * generic scsi cmd buffer for this target
	 *****************************************/
/*7b*/	u_int8_t scsi_cmd[12];
	struct scsi_sense_data sense;
};

TAILQ_HEAD(titab, targ_info);
struct bshw;

struct bs_softc {
	/*****************************************
	 * OS depend header
	 *****************************************/
	OS_DEPEND_DEVICE_HEADER

	OS_DEPEND_MISC_HEADER

	/*****************************************
	 * target link
	 *****************************************/
	struct targ_info *sc_ti[NTARGETS];
	u_int sc_openf;

	struct titab sc_sttab;
	struct titab sc_titab;

	/*****************************************
	 * current scsi phase
	 *****************************************/
	struct targ_info *sc_nexus;		/* current active nexus */

	enum scsi_phase sc_msgphase;		/* scsi phase pointed by msg */

	struct targ_info *sc_selwait;		/* selection assert */

	u_int sc_dtgnum;			/* disconnected target */

	/*****************************************
	 * current scsi data pointer
	 *****************************************/
	struct sc_p sc_p;			/* scsi data pointer */

	int sc_dmadir;				/* dma direction */

	int sm_tdatalen;			/* smit data transfer bytes */

	/*****************************************
	 * parameter
	 *****************************************/
	u_int sc_retry;				/* max retry count */

#define	BSDMATRANSFER	0x01
#define	BSDMASTART	0x02
#define	BSSMITSTART	0x04
#define	BSUNDERRESET	0x08
#define	BSRESET		0x10
#define	BSSTARTTIMEOUT	0x20
#define	BSPOLLDONE	0x100
#define	BSJOBDONE	0x200
#define	BSBSMODE	0x400
#define	BSINACTIVE	0x800
	volatile int sc_flags;			/* host flags */

#define	BSC_NULL	0
#define	BSC_BOOTUP	1
#define	BSC_TARG_CHECK	2
#define	BSC_RDY		3
	int sc_hstate;				/* host state */

	/*****************************************
	 * polling misc
	 *****************************************/
	u_int sc_wc;				/* weight count */

	int sc_poll;
	struct bsccb *sc_outccb;

	/*****************************************
	 * wd33c93 chip depend section
	 *****************************************/
	u_int sc_cfgflags;			/* hardware cfg flags */

	struct bshw *sc_hw;			/* hw selection */

	u_long sm_offset;			/* smit buffer offset */

	u_int sc_RSTdelay;

	int sc_hwlock;				/* hardware lock count */

	int sc_iobase;				/* iobase for FreeBSD */
	u_int32_t sc_irq;			/* irq */

	u_int sc_dmachan;			/* dma channel */
	u_int8_t sc_busstat;			/* scsi bus status register */
	u_int8_t sc_hostid;			/* host scsi id */
	u_int8_t sc_cspeed;			/* chip clock rate */
	u_int8_t sc_membank;			/* memory back (NEC) register */

	/*****************************************
	 * our name
	 *****************************************/
#define	BS_DVNAME_LEN	16
	u_char sc_dvname[BS_DVNAME_LEN];

	/*****************************************
	 * CAM support
	 *****************************************/
	struct		cam_sim  *sim;
	struct		cam_path *path;
};

/*************************************************
 * debug
 *************************************************/
#ifdef	BS_STATICS
struct bs_statics {
	u_int select;
	u_int select_miss_in_assert;
	u_int select_miss_by_reselect;
	u_int select_miss;
	u_int select_win;
	u_int selected;
	u_int disconnected;
	u_int reselect;
};

extern struct bs_statics bs_statics[NTARGETS];
extern u_int bs_linkcmd_count[];
extern u_int bs_bounce_used[];
#endif	/* BS_STATICS */

#ifdef	BS_DEBUG_ROUTINE
#ifndef	DDB
#define	Debugger()	panic("should call debugger here (bs.c)")
#endif	/* DDB */
#ifdef	BS_DEBUG
extern int bs_debug_flag;
#endif /* BS_DEBUG */
#endif /* BS_DEBUG_ROUTINE */

/*************************************************
 * Function declare
 *************************************************/
int bs_scsi_cmd_internal __P((struct bsccb *, u_int));
struct bsccb *bscmddone __P((struct targ_info *));
int bscmdstart __P((struct targ_info *, int));
int bs_scsi_cmd_poll __P((struct targ_info *, struct bsccb *));
int bs_sequencer __P((struct bs_softc *));
void bs_poll_timeout __P((struct bs_softc *, char *));

/*************************************************
 * XXX
 *************************************************/
/* misc error */
#define COMPLETE	2
#define NOTARGET	(-2)
#define	HASERROR	(-1)

/* XXX: use scsi_message.h */
/* status */
#define	ST_GOOD		0x00
#define	ST_CHKCOND	0x02
#define	ST_MET		0x04
#define	ST_BUSY		0x08
#define	ST_INTERGOOD	0x10
#define	ST_INTERMET	0x14
#define	ST_CONFLICT	0x18
#define	ST_QUEFULL	0x28
#define	ST_UNK		0xff

/* message */
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
