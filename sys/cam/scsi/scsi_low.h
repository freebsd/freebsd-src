/*	$FreeBSD$	*/
/*	$NecBSD: scsi_low.h,v 1.24.10.5 2001/06/26 07:31:46 honda Exp $	*/
/*	$NetBSD$	*/

#define	SCSI_LOW_DIAGNOSTIC
#define	SCSI_LOW_ALT_QTAG_ALLOCATE

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	Naofumi HONDA. All rights reserved.
 *
 * [Ported for FreeBSD CAM]
 *  Copyright (c) 2000, 2001
 *      MITSUNAGA Noriaki, NOKUBI Hirotaka and TAKAHASHI Yoshihiro.
 *      All rights reserved.
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

/*================================================
 * Scsi low OSDEP 
 * (All os depend structures should be here!)
 ================================================*/
/******** interface ******************************/
#ifdef	__NetBSD__
#define	SCSI_LOW_INTERFACE_XS
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#define	SCSI_LOW_INTERFACE_CAM
#define	CAM
#endif	/* __FreeBSD__ */

/******** includes *******************************/
#ifdef	__NetBSD__
#include <i386/Cbus/dev/scsi_dvcfg.h>
#include <dev/isa/ccbque.h>
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#include <sys/device_port.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_dvcfg.h>
#include <i386/isa/ccbque.h>
#endif	/* __FreeBSD__ */

/******** functions macro ************************/
#ifdef	__NetBSD__
#define	SCSI_LOW_DEBUGGER(dev)	Debugger()
#define	SCSI_LOW_DELAY(mu)	delay((mu))
#define	SCSI_LOW_SPLSCSI	splbio
#define	SCSI_LOW_BZERO(pt, size)	memset((pt), 0, (size))
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#undef	MSG_IDENTIFY
#define	SCSI_LOW_DEBUGGER(dev)	Debugger((dev))
#define	SCSI_LOW_DELAY(mu)	DELAY((mu))
#define	SCSI_LOW_SPLSCSI	splcam
#define	SCSI_LOW_BZERO(pt, size)	bzero((pt), (size))
#endif	/* __FreeBSD__ */

/******** os depend interface structures **********/
#ifdef	__NetBSD__
typedef	struct scsipi_sense_data scsi_low_osdep_sense_data_t;

struct scsi_low_osdep_interface {
	struct device si_dev;

	struct scsipi_link *si_splp;
};

struct scsi_low_osdep_targ_interface {
};

struct scsi_low_osdep_lun_interface {
	u_int sloi_quirks;
};
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
typedef	struct scsi_sense_data scsi_low_osdep_sense_data_t;

struct scsi_low_osdep_interface {
	DEVPORT_DEVICE si_dev;

	struct cam_sim *sim;
	struct cam_path *path;

	int si_poll_count;

	struct callout_handle engage_ch;
	struct callout_handle timeout_ch;
#ifdef	SCSI_LOW_POWFUNC
	struct callout_handle recover_ch;
#endif
};

struct scsi_low_osdep_targ_interface {
};

struct scsi_low_osdep_lun_interface {
};
#endif	/* __FreeBSD__ */

/******** os depend interface functions *************/
struct slccb;
struct scsi_low_softc;
#define	SCSI_LOW_TIMEOUT_STOP		0
#define	SCSI_LOW_TIMEOUT_START		1
#define	SCSI_LOW_TIMEOUT_CH_IO		0
#define	SCSI_LOW_TIMEOUT_CH_ENGAGE	1
#define	SCSI_LOW_TIMEOUT_CH_RECOVER	2

struct scsi_low_osdep_funcs {
	int (*scsi_low_osdep_attach) \
			__P((struct scsi_low_softc *));
	int (*scsi_low_osdep_world_start) \
			__P((struct scsi_low_softc *));
	int (*scsi_low_osdep_dettach) \
			__P((struct scsi_low_softc *));
	int (*scsi_low_osdep_ccb_setup) \
			__P((struct scsi_low_softc *, struct slccb *));
	int (*scsi_low_osdep_done) \
			__P((struct scsi_low_softc *, struct slccb *));
	void (*scsi_low_osdep_timeout) \
			__P((struct scsi_low_softc *, int, int));
};

/*================================================
 * Generic Scsi Low header file 
 * (All os depend structures should be above!)
 ================================================*/
/*************************************************
 * Scsi low definitions
 *************************************************/
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
#define	SCSI_LOW_NCCB				128

#define	SCSI_LOW_MAX_RETRY			3
#define	SCSI_LOW_MAX_SELECTION_RETRY		10

/* timeout control macro */
#define	SCSI_LOW_TIMEOUT_HZ			10
#define SCSI_LOW_MIN_TOUT			12
#define SCSI_LOW_TIMEOUT_CHECK_INTERVAL 	1
#define	SCSI_LOW_POWDOWN_TC			15
#define	SCSI_LOW_MAX_PHCHANGES			256
#define	SCSI2_RESET_DELAY			5000000

/* msg */
#define	SCSI_LOW_MAX_MSGLEN			32
#define	SCSI_LOW_MSG_LOG_DATALEN  		8

/*************************************************
 * Scsi Data Pointer
 *************************************************/
/* scsi pointer */
struct sc_p {
	u_int8_t *scp_data;
	int scp_datalen;

	u_int8_t *scp_cmd;
	int scp_cmdlen;

	u_int8_t scp_direction;
#define	SCSI_LOW_RWUNK	(-1)
#define	SCSI_LOW_WRITE	0
#define	SCSI_LOW_READ	1
	u_int8_t scp_status;
	u_int8_t scp_spare[2];
};

/*************************************************
 * Command Control Block Structure
 *************************************************/
typedef int scsi_low_tag_t;			
struct targ_info;

#define	SCSI_LOW_UNKLUN	((u_int) -1)
#define	SCSI_LOW_UNKTAG	((scsi_low_tag_t) -1)

struct slccb {
	TAILQ_ENTRY(slccb) ccb_chain;

	void *osdep;			/* os depend structure */

	struct targ_info *ti;		/* targ_info */
	struct lun_info *li;		/* lun info */
	struct buf *bp;			/* io bufs */

	scsi_low_tag_t ccb_tag;		/* effective qtag */
	scsi_low_tag_t ccb_otag;	/* allocated qtag */

	/*****************************************
	 * Scsi data pointers (original and saved)
	 *****************************************/
	struct sc_p ccb_scp;		/* given */
	struct sc_p ccb_sscp;		/* saved scsi data pointer */
	int ccb_datalen;		/* transfered data counter */

	/*****************************************
	 * Msgout 
	 *****************************************/
	u_int ccb_msgoutflag;
	u_int ccb_omsgoutflag;

	/*****************************************
	 * Error or Timeout counters
	 *****************************************/
	u_int ccb_flags;
#define	CCB_INTERNAL	0x0001
#define	CCB_SENSE	0x0002
#define	CCB_CLEARQ	0x0004
#define	CCB_DISCQ	0x0008
#define	CCB_STARTQ	0x0010
#define	CCB_POLLED	0x0100	/* polling ccb */
#define	CCB_NORETRY	0x0200	/* do NOT retry */
#define	CCB_AUTOSENSE	0x0400	/* do a sence after CA */
#define	CCB_URGENT	0x0800	/* an urgent ccb */
#define	CCB_NOSDONE	0x1000	/* do not call an os done routine */
#define	CCB_SCSIIO	0x2000	/* a normal scsi io coming from upper layer */
#define	CCB_SILENT	0x4000	/* no terminate messages */

	u_int ccb_error;

	int ccb_rcnt;			/* retry counter */
	int ccb_selrcnt;		/* selection retry counter */
	int ccb_tc;			/* timer counter */
	int ccb_tcmax;			/* max timeout */

	/*****************************************
	 * Sense data buffer
	 *****************************************/
	u_int8_t ccb_scsi_cmd[12];
	scsi_low_osdep_sense_data_t ccb_sense;
};

/*************************************************
 * Slccb functions
 *************************************************/
GENERIC_CCB_ASSERT(scsi_low, slccb)

/*************************************************
 * Target and Lun structures
 *************************************************/
struct scsi_low_softc;
LIST_HEAD(scsi_low_softc_tab, scsi_low_softc);
TAILQ_HEAD(targ_info_tab, targ_info);
LIST_HEAD(lun_info_tab, lun_info);

struct lun_info {
	struct scsi_low_osdep_lun_interface li_sloi;

	int li_lun;
	struct targ_info *li_ti;		/* my target */

	LIST_ENTRY(lun_info) lun_chain;		/* targ_info link */

	struct slccbtab li_discq;			/* disconnect queue */

	/*
	 * qtag control
	 */
	int li_maxnexus;
	int li_maxnqio;	
	int li_nqio;
	int li_disc;

#define	SCSI_LOW_MAXNEXUS (sizeof(u_int) * NBBY)
	u_int li_qtagbits;

#ifdef	SCSI_LOW_ALT_QTAG_ALLOCATE
	u_int8_t li_qtagarray[SCSI_LOW_MAXNEXUS];
	u_int li_qd;
#endif	/* SCSI_LOW_ALT_QTAG_ALLOCATE */

#define	SCSI_LOW_QFLAG_CA_QCLEAR	0x01
	u_int li_qflags;

	/*
	 * lun state
	 */
#define	SCSI_LOW_LUN_SLEEP	0x00
#define	SCSI_LOW_LUN_START	0x01
#define	SCSI_LOW_LUN_INQ	0x02
#define	SCSI_LOW_LUN_MODEQ	0x03
#define	SCSI_LOW_LUN_OK		0x04
	u_int li_state;				/* target lun state */

	/*
	 * lun control flags
 	 */
	u_int li_flags_valid;	/* valid flags */
#define	SCSI_LOW_LUN_FLAGS_USER_VALID	0x0001
#define	SCSI_LOW_LUN_FLAGS_DISK_VALID	0x0002
#define	SCSI_LOW_LUN_FLAGS_QUIRKS_VALID	0x0004
#define	SCSI_LOW_LUN_FLAGS_ALL_VALID \
	(SCSI_LOW_LUN_FLAGS_USER_VALID | \
	 SCSI_LOW_LUN_FLAGS_DISK_VALID | SCSI_LOW_LUN_FLAGS_QUIRKS_VALID)

	u_int li_flags;		/* real lun control flags */
	u_int li_cfgflags;	/* lun control flags given by user */
	u_int li_diskflags;	/* lun control flags given by hardware info */
	u_int li_quirks;	/* lun control flags given by upper layer */

	/* inq buffer */
	struct scsi_low_inq_data {
		u_int8_t sd_type;	
		u_int8_t sd_sp1;
		u_int8_t sd_version;
		u_int8_t sd_resp;
		u_int8_t sd_len;
		u_int8_t sd_sp2[2];
		u_int8_t sd_support;
	} __attribute__((packed)) li_inq;

	/* modeq buffer */
	struct scsi_low_mode_sense_data {
		u_int8_t sms_header[4];
		struct {
			u_int8_t cmp_page;
			u_int8_t cmp_length;
			u_int8_t cmp_rlec;
			u_int8_t cmp_qc;
			u_int8_t cmp_eca;
			u_int8_t cmp_spare[3];
		} __attribute__((packed)) sms_cmp;	
	
	} li_sms;	
};

struct scsi_low_msg_log {
	int slml_ptr;
	struct {
		u_int8_t msg[2];
	} slml_msg[SCSI_LOW_MSG_LOG_DATALEN];
};

struct targ_info {
	struct scsi_low_osdep_targ_interface ti_slti;

	TAILQ_ENTRY(targ_info) ti_chain;	/* targ_info link */

	struct scsi_low_softc *ti_sc;		/* our softc */
	u_int ti_id;				/* scsi id */

	/*
	 * Lun chain
	 */
	struct lun_info_tab ti_litab;		/* lun chain */

	/*
	 * total disconnected nexus
	 */
	int ti_disc;

	/*
	 * Scsi phase control
 	 */

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
	 * Msg in
 	 */
	u_int ti_msginptr;			/* msgin ptr */
	u_int ti_msginlen;			/* expected msg length */
	int ti_msgin_parity_error;		/* parity error detected */
	u_int8_t ti_msgin[SCSI_LOW_MAX_MSGLEN];	/* msgin buffer */

	/*
	 * Msg out
 	 */
	u_int ti_msgflags;			/* msgs to be asserted */
	u_int ti_omsgflags;			/* msgs asserted */
	u_int ti_emsgflags;			/* a msg currently asserted */
#define	SCSI_LOW_MSG_RESET	0x00000001
#define	SCSI_LOW_MSG_REJECT	0x00000002
#define	SCSI_LOW_MSG_PARITY	0x00000004
#define	SCSI_LOW_MSG_ERROR	0x00000008
#define	SCSI_LOW_MSG_IDENTIFY	0x00000010
#define	SCSI_LOW_MSG_ABORT	0x00000020
#define	SCSI_LOW_MSG_TERMIO	0x00000040
#define	SCSI_LOW_MSG_SIMPLE_QTAG	0x00000080
#define	SCSI_LOW_MSG_ORDERED_QTAG	0x00000100
#define	SCSI_LOW_MSG_HEAD_QTAG		0x00000200
#define	SCSI_LOW_MSG_ABORT_QTAG 0x00000400
#define	SCSI_LOW_MSG_CLEAR_QTAG 0x00000800
#define	SCSI_LOW_MSG_WIDE	0x00001000
#define	SCSI_LOW_MSG_SYNCH	0x00002000
#define	SCSI_LOW_MSG_NOOP	0x00004000
#define	SCSI_LOW_MSG_LAST	0x00008000
#define	SCSI_LOW_MSG_ALL	0xffffffff

	/* msgout buffer */
	u_int8_t ti_msgoutstr[SCSI_LOW_MAX_MSGLEN];	/* scsi msgout */
	u_int ti_msgoutlen;			/* msgout strlen */

	/*
	 * target initialize msgout 
 	 */
	u_int ti_setup_msg;		/* setup msgout requests */
	u_int ti_setup_msg_done;

	/*
	 * synch and wide data info
 	 */
	u_int ti_flags_valid;	/* valid flags */
#define	SCSI_LOW_TARG_FLAGS_USER_VALID		0x0001
#define	SCSI_LOW_TARG_FLAGS_DISK_VALID		0x0002
#define	SCSI_LOW_TARG_FLAGS_QUIRKS_VALID	0x0004
#define	SCSI_LOW_TARG_FLAGS_ALL_VALID \
	(SCSI_LOW_TARG_FLAGS_USER_VALID | \
	 SCSI_LOW_TARG_FLAGS_DISK_VALID | SCSI_LOW_TARG_FLAGS_QUIRKS_VALID)

	u_int ti_diskflags;	/* given target disk flags */
	u_int ti_quirks;	/* given target quirk */

	struct synch {
		u_int8_t offset;
		u_int8_t period;
	} ti_osynch, ti_maxsynch;		/* synch data */

#define	SCSI_LOW_BUS_WIDTH_8	0
#define	SCSI_LOW_BUS_WIDTH_16	1
#define	SCSI_LOW_BUS_WIDTH_32	2
	u_int ti_owidth, ti_width;

	/*
	 * lun info size.
 	 */
	int ti_lunsize;	

#ifdef	SCSI_LOW_DIAGNOSTIC
	struct scsi_low_msg_log ti_log_msgout;
	struct scsi_low_msg_log ti_log_msgin;
#endif	/* SCSI_LOW_DIAGNOSTIC */
};

/*************************************************
 * COMMON HEADER STRUCTURE
 *************************************************/
struct scsi_low_softc;
struct proc;
typedef struct scsi_low_softc *sc_low_t;

#define	SCSI_LOW_START_OK	0
#define	SCSI_LOW_START_FAIL	1
#define	SCSI_LOW_INFO_ALLOC	0
#define	SCSI_LOW_INFO_REVOKE	1
#define	SCSI_LOW_INFO_DEALLOC	2
#define	SCSI_LOW_POWDOWN	1
#define	SCSI_LOW_ENGAGE		2

#define	SC_LOW_INIT_T (int (*) __P((sc_low_t, int)))
#define	SC_LOW_BUSRST_T (void (*) __P((sc_low_t)))
#define	SC_LOW_TARG_INIT_T (int (*) __P((sc_low_t, struct targ_info *, int)))
#define	SC_LOW_LUN_INIT_T (int (*) __P((sc_low_t, struct targ_info *, struct lun_info *, int)))
#define	SC_LOW_SELECT_T (int (*) __P((sc_low_t, struct slccb *)))
#define	SC_LOW_ATTEN_T (void (*) __P((sc_low_t)))
#define	SC_LOW_NEXUS_T (int (*) __P((sc_low_t)))
#define	SC_LOW_MSG_T (int (*) __P((sc_low_t, struct targ_info *, u_int)))
#define	SC_LOW_POLL_T (int (*) __P((void *)))
#define	SC_LOW_POWER_T (int (*) __P((sc_low_t, u_int)))
#define	SC_LOW_TIMEOUT_T (int (*) __P((sc_low_t)))

struct scsi_low_funcs {
	int (*scsi_low_init) __P((sc_low_t, int));
	void (*scsi_low_bus_reset) __P((sc_low_t));
	int (*scsi_low_targ_init) __P((sc_low_t, struct targ_info *, int));
	int (*scsi_low_lun_init) __P((sc_low_t, struct targ_info *, struct lun_info *, int));
	int (*scsi_low_start_bus) __P((sc_low_t, struct slccb *));
	int (*scsi_low_establish_lun_nexus) __P((sc_low_t));
	int (*scsi_low_establish_ccb_nexus) __P((sc_low_t));
	void (*scsi_low_attention) __P((sc_low_t));
	int (*scsi_low_msg) __P((sc_low_t, struct targ_info *, u_int));
	int (*scsi_low_timeout) __P((sc_low_t));
	int (*scsi_low_poll) __P((void *));
	int (*scsi_low_power) __P((sc_low_t, u_int));
	int (*scsi_low_ioctl) __P((sc_low_t, u_long, caddr_t, int, struct proc *));
};

struct scsi_low_softc {
	/* os depend structure */
	struct scsi_low_osdep_interface sl_si;
#define	sl_dev	sl_si.si_dev
	struct scsi_low_osdep_funcs *sl_osdep_fp;
	u_char sl_xname[16];
				
	/* our chain */
	LIST_ENTRY(scsi_low_softc) sl_chain;

	/* my targets */
	struct targ_info *sl_ti[SCSI_LOW_NTARGETS];
	struct targ_info_tab sl_titab;

	/* current active T_L_Q nexus */
	struct targ_info *sl_Tnexus;		/* Target nexus */
	struct lun_info *sl_Lnexus;		/* Lun nexus */
	struct slccb *sl_Qnexus;			/* Qtag nexus */
	int sl_nexus_call;

	/* ccb start queue */
	struct slccbtab sl_start;	

	/* retry limit and phase change counter */
	int sl_max_retry;
	int sl_ph_count;
	int sl_timeout_count;

	/* selection & total num disconnect targets */
	int sl_nio;
	int sl_disc;
	int sl_retry_sel;
	struct slccb *sl_selid;

	/* attention */
	int sl_atten;			/* ATN asserted */
	int sl_clear_atten;		/* negate ATN required */

	/* scsi phase suggested by scsi msg */
	u_int sl_msgphase;	
#define	MSGPH_NULL	0x00		/* no msg */
#define	MSGPH_DISC	0x01		/* disconnect msg */
#define	MSGPH_CMDC	0x02		/* cmd complete msg */
#define	MSGPH_ABORT	0x03		/* abort seq */
#define	MSGPH_TERM	0x04		/* current io terminate */
#define	MSGPH_LCTERM	0x05		/* cmd link terminated */
#define	MSGPH_RESET	0x06		/* reset target */

	/* error */
	u_int sl_error;			/* error flags */
#define	FATALIO		0x0001		/* generic io error & retry io */
#define	ABORTIO		0x0002		/* generic io error & terminate io */
#define	TIMEOUTIO	0x0004		/* watch dog timeout */
#define	SELTIMEOUTIO	0x0008		/* selection timeout */
#define	PDMAERR		0x0010		/* dma xfer error */
#define	MSGERR		0x0020		/* msgsys error */
#define	PARITYERR	0x0040		/* parity error */
#define	BUSYERR		0x0080		/* target busy error */
#define	STATERR		0x0100		/* status error */
#define	UACAERR		0x0200		/* target CA state, no sense check */
#define	SENSEIO		0x1000		/* cmd not excuted but sense data ok */
#define	SENSEERR	0x2000		/* cmd not excuted and sense data bad */
#define	UBFERR		0x4000		/* unexpected bus free */
#define	PENDINGIO	0x8000		/* ccb start not yet */
#define	SCSI_LOW_ERRORBITS "\020\017ubferr\016senseerr\015senseio\012uacaerr\011staterr\010busy\007parity\006msgerr\005pdmaerr\004seltimeout\003timeout\002abort\001fatal"

	/* current scsi data pointer */
	struct sc_p sl_scp;

	/* power control */
	u_int sl_active;		/* host is busy state */
	int sl_powc;			/* power down timer counter */
	u_int sl_rstep;			/* resume step */

	/* configuration flags */
	u_int sl_flags;		
#define	HW_POWDOWN	0x0001
#define	HW_RESUME	0x0002
#define	HW_PDMASTART	0x0004
#define	HW_INACTIVE	0x0008
#define	HW_POWERCTRL	0x0010
#define	HW_INITIALIZING 0x0020
#define	HW_READ_PADDING		0x1000
#define	HW_WRITE_PADDING	0x2000

	u_int sl_cfgflags;
#define	CFG_NODISC		0x0001
#define	CFG_NOPARITY		0x0002
#define	CFG_NOATTEN		0x0004
#define	CFG_ASYNC		0x0008
#define	CFG_NOQTAG		0x0010

	int sl_show_result;
#define	SHOW_SYNCH_NEG	0x0001
#define	SHOW_WIDE_NEG	0x0002
#define	SHOW_CALCF_RES	0x0010
#define	SHOW_PROBE_RES	0x0020
#define	SHOW_ALL_NEG	-1

	/* host informations */
	u_int sl_hostid;
	int sl_nluns;
	int sl_ntargs;
	int sl_openings;

	/* interface functions */
	struct scsi_low_funcs *sl_funcs;

	/* targinfo size */
	int sl_targsize;

#if	defined(i386) || defined(__i386__)
	u_int sl_irq;		/* XXX */
#endif	/* i386 */
};

/*************************************************
 * SCSI LOW service functions
 *************************************************/
/* 
 * Scsi low attachment function.
 */
int scsi_low_attach __P((struct scsi_low_softc *, int, int, int, int, int));
int scsi_low_dettach __P((struct scsi_low_softc *));

/* 
 * Scsi low interface activate or deactivate functions
 */
int scsi_low_is_busy __P((struct scsi_low_softc *));
int scsi_low_activate __P((struct scsi_low_softc *));
int scsi_low_deactivate __P((struct scsi_low_softc *));

/* 
 * Scsi phase "bus service" functions.
 * These functions are corresponding to each scsi bus phaeses.
 */
/* bus idle phase (other initiators or targets release bus) */
void scsi_low_bus_idle __P((struct scsi_low_softc *));

/* arbitration and selection phase */
void scsi_low_arbit_fail __P((struct scsi_low_softc *, struct slccb *));
static __inline void scsi_low_arbit_win __P((struct scsi_low_softc *));

/* msgout phase */
#define	SCSI_LOW_MSGOUT_INIT		0x00000001
#define	SCSI_LOW_MSGOUT_UNIFY		0x00000002
int scsi_low_msgout __P((struct scsi_low_softc *, struct targ_info *, u_int));

/* msgin phase */
#define SCSI_LOW_DATA_PE	0x80000000
int scsi_low_msgin __P((struct scsi_low_softc *, struct targ_info *, u_int));

/* statusin phase */
static __inline int scsi_low_statusin __P((struct scsi_low_softc *, struct targ_info *, u_int));

/* data phase */
int scsi_low_data __P((struct scsi_low_softc *, struct targ_info *, struct buf **, int));
static __inline void scsi_low_data_finish __P((struct scsi_low_softc *));

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

/* bus reset utility */
void scsi_low_bus_reset __P((struct scsi_low_softc *));

/*************************************************
 * Message macro defs
 *************************************************/
#define	SCSI_LOW_SETUP_PHASE(ti, phase)			\
{							\
	(ti)->ti_ophase = ti->ti_phase;			\
	(ti)->ti_phase = (phase);			\
}

#define	SCSI_LOW_SETUP_MSGPHASE(slp, PHASE)		\
{							\
	(slp)->sl_msgphase = (PHASE);			\
}

#define	SCSI_LOW_ASSERT_ATN(slp)			\
{							\
	(slp)->sl_atten = 1;				\
}

#define	SCSI_LOW_DEASSERT_ATN(slp)			\
{							\
	(slp)->sl_atten = 0;				\
}

/*************************************************
 * Inline functions
 *************************************************/
static __inline void scsi_low_attention __P((struct scsi_low_softc *));
static __inline int scsi_low_is_msgout_continue __P((struct targ_info *, u_int));
static __inline int scsi_low_assert_msg __P((struct scsi_low_softc *, struct targ_info *, u_int, int));
static __inline int scsi_low_is_disconnect_ok __P((struct slccb *));

static __inline int
scsi_low_is_msgout_continue(ti, mask)
	struct targ_info *ti;
	u_int mask;
{
	
	return ((ti->ti_msgflags & (~mask)) != 0);
}

static __inline int
scsi_low_is_disconnect_ok(cb)
	struct slccb *cb;
{

	return ((cb->li->li_flags & SCSI_LOW_DISC) != 0 &&
		    (cb->ccb_flags & (CCB_SENSE | CCB_CLEARQ)) == 0);
}

static __inline void
scsi_low_attention(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_atten != 0)
		return;

	(*slp->sl_funcs->scsi_low_attention) (slp);
	SCSI_LOW_ASSERT_ATN(slp);
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
		scsi_low_attention(slp);
	return 0;
}

static __inline void
scsi_low_arbit_win(slp)
	struct scsi_low_softc *slp;
{

	slp->sl_selid = NULL;
}

static __inline void
scsi_low_data_finish(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_Qnexus != NULL)
	{
		slp->sl_Qnexus->ccb_datalen = slp->sl_scp.scp_datalen;
	}
}

static __inline int
scsi_low_statusin(slp, ti, c)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int c;
{

	slp->sl_ph_count ++;
	if ((c & SCSI_LOW_DATA_PE) != 0)
	{
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ERROR, 0);
		return EIO;
	}
	slp->sl_scp.scp_status = (u_int8_t) c;
	return 0;
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
#define	ST_CMDTERM	0x22
#define	ST_QUEFULL	0x28
#define	ST_UNKNOWN	0xff

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
#define	MSG_ABORT_QTAG	0x0d
#define	MSG_CLEAR_QTAG	0x0e
#define	MSG_TERM_IO	0x11
#define	MSG_SIMPLE_QTAG	0x20
#define	MSG_HEAD_QTAG	0x21
#define	MSG_ORDERED_QTAG	0x22
#define	MSG_IDENTIFY	  	0x80
#define	MSG_IDENTIFY_DISCPRIV	0x40
#endif	/* !_SCSI_LOW_H_ */
