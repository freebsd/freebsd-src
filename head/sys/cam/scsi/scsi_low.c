/*	$NecBSD: scsi_low.c,v 1.24.10.8 2001/06/26 07:39:44 honda Exp $	*/
/*	$NetBSD$	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define	SCSI_LOW_STATICS
#define	SCSI_LOW_DEBUG
#define SCSI_LOW_NEGOTIATE_BEFORE_SENSE
#define	SCSI_LOW_START_UP_CHECK

/* #define	SCSI_LOW_INFO_DETAIL */

/* #define	SCSI_LOW_QCLEAR_AFTER_CA */
/* #define	SCSI_LOW_FLAGS_QUIRKS_OK */

#ifdef __NetBSD__
#define	SCSI_LOW_TARGET_OPEN
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#define	SCSI_LOW_FLAGS_QUIRKS_OK
#endif	/* __FreeBSD__ */

/*-
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

/* <On the nexus establishment>
 * When our host is reselected, 
 * nexus establish processes are little complicated.
 * Normal steps are followings:
 * 1) Our host selected by target => target nexus (slp->sl_Tnexus) 
 * 2) Identify msgin => lun nexus (slp->sl_Lnexus)
 * 3) Qtag msg => ccb nexus (slp->sl_Qnexus)
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#ifdef __FreeBSD__
#if __FreeBSD_version >= 500001
#include <sys/bio.h>
#else
#include <machine/clock.h>
#endif
#endif	/* __FreeBSD__ */

#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#ifdef	__NetBSD__
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <dev/cons.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <sys/scsiio.h>

#include <i386/Cbus/dev/scsi_low.h>
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <cam/scsi/scsi_low.h>

#include <sys/cons.h>
#endif	/* __FreeBSD__ */

/**************************************************************
 * Constants
 **************************************************************/
#define	SCSI_LOW_POLL_HZ	1000

/* functions return values */
#define	SCSI_LOW_START_NO_QTAG	0
#define	SCSI_LOW_START_QTAG	1

#define	SCSI_LOW_DONE_COMPLETE	0
#define	SCSI_LOW_DONE_RETRY	1

/* internal disk flags */
#define	SCSI_LOW_DISK_DISC	0x00000001
#define	SCSI_LOW_DISK_QTAG	0x00000002
#define	SCSI_LOW_DISK_LINK	0x00000004
#define	SCSI_LOW_DISK_PARITY	0x00000008
#define	SCSI_LOW_DISK_SYNC	0x00010000
#define	SCSI_LOW_DISK_WIDE_16	0x00020000
#define	SCSI_LOW_DISK_WIDE_32	0x00040000
#define	SCSI_LOW_DISK_WIDE	(SCSI_LOW_DISK_WIDE_16 | SCSI_LOW_DISK_WIDE_32)
#define	SCSI_LOW_DISK_LFLAGS	0x0000ffff
#define	SCSI_LOW_DISK_TFLAGS	0xffff0000

MALLOC_DEFINE(M_SCSILOW, "SCSI low", "SCSI low buffers");

/**************************************************************
 * Declarations
 **************************************************************/
/* static */ void scsi_low_info(struct scsi_low_softc *, struct targ_info *, u_char *);
static void scsi_low_engage(void *);
static struct slccb *scsi_low_establish_ccb(struct targ_info *, struct lun_info *, scsi_low_tag_t);
static int scsi_low_done(struct scsi_low_softc *, struct slccb *);
static int scsi_low_setup_done(struct scsi_low_softc *, struct slccb *);
static void scsi_low_bus_release(struct scsi_low_softc *, struct targ_info *);
static void scsi_low_twiddle_wait(void);
static struct lun_info *scsi_low_alloc_li(struct targ_info *, int, int);
static struct targ_info *scsi_low_alloc_ti(struct scsi_low_softc *, int);
static void scsi_low_calcf_lun(struct lun_info *);
static void scsi_low_calcf_target(struct targ_info *);
static void scsi_low_calcf_show(struct lun_info *);
static void scsi_low_reset_nexus(struct scsi_low_softc *, int);
static void scsi_low_reset_nexus_target(struct scsi_low_softc *, struct targ_info *, int);
static void scsi_low_reset_nexus_lun(struct scsi_low_softc *, struct lun_info *, int);
static int scsi_low_init(struct scsi_low_softc *, u_int);
static void scsi_low_start(struct scsi_low_softc *);
static void scsi_low_free_ti(struct scsi_low_softc *);

static int scsi_low_alloc_qtag(struct slccb *);
static int scsi_low_dealloc_qtag(struct slccb *);
static int scsi_low_enqueue(struct scsi_low_softc *, struct targ_info *, struct lun_info *, struct slccb *, u_int, u_int);
static int scsi_low_message_enqueue(struct scsi_low_softc *, struct targ_info *, struct lun_info *, u_int);
static void scsi_low_unit_ready_cmd(struct slccb *);
static void scsi_low_timeout(void *);
static int scsi_low_timeout_check(struct scsi_low_softc *);
#ifdef	SCSI_LOW_START_UP_CHECK
static int scsi_low_start_up(struct scsi_low_softc *);
#endif	/* SCSI_LOW_START_UP_CHECK */
static int scsi_low_abort_ccb(struct scsi_low_softc *, struct slccb *);
static struct slccb *scsi_low_revoke_ccb(struct scsi_low_softc *, struct slccb *, int);

int scsi_low_version_major = 2;
int scsi_low_version_minor = 17;

static struct scsi_low_softc_tab sl_tab = LIST_HEAD_INITIALIZER(sl_tab);

/**************************************************************
 * Debug, Run test and Statics
 **************************************************************/
#ifdef	SCSI_LOW_INFO_DETAIL
#define	SCSI_LOW_INFO(slp, ti, s) scsi_low_info((slp), (ti), (s))
#else	/* !SCSI_LOW_INFO_DETAIL */
#define	SCSI_LOW_INFO(slp, ti, s) printf("%s: %s\n", (slp)->sl_xname, (s))
#endif	/* !SCSI_LOW_INFO_DETAIL */

#ifdef	SCSI_LOW_STATICS
static struct scsi_low_statics {
	int nexus_win;
	int nexus_fail;
	int nexus_disconnected;
	int nexus_reselected;
	int nexus_conflict;
} scsi_low_statics;
#endif	/* SCSI_LOW_STATICS */

#ifdef	SCSI_LOW_DEBUG
#define	SCSI_LOW_DEBUG_DONE	0x00001
#define	SCSI_LOW_DEBUG_DISC	0x00002
#define	SCSI_LOW_DEBUG_SENSE	0x00004
#define	SCSI_LOW_DEBUG_CALCF	0x00008
#define	SCSI_LOW_DEBUG_ACTION	0x10000
int scsi_low_debug = 0;

#define	SCSI_LOW_MAX_ATTEN_CHECK	32
#define	SCSI_LOW_ATTEN_CHECK	0x0001
#define	SCSI_LOW_CMDLNK_CHECK	0x0002
#define	SCSI_LOW_ABORT_CHECK	0x0004
#define	SCSI_LOW_NEXUS_CHECK	0x0008
int scsi_low_test = 0;
int scsi_low_test_id = 0;

static void scsi_low_test_abort(struct scsi_low_softc *, struct targ_info *, struct lun_info *);
static void scsi_low_test_cmdlnk(struct scsi_low_softc *, struct slccb *);
static void scsi_low_test_atten(struct scsi_low_softc *, struct targ_info *, u_int);
#define	SCSI_LOW_DEBUG_TEST_GO(fl, id) \
	((scsi_low_test & (fl)) != 0 && (scsi_low_test_id & (1 << (id))) == 0)
#define	SCSI_LOW_DEBUG_GO(fl, id) \
	((scsi_low_debug & (fl)) != 0 && (scsi_low_test_id & (1 << (id))) == 0)
#endif	/* SCSI_LOW_DEBUG */

/**************************************************************
 * CCB
 **************************************************************/
GENERIC_CCB_STATIC_ALLOC(scsi_low, slccb)
GENERIC_CCB(scsi_low, slccb, ccb_chain)

/**************************************************************
 * Inline functions
 **************************************************************/
#define	SCSI_LOW_INLINE	static __inline
SCSI_LOW_INLINE void scsi_low_activate_qtag(struct slccb *);
SCSI_LOW_INLINE void scsi_low_deactivate_qtag(struct slccb *);
SCSI_LOW_INLINE void scsi_low_ccb_message_assert(struct slccb *, u_int);
SCSI_LOW_INLINE void scsi_low_ccb_message_exec(struct scsi_low_softc *, struct slccb *);
SCSI_LOW_INLINE void scsi_low_ccb_message_retry(struct slccb *);
SCSI_LOW_INLINE void scsi_low_ccb_message_clear(struct slccb *);
SCSI_LOW_INLINE void scsi_low_init_msgsys(struct scsi_low_softc *, struct targ_info *);

SCSI_LOW_INLINE void
scsi_low_activate_qtag(cb)
	struct slccb *cb;
{
	struct lun_info *li = cb->li;

	if (cb->ccb_tag != SCSI_LOW_UNKTAG)
		return;

	li->li_nqio ++;
	cb->ccb_tag = cb->ccb_otag;
}
	
SCSI_LOW_INLINE void
scsi_low_deactivate_qtag(cb)
	struct slccb *cb;
{
	struct lun_info *li = cb->li;

	if (cb->ccb_tag == SCSI_LOW_UNKTAG)
		return;

	li->li_nqio --;
	cb->ccb_tag = SCSI_LOW_UNKTAG;
}
	
SCSI_LOW_INLINE void
scsi_low_ccb_message_exec(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{

	scsi_low_assert_msg(slp, cb->ti, cb->ccb_msgoutflag, 0);
	cb->ccb_msgoutflag = 0;
}

SCSI_LOW_INLINE void
scsi_low_ccb_message_assert(cb, msg)
	struct slccb *cb;
	u_int msg;
{

	cb->ccb_msgoutflag = cb->ccb_omsgoutflag = msg;
}

SCSI_LOW_INLINE void
scsi_low_ccb_message_retry(cb)
	struct slccb *cb;
{
	cb->ccb_msgoutflag = cb->ccb_omsgoutflag;
}

SCSI_LOW_INLINE void
scsi_low_ccb_message_clear(cb)
	struct slccb *cb;
{
	cb->ccb_msgoutflag = 0;
}

SCSI_LOW_INLINE void
scsi_low_init_msgsys(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	ti->ti_msginptr = 0;
	ti->ti_emsgflags = ti->ti_msgflags = ti->ti_omsgflags = 0;
	SCSI_LOW_DEASSERT_ATN(slp);
	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_NULL);
}

/*=============================================================
 * START OF OS switch  (All OS depend fucntions should be here)
 =============================================================*/
/* common os depend utitlities */
#define	SCSI_LOW_CMD_RESIDUAL_CHK	0x0001
#define	SCSI_LOW_CMD_ORDERED_QTAG	0x0002
#define	SCSI_LOW_CMD_ABORT_WARNING	0x0004

static u_int8_t scsi_low_cmd_flags[256] = {
/*	0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
/*0*/	0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 5, 0, 0, 0, 0, 0,
/*1*/	0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0,
/*2*/	0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 5, 0, 0, 0, 5, 5,
/*3*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5,
};

struct scsi_low_error_code {
	int error_bits;
	int error_code;
};

static struct slccb *scsi_low_find_ccb(struct scsi_low_softc *, u_int, u_int, void *);
static int scsi_low_translate_error_code(struct slccb *, struct scsi_low_error_code *);

static struct slccb *
scsi_low_find_ccb(slp, target, lun, osdep)
	struct scsi_low_softc *slp;
	u_int target, lun;
	void *osdep;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;

	ti = slp->sl_ti[target];
	li = scsi_low_alloc_li(ti, lun, 0);
	if (li == NULL)
		return NULL;

	if ((cb = slp->sl_Qnexus) != NULL && cb->osdep == osdep)
		return cb;

	for (cb = TAILQ_FIRST(&slp->sl_start); cb != NULL;
	     cb = TAILQ_NEXT(cb, ccb_chain))
	{
		if (cb->osdep == osdep)
			return cb;
	}

	for (cb = TAILQ_FIRST(&li->li_discq); cb != NULL;
	     cb = TAILQ_NEXT(cb, ccb_chain))
	{
		if (cb->osdep == osdep)
			return cb;
	}
	return NULL;
}

static int 
scsi_low_translate_error_code(cb, tp)
	struct slccb *cb;
	struct scsi_low_error_code *tp;
{

	if (cb->ccb_error == 0)
		return tp->error_code;

	for (tp ++; (cb->ccb_error & tp->error_bits) == 0; tp ++)
		;
	return tp->error_code;
}

#ifdef SCSI_LOW_INTERFACE_XS
/**************************************************************
 * SCSI INTERFACE (XS)
 **************************************************************/
#define	SCSI_LOW_MINPHYS		0x10000
#define	SCSI_LOW_MALLOC(size)		malloc((size), M_SCSILOW, M_NOWAIT)
#define	SCSI_LOW_FREE(pt)		free((pt), M_SCSILOW)
#define	SCSI_LOW_ALLOC_CCB(flags)	scsi_low_get_ccb((flags))
#define	SCSI_LOW_XS_POLL_HZ		1000

static int scsi_low_poll_xs(struct scsi_low_softc *, struct slccb *);
static void scsi_low_scsi_minphys_xs(struct buf *);
#ifdef	SCSI_LOW_TARGET_OPEN
static int scsi_low_target_open(struct scsipi_link *, struct cfdata *);
#endif	/* SCSI_LOW_TARGET_OPEN */
static int scsi_low_scsi_cmd_xs(struct scsipi_xfer *);
static int scsi_low_enable_xs(void *, int);
static int scsi_low_ioctl_xs(struct scsipi_link *, u_long, caddr_t, int, struct proc *);

static int scsi_low_attach_xs(struct scsi_low_softc *);
static int scsi_low_world_start_xs(struct scsi_low_softc *);
static int scsi_low_dettach_xs(struct scsi_low_softc *);
static int scsi_low_ccb_setup_xs(struct scsi_low_softc *, struct slccb *);
static int scsi_low_done_xs(struct scsi_low_softc *, struct slccb *);
static void scsi_low_timeout_xs(struct scsi_low_softc *, int, int);
static u_int scsi_low_translate_quirks_xs(u_int);
static void scsi_low_setup_quirks_xs(struct targ_info *, struct lun_info *, u_int);

struct scsi_low_osdep_funcs scsi_low_osdep_funcs_xs = {
	scsi_low_attach_xs,
	scsi_low_world_start_xs,
	scsi_low_dettach_xs,
	scsi_low_ccb_setup_xs,
	scsi_low_done_xs,
	scsi_low_timeout_xs
};
	
struct scsipi_device scsi_low_dev = {
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
};

struct scsi_low_error_code scsi_low_error_code_xs[] = {
	{0,		XS_NOERROR},
	{SENSEIO,	XS_SENSE},
	{BUSYERR,	XS_BUSY	},
	{SELTIMEOUTIO,	XS_SELTIMEOUT},
	{TIMEOUTIO,	XS_TIMEOUT},
	{-1,		XS_DRIVER_STUFFUP}
};

static int
scsi_low_ioctl_xs(link, cmd, addr, flag, p)
	struct scsipi_link *link;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct scsi_low_softc *slp;
	int s, error = ENOTTY;

	slp = (struct scsi_low_softc *) link->adapter_softc;
	if ((slp->sl_flags & HW_INACTIVE) != 0)
		return ENXIO;

	if (cmd == SCBUSIORESET)
	{
		s = SCSI_LOW_SPLSCSI();
		scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, NULL);
		splx(s);
		error = 0;
	}
	else if (slp->sl_funcs->scsi_low_ioctl != 0)
	{
		error = (*slp->sl_funcs->scsi_low_ioctl)
				(slp, cmd, addr, flag, p);
	}

	return error;
}

static int
scsi_low_enable_xs(arg, enable)
	void *arg;
	int enable;
{
	struct scsi_low_softc *slp = arg;

	if (enable != 0)
	{
		if ((slp->sl_flags & HW_INACTIVE) != 0)
			return ENXIO;
	}
	else
	{
		if ((slp->sl_flags & HW_INACTIVE) != 0 ||
		    (slp->sl_flags & HW_POWERCTRL) == 0)
			return 0;

		slp->sl_flags |= HW_POWDOWN;
		if (slp->sl_funcs->scsi_low_power != NULL)
		{
			(*slp->sl_funcs->scsi_low_power)
					(slp, SCSI_LOW_POWDOWN);
		}
	}
	return 0;
}

static void
scsi_low_scsi_minphys_xs(bp)
	struct buf *bp;
{

	if (bp->b_bcount > SCSI_LOW_MINPHYS)
		bp->b_bcount = SCSI_LOW_MINPHYS;
	minphys(bp);
}

static int
scsi_low_poll_xs(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct scsipi_xfer *xs = cb->osdep;
	int tcount;

	cb->ccb_flags |= CCB_NOSDONE;
	tcount = 0;

	while (slp->sl_nio > 0)
	{
		SCSI_LOW_DELAY((1000 * 1000) / SCSI_LOW_XS_POLL_HZ);

		(*slp->sl_funcs->scsi_low_poll) (slp);

		if ((slp->sl_flags & (HW_INACTIVE | HW_INITIALIZING)) != 0)
		{
			cb->ccb_flags |= CCB_NORETRY;
			cb->ccb_error |= FATALIO;
			(void) scsi_low_revoke_ccb(slp, cb, 1);
			printf("%s: hardware inactive in poll mode\n", 
				slp->sl_xname);
		}

		if ((xs->flags & ITSDONE) != 0)
			break;

		if (tcount ++ < SCSI_LOW_XS_POLL_HZ / SCSI_LOW_TIMEOUT_HZ)
			continue;

		tcount = 0;
		scsi_low_timeout_check(slp);
	}

	xs->flags |= ITSDONE;
	scsipi_done(xs);
	return COMPLETE;
}

static int
scsi_low_scsi_cmd_xs(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *splp = xs->sc_link;
	struct scsi_low_softc *slp = splp->adapter_softc;
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	int s, targ, lun, flags, rv;

	if ((cb = SCSI_LOW_ALLOC_CCB(xs->flags & SCSI_NOSLEEP)) == NULL)
		return TRY_AGAIN_LATER;

	targ = splp->scsipi_scsi.target,
	lun = splp->scsipi_scsi.lun;
	ti = slp->sl_ti[targ];

	cb->osdep = xs;
	cb->bp = xs->bp;

	if ((xs->flags & SCSI_POLL) == 0)
		flags = CCB_AUTOSENSE;
	else
		flags = CCB_AUTOSENSE | CCB_POLLED;
		

	s = SCSI_LOW_SPLSCSI();
	li = scsi_low_alloc_li(ti, lun, 1);
	if ((u_int) splp->quirks != li->li_sloi.sloi_quirks)
	{
		scsi_low_setup_quirks_xs(ti, li, (u_int) splp->quirks);
	}

	if ((xs->flags & SCSI_RESET) != 0)
	{
		flags |= CCB_NORETRY | CCB_URGENT;
		scsi_low_enqueue(slp, ti, li, cb, flags, SCSI_LOW_MSG_RESET);
	}
	else
	{
		if (ti->ti_setup_msg != 0)
		{
			scsi_low_message_enqueue(slp, ti, li, flags);
		}

		flags |= CCB_SCSIIO;
		scsi_low_enqueue(slp, ti, li, cb, flags, 0);
	}

#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_ABORT_CHECK, ti->ti_id) != 0)
	{
		scsi_low_test_abort(slp, ti, li);
	}
#endif	/* SCSI_LOW_DEBUG */

	if ((cb->ccb_flags & CCB_POLLED) != 0)
	{
		rv = scsi_low_poll_xs(slp, cb);
	}
	else
	{
		rv = SUCCESSFULLY_QUEUED;
	}
	splx(s);
	return rv;
}

static int
scsi_low_attach_xs(slp)
	struct scsi_low_softc *slp;
{
	struct scsipi_adapter *sap;
	struct scsipi_link *splp;

	strncpy(slp->sl_xname, slp->sl_dev.dv_xname, 16);

	sap = SCSI_LOW_MALLOC(sizeof(*sap));
	if (sap == NULL)
		return ENOMEM;
	splp = SCSI_LOW_MALLOC(sizeof(*splp));
	if (splp == NULL)
	{
		SCSI_LOW_FREE(sap);
		return ENOMEM;
	}

	SCSI_LOW_BZERO(sap, sizeof(*sap));
	SCSI_LOW_BZERO(splp, sizeof(*splp));

	sap->scsipi_cmd = scsi_low_scsi_cmd_xs;
	sap->scsipi_minphys = scsi_low_scsi_minphys_xs;
	sap->scsipi_enable = scsi_low_enable_xs;
	sap->scsipi_ioctl = scsi_low_ioctl_xs;
#ifdef	SCSI_LOW_TARGET_OPEN
	sap->open_target_lu = scsi_low_target_open;
#endif	/* SCSI_LOW_TARGET_OPEN */

	splp->adapter_softc = slp;
	splp->scsipi_scsi.adapter_target = slp->sl_hostid;
	splp->scsipi_scsi.max_target = slp->sl_ntargs - 1;
	splp->scsipi_scsi.max_lun = slp->sl_nluns - 1;
	splp->scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	splp->openings = slp->sl_openings;
	splp->type = BUS_SCSI;
	splp->adapter_softc = slp;
	splp->adapter = sap;
	splp->device = &scsi_low_dev;

	slp->sl_si.si_splp = splp;
	slp->sl_show_result = SHOW_ALL_NEG;
	return 0;
}

static int
scsi_low_world_start_xs(slp)
	struct scsi_low_softc *slp;
{

	return 0;
}

static int
scsi_low_dettach_xs(slp)
	struct scsi_low_softc *slp;
{

	/*
	 * scsipi does not have dettach bus fucntion.
	 *
	scsipi_dettach_scsibus(slp->sl_si.si_splp);
	*/
	return 0;
}

static int
scsi_low_ccb_setup_xs(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct scsipi_xfer *xs = (struct scsipi_xfer *) cb->osdep;

	if ((cb->ccb_flags & CCB_SCSIIO) != 0)
	{
		cb->ccb_scp.scp_cmd = (u_int8_t *) xs->cmd;
		cb->ccb_scp.scp_cmdlen = xs->cmdlen;
		cb->ccb_scp.scp_data = xs->data;
		cb->ccb_scp.scp_datalen = xs->datalen;
		cb->ccb_scp.scp_direction = (xs->flags & SCSI_DATA_OUT) ? 
					SCSI_LOW_WRITE : SCSI_LOW_READ;
		cb->ccb_tcmax = xs->timeout / 1000;
	}
	else
	{
		scsi_low_unit_ready_cmd(cb);
	}
	return SCSI_LOW_START_QTAG;
}

static int
scsi_low_done_xs(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct scsipi_xfer *xs;

	xs = (struct scsipi_xfer *) cb->osdep;
	if (cb->ccb_error == 0)
	{
		xs->error = XS_NOERROR;
		xs->resid = 0;
	}
	else 	
	{
	        if (cb->ccb_rcnt >= slp->sl_max_retry)
			cb->ccb_error |= ABORTIO;

		if ((cb->ccb_flags & CCB_NORETRY) == 0 &&
		    (cb->ccb_error & ABORTIO) == 0)
			return EJUSTRETURN;

		if ((cb->ccb_error & SENSEIO) != 0)
		{
			xs->sense.scsi_sense = cb->ccb_sense;
		}

		xs->error = scsi_low_translate_error_code(cb,
				&scsi_low_error_code_xs[0]);
	
#ifdef	SCSI_LOW_DIAGNOSTIC
		if ((cb->ccb_flags & CCB_SILENT) == 0 &&
		    cb->ccb_scp.scp_cmdlen > 0 &&
		    (scsi_low_cmd_flags[cb->ccb_scp.scp_cmd[0]] &
		     SCSI_LOW_CMD_ABORT_WARNING) != 0)
		{
			printf("%s: WARNING: scsi_low IO abort\n",
				slp->sl_xname);
			scsi_low_print(slp, NULL);
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	if (cb->ccb_scp.scp_status == ST_UNKNOWN)
		xs->status = 0;	/* XXX */
	else
		xs->status = cb->ccb_scp.scp_status;

	xs->flags |= ITSDONE;
	if ((cb->ccb_flags & CCB_NOSDONE) == 0)
		scsipi_done(xs);

	return 0;
}

static void
scsi_low_timeout_xs(slp, ch, action)
	struct scsi_low_softc *slp;
	int ch;
	int action;
{

	switch (ch)
	{
	case SCSI_LOW_TIMEOUT_CH_IO:
		switch (action)
		{
		case SCSI_LOW_TIMEOUT_START:
			timeout(scsi_low_timeout, slp,
				hz / SCSI_LOW_TIMEOUT_HZ);
			break;
		case SCSI_LOW_TIMEOUT_STOP:
			untimeout(scsi_low_timeout, slp);
			break;
		}
		break;

	case SCSI_LOW_TIMEOUT_CH_ENGAGE:
		switch (action)
		{
		case SCSI_LOW_TIMEOUT_START:
			timeout(scsi_low_engage, slp, 1);
			break;
		case SCSI_LOW_TIMEOUT_STOP:
			untimeout(scsi_low_engage, slp);
			break;
		}
		break;

	case SCSI_LOW_TIMEOUT_CH_RECOVER:
		break;
	}
}

u_int
scsi_low_translate_quirks_xs(quirks)
	u_int quirks;
{
	u_int flags;
	
	flags = SCSI_LOW_DISK_LFLAGS | SCSI_LOW_DISK_TFLAGS;

#ifdef	SDEV_NODISC
	if (quirks & SDEV_NODISC)
		flags &= ~SCSI_LOW_DISK_DISC;
#endif	/* SDEV_NODISC */
#ifdef	SDEV_NOPARITY
	if (quirks & SDEV_NOPARITY)
		flags &= ~SCSI_LOW_DISK_PARITY;
#endif	/* SDEV_NOPARITY */
#ifdef	SDEV_NOCMDLNK
	if (quirks & SDEV_NOCMDLNK)
		flags &= ~SCSI_LOW_DISK_LINK;
#endif	/* SDEV_NOCMDLNK */
#ifdef	SDEV_NOTAG
	if (quirks & SDEV_NOTAG)
		flags &= ~SCSI_LOW_DISK_QTAG;
#endif	/* SDEV_NOTAG */
#ifdef	SDEV_NOSYNC
	if (quirks & SDEV_NOSYNC)
		flags &= ~SCSI_LOW_DISK_SYNC;
#endif	/* SDEV_NOSYNC */

	return flags;
}

static void
scsi_low_setup_quirks_xs(ti, li, flags)
	struct targ_info *ti;
	struct lun_info *li;
	u_int flags;
{
	u_int quirks;

	li->li_sloi.sloi_quirks = flags;
	quirks = scsi_low_translate_quirks_xs(flags);
	ti->ti_quirks = quirks & SCSI_LOW_DISK_TFLAGS;
	li->li_quirks = quirks & SCSI_LOW_DISK_LFLAGS;
	ti->ti_flags_valid |= SCSI_LOW_TARG_FLAGS_QUIRKS_VALID;
	li->li_flags_valid |= SCSI_LOW_LUN_FLAGS_QUIRKS_VALID;
	scsi_low_calcf_target(ti);
	scsi_low_calcf_lun(li);
	scsi_low_calcf_show(li);
}

#ifdef	SCSI_LOW_TARGET_OPEN
static int
scsi_low_target_open(link, cf)
	struct scsipi_link *link;
	struct cfdata *cf;
{
	u_int target = link->scsipi_scsi.target;
	u_int lun = link->scsipi_scsi.lun;
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;

	slp = (struct scsi_low_softc *) link->adapter_softc;
	ti = slp->sl_ti[target];
	li = scsi_low_alloc_li(ti, lun, 0);
	if (li == NULL)
		return 0;

	li->li_cfgflags = cf->cf_flags;
	scsi_low_setup_quirks_xs(ti, li, (u_int) link->quirks);
	return 0;
}
#endif	/* SCSI_LOW_TARGET_OPEN */

#endif	/* SCSI_LOW_INTERFACE_XS */

#ifdef SCSI_LOW_INTERFACE_CAM
/**************************************************************
 * SCSI INTERFACE (CAM)
 **************************************************************/
#define	SCSI_LOW_MALLOC(size)		malloc((size), M_SCSILOW, M_NOWAIT)
#define	SCSI_LOW_FREE(pt)		free((pt), M_SCSILOW)
#define	SCSI_LOW_ALLOC_CCB(flags)	scsi_low_get_ccb()

static void scsi_low_poll_cam(struct cam_sim *);
static void scsi_low_cam_rescan_callback(struct cam_periph *, union ccb *);
static void scsi_low_rescan_bus_cam(struct scsi_low_softc *);
void scsi_low_scsi_action_cam(struct cam_sim *, union ccb *);

static int scsi_low_attach_cam(struct scsi_low_softc *);
static int scsi_low_world_start_cam(struct scsi_low_softc *);
static int scsi_low_dettach_cam(struct scsi_low_softc *);
static int scsi_low_ccb_setup_cam(struct scsi_low_softc *, struct slccb *);
static int scsi_low_done_cam(struct scsi_low_softc *, struct slccb *);
static void scsi_low_timeout_cam(struct scsi_low_softc *, int, int);

struct scsi_low_osdep_funcs scsi_low_osdep_funcs_cam = {
	scsi_low_attach_cam,
	scsi_low_world_start_cam,
	scsi_low_dettach_cam,
	scsi_low_ccb_setup_cam,
	scsi_low_done_cam,
	scsi_low_timeout_cam
};
	
struct scsi_low_error_code scsi_low_error_code_cam[] = {
	{0,			CAM_REQ_CMP},
	{SENSEIO, 		CAM_AUTOSNS_VALID | CAM_REQ_CMP_ERR},
	{SENSEERR,		CAM_AUTOSENSE_FAIL},
	{UACAERR,		CAM_SCSI_STATUS_ERROR},
	{BUSYERR | STATERR,	CAM_SCSI_STATUS_ERROR},
	{SELTIMEOUTIO,		CAM_SEL_TIMEOUT},	
	{TIMEOUTIO,		CAM_CMD_TIMEOUT},
	{PDMAERR,		CAM_DATA_RUN_ERR},
	{PARITYERR,		CAM_UNCOR_PARITY},
	{UBFERR,		CAM_UNEXP_BUSFREE},
	{ABORTIO,		CAM_REQ_ABORTED},
	{-1,			CAM_UNREC_HBA_ERROR}
};

#define	SIM2SLP(sim)	((struct scsi_low_softc *) cam_sim_softc((sim)))

/* XXX:
 * Please check a polling hz, currently we assume scsi_low_poll() is
 * called each 1 ms.
 */
#define	SCSI_LOW_CAM_POLL_HZ	1000	/* OK ? */

static void
scsi_low_poll_cam(sim)
	struct cam_sim *sim;
{
	struct scsi_low_softc *slp = SIM2SLP(sim);

	(*slp->sl_funcs->scsi_low_poll) (slp);

	if (slp->sl_si.si_poll_count ++ >= 
	    SCSI_LOW_CAM_POLL_HZ / SCSI_LOW_TIMEOUT_HZ)
	{
		slp->sl_si.si_poll_count = 0;
		scsi_low_timeout_check(slp);
	}
}

static void
scsi_low_cam_rescan_callback(periph, ccb)
	struct cam_periph *periph;
	union ccb *ccb;
{

	xpt_free_path(ccb->ccb_h.path);
	xpt_free_ccb(ccb);
}

static void
scsi_low_rescan_bus_cam(slp)
	struct scsi_low_softc *slp;
{
  	struct cam_path *path;
	union ccb *ccb; 
	cam_status status;

	status = xpt_create_path(&path, xpt_periph,
				 cam_sim_path(slp->sl_si.sim), -1, 0);
	if (status != CAM_REQ_CMP)
		return;

	ccb = xpt_alloc_ccb();
	bzero(ccb, sizeof(union ccb));
	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_BUS;
	ccb->ccb_h.cbfcnp = scsi_low_cam_rescan_callback;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);
}

void
scsi_low_scsi_action_cam(sim, ccb)
	struct cam_sim *sim;
	union ccb *ccb;
{
	struct scsi_low_softc *slp = SIM2SLP(sim);
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	u_int lun, flags, msg, target;
	int s, rv;

	target = (u_int) (ccb->ccb_h.target_id);
	lun = (u_int) ccb->ccb_h.target_lun;

#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_ACTION, target) != 0)
	{
		printf("%s: cam_action: func code 0x%x target: %d, lun: %d\n",
			slp->sl_xname, ccb->ccb_h.func_code, target, lun);
	}
#endif	/* SCSI_LOW_DEBUG */

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
#ifdef	SCSI_LOW_DIAGNOSTIC
		if (target == CAM_TARGET_WILDCARD || lun == CAM_LUN_WILDCARD)
		{
			printf("%s: invalid target/lun\n", slp->sl_xname);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */

		if (((cb = SCSI_LOW_ALLOC_CCB(1)) == NULL)) {
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(ccb);
			return;
		}

		ti = slp->sl_ti[target];
		cb->osdep = ccb;
		cb->bp = NULL;
		if ((ccb->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0)
			flags = CCB_AUTOSENSE | CCB_SCSIIO;
		else
			flags = CCB_SCSIIO;

		s = SCSI_LOW_SPLSCSI();
		li = scsi_low_alloc_li(ti, lun, 1);

		if (ti->ti_setup_msg != 0)
		{
			scsi_low_message_enqueue(slp, ti, li, CCB_AUTOSENSE);
		}

		scsi_low_enqueue(slp, ti, li, cb, flags, 0);

#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_ABORT_CHECK, target) != 0)
		{
			scsi_low_test_abort(slp, ti, li);
		}
#endif	/* SCSI_LOW_DEBUG */
		splx(s);
		break;

	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_ABORT:			/* Abort the specified CCB */
#ifdef	SCSI_LOW_DIAGNOSTIC
		if (target == CAM_TARGET_WILDCARD || lun == CAM_LUN_WILDCARD)
		{
			printf("%s: invalid target/lun\n", slp->sl_xname);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */

		s = SCSI_LOW_SPLSCSI();
		cb = scsi_low_find_ccb(slp, target, lun, ccb->cab.abort_ccb);
		rv = scsi_low_abort_ccb(slp, cb);
		splx(s);

		if (rv == 0)
			ccb->ccb_h.status = CAM_REQ_CMP;
		else
			ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_SET_TRAN_SETTINGS: {
		struct ccb_trans_settings_scsi *scsi;
        	struct ccb_trans_settings_spi *spi;
		struct ccb_trans_settings *cts;
		u_int val;

#ifdef	SCSI_LOW_DIAGNOSTIC
		if (target == CAM_TARGET_WILDCARD)
		{
			printf("%s: invalid target\n", slp->sl_xname);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */
		cts = &ccb->cts;
		ti = slp->sl_ti[target];
		if (lun == CAM_LUN_WILDCARD)
			lun = 0;

		s = SCSI_LOW_SPLSCSI();
		scsi = &cts->proto_specific.scsi;
		spi = &cts->xport_specific.spi;
		if ((spi->valid & (CTS_SPI_VALID_BUS_WIDTH |
				   CTS_SPI_VALID_SYNC_RATE |
				   CTS_SPI_VALID_SYNC_OFFSET)) != 0)
		{
			if (spi->valid & CTS_SPI_VALID_BUS_WIDTH) {
				val = spi->bus_width;
				if (val < ti->ti_width)
					ti->ti_width = val;
			}
			if (spi->valid & CTS_SPI_VALID_SYNC_RATE) {
				val = spi->sync_period;
				if (val == 0 || val > ti->ti_maxsynch.period)
					ti->ti_maxsynch.period = val;
			}
			if (spi->valid & CTS_SPI_VALID_SYNC_OFFSET) {
				val = spi->sync_offset;
				if (val < ti->ti_maxsynch.offset)
					ti->ti_maxsynch.offset = val;
			}
			ti->ti_flags_valid |= SCSI_LOW_TARG_FLAGS_QUIRKS_VALID;
			scsi_low_calcf_target(ti);
		}

		if ((spi->valid & CTS_SPI_FLAGS_DISC_ENB) != 0 ||
                    (scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0) {

			li = scsi_low_alloc_li(ti, lun, 1);
			if (spi->valid & CTS_SPI_FLAGS_DISC_ENB) {
				li->li_quirks |= SCSI_LOW_DISK_DISC;
			} else {
				li->li_quirks &= ~SCSI_LOW_DISK_DISC;
			}

			if (scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) {
				li->li_quirks |= SCSI_LOW_DISK_QTAG;
			} else {
				li->li_quirks &= ~SCSI_LOW_DISK_QTAG;
			}
			li->li_flags_valid |= SCSI_LOW_LUN_FLAGS_QUIRKS_VALID;
			scsi_low_calcf_target(ti);
			scsi_low_calcf_lun(li);
			if ((slp->sl_show_result & SHOW_CALCF_RES) != 0)
				scsi_low_calcf_show(li);
		}
		splx(s);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	case XPT_GET_TRAN_SETTINGS: {
		struct ccb_trans_settings *cts;
		u_int diskflags;

		cts = &ccb->cts;
#ifdef	SCSI_LOW_DIAGNOSTIC
		if (target == CAM_TARGET_WILDCARD)
		{
			printf("%s: invalid target\n", slp->sl_xname);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */
		ti = slp->sl_ti[target];
		if (lun == CAM_LUN_WILDCARD)
			lun = 0;

		s = SCSI_LOW_SPLSCSI();
		li = scsi_low_alloc_li(ti, lun, 1);
		if (li != NULL && cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			struct ccb_trans_settings_scsi *scsi =
				&cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi =
				&cts->xport_specific.spi;
#ifdef	SCSI_LOW_DIAGNOSTIC
			if (li->li_flags_valid != SCSI_LOW_LUN_FLAGS_ALL_VALID)
			{
				ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
				printf("%s: invalid GET_TRANS_CURRENT_SETTINGS call\n",
					slp->sl_xname);
				goto settings_out;
			}
#endif	/* SCSI_LOW_DIAGNOSTIC */
			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_SPI;
			cts->transport_version = 2;

			scsi->flags &= ~CTS_SCSI_FLAGS_TAG_ENB;
			spi->flags &= ~CTS_SPI_FLAGS_DISC_ENB;

			diskflags = li->li_diskflags & li->li_cfgflags;
			if (diskflags & SCSI_LOW_DISK_DISC)
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
			if (diskflags & SCSI_LOW_DISK_QTAG)
				scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;

			spi->sync_period = ti->ti_maxsynch.period;
			spi->valid |= CTS_SPI_VALID_SYNC_RATE;
			spi->sync_offset = ti->ti_maxsynch.offset;
			spi->valid |= CTS_SPI_VALID_SYNC_OFFSET;

			spi->valid |= CTS_SPI_VALID_BUS_WIDTH;
			spi->bus_width = ti->ti_width;

			if (cts->ccb_h.target_lun != CAM_LUN_WILDCARD) {
				scsi->valid = CTS_SCSI_VALID_TQ;
				spi->valid |= CTS_SPI_VALID_DISC;
			} else
				scsi->valid = 0;
		} else
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
settings_out:
		splx(s);
		xpt_done(ccb);
		break;
	}

	case XPT_CALC_GEOMETRY: { /* not yet HN2 */
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		xpt_done(ccb);
		break;
	}

	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
		s = SCSI_LOW_SPLSCSI();
		scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, NULL);
		splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:	/* Terminate the I/O process */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
#ifdef	SCSI_LOW_DIAGNOSTIC
		if (target == CAM_TARGET_WILDCARD)
		{
			printf("%s: invalid target\n", slp->sl_xname);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */

		msg = SCSI_LOW_MSG_RESET;
		if (((cb = SCSI_LOW_ALLOC_CCB(1)) == NULL))
		{
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(ccb);
			return;
		}

		ti = slp->sl_ti[target];
		if (lun == CAM_LUN_WILDCARD)
			lun = 0;
		cb->osdep = ccb;
		cb->bp = NULL;
		if ((ccb->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0)
			flags = CCB_AUTOSENSE | CCB_NORETRY | CCB_URGENT;
		else
			flags = CCB_NORETRY | CCB_URGENT;

		s = SCSI_LOW_SPLSCSI();
		li = scsi_low_alloc_li(ti, lun, 1);
		scsi_low_enqueue(slp, ti, li, cb, flags, msg);
		splx(s);
		break;

	case XPT_PATH_INQ: {		/* Path routing inquiry */
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = scsi_low_version_major;
		cpi->hba_inquiry = PI_TAG_ABLE | PI_LINKED_CDB;
		ti = slp->sl_ti[slp->sl_hostid];	/* host id */
		if (ti->ti_width > SCSI_LOW_BUS_WIDTH_8)
			cpi->hba_inquiry |= PI_WIDE_16;
		if (ti->ti_width > SCSI_LOW_BUS_WIDTH_16)
			cpi->hba_inquiry |= PI_WIDE_32;
		if (ti->ti_maxsynch.offset > 0)
			cpi->hba_inquiry |= PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = slp->sl_ntargs - 1;
		cpi->max_lun = slp->sl_nluns - 1;
		cpi->initiator_id = slp->sl_hostid;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		cpi->transport = XPORT_SPI;
		cpi->transport_version = 2;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_2;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SCSI_LOW", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}

	default:
	        printf("scsi_low: non support func_code = %d ",
			ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static int
scsi_low_attach_cam(slp)
	struct scsi_low_softc *slp;
{
	struct cam_devq *devq;
	int tagged_openings;

	sprintf(slp->sl_xname, "%s%d",
		DEVPORT_DEVNAME(slp->sl_dev), DEVPORT_DEVUNIT(slp->sl_dev));

	devq = cam_simq_alloc(SCSI_LOW_NCCB);
	if (devq == NULL)
		return (ENOMEM);

	/*
	 * ask the adapter what subunits are present
	 */
	tagged_openings = min(slp->sl_openings, SCSI_LOW_MAXNEXUS);
	slp->sl_si.sim = cam_sim_alloc(scsi_low_scsi_action_cam,
				scsi_low_poll_cam,
				DEVPORT_DEVNAME(slp->sl_dev), slp,
				DEVPORT_DEVUNIT(slp->sl_dev), &Giant,
				slp->sl_openings, tagged_openings, devq);

	if (slp->sl_si.sim == NULL) {
		cam_simq_free(devq);
	 	return ENODEV;
	}

	if (xpt_bus_register(slp->sl_si.sim, NULL, 0) != CAM_SUCCESS) {
		free(slp->sl_si.sim, M_SCSILOW);
	 	return ENODEV;
	}
       
	if (xpt_create_path(&slp->sl_si.path, /*periph*/NULL,
			cam_sim_path(slp->sl_si.sim), CAM_TARGET_WILDCARD,
			CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(slp->sl_si.sim));
		cam_sim_free(slp->sl_si.sim, /*free_simq*/TRUE);
		return ENODEV;
	}

	slp->sl_show_result = SHOW_CALCF_RES;		/* OK ? */
	return 0;
}

static int
scsi_low_world_start_cam(slp)
	struct scsi_low_softc *slp;
{

	if (!cold)
		scsi_low_rescan_bus_cam(slp);
	return 0;
}

static int
scsi_low_dettach_cam(slp)
	struct scsi_low_softc *slp;
{

	xpt_async(AC_LOST_DEVICE, slp->sl_si.path, NULL);
	xpt_free_path(slp->sl_si.path);
	xpt_bus_deregister(cam_sim_path(slp->sl_si.sim));
	cam_sim_free(slp->sl_si.sim, /* free_devq */ TRUE);
	return 0;
}

static int
scsi_low_ccb_setup_cam(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
        union ccb *ccb = (union ccb *) cb->osdep;

	if ((cb->ccb_flags & CCB_SCSIIO) != 0)
	{
		cb->ccb_scp.scp_cmd = ccb->csio.cdb_io.cdb_bytes;
		cb->ccb_scp.scp_cmdlen = (int) ccb->csio.cdb_len;
		cb->ccb_scp.scp_data = ccb->csio.data_ptr;
		cb->ccb_scp.scp_datalen = (int) ccb->csio.dxfer_len;
		if((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			cb->ccb_scp.scp_direction = SCSI_LOW_WRITE;
		else /* if((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) */
			cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = ccb->ccb_h.timeout / 1000;
	}
	else
	{
		scsi_low_unit_ready_cmd(cb);
	}
	return SCSI_LOW_START_QTAG;
}

static int
scsi_low_done_cam(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	union ccb *ccb;

	ccb = (union ccb *) cb->osdep;
	if (cb->ccb_error == 0)
	{
		ccb->ccb_h.status = CAM_REQ_CMP;
		ccb->csio.resid = 0;
	}
	else 	
	{
	        if (cb->ccb_rcnt >= slp->sl_max_retry)
			cb->ccb_error |= ABORTIO;

		if ((cb->ccb_flags & CCB_NORETRY) == 0 &&
		    (cb->ccb_error & ABORTIO) == 0)
			return EJUSTRETURN;

		if ((cb->ccb_error & SENSEIO) != 0)
		{
			memcpy(&ccb->csio.sense_data,
			       &cb->ccb_sense,
			       sizeof(ccb->csio.sense_data));
		}

		ccb->ccb_h.status = scsi_low_translate_error_code(cb,
					&scsi_low_error_code_cam[0]);
	
#ifdef	SCSI_LOW_DIAGNOSTIC
		if ((cb->ccb_flags & CCB_SILENT) == 0 &&
		    cb->ccb_scp.scp_cmdlen > 0 &&
		    (scsi_low_cmd_flags[cb->ccb_scp.scp_cmd[0]] &
		     SCSI_LOW_CMD_ABORT_WARNING) != 0)
		{
			printf("%s: WARNING: scsi_low IO abort\n",
				slp->sl_xname);
			scsi_low_print(slp, NULL);
		}
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == 0)
		ccb->ccb_h.status |= CAM_REQ_CMP_ERR;

	if (cb->ccb_scp.scp_status == ST_UNKNOWN)
		ccb->csio.scsi_status = 0;	/* XXX */
	else
		ccb->csio.scsi_status = cb->ccb_scp.scp_status;

	if ((cb->ccb_flags & CCB_NOSDONE) == 0)
		xpt_done(ccb);
	return 0;
}

static void
scsi_low_timeout_cam(slp, ch, action)
	struct scsi_low_softc *slp;
	int ch;
	int action;
{

	switch (ch)
	{
	case SCSI_LOW_TIMEOUT_CH_IO:
		switch (action)
		{
		case SCSI_LOW_TIMEOUT_START:
			slp->sl_si.timeout_ch = timeout(scsi_low_timeout, slp,
				hz / SCSI_LOW_TIMEOUT_HZ);
			break;
		case SCSI_LOW_TIMEOUT_STOP:
			untimeout(scsi_low_timeout, slp, slp->sl_si.timeout_ch);
			break;
		}
		break;

	case SCSI_LOW_TIMEOUT_CH_ENGAGE:
		switch (action)
		{
		case SCSI_LOW_TIMEOUT_START:
			slp->sl_si.engage_ch = timeout(scsi_low_engage, slp, 1);
			break;
		case SCSI_LOW_TIMEOUT_STOP:
			untimeout(scsi_low_engage, slp, slp->sl_si.engage_ch);
			break;
		}
		break;
	case SCSI_LOW_TIMEOUT_CH_RECOVER:
		break;
	}
}

#endif	/* SCSI_LOW_INTERFACE_CAM */

/*=============================================================
 * END OF OS switch  (All OS depend fucntions should be above)
 =============================================================*/

/**************************************************************
 * scsi low deactivate and activate
 **************************************************************/
int
scsi_low_is_busy(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_nio > 0)
		return EBUSY;
	return 0;
}

int
scsi_low_deactivate(slp)
	struct scsi_low_softc *slp;
{
	int s;

	s = SCSI_LOW_SPLSCSI();
	slp->sl_flags |= HW_INACTIVE;
	(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
		(slp, SCSI_LOW_TIMEOUT_CH_IO, SCSI_LOW_TIMEOUT_STOP);
	(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
		(slp, SCSI_LOW_TIMEOUT_CH_ENGAGE, SCSI_LOW_TIMEOUT_STOP);
	splx(s);
	return 0;
}

int
scsi_low_activate(slp)
	struct scsi_low_softc *slp;
{
	int error, s;

	s = SCSI_LOW_SPLSCSI();
	slp->sl_flags &= ~HW_INACTIVE;
	if ((error = scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, NULL)) != 0)
	{
		slp->sl_flags |= HW_INACTIVE;
		splx(s);
		return error;
	}

	slp->sl_timeout_count = 0;
	(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
		(slp, SCSI_LOW_TIMEOUT_CH_IO, SCSI_LOW_TIMEOUT_START);
	splx(s);
	return 0;
}

/**************************************************************
 * scsi low log
 **************************************************************/
#ifdef	SCSI_LOW_DIAGNOSTIC
static void scsi_low_msg_log_init(struct scsi_low_msg_log *);
static void scsi_low_msg_log_write(struct scsi_low_msg_log *, u_int8_t *, int);
static void scsi_low_msg_log_show(struct scsi_low_msg_log *, char *, int);

static void
scsi_low_msg_log_init(slmlp)
	struct scsi_low_msg_log *slmlp;
{

	slmlp->slml_ptr = 0;
}

static void
scsi_low_msg_log_write(slmlp, datap, len)
	struct scsi_low_msg_log *slmlp;
	u_int8_t *datap;
	int len;
{
	int ptr, ind;

	if (slmlp->slml_ptr >= SCSI_LOW_MSG_LOG_DATALEN)
		return;

	ptr = slmlp->slml_ptr ++;
	for (ind = 0; ind < sizeof(slmlp->slml_msg[0]) && ind < len; ind ++)
		slmlp->slml_msg[ptr].msg[ind] = datap[ind];
	for ( ; ind < sizeof(slmlp->slml_msg[0]); ind ++)
		slmlp->slml_msg[ptr].msg[ind] = 0;
}
	
static void
scsi_low_msg_log_show(slmlp, s, len)
	struct scsi_low_msg_log *slmlp;
	char *s;
	int len;
{
	int ptr, ind;

	printf("%s: (%d) ", s, slmlp->slml_ptr);
	for (ptr = 0; ptr < slmlp->slml_ptr; ptr ++)
	{
		for (ind = 0; ind < len && ind < sizeof(slmlp->slml_msg[0]);
		     ind ++)
		{
			printf("[%x]", (u_int) slmlp->slml_msg[ptr].msg[ind]);
		}
		printf(">");
	}
	printf("\n");
}
#endif	/* SCSI_LOW_DIAGNOSTIC */

/**************************************************************
 * power control
 **************************************************************/
static void
scsi_low_engage(arg)
	void *arg;
{
	struct scsi_low_softc *slp = arg;
	int s = SCSI_LOW_SPLSCSI();

	switch (slp->sl_rstep)
	{
	case 0:
		slp->sl_rstep ++;
		(*slp->sl_funcs->scsi_low_power) (slp, SCSI_LOW_ENGAGE);
		(*slp->sl_osdep_fp->scsi_low_osdep_timeout) (slp, 
			SCSI_LOW_TIMEOUT_CH_ENGAGE, SCSI_LOW_TIMEOUT_START);
		break;

	case 1:
		slp->sl_rstep ++;
		slp->sl_flags &= ~HW_RESUME;
		scsi_low_start(slp);
		break;

	case 2:
		break;
	}
	splx(s);
}

static int
scsi_low_init(slp, flags)
	struct scsi_low_softc *slp;
	u_int flags;
{
	int rv = 0;

	slp->sl_flags |= HW_INITIALIZING;

	/* clear power control timeout */
	if ((slp->sl_flags & HW_POWERCTRL) != 0)
	{
		(*slp->sl_osdep_fp->scsi_low_osdep_timeout) (slp, 
			SCSI_LOW_TIMEOUT_CH_ENGAGE, SCSI_LOW_TIMEOUT_STOP);
		slp->sl_flags &= ~(HW_POWDOWN | HW_RESUME);
		slp->sl_active = 1;
		slp->sl_powc = SCSI_LOW_POWDOWN_TC;
	}

	/* reset current nexus */
	scsi_low_reset_nexus(slp, flags);
	if ((slp->sl_flags & HW_INACTIVE) != 0)
	{
		rv = EBUSY;
		goto out;
	}

	if (flags != SCSI_LOW_RESTART_SOFT)
	{
		rv = ((*slp->sl_funcs->scsi_low_init) (slp, flags));
	}

out:
	slp->sl_flags &= ~HW_INITIALIZING;
	return rv;
}

/**************************************************************
 * allocate lun_info
 **************************************************************/
static struct lun_info *
scsi_low_alloc_li(ti, lun, alloc)
	struct targ_info *ti;
	int lun;
	int alloc;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct lun_info *li;

	li = LIST_FIRST(&ti->ti_litab); 
	if (li != NULL)
	{ 
		if (li->li_lun == lun)
			return li;

		while ((li = LIST_NEXT(li, lun_chain)) != NULL)
		{
			if (li->li_lun == lun)
			{
				LIST_REMOVE(li, lun_chain);
				LIST_INSERT_HEAD(&ti->ti_litab, li, lun_chain);
				return li;
			}
		}
	}

	if (alloc == 0)
		return li;

	li = SCSI_LOW_MALLOC(ti->ti_lunsize);
	if (li == NULL)
		panic("no lun info mem");

	SCSI_LOW_BZERO(li, ti->ti_lunsize);
	li->li_lun = lun;
	li->li_ti = ti;

	li->li_cfgflags = SCSI_LOW_SYNC | SCSI_LOW_LINK | SCSI_LOW_DISC |
			  SCSI_LOW_QTAG;
	li->li_quirks = li->li_diskflags = SCSI_LOW_DISK_LFLAGS;
	li->li_flags_valid = SCSI_LOW_LUN_FLAGS_USER_VALID;
#ifdef	SCSI_LOW_FLAGS_QUIRKS_OK
	li->li_flags_valid |= SCSI_LOW_LUN_FLAGS_QUIRKS_VALID;
#endif	/* SCSI_LOW_FLAGS_QUIRKS_OK */

	li->li_qtagbits = (u_int) -1;

	TAILQ_INIT(&li->li_discq);
	LIST_INSERT_HEAD(&ti->ti_litab, li, lun_chain);

	/* host specific structure initialization per lun */
	if (slp->sl_funcs->scsi_low_lun_init != NULL)
		(*slp->sl_funcs->scsi_low_lun_init)
			(slp, ti, li, SCSI_LOW_INFO_ALLOC);
	scsi_low_calcf_lun(li);
	return li;
}

/**************************************************************
 * allocate targ_info
 **************************************************************/
static struct targ_info *
scsi_low_alloc_ti(slp, targ)
	struct scsi_low_softc *slp;
	int targ;
{
	struct targ_info *ti;

	if (TAILQ_FIRST(&slp->sl_titab) == NULL)
		TAILQ_INIT(&slp->sl_titab);

	ti = SCSI_LOW_MALLOC(slp->sl_targsize);
	if (ti == NULL)
		panic("%s short of memory", slp->sl_xname);

	SCSI_LOW_BZERO(ti, slp->sl_targsize);
	ti->ti_id = targ;
	ti->ti_sc = slp;

	slp->sl_ti[targ] = ti;
	TAILQ_INSERT_TAIL(&slp->sl_titab, ti, ti_chain);
	LIST_INIT(&ti->ti_litab);

	ti->ti_quirks = ti->ti_diskflags = SCSI_LOW_DISK_TFLAGS;
	ti->ti_owidth = SCSI_LOW_BUS_WIDTH_8;
	ti->ti_flags_valid = SCSI_LOW_TARG_FLAGS_USER_VALID;
#ifdef	SCSI_LOW_FLAGS_QUIRKS_OK
	ti->ti_flags_valid |= SCSI_LOW_TARG_FLAGS_QUIRKS_VALID;
#endif	/* SCSI_LOW_FLAGS_QUIRKS_OK */

	if (slp->sl_funcs->scsi_low_targ_init != NULL)
	{
		(*slp->sl_funcs->scsi_low_targ_init)
			(slp, ti, SCSI_LOW_INFO_ALLOC);
	}
	scsi_low_calcf_target(ti);
	return ti;
}

static void
scsi_low_free_ti(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti, *tib;
	struct lun_info *li, *nli;

	for (ti = TAILQ_FIRST(&slp->sl_titab); ti; ti = tib)
	{
		for (li = LIST_FIRST(&ti->ti_litab); li != NULL; li = nli)
		{
			if (slp->sl_funcs->scsi_low_lun_init != NULL)
			{
				(*slp->sl_funcs->scsi_low_lun_init)
					(slp, ti, li, SCSI_LOW_INFO_DEALLOC);
			}
			nli = LIST_NEXT(li, lun_chain);
			SCSI_LOW_FREE(li);
		}

		if (slp->sl_funcs->scsi_low_targ_init != NULL)
		{
			(*slp->sl_funcs->scsi_low_targ_init)
				(slp, ti, SCSI_LOW_INFO_DEALLOC);
		}
		tib = TAILQ_NEXT(ti, ti_chain);
		SCSI_LOW_FREE(ti);
	}
}

/**************************************************************
 * timeout
 **************************************************************/
void
scsi_low_bus_idle(slp)
	struct scsi_low_softc *slp;
{

	slp->sl_retry_sel = 0;
	if (slp->sl_Tnexus == NULL)
		scsi_low_start(slp);
}

static void
scsi_low_timeout(arg)
	void *arg;
{
	struct scsi_low_softc *slp = arg;
	int s;

	s = SCSI_LOW_SPLSCSI();
	(void) scsi_low_timeout_check(slp);
	(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
		(slp, SCSI_LOW_TIMEOUT_CH_IO, SCSI_LOW_TIMEOUT_START);
	splx(s);
}

static int
scsi_low_timeout_check(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb = NULL;		/* XXX */

	/* selection restart */
	if (slp->sl_retry_sel != 0)
	{
		slp->sl_retry_sel = 0;
		if (slp->sl_Tnexus != NULL)
			goto step1;

		cb = TAILQ_FIRST(&slp->sl_start);
		if (cb == NULL)
			goto step1;

		if (cb->ccb_selrcnt >= SCSI_LOW_MAX_SELECTION_RETRY)
		{
			cb->ccb_flags |= CCB_NORETRY;
			cb->ccb_error |= SELTIMEOUTIO;
			if (scsi_low_revoke_ccb(slp, cb, 1) != NULL)
				panic("%s: ccb not finished", slp->sl_xname);
		}

		if (slp->sl_Tnexus == NULL)
			scsi_low_start(slp);
	}

	/* call hardware timeout */
step1:
	if (slp->sl_funcs->scsi_low_timeout != NULL)
	{
		(*slp->sl_funcs->scsi_low_timeout) (slp);
	}
	
	if (slp->sl_timeout_count ++ < 
	    SCSI_LOW_TIMEOUT_CHECK_INTERVAL * SCSI_LOW_TIMEOUT_HZ)
		return 0;

	slp->sl_timeout_count = 0;
	if (slp->sl_nio > 0)
	{
		if ((cb = slp->sl_Qnexus) != NULL)
		{
			cb->ccb_tc -= SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
			if (cb->ccb_tc < 0)
				goto bus_reset;
		}
		else if (slp->sl_disc == 0)
		{
		        if ((cb = TAILQ_FIRST(&slp->sl_start)) == NULL)
				return 0;

			cb->ccb_tc -= SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
			if (cb->ccb_tc < 0)
				goto bus_reset;
		}
		else for (ti = TAILQ_FIRST(&slp->sl_titab); ti != NULL;
		          ti = TAILQ_NEXT(ti, ti_chain))
		{
			if (ti->ti_disc == 0)
				continue;

			for (li = LIST_FIRST(&ti->ti_litab); li != NULL;
			     li = LIST_NEXT(li, lun_chain))
			{
				for (cb = TAILQ_FIRST(&li->li_discq); 
				     cb != NULL;
				     cb = TAILQ_NEXT(cb, ccb_chain))
				{
					cb->ccb_tc -=
						SCSI_LOW_TIMEOUT_CHECK_INTERVAL;
					if (cb->ccb_tc < 0)
						goto bus_reset;
				}
			}
		}

	}
	else if ((slp->sl_flags & HW_POWERCTRL) != 0)
	{
		if ((slp->sl_flags & (HW_POWDOWN | HW_RESUME)) != 0)
			return 0;

		if (slp->sl_active != 0)
		{
			slp->sl_powc = SCSI_LOW_POWDOWN_TC;
			slp->sl_active = 0;
			return 0;
		}

		slp->sl_powc --;
		if (slp->sl_powc < 0)
		{
			slp->sl_powc = SCSI_LOW_POWDOWN_TC;
			slp->sl_flags |= HW_POWDOWN;
			(*slp->sl_funcs->scsi_low_power)
					(slp, SCSI_LOW_POWDOWN);
		}
	}
	return 0;

bus_reset:
	cb->ccb_error |= TIMEOUTIO;
	printf("%s: slccb (0x%lx) timeout!\n", slp->sl_xname, (u_long) cb);
	scsi_low_info(slp, NULL, "scsi bus hangup. try to recover.");
	scsi_low_init(slp, SCSI_LOW_RESTART_HARD);
	scsi_low_start(slp);
	return ERESTART;
}


static int
scsi_low_abort_ccb(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct targ_info *ti;
	struct lun_info *li;
	u_int msg;

	if (cb == NULL)
		return EINVAL;
	if ((cb->ccb_omsgoutflag & 
	     (SCSI_LOW_MSG_ABORT | SCSI_LOW_MSG_ABORT_QTAG)) != 0)
		return EBUSY;

	ti = cb->ti;
	li = cb->li;
	if (cb->ccb_tag == SCSI_LOW_UNKTAG)
		msg = SCSI_LOW_MSG_ABORT;
	else
		msg = SCSI_LOW_MSG_ABORT_QTAG;

	cb->ccb_error |= ABORTIO;
	cb->ccb_flags |= CCB_NORETRY;
	scsi_low_ccb_message_assert(cb, msg);

	if (cb == slp->sl_Qnexus)
	{
		scsi_low_assert_msg(slp, ti, msg, 1);
	}
	else if ((cb->ccb_flags & CCB_DISCQ) != 0)
	{
		if (scsi_low_revoke_ccb(slp, cb, 0) == NULL)
			panic("%s: revoked ccb done", slp->sl_xname);

		cb->ccb_flags |= CCB_STARTQ;
		TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);

		if (slp->sl_Tnexus == NULL)
			scsi_low_start(slp);
	}
	else
	{
		if (scsi_low_revoke_ccb(slp, cb, 1) != NULL)
			panic("%s: revoked ccb retried", slp->sl_xname);
	}
	return 0;
}

/**************************************************************
 * Generic SCSI INTERFACE
 **************************************************************/
int
scsi_low_attach(slp, openings, ntargs, nluns, targsize, lunsize)
	struct scsi_low_softc *slp;
	int openings, ntargs, nluns, targsize, lunsize;
{
	struct targ_info *ti;
	struct lun_info *li;
	int s, i, nccb, rv;

#ifdef	SCSI_LOW_INTERFACE_XS
	slp->sl_osdep_fp = &scsi_low_osdep_funcs_xs;
#endif	/* SCSI_LOW_INTERFACE_XS */
#ifdef	SCSI_LOW_INTERFACE_CAM
	slp->sl_osdep_fp = &scsi_low_osdep_funcs_cam;
#endif	/* SCSI_LOW_INTERFACE_CAM */

	if (slp->sl_osdep_fp == NULL)
		panic("scsi_low: interface not spcified");

	if (ntargs > SCSI_LOW_NTARGETS)
	{
		printf("scsi_low: %d targets are too large\n", ntargs);
		printf("change kernel options SCSI_LOW_NTARGETS");
		return EINVAL;
	}

	if (openings <= 0)
		slp->sl_openings = (SCSI_LOW_NCCB / ntargs);
	else
		slp->sl_openings = openings;
	slp->sl_ntargs = ntargs;
	slp->sl_nluns = nluns;
	slp->sl_max_retry = SCSI_LOW_MAX_RETRY;

	if (lunsize < sizeof(struct lun_info))
		lunsize = sizeof(struct lun_info);

	if (targsize < sizeof(struct targ_info))
		targsize = sizeof(struct targ_info);

	slp->sl_targsize = targsize;
	for (i = 0; i < ntargs; i ++)
	{
		ti = scsi_low_alloc_ti(slp, i);
		ti->ti_lunsize = lunsize;
		li = scsi_low_alloc_li(ti, 0, 1);
	}

	/* initialize queue */
	nccb = openings * ntargs;
	if (nccb >= SCSI_LOW_NCCB || nccb <= 0)
		nccb = SCSI_LOW_NCCB;
	scsi_low_init_ccbque(nccb);
	TAILQ_INIT(&slp->sl_start);

	/* call os depend attach */
	s = SCSI_LOW_SPLSCSI();
	rv = (*slp->sl_osdep_fp->scsi_low_osdep_attach) (slp);
	if (rv != 0)
	{
		splx(s);
		printf("%s: scsi_low_attach: osdep attach failed\n",
			slp->sl_xname);
		return EINVAL;
	}

	/* check hardware */
	SCSI_LOW_DELAY(1000);	/* wait for 1ms */
	if (scsi_low_init(slp, SCSI_LOW_RESTART_HARD) != 0)
	{
		splx(s);
		printf("%s: scsi_low_attach: initialization failed\n",
			slp->sl_xname);
		return EINVAL;
	}

	/* start watch dog */
	slp->sl_timeout_count = 0;
	(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
		 (slp, SCSI_LOW_TIMEOUT_CH_IO, SCSI_LOW_TIMEOUT_START);
	LIST_INSERT_HEAD(&sl_tab, slp, sl_chain);

	/* fake call */
	scsi_low_abort_ccb(slp, scsi_low_find_ccb(slp, 0, 0, NULL));

#ifdef	SCSI_LOW_START_UP_CHECK
	/* probing devices */
	scsi_low_start_up(slp);
#endif	/* SCSI_LOW_START_UP_CHECK */

	/* call os depend attach done*/
	(*slp->sl_osdep_fp->scsi_low_osdep_world_start) (slp);
	splx(s);
	return 0;
}

int
scsi_low_dettach(slp)
	struct scsi_low_softc *slp;
{
	int s, rv;

	s = SCSI_LOW_SPLSCSI();
	if (scsi_low_is_busy(slp) != 0)
	{
		splx(s);
		return EBUSY;
	}

	scsi_low_deactivate(slp);

	rv = (*slp->sl_osdep_fp->scsi_low_osdep_dettach) (slp);
	if (rv != 0)
	{
		splx(s);
		return EBUSY;
	}

	scsi_low_free_ti(slp);
	LIST_REMOVE(slp, sl_chain);
	splx(s);
	return 0;
}

/**************************************************************
 * Generic enqueue
 **************************************************************/
static int
scsi_low_enqueue(slp, ti, li, cb, flags, msg)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	u_int flags, msg;
{	

	cb->ti = ti;
	cb->li = li;

	scsi_low_ccb_message_assert(cb, msg);

	cb->ccb_otag = cb->ccb_tag = SCSI_LOW_UNKTAG;
	scsi_low_alloc_qtag(cb);

	cb->ccb_flags = flags | CCB_STARTQ;
	cb->ccb_tc = cb->ccb_tcmax = SCSI_LOW_MIN_TOUT;
	cb->ccb_error |= PENDINGIO;

	if ((flags & CCB_URGENT) != 0)
	{
		TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);
	}
	else
	{
		TAILQ_INSERT_TAIL(&slp->sl_start, cb, ccb_chain);
	}

	slp->sl_nio ++;

	if (slp->sl_Tnexus == NULL)
		scsi_low_start(slp);
	return 0;
}

static int
scsi_low_message_enqueue(slp, ti, li, flags)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;
	u_int flags;
{
	struct slccb *cb;
	u_int tmsgflags;

	tmsgflags = ti->ti_setup_msg;
	ti->ti_setup_msg = 0;

	flags |= CCB_NORETRY;
	if ((cb = SCSI_LOW_ALLOC_CCB(1)) == NULL)
		return ENOMEM;

	cb->osdep = NULL;
	cb->bp = NULL;
	scsi_low_enqueue(slp, ti, li, cb, flags, tmsgflags);
	return 0;
}

/**************************************************************
 * Generic Start & Done
 **************************************************************/
#define	SLSC_MODE_SENSE_SHORT   0x1a
static u_int8_t ss_cmd[6] = {START_STOP, 0, 0, 0, SSS_START, 0}; 
static u_int8_t sms_cmd[6] = {SLSC_MODE_SENSE_SHORT, 0x08, 0x0a, 0, 
			      sizeof(struct scsi_low_mode_sense_data), 0}; 
static u_int8_t inq_cmd[6] = {INQUIRY, 0, 0, 0, 
			      sizeof(struct scsi_low_inq_data), 0}; 
static u_int8_t unit_ready_cmd[6];
static int scsi_low_setup_start(struct scsi_low_softc *, struct targ_info *, struct lun_info *, struct slccb *);
static int scsi_low_sense_abort_start(struct scsi_low_softc *, struct targ_info *, struct lun_info *, struct slccb *);
static int scsi_low_resume(struct scsi_low_softc *);

static void
scsi_low_unit_ready_cmd(cb)
	struct slccb *cb;
{

	cb->ccb_scp.scp_cmd = unit_ready_cmd;
	cb->ccb_scp.scp_cmdlen = sizeof(unit_ready_cmd);
	cb->ccb_scp.scp_datalen = 0;
	cb->ccb_scp.scp_direction = SCSI_LOW_READ;
	cb->ccb_tcmax = 15;
}

static int
scsi_low_sense_abort_start(slp, ti, li, cb)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
{

	cb->ccb_scp.scp_cmdlen = 6;
	SCSI_LOW_BZERO(cb->ccb_scsi_cmd, cb->ccb_scp.scp_cmdlen);
	cb->ccb_scsi_cmd[0] = REQUEST_SENSE;
	cb->ccb_scsi_cmd[4] = sizeof(cb->ccb_sense);
	cb->ccb_scp.scp_cmd = cb->ccb_scsi_cmd;
	cb->ccb_scp.scp_data = (u_int8_t *) &cb->ccb_sense;
	cb->ccb_scp.scp_datalen = sizeof(cb->ccb_sense);
	cb->ccb_scp.scp_direction = SCSI_LOW_READ;
	cb->ccb_tcmax = 15;
	scsi_low_ccb_message_clear(cb);
	if ((cb->ccb_flags & CCB_CLEARQ) != 0)
	{
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
	}
	else
	{
		SCSI_LOW_BZERO(&cb->ccb_sense, sizeof(cb->ccb_sense));
#ifdef	SCSI_LOW_NEGOTIATE_BEFORE_SENSE
		scsi_low_assert_msg(slp, ti, ti->ti_setup_msg_done, 0);
#endif	/* SCSI_LOW_NEGOTIATE_BEFORE_SENSE */
	}

	return SCSI_LOW_START_NO_QTAG;
}

static int
scsi_low_setup_start(slp, ti, li, cb)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
{

	switch(li->li_state)
	{
	case SCSI_LOW_LUN_SLEEP:
		scsi_low_unit_ready_cmd(cb);
		break;

	case SCSI_LOW_LUN_START:
		cb->ccb_scp.scp_cmd = ss_cmd;
		cb->ccb_scp.scp_cmdlen = sizeof(ss_cmd);
		cb->ccb_scp.scp_datalen = 0;
		cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = 30;
		break;

	case SCSI_LOW_LUN_INQ:
		cb->ccb_scp.scp_cmd = inq_cmd;
		cb->ccb_scp.scp_cmdlen = sizeof(inq_cmd);
		cb->ccb_scp.scp_data = (u_int8_t *)&li->li_inq;
		cb->ccb_scp.scp_datalen = sizeof(li->li_inq);
		cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = 15;
		break;

	case SCSI_LOW_LUN_MODEQ:
		cb->ccb_scp.scp_cmd = sms_cmd;
		cb->ccb_scp.scp_cmdlen = sizeof(sms_cmd);
		cb->ccb_scp.scp_data = (u_int8_t *)&li->li_sms;
		cb->ccb_scp.scp_datalen = sizeof(li->li_sms);
		cb->ccb_scp.scp_direction = SCSI_LOW_READ;
		cb->ccb_tcmax = 15;
		return SCSI_LOW_START_QTAG;

	default:
		panic("%s: no setup phase", slp->sl_xname);
	}

	return SCSI_LOW_START_NO_QTAG;
}

static int
scsi_low_resume(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_flags & HW_RESUME)
		return EJUSTRETURN;
	slp->sl_flags &= ~HW_POWDOWN;
	if (slp->sl_funcs->scsi_low_power != NULL)
	{
		slp->sl_flags |= HW_RESUME;
		slp->sl_rstep = 0;
		(*slp->sl_funcs->scsi_low_power) (slp, SCSI_LOW_ENGAGE);
		(*slp->sl_osdep_fp->scsi_low_osdep_timeout)
					(slp, SCSI_LOW_TIMEOUT_CH_ENGAGE,
				         SCSI_LOW_TIMEOUT_START);
		return EJUSTRETURN;
	}
	return 0;
}

static void
scsi_low_start(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	int rv;

	/* check hardware exists or under initializations ? */
	if ((slp->sl_flags & (HW_INACTIVE | HW_INITIALIZING)) != 0)
		return;

	/* check hardware power up ? */
	if ((slp->sl_flags & HW_POWERCTRL) != 0)
	{
		slp->sl_active ++;
		if (slp->sl_flags & (HW_POWDOWN | HW_RESUME))
		{
			if (scsi_low_resume(slp) == EJUSTRETURN)
				return;
		}
	}

	/* setup nexus */
#ifdef	SCSI_LOW_DIAGNOSTIC
	if (slp->sl_Tnexus || slp->sl_Lnexus || slp->sl_Qnexus)
	{
		scsi_low_info(slp, NULL, "NEXUS INCOSISTENT");
		panic("%s: inconsistent", slp->sl_xname);
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	for (cb = TAILQ_FIRST(&slp->sl_start); cb != NULL;
	     cb = TAILQ_NEXT(cb, ccb_chain))
	{
		li = cb->li;

		if (li->li_disc == 0)
		{
			goto scsi_low_cmd_start;
		}
		else if (li->li_nqio > 0)
		{
			if (li->li_nqio < li->li_maxnqio ||
		            (cb->ccb_flags & (CCB_SENSE | CCB_CLEARQ)) != 0)
				goto scsi_low_cmd_start;
		}
	}
	return;

scsi_low_cmd_start:
	cb->ccb_flags &= ~CCB_STARTQ;
	TAILQ_REMOVE(&slp->sl_start, cb, ccb_chain);
	ti = cb->ti;

	/* clear all error flag bits (for restart) */
	cb->ccb_error = 0;
	cb->ccb_datalen = -1;
	cb->ccb_scp.scp_status = ST_UNKNOWN;

	/* setup nexus pointer */
	slp->sl_Qnexus = cb;
	slp->sl_Lnexus = li;
	slp->sl_Tnexus = ti;

	/* initialize msgsys */
	scsi_low_init_msgsys(slp, ti);

	/* exec cmd */
	if ((cb->ccb_flags & (CCB_SENSE | CCB_CLEARQ)) != 0)
	{
		/* CA state or forced abort */
		rv = scsi_low_sense_abort_start(slp, ti, li, cb);
	}
	else if (li->li_state >= SCSI_LOW_LUN_OK)
	{
		cb->ccb_flags &= ~CCB_INTERNAL;
		rv = (*slp->sl_osdep_fp->scsi_low_osdep_ccb_setup) (slp, cb);
		if (cb->ccb_msgoutflag != 0)
		{
			scsi_low_ccb_message_exec(slp, cb);
		}
	}
	else
	{
		cb->ccb_flags |= CCB_INTERNAL;
		rv = scsi_low_setup_start(slp, ti, li, cb);
	}

	/* allocate qtag */
#define	SCSI_LOW_QTAG_OK (SCSI_LOW_QTAG | SCSI_LOW_DISC)

	if (rv == SCSI_LOW_START_QTAG &&
	    (li->li_flags & SCSI_LOW_QTAG_OK) == SCSI_LOW_QTAG_OK &&
	    li->li_maxnqio > 0)
	{
		u_int qmsg;

		scsi_low_activate_qtag(cb);
		if ((scsi_low_cmd_flags[cb->ccb_scp.scp_cmd[0]] &
		     SCSI_LOW_CMD_ORDERED_QTAG) != 0)
			qmsg = SCSI_LOW_MSG_ORDERED_QTAG;
		else if ((cb->ccb_flags & CCB_URGENT) != 0)
			qmsg = SCSI_LOW_MSG_HEAD_QTAG;
		else
			qmsg = SCSI_LOW_MSG_SIMPLE_QTAG;
		scsi_low_assert_msg(slp, ti, qmsg, 0);
	}

	/* timeout */
	if (cb->ccb_tcmax < SCSI_LOW_MIN_TOUT)
		cb->ccb_tcmax = SCSI_LOW_MIN_TOUT;
	cb->ccb_tc = cb->ccb_tcmax;

	/* setup saved scsi data pointer */
	cb->ccb_sscp = cb->ccb_scp;

	/* setup current scsi pointer */ 
	slp->sl_scp = cb->ccb_sscp;
	slp->sl_error = cb->ccb_error;

	/* assert always an identify msg */
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_IDENTIFY, 0);

	/* debug section */
#ifdef	SCSI_LOW_DIAGNOSTIC
	scsi_low_msg_log_init(&ti->ti_log_msgin);
	scsi_low_msg_log_init(&ti->ti_log_msgout);
#endif	/* SCSI_LOW_DIAGNOSTIC */

	/* selection start */
	slp->sl_selid = cb;
	rv = ((*slp->sl_funcs->scsi_low_start_bus) (slp, cb));
	if (rv == SCSI_LOW_START_OK)
	{
#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_win ++;
#endif	/* SCSI_LOW_STATICS */
		return;
	}

	scsi_low_arbit_fail(slp, cb);
#ifdef	SCSI_LOW_STATICS
	scsi_low_statics.nexus_fail ++;
#endif	/* SCSI_LOW_STATICS */
}

void
scsi_low_arbit_fail(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct targ_info *ti = cb->ti;

	scsi_low_deactivate_qtag(cb);
	scsi_low_ccb_message_retry(cb);
	cb->ccb_flags |= CCB_STARTQ;
	TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);

	scsi_low_bus_release(slp, ti);

	cb->ccb_selrcnt ++;
	if (slp->sl_disc == 0)
	{
#ifdef	SCSI_LOW_DIAGNOSTIC
		printf("%s: try selection again\n", slp->sl_xname);
#endif	/* SCSI_LOW_DIAGNOSTIC */
		slp->sl_retry_sel = 1;
	}
}

static void
scsi_low_bus_release(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{

	if (ti->ti_disc > 0)
	{
		SCSI_LOW_SETUP_PHASE(ti, PH_DISC);
	}
	else
	{
		SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
	}

	/* clear all nexus pointer */
	slp->sl_Qnexus = NULL;
	slp->sl_Lnexus = NULL;
	slp->sl_Tnexus = NULL;

	/* clear selection assert */
	slp->sl_selid = NULL;

	/* clear nexus data */
	slp->sl_scp.scp_direction = SCSI_LOW_RWUNK;

	/* clear phase change counter */
	slp->sl_ph_count = 0;
}

static int
scsi_low_setup_done(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	struct targ_info *ti;
	struct lun_info *li;

	ti = cb->ti;
	li = cb->li;

	if (cb->ccb_rcnt >= slp->sl_max_retry)
	{
		cb->ccb_error |= ABORTIO;
		return SCSI_LOW_DONE_COMPLETE;
	}

	/* XXX: special huck for selection timeout */
	if (li->li_state == SCSI_LOW_LUN_SLEEP &&
	    (cb->ccb_error & SELTIMEOUTIO) != 0)
	{
		cb->ccb_error |= ABORTIO;
		return SCSI_LOW_DONE_COMPLETE;
	}

	switch(li->li_state)
	{
	case SCSI_LOW_LUN_INQ:
		if (cb->ccb_error != 0)
		{
			li->li_diskflags &= 
				~(SCSI_LOW_DISK_LINK | SCSI_LOW_DISK_QTAG);
			if (li->li_lun > 0)
				goto resume;
			ti->ti_diskflags &=
				~(SCSI_LOW_DISK_SYNC | SCSI_LOW_DISK_WIDE);
		}
		else if ((li->li_inq.sd_version & 7) >= 2 ||
		         (li->li_inq.sd_len >= 4))
		{
			if ((li->li_inq.sd_support & 0x2) == 0)
				li->li_diskflags &= ~SCSI_LOW_DISK_QTAG;
			if ((li->li_inq.sd_support & 0x8) == 0)
				li->li_diskflags &= ~SCSI_LOW_DISK_LINK;
			if (li->li_lun > 0)
				goto resume;
			if ((li->li_inq.sd_support & 0x10) == 0)
				ti->ti_diskflags &= ~SCSI_LOW_DISK_SYNC;
			if ((li->li_inq.sd_support & 0x20) == 0)
				ti->ti_diskflags &= ~SCSI_LOW_DISK_WIDE_16;
			if ((li->li_inq.sd_support & 0x40) == 0)
				ti->ti_diskflags &= ~SCSI_LOW_DISK_WIDE_32;
		}
		else
		{
			li->li_diskflags &= 
				~(SCSI_LOW_DISK_QTAG | SCSI_LOW_DISK_LINK);
			if (li->li_lun > 0)
				goto resume;
			ti->ti_diskflags &= ~SCSI_LOW_DISK_WIDE;
		}
		ti->ti_flags_valid |= SCSI_LOW_TARG_FLAGS_DISK_VALID;
resume:
		scsi_low_calcf_target(ti);
		scsi_low_calcf_lun(li);
		break;

	case SCSI_LOW_LUN_MODEQ:
		if (cb->ccb_error != 0)
		{
			if (cb->ccb_error & SENSEIO)
			{
#ifdef	SCSI_LOW_DEBUG
				if (scsi_low_debug & SCSI_LOW_DEBUG_SENSE)
				{
					printf("SENSE: [%x][%x][%x][%x][%x]\n",
					(u_int) cb->ccb_sense.error_code,
					(u_int) cb->ccb_sense.segment,
					(u_int) cb->ccb_sense.flags,
					(u_int) cb->ccb_sense.add_sense_code,
					(u_int) cb->ccb_sense.add_sense_code_qual);
				}
#endif	/* SCSI_LOW_DEBUG */
			}
			else
			{
				li->li_diskflags &= ~SCSI_LOW_DISK_QTAG;
			}
		}
		else if ((li->li_sms.sms_cmp.cmp_page & 0x3f) == 0x0a)
		{	
			if (li->li_sms.sms_cmp.cmp_qc & 0x02)
				li->li_qflags |= SCSI_LOW_QFLAG_CA_QCLEAR;
			else
				li->li_qflags &= ~SCSI_LOW_QFLAG_CA_QCLEAR;
			if ((li->li_sms.sms_cmp.cmp_qc & 0x01) != 0)
				li->li_diskflags &= ~SCSI_LOW_DISK_QTAG;
		}
		li->li_flags_valid |= SCSI_LOW_LUN_FLAGS_DISK_VALID;
		scsi_low_calcf_lun(li);
		break;

	default:
		break;
	}

	li->li_state ++;
	if (li->li_state == SCSI_LOW_LUN_OK)
	{
		scsi_low_calcf_target(ti);
		scsi_low_calcf_lun(li);
		if (li->li_flags_valid == SCSI_LOW_LUN_FLAGS_ALL_VALID &&
	            (slp->sl_show_result & SHOW_CALCF_RES) != 0)
		{
			scsi_low_calcf_show(li);
		}	
	}

	cb->ccb_rcnt --;
	return SCSI_LOW_DONE_RETRY;
}

static int
scsi_low_done(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	int rv;

	if (cb->ccb_error == 0)
	{
		if ((cb->ccb_flags & (CCB_SENSE | CCB_CLEARQ)) != 0)
		{
#ifdef	SCSI_LOW_QCLEAR_AFTER_CA
			/* XXX:
			 * SCSI-2 draft suggests 
			 * page 0x0a QErr bit determins if
			 * the target aborts or continues
			 * the queueing io's after CA state resolved.
			 * However many targets seem not to support
			 * the page 0x0a. Thus we should manually clear the
			 * queuing io's after CA state.
			 */
			if ((cb->ccb_flags & CCB_CLEARQ) == 0)
			{
				cb->ccb_rcnt --;
				cb->ccb_flags |= CCB_CLEARQ;
				goto retry;
			}
#endif	/* SCSI_LOW_QCLEAR_AFTER_CA */

			if ((cb->ccb_flags & CCB_SENSE) != 0)
				cb->ccb_error |= (SENSEIO | ABORTIO);
			cb->ccb_flags &= ~(CCB_SENSE | CCB_CLEARQ);
		}
		else switch (cb->ccb_sscp.scp_status)
		{
		case ST_GOOD:
		case ST_MET:
		case ST_INTERGOOD:
		case ST_INTERMET:
			if (cb->ccb_datalen == 0 ||
			    cb->ccb_scp.scp_datalen == 0)
				break;

			if (cb->ccb_scp.scp_cmdlen > 0 &&
			    (scsi_low_cmd_flags[cb->ccb_scp.scp_cmd[0]] &
			     SCSI_LOW_CMD_RESIDUAL_CHK) == 0)
				break;

			cb->ccb_error |= PDMAERR;
			break;

		case ST_BUSY:
		case ST_QUEFULL:
			cb->ccb_error |= (BUSYERR | STATERR);
			break;

		case ST_CONFLICT:
			cb->ccb_error |= (STATERR | ABORTIO);
			break;

		case ST_CHKCOND:
		case ST_CMDTERM:
			if (cb->ccb_flags & (CCB_AUTOSENSE | CCB_INTERNAL))
			{
				cb->ccb_rcnt --;
				cb->ccb_flags |= CCB_SENSE;
				goto retry;
			}
			cb->ccb_error |= (UACAERR | STATERR | ABORTIO);
			break;

		case ST_UNKNOWN:
		default:
			cb->ccb_error |= FATALIO;
			break;
		}
	}
	else
	{
		if (cb->ccb_flags & CCB_SENSE)
		{
			cb->ccb_error |= (SENSEERR | ABORTIO);
		}
		cb->ccb_flags &= ~(CCB_CLEARQ | CCB_SENSE);
	}

	/* internal ccb */
	if ((cb->ccb_flags & CCB_INTERNAL) != 0)
	{
		if (scsi_low_setup_done(slp, cb) == SCSI_LOW_DONE_RETRY)
			goto retry;
	}

	/* check a ccb msgout flag */
	if (cb->ccb_omsgoutflag != 0)
	{
#define	SCSI_LOW_MSG_ABORT_OK	(SCSI_LOW_MSG_ABORT | \
				 SCSI_LOW_MSG_ABORT_QTAG | \
				 SCSI_LOW_MSG_CLEAR_QTAG | \
				 SCSI_LOW_MSG_TERMIO)

		if ((cb->ccb_omsgoutflag & SCSI_LOW_MSG_ABORT_OK) != 0)
		{
			cb->ccb_error |= ABORTIO;
		}
	}

	/* call OS depend done */
	if (cb->osdep != NULL)
	{
		rv = (*slp->sl_osdep_fp->scsi_low_osdep_done) (slp, cb);
		if (rv == EJUSTRETURN)
			goto retry;
	}
	else if (cb->ccb_error != 0)
	{
	        if (cb->ccb_rcnt >= slp->sl_max_retry)
			cb->ccb_error |= ABORTIO;

		if ((cb->ccb_flags & CCB_NORETRY) == 0 &&
		    (cb->ccb_error & ABORTIO) == 0)
			goto retry;
	}

	/* free our target */
#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_DONE, cb->ti->ti_id) != 0)
	{
		printf(">> SCSI_LOW_DONE_COMPLETE ===============\n");
		scsi_low_print(slp, NULL);
	}
#endif	/* SCSI_LOW_DEBUG */

	scsi_low_deactivate_qtag(cb);
	scsi_low_dealloc_qtag(cb);
	scsi_low_free_ccb(cb);
	slp->sl_nio --;
	return SCSI_LOW_DONE_COMPLETE;

retry:
#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_DONE, cb->ti->ti_id) != 0)
	{
		printf("** SCSI_LOW_DONE_RETRY ===============\n");
		scsi_low_print(slp, NULL);
	}
#endif	/* SCSI_LOW_DEBUG */
		
	cb->ccb_rcnt ++;
	scsi_low_deactivate_qtag(cb);
	scsi_low_ccb_message_retry(cb);
	return SCSI_LOW_DONE_RETRY;
}

/**************************************************************
 * Reset
 **************************************************************/
static void
scsi_low_reset_nexus_target(slp, ti, fdone)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	int fdone;
{
	struct lun_info *li;

	for (li = LIST_FIRST(&ti->ti_litab); li != NULL;
	     li = LIST_NEXT(li, lun_chain))
	{
		scsi_low_reset_nexus_lun(slp, li, fdone);
		li->li_state = SCSI_LOW_LUN_SLEEP;
		li->li_maxnqio = 0;
	}

	ti->ti_disc = 0;
	ti->ti_setup_msg = 0;
	ti->ti_setup_msg_done = 0;

	ti->ti_osynch.offset = ti->ti_osynch.period = 0;
	ti->ti_owidth = SCSI_LOW_BUS_WIDTH_8;

	ti->ti_diskflags = SCSI_LOW_DISK_TFLAGS;
	ti->ti_flags_valid &= ~SCSI_LOW_TARG_FLAGS_DISK_VALID;

	if (slp->sl_funcs->scsi_low_targ_init != NULL)
	{
		((*slp->sl_funcs->scsi_low_targ_init)
			(slp, ti, SCSI_LOW_INFO_REVOKE));
	}
	scsi_low_calcf_target(ti);

	for (li = LIST_FIRST(&ti->ti_litab); li != NULL;
	     li = LIST_NEXT(li, lun_chain))
	{
		li->li_flags = 0;

		li->li_diskflags = SCSI_LOW_DISK_LFLAGS;
		li->li_flags_valid &= ~SCSI_LOW_LUN_FLAGS_DISK_VALID;

		if (slp->sl_funcs->scsi_low_lun_init != NULL)
		{
			((*slp->sl_funcs->scsi_low_lun_init)
				(slp, ti, li, SCSI_LOW_INFO_REVOKE));
		}
		scsi_low_calcf_lun(li);
	}
}

static void
scsi_low_reset_nexus(slp, fdone)
	struct scsi_low_softc *slp;
	int fdone;
{
	struct targ_info *ti;
	struct slccb *cb, *topcb;

	if ((cb = slp->sl_Qnexus) != NULL)
	{
		topcb = scsi_low_revoke_ccb(slp, cb, fdone);
	}
	else
	{
		topcb = NULL;
	}

	for (ti = TAILQ_FIRST(&slp->sl_titab); ti != NULL;
	     ti = TAILQ_NEXT(ti, ti_chain))
	{
		scsi_low_reset_nexus_target(slp, ti, fdone);
		scsi_low_bus_release(slp, ti);
		scsi_low_init_msgsys(slp, ti);
	}

	if (topcb != NULL)
	{
		topcb->ccb_flags |= CCB_STARTQ;
		TAILQ_INSERT_HEAD(&slp->sl_start, topcb, ccb_chain);
	}

	slp->sl_disc = 0;
	slp->sl_retry_sel = 0;
	slp->sl_flags &= ~HW_PDMASTART;
}

/* misc */
static int tw_pos;
static char tw_chars[] = "|/-\\";
#define	TWIDDLEWAIT		10000

static void
scsi_low_twiddle_wait(void)
{

	cnputc('\b');
	cnputc(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
	SCSI_LOW_DELAY(TWIDDLEWAIT);
}

void
scsi_low_bus_reset(slp)
	struct scsi_low_softc *slp;
{
	int i;

	(*slp->sl_funcs->scsi_low_bus_reset) (slp);

	printf("%s: try to reset scsi bus  ", slp->sl_xname);
	for (i = 0; i <= SCSI2_RESET_DELAY / TWIDDLEWAIT ; i++)
		scsi_low_twiddle_wait();
	cnputc('\b');
	printf("\n");
}

int
scsi_low_restart(slp, flags, s)
	struct scsi_low_softc *slp;
	int flags;
	u_char *s;
{
	int error;

	if (s != NULL)
		printf("%s: scsi bus restart. reason: %s\n", slp->sl_xname, s);

	if ((error = scsi_low_init(slp, flags)) != 0)
		return error;

	scsi_low_start(slp);
	return 0;
}

/**************************************************************
 * disconnect and reselect
 **************************************************************/
#define	MSGCMD_LUN(msg)	(msg & 0x07)

static struct slccb *
scsi_low_establish_ccb(ti, li, tag)
	struct targ_info *ti;
	struct lun_info *li;
	scsi_low_tag_t tag;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	struct slccb *cb;

	if (li == NULL)
		return NULL;

	cb = TAILQ_FIRST(&li->li_discq);
	for ( ; cb != NULL; cb = TAILQ_NEXT(cb, ccb_chain))
		if (cb->ccb_tag == tag)
			goto found;
	return cb;

	/* 
	 * establish our ccb nexus
	 */
found:
#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_NEXUS_CHECK, ti->ti_id) != 0)
	{
		printf("%s: nexus(0x%lx) abort check start\n",
			slp->sl_xname, (u_long) cb);
		cb->ccb_flags |= (CCB_NORETRY | CCB_SILENT);
		scsi_low_revoke_ccb(slp, cb, 1);
		return NULL;
	}

	if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_ATTEN_CHECK, ti->ti_id) != 0)
	{
		if (cb->ccb_omsgoutflag == 0)
			scsi_low_ccb_message_assert(cb, SCSI_LOW_MSG_NOOP);
	}
#endif	/* SCSI_LOW_DEBUG */

	TAILQ_REMOVE(&li->li_discq, cb, ccb_chain);
	cb->ccb_flags &= ~CCB_DISCQ;
	slp->sl_Qnexus = cb;

	slp->sl_scp = cb->ccb_sscp;
	slp->sl_error |= cb->ccb_error;

	slp->sl_disc --;
	ti->ti_disc --;
	li->li_disc --;

	/* inform "ccb nexus established" to the host driver */
	(*slp->sl_funcs->scsi_low_establish_ccb_nexus) (slp);

	/* check msg */
	if (cb->ccb_msgoutflag != 0)
	{
		scsi_low_ccb_message_exec(slp, cb);
	}

	return cb;
}

struct targ_info *
scsi_low_reselected(slp, targ)
	struct scsi_low_softc *slp;
	u_int targ;
{
	struct targ_info *ti;
	struct slccb *cb;
	u_char *s;

	/* 
	 * Check select vs reselected collision.
	 */

	if ((cb = slp->sl_selid) != NULL)
	{
		scsi_low_arbit_fail(slp, cb);
#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_conflict ++;
#endif	/* SCSI_LOW_STATICS */
	}

	/* 
	 * Check if no current active nexus.
	 */
	if (slp->sl_Tnexus != NULL)
	{
		s = "host busy";
		goto world_restart;
	}

	/* 
	 * Check a valid target id asserted ?
	 */
	if (targ >= slp->sl_ntargs || targ == slp->sl_hostid)
	{
		s = "scsi id illegal";
		goto world_restart;
	}

	/* 
	 * Check the target scsi status.
	 */
	ti = slp->sl_ti[targ];
	if (ti->ti_phase != PH_DISC && ti->ti_phase != PH_NULL)
	{
		s = "phase mismatch";
		goto world_restart;
	}

	/* 
	 * Setup init msgsys
	 */
	slp->sl_error = 0;
	scsi_low_init_msgsys(slp, ti);

	/* 
	 * Establish our target nexus
	 */
	SCSI_LOW_SETUP_PHASE(ti, PH_RESEL);
	slp->sl_Tnexus = ti;
#ifdef	SCSI_LOW_STATICS
	scsi_low_statics.nexus_reselected ++;
#endif	/* SCSI_LOW_STATICS */
	return ti;

world_restart:
	printf("%s: reselect(%x:unknown) %s\n", slp->sl_xname, targ, s);
	scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, 
		         "reselect: scsi world confused");
	return NULL;
}

/**************************************************************
 * cmd out pointer setup
 **************************************************************/
int
scsi_low_cmd(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct slccb *cb = slp->sl_Qnexus;
	
	slp->sl_ph_count ++;
	if (cb == NULL)
	{
		/*
		 * no ccb, abort!
		 */
		slp->sl_scp.scp_cmd = (u_int8_t *) &unit_ready_cmd;
		slp->sl_scp.scp_cmdlen = sizeof(unit_ready_cmd);
		slp->sl_scp.scp_datalen = 0;
		slp->sl_scp.scp_direction = SCSI_LOW_READ;
		slp->sl_error |= FATALIO;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
		SCSI_LOW_INFO(slp, ti, "CMDOUT: ccb nexus not found");
		return EINVAL;
	}
	else 
	{
#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_CMDLNK_CHECK, ti->ti_id))
		{
			scsi_low_test_cmdlnk(slp, cb);
		}
#endif	/* SCSI_LOW_DEBUG */
	}
	return 0;
}

/**************************************************************
 * data out pointer setup
 **************************************************************/
int
scsi_low_data(slp, ti, bp, direction)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct buf **bp;
	int direction;
{
	struct slccb *cb = slp->sl_Qnexus;

	if (cb != NULL && direction == cb->ccb_sscp.scp_direction)
	{
		*bp = cb->bp;
		return 0;
	}

	slp->sl_error |= (FATALIO | PDMAERR);
	slp->sl_scp.scp_datalen = 0;
	slp->sl_scp.scp_direction = direction;
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
	if (ti->ti_ophase != ti->ti_phase)
	{
		char *s;

		if (cb == NULL)
			s = "DATA PHASE: ccb nexus not found";
		else
			s = "DATA PHASE: xfer direction mismatch";
		SCSI_LOW_INFO(slp, ti, s);
	}

	*bp = NULL;
	return EINVAL;
}

/**************************************************************
 * MSG_SYS 
 **************************************************************/
#define	MSGINPTR_CLR(ti) {(ti)->ti_msginptr = 0; (ti)->ti_msginlen = 0;}
#define	MSGIN_PERIOD(ti) ((ti)->ti_msgin[3])
#define	MSGIN_OFFSET(ti) ((ti)->ti_msgin[4])
#define	MSGIN_WIDTHP(ti) ((ti)->ti_msgin[3])
#define	MSGIN_DATA_LAST	0x30

static int scsi_low_errfunc_synch(struct scsi_low_softc *, u_int);
static int scsi_low_errfunc_wide(struct scsi_low_softc *, u_int);
static int scsi_low_errfunc_identify(struct scsi_low_softc *, u_int);
static int scsi_low_errfunc_qtag(struct scsi_low_softc *, u_int);

static int scsi_low_msgfunc_synch(struct scsi_low_softc *);
static int scsi_low_msgfunc_wide(struct scsi_low_softc *);
static int scsi_low_msgfunc_identify(struct scsi_low_softc *);
static int scsi_low_msgfunc_abort(struct scsi_low_softc *);
static int scsi_low_msgfunc_qabort(struct scsi_low_softc *);
static int scsi_low_msgfunc_qtag(struct scsi_low_softc *);
static int scsi_low_msgfunc_reset(struct scsi_low_softc *);

struct scsi_low_msgout_data {
	u_int	md_flags;
	u_int8_t md_msg;
	int (*md_msgfunc)(struct scsi_low_softc *);
	int (*md_errfunc)(struct scsi_low_softc *, u_int);
#define	MSG_RELEASE_ATN	0x0001
	u_int md_condition;
};

struct scsi_low_msgout_data scsi_low_msgout_data[] = {
/* 0 */	{SCSI_LOW_MSG_RESET, MSG_RESET, scsi_low_msgfunc_reset, NULL, MSG_RELEASE_ATN},
/* 1 */ {SCSI_LOW_MSG_REJECT, MSG_REJECT, NULL, NULL, MSG_RELEASE_ATN},
/* 2 */ {SCSI_LOW_MSG_PARITY, MSG_PARITY, NULL, NULL, MSG_RELEASE_ATN},
/* 3 */ {SCSI_LOW_MSG_ERROR, MSG_I_ERROR, NULL, NULL, MSG_RELEASE_ATN},
/* 4 */ {SCSI_LOW_MSG_IDENTIFY, MSG_IDENTIFY, scsi_low_msgfunc_identify, scsi_low_errfunc_identify, 0},
/* 5 */ {SCSI_LOW_MSG_ABORT, MSG_ABORT, scsi_low_msgfunc_abort, NULL, MSG_RELEASE_ATN},
/* 6 */ {SCSI_LOW_MSG_TERMIO, MSG_TERM_IO, NULL, NULL, MSG_RELEASE_ATN},
/* 7 */ {SCSI_LOW_MSG_SIMPLE_QTAG,  MSG_SIMPLE_QTAG, scsi_low_msgfunc_qtag, scsi_low_errfunc_qtag, 0},
/* 8 */ {SCSI_LOW_MSG_ORDERED_QTAG, MSG_ORDERED_QTAG, scsi_low_msgfunc_qtag, scsi_low_errfunc_qtag, 0},
/* 9 */{SCSI_LOW_MSG_HEAD_QTAG,  MSG_HEAD_QTAG, scsi_low_msgfunc_qtag, scsi_low_errfunc_qtag, 0},
/* 10 */ {SCSI_LOW_MSG_ABORT_QTAG, MSG_ABORT_QTAG, scsi_low_msgfunc_qabort, NULL,  MSG_RELEASE_ATN},
/* 11 */ {SCSI_LOW_MSG_CLEAR_QTAG, MSG_CLEAR_QTAG, scsi_low_msgfunc_abort, NULL, MSG_RELEASE_ATN},
/* 12 */{SCSI_LOW_MSG_WIDE, MSG_EXTEND, scsi_low_msgfunc_wide, scsi_low_errfunc_wide, MSG_RELEASE_ATN},
/* 13 */{SCSI_LOW_MSG_SYNCH, MSG_EXTEND, scsi_low_msgfunc_synch, scsi_low_errfunc_synch, MSG_RELEASE_ATN},
/* 14 */{SCSI_LOW_MSG_NOOP, MSG_NOOP, NULL, NULL, MSG_RELEASE_ATN},
/* 15 */{SCSI_LOW_MSG_ALL, 0},
};

static int scsi_low_msginfunc_ext(struct scsi_low_softc *);
static int scsi_low_synch(struct scsi_low_softc *);
static int scsi_low_wide(struct scsi_low_softc *);
static int scsi_low_msginfunc_msg_reject(struct scsi_low_softc *);
static int scsi_low_msginfunc_rejop(struct scsi_low_softc *);
static int scsi_low_msginfunc_rp(struct scsi_low_softc *);
static int scsi_low_msginfunc_sdp(struct scsi_low_softc *);
static int scsi_low_msginfunc_disc(struct scsi_low_softc *);
static int scsi_low_msginfunc_cc(struct scsi_low_softc *);
static int scsi_low_msginfunc_lcc(struct scsi_low_softc *);
static int scsi_low_msginfunc_parity(struct scsi_low_softc *);
static int scsi_low_msginfunc_noop(struct scsi_low_softc *);
static int scsi_low_msginfunc_simple_qtag(struct scsi_low_softc *);
static int scsi_low_msginfunc_i_wide_residue(struct scsi_low_softc *);

struct scsi_low_msgin_data {
	u_int md_len;
	int (*md_msgfunc)(struct scsi_low_softc *);
};

struct scsi_low_msgin_data scsi_low_msgin_data[] = {
/* 0 */	{1,	scsi_low_msginfunc_cc},
/* 1 */ {2,	scsi_low_msginfunc_ext},
/* 2 */ {1,	scsi_low_msginfunc_sdp},
/* 3 */ {1,	scsi_low_msginfunc_rp},
/* 4 */ {1,	scsi_low_msginfunc_disc},
/* 5 */ {1,	scsi_low_msginfunc_rejop},
/* 6 */ {1,	scsi_low_msginfunc_rejop},
/* 7 */ {1,	scsi_low_msginfunc_msg_reject},
/* 8 */ {1,	scsi_low_msginfunc_noop},
/* 9 */ {1,	scsi_low_msginfunc_parity},
/* a */ {1,	scsi_low_msginfunc_lcc},
/* b */ {1,	scsi_low_msginfunc_lcc},
/* c */ {1,	scsi_low_msginfunc_rejop},
/* d */ {2,	scsi_low_msginfunc_rejop},
/* e */ {1,	scsi_low_msginfunc_rejop},
/* f */ {1,	scsi_low_msginfunc_rejop},
/* 0x10 */ {1,	scsi_low_msginfunc_rejop},
/* 0x11 */ {1,	scsi_low_msginfunc_rejop},
/* 0x12 */ {1,	scsi_low_msginfunc_rejop},
/* 0x13 */ {1,	scsi_low_msginfunc_rejop},
/* 0x14 */ {1,	scsi_low_msginfunc_rejop},
/* 0x15 */ {1,	scsi_low_msginfunc_rejop},
/* 0x16 */ {1,	scsi_low_msginfunc_rejop},
/* 0x17 */ {1,	scsi_low_msginfunc_rejop},
/* 0x18 */ {1,	scsi_low_msginfunc_rejop},
/* 0x19 */ {1,	scsi_low_msginfunc_rejop},
/* 0x1a */ {1,	scsi_low_msginfunc_rejop},
/* 0x1b */ {1,	scsi_low_msginfunc_rejop},
/* 0x1c */ {1,	scsi_low_msginfunc_rejop},
/* 0x1d */ {1,	scsi_low_msginfunc_rejop},
/* 0x1e */ {1,	scsi_low_msginfunc_rejop},
/* 0x1f */ {1,	scsi_low_msginfunc_rejop},
/* 0x20 */ {2,	scsi_low_msginfunc_simple_qtag},
/* 0x21 */ {2,	scsi_low_msginfunc_rejop},
/* 0x22 */ {2,	scsi_low_msginfunc_rejop},
/* 0x23 */ {2,	scsi_low_msginfunc_i_wide_residue},
/* 0x24 */ {2,	scsi_low_msginfunc_rejop},
/* 0x25 */ {2,	scsi_low_msginfunc_rejop},
/* 0x26 */ {2,	scsi_low_msginfunc_rejop},
/* 0x27 */ {2,	scsi_low_msginfunc_rejop},
/* 0x28 */ {2,	scsi_low_msginfunc_rejop},
/* 0x29 */ {2,	scsi_low_msginfunc_rejop},
/* 0x2a */ {2,	scsi_low_msginfunc_rejop},
/* 0x2b */ {2,	scsi_low_msginfunc_rejop},
/* 0x2c */ {2,	scsi_low_msginfunc_rejop},
/* 0x2d */ {2,	scsi_low_msginfunc_rejop},
/* 0x2e */ {2,	scsi_low_msginfunc_rejop},
/* 0x2f */ {2,	scsi_low_msginfunc_rejop},
/* 0x30 */ {1,	scsi_low_msginfunc_rejop}	/* default rej op */
};

/**************************************************************
 * msgout
 **************************************************************/
static int
scsi_low_msgfunc_synch(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	int ptr = ti->ti_msgoutlen;

	ti->ti_msgoutstr[ptr + 1] = MSG_EXTEND_SYNCHLEN;
	ti->ti_msgoutstr[ptr + 2] = MSG_EXTEND_SYNCHCODE;
	ti->ti_msgoutstr[ptr + 3] = ti->ti_maxsynch.period;
	ti->ti_msgoutstr[ptr + 4] = ti->ti_maxsynch.offset;
	return MSG_EXTEND_SYNCHLEN + 2;
}

static int
scsi_low_msgfunc_wide(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	int ptr = ti->ti_msgoutlen;

	ti->ti_msgoutstr[ptr + 1] = MSG_EXTEND_WIDELEN;
	ti->ti_msgoutstr[ptr + 2] = MSG_EXTEND_WIDECODE;
	ti->ti_msgoutstr[ptr + 3] = ti->ti_width;
	return MSG_EXTEND_WIDELEN + 2;
}

static int
scsi_low_msgfunc_identify(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	struct lun_info *li = slp->sl_Lnexus;
	struct slccb *cb = slp->sl_Qnexus;
	int ptr = ti->ti_msgoutlen;
	u_int8_t msg;

	msg = MSG_IDENTIFY;
	if (cb == NULL)
	{
		slp->sl_error |= FATALIO;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
		SCSI_LOW_INFO(slp, ti, "MSGOUT: nexus unknown");
	}
	else
	{
		if (scsi_low_is_disconnect_ok(cb) != 0)
			msg |= (MSG_IDENTIFY_DISCPRIV | li->li_lun);
		else
			msg |= li->li_lun;

		if (ti->ti_phase == PH_MSGOUT)
		{
			(*slp->sl_funcs->scsi_low_establish_lun_nexus) (slp);
			if (cb->ccb_tag == SCSI_LOW_UNKTAG)
			{
				(*slp->sl_funcs->scsi_low_establish_ccb_nexus) (slp);
			}
		}
	}
	ti->ti_msgoutstr[ptr + 0] = msg;
	return 1;
}

static int
scsi_low_msgfunc_abort(slp)
	struct scsi_low_softc *slp;
{

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_ABORT);
	return 1;
}

static int
scsi_low_msgfunc_qabort(slp)
	struct scsi_low_softc *slp;
{

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_TERM);
	return 1;
}

static int
scsi_low_msgfunc_reset(slp)
	struct scsi_low_softc *slp;
{

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_RESET);
	return 1;
}

static int
scsi_low_msgfunc_qtag(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	struct slccb *cb = slp->sl_Qnexus;
	int ptr = ti->ti_msgoutlen;

	if (cb == NULL || cb->ccb_tag == SCSI_LOW_UNKTAG)
	{
		ti->ti_msgoutstr[ptr + 0] = MSG_NOOP;
		return 1;
	}
	else
	{
		ti->ti_msgoutstr[ptr + 1] = (u_int8_t) cb->ccb_tag;
		if (ti->ti_phase == PH_MSGOUT)
		{
			(*slp->sl_funcs->scsi_low_establish_ccb_nexus) (slp);
		}
	}
	return 2;
}

/*
 * The following functions are called when targets give unexpected
 * responces in msgin (after msgout).
 */
static int
scsi_low_errfunc_identify(slp, msgflags)
	struct scsi_low_softc *slp;
	u_int msgflags;
{

	if (slp->sl_Lnexus != NULL)
	{
	        slp->sl_Lnexus->li_cfgflags &= ~SCSI_LOW_DISC;
		scsi_low_calcf_lun(slp->sl_Lnexus);
	}
	return 0;
}

static int
scsi_low_errfunc_synch(slp, msgflags)
	struct scsi_low_softc *slp;
	u_int msgflags;
{
	struct targ_info *ti = slp->sl_Tnexus;

	MSGIN_PERIOD(ti) = 0;
	MSGIN_OFFSET(ti) = 0;
	scsi_low_synch(slp);
	return 0;
}

static int
scsi_low_errfunc_wide(slp, msgflags)
	struct scsi_low_softc *slp;
	u_int msgflags;
{
	struct targ_info *ti = slp->sl_Tnexus;

	MSGIN_WIDTHP(ti) = 0;
	scsi_low_wide(slp);
	return 0;
}

static int
scsi_low_errfunc_qtag(slp, msgflags)
	struct scsi_low_softc *slp;
	u_int msgflags;
{

	if ((msgflags & SCSI_LOW_MSG_REJECT) != 0)
	{
		if (slp->sl_Qnexus != NULL)
		{
			scsi_low_deactivate_qtag(slp->sl_Qnexus);
		}
		if (slp->sl_Lnexus != NULL)
		{
			slp->sl_Lnexus->li_cfgflags &= ~SCSI_LOW_QTAG;
			scsi_low_calcf_lun(slp->sl_Lnexus);
		}
		printf("%s: scsi_low: qtag msg rejected\n", slp->sl_xname);
	}
	return 0;
}


int
scsi_low_msgout(slp, ti, fl)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int fl;
{
	struct scsi_low_msgout_data *mdp;
	int len = 0;

#ifdef	SCSI_LOW_DIAGNOSTIC
	if (ti != slp->sl_Tnexus)
	{
		scsi_low_print(slp, NULL);
		panic("scsi_low_msgout: Target nexus inconsistent");
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	slp->sl_ph_count ++;
	if (slp->sl_ph_count > SCSI_LOW_MAX_PHCHANGES)
	{
		printf("%s: too many phase changes\n", slp->sl_xname);
		slp->sl_error |= FATALIO;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
	}
		
	/* STEP I.
	 * Scsi phase changes.
	 * Previously msgs asserted are accepted by our target or
	 * processed by scsi_low_msgin.
	 * Thus clear all saved informations.
	 */
	if ((fl & SCSI_LOW_MSGOUT_INIT) != 0)
	{
		ti->ti_omsgflags = 0;
		ti->ti_emsgflags = 0;
	}
	else if (slp->sl_atten == 0)
	{
	/* STEP II.
	 * We did not assert attention, however still our target required
	 * msgs. Resend previous msgs. 
	 */
		ti->ti_msgflags |= ti->ti_omsgflags;
		ti->ti_omsgflags = 0;
#ifdef	SCSI_LOW_DIAGNOSTIC
		printf("%s: scsi_low_msgout: retry msgout\n", slp->sl_xname);
#endif	/* SCSI_LOW_DIAGNOSTIC */
	}

	/* STEP III.
	 * We have no msgs. send MSG_NOOP (OK?)
	 */
	if (scsi_low_is_msgout_continue(ti, 0) == 0)
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_NOOP, 0);

	/* STEP IV.
	 * Process all msgs
	 */
	ti->ti_msgoutlen = 0;
	slp->sl_clear_atten = 0;
	mdp = &scsi_low_msgout_data[0];
	for ( ; mdp->md_flags != SCSI_LOW_MSG_ALL; mdp ++)
	{
		if ((ti->ti_msgflags & mdp->md_flags) != 0)
		{
			ti->ti_omsgflags |= mdp->md_flags;
			ti->ti_msgflags &= ~mdp->md_flags;
			ti->ti_emsgflags = mdp->md_flags;

			ti->ti_msgoutstr[ti->ti_msgoutlen] = mdp->md_msg;
			if (mdp->md_msgfunc != NULL)
				len = (*mdp->md_msgfunc) (slp);
			else
				len = 1;

#ifdef	SCSI_LOW_DIAGNOSTIC
			scsi_low_msg_log_write(&ti->ti_log_msgout,
			       &ti->ti_msgoutstr[ti->ti_msgoutlen], len);
#endif	/* SCSI_LOW_DIAGNOSTIC */

			ti->ti_msgoutlen += len;
			if ((mdp->md_condition & MSG_RELEASE_ATN) != 0)
			{
				slp->sl_clear_atten = 1;
				break;
			}

			if ((fl & SCSI_LOW_MSGOUT_UNIFY) == 0 ||
			    ti->ti_msgflags == 0)
				break;

			if (ti->ti_msgoutlen >= SCSI_LOW_MAX_MSGLEN - 5)
				break;
		}
	}

	if (scsi_low_is_msgout_continue(ti, 0) == 0)
		slp->sl_clear_atten = 1;

	return ti->ti_msgoutlen;
}

/**************************************************************
 * msgin
 **************************************************************/
static int
scsi_low_msginfunc_noop(slp)
	struct scsi_low_softc *slp;
{

	return 0;
}

static int
scsi_low_msginfunc_rejop(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	u_int8_t msg = ti->ti_msgin[0];

	printf("%s: MSGIN: msg 0x%x rejected\n", slp->sl_xname, (u_int) msg);
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_msginfunc_cc(slp)
	struct scsi_low_softc *slp;
{
	struct lun_info *li;

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_CMDC);

	/* validate status */
	if (slp->sl_Qnexus == NULL)
		return ENOENT;

	slp->sl_Qnexus->ccb_sscp.scp_status = slp->sl_scp.scp_status;
 	li = slp->sl_Lnexus;
	switch (slp->sl_scp.scp_status)
	{
	case ST_GOOD:
		li->li_maxnqio = li->li_maxnexus;
		break;

	case ST_CHKCOND:
		li->li_maxnqio = 0;
		if (li->li_qflags & SCSI_LOW_QFLAG_CA_QCLEAR)
			scsi_low_reset_nexus_lun(slp, li, 0);
		break;

	case ST_BUSY:
		li->li_maxnqio = 0;
		break;

	case ST_QUEFULL:
		if (li->li_maxnexus >= li->li_nqio)
			li->li_maxnexus = li->li_nqio - 1;
		li->li_maxnqio = li->li_maxnexus;
		break;

	case ST_INTERGOOD:
	case ST_INTERMET:
		slp->sl_error |= MSGERR;
		break;

	default:
		break;
	}
	return 0;
}

static int
scsi_low_msginfunc_lcc(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *ncb, *cb;

	ti = slp->sl_Tnexus;
 	li = slp->sl_Lnexus;
	if ((cb = slp->sl_Qnexus) == NULL)
		goto bad;
		
	cb->ccb_sscp.scp_status = slp->sl_scp.scp_status;
	switch (slp->sl_scp.scp_status)
	{
	case ST_INTERGOOD:
	case ST_INTERMET:
		li->li_maxnqio = li->li_maxnexus;
		break;

	default:
		slp->sl_error |= MSGERR;
		break;
	}

	if ((li->li_flags & SCSI_LOW_LINK) == 0)
		goto bad;

	cb->ccb_error |= slp->sl_error;
	if (cb->ccb_error != 0)
		goto bad;

	for (ncb = TAILQ_FIRST(&slp->sl_start); ncb != NULL;
	     ncb = TAILQ_NEXT(ncb, ccb_chain))
	{
		if (ncb->li == li)
			goto cmd_link_start;
	}


bad:
	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_LCTERM);
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return EIO;

cmd_link_start:
	ncb->ccb_flags &= ~CCB_STARTQ;
	TAILQ_REMOVE(&slp->sl_start, ncb, ccb_chain);

	scsi_low_dealloc_qtag(ncb);
	ncb->ccb_tag = cb->ccb_tag;
	ncb->ccb_otag = cb->ccb_otag;
	cb->ccb_tag = SCSI_LOW_UNKTAG;
	cb->ccb_otag = SCSI_LOW_UNKTAG;
	if (scsi_low_done(slp, cb) == SCSI_LOW_DONE_RETRY)
		panic("%s: linked ccb retried", slp->sl_xname);

	slp->sl_Qnexus = ncb;
	slp->sl_ph_count = 0;

	ncb->ccb_error = 0;
	ncb->ccb_datalen = -1;
	ncb->ccb_scp.scp_status = ST_UNKNOWN;
	ncb->ccb_flags &= ~CCB_INTERNAL;

	scsi_low_init_msgsys(slp, ti);

	(*slp->sl_osdep_fp->scsi_low_osdep_ccb_setup) (slp, ncb);

	if (ncb->ccb_tcmax < SCSI_LOW_MIN_TOUT)
		ncb->ccb_tcmax = SCSI_LOW_MIN_TOUT;
	ncb->ccb_tc = ncb->ccb_tcmax;

	/* setup saved scsi data pointer */
	ncb->ccb_sscp = ncb->ccb_scp;
	slp->sl_scp = ncb->ccb_sscp;
	slp->sl_error = ncb->ccb_error;

#ifdef	SCSI_LOW_DIAGNOSTIC
	scsi_low_msg_log_init(&ti->ti_log_msgin);
	scsi_low_msg_log_init(&ti->ti_log_msgout);
#endif	/* SCSI_LOW_DIAGNOSTIC */
	return EJUSTRETURN;
}

static int
scsi_low_msginfunc_disc(slp)
	struct scsi_low_softc *slp;
{

	SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_DISC);
	return 0;
}

static int
scsi_low_msginfunc_sdp(slp)
	struct scsi_low_softc *slp;
{
	struct slccb *cb = slp->sl_Qnexus;

	if (cb != NULL)
	{
		cb->ccb_sscp.scp_datalen = slp->sl_scp.scp_datalen;
		cb->ccb_sscp.scp_data = slp->sl_scp.scp_data;
	}
	else
		scsi_low_assert_msg(slp, slp->sl_Tnexus, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_msginfunc_rp(slp)
	struct scsi_low_softc *slp;
{

	if (slp->sl_Qnexus != NULL)
		slp->sl_scp = slp->sl_Qnexus->ccb_sscp;
	else
		scsi_low_assert_msg(slp, slp->sl_Tnexus, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_synch(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	u_int period = 0, offset = 0, speed;
	u_char *s;
	int error;

	if ((MSGIN_PERIOD(ti) >= ti->ti_maxsynch.period &&
	     MSGIN_OFFSET(ti) <= ti->ti_maxsynch.offset) ||
	     MSGIN_OFFSET(ti) == 0)
	{
		if ((offset = MSGIN_OFFSET(ti)) != 0)
			period = MSGIN_PERIOD(ti);
		s = offset ? "synchronous" : "async";
	}
	else
	{
		/* XXX:
		 * Target seems to be brain damaged.
		 * Force async transfer.
		 */
		ti->ti_maxsynch.period = 0;
		ti->ti_maxsynch.offset = 0;
		printf("%s: target brain damaged. async transfer\n",
			slp->sl_xname);
		return EINVAL;
	}

	ti->ti_maxsynch.period = period;
	ti->ti_maxsynch.offset = offset;

	error = (*slp->sl_funcs->scsi_low_msg) (slp, ti, SCSI_LOW_MSG_SYNCH);
	if (error != 0)
	{
		/* XXX:
		 * Current period and offset are not acceptable 
		 * for our adapter.
		 * The adapter changes max synch and max offset.
		 */
		printf("%s: synch neg failed. retry synch msg neg ...\n",
			slp->sl_xname);
		return error;
	}

	ti->ti_osynch = ti->ti_maxsynch;
	if (offset > 0)
	{
		ti->ti_setup_msg_done |= SCSI_LOW_MSG_SYNCH;
	}

	/* inform data */
	if ((slp->sl_show_result & SHOW_SYNCH_NEG) != 0)
	{
#ifdef	SCSI_LOW_NEGOTIATE_BEFORE_SENSE
		struct slccb *cb = slp->sl_Qnexus;

		if (cb != NULL && (cb->ccb_flags & CCB_SENSE) != 0)
			return 0;
#endif	/* SCSI_LOW_NEGOTIATE_BEFORE_SENSE */

		printf("%s(%d:*): <%s> offset %d period %dns ",
			slp->sl_xname, ti->ti_id, s, offset, period * 4);

		if (period != 0)
		{
			speed = 1000 * 10 / (period * 4);
			printf("%d.%d M/s", speed / 10, speed % 10);
		}
		printf("\n");
	}
	return 0;
}

static int
scsi_low_wide(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	int error;

	ti->ti_width = MSGIN_WIDTHP(ti);
	error = (*slp->sl_funcs->scsi_low_msg) (slp, ti, SCSI_LOW_MSG_WIDE);
	if (error != 0)
	{
		/* XXX:
		 * Current width is not acceptable for our adapter.
		 * The adapter changes max width.
		 */
		printf("%s: wide neg failed. retry wide msg neg ...\n",
			slp->sl_xname);
		return error;
	}

	ti->ti_owidth = ti->ti_width;
	if (ti->ti_width > SCSI_LOW_BUS_WIDTH_8)
	{
		ti->ti_setup_msg_done |= 
			(SCSI_LOW_MSG_SYNCH | SCSI_LOW_MSG_WIDE);
	}
		
	/* inform data */
	if ((slp->sl_show_result & SHOW_WIDE_NEG) != 0)
	{
#ifdef	SCSI_LOW_NEGOTIATE_BEFORE_SENSE
		struct slccb *cb = slp->sl_Qnexus;

		if (cb != NULL && (cb->ccb_flags & CCB_SENSE) != 0)
			return 0;
#endif	/* SCSI_LOW_NEGOTIATE_BEFORE_SENSE */

		printf("%s(%d:*): transfer width %d bits\n",
			slp->sl_xname, ti->ti_id, 1 << (3 + ti->ti_width));
	}
	return 0;
}

static int
scsi_low_msginfunc_simple_qtag(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	scsi_low_tag_t etag = (scsi_low_tag_t) ti->ti_msgin[1];

	if (slp->sl_Qnexus != NULL)
	{
		if (slp->sl_Qnexus->ccb_tag != etag)
		{
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
			SCSI_LOW_INFO(slp, ti, "MSGIN: qtag mismatch");
		}
	}
	else if (scsi_low_establish_ccb(ti, slp->sl_Lnexus, etag) == NULL)
	{
#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_NEXUS_CHECK, ti->ti_id))
			return 0;
#endif	/* SCSI_LOW_DEBUG */

		slp->sl_error |= FATALIO;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT_QTAG, 0);
		SCSI_LOW_INFO(slp, ti, "MSGIN: taged ccb not found");
	}
	return 0;
}

static int
scsi_low_msginfunc_i_wide_residue(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	struct slccb *cb = slp->sl_Qnexus;
	int res = (int) ti->ti_msgin[1];

	if (cb == NULL || res <= 0 ||
	    (ti->ti_width == SCSI_LOW_BUS_WIDTH_16 && res > 1) ||
	    (ti->ti_width == SCSI_LOW_BUS_WIDTH_32 && res > 3))
		return EINVAL;
		
	if (slp->sl_scp.scp_datalen + res > cb->ccb_scp.scp_datalen)
		return EINVAL;

	slp->sl_scp.scp_datalen += res;
	slp->sl_scp.scp_data -= res;
	scsi_low_data_finish(slp);
	return 0;
}

static int
scsi_low_msginfunc_ext(slp)
	struct scsi_low_softc *slp;
{
	struct slccb *cb = slp->sl_Qnexus;
	struct lun_info *li = slp->sl_Lnexus;
	struct targ_info *ti = slp->sl_Tnexus;
	int count, retry;
	u_int32_t *ptr;

	if (ti->ti_msginptr == 2)
	{
		ti->ti_msginlen = ti->ti_msgin[1] + 2;
		return 0;
	}

	switch (MKMSG_EXTEND(ti->ti_msgin[1], ti->ti_msgin[2]))
	{
	case MKMSG_EXTEND(MSG_EXTEND_MDPLEN, MSG_EXTEND_MDPCODE):
		if (cb == NULL)
			break;

		ptr = (u_int32_t *)(&ti->ti_msgin[3]);
		count = (int) htonl((long) (*ptr));
		if(slp->sl_scp.scp_datalen - count < 0 || 
		   slp->sl_scp.scp_datalen - count > cb->ccb_scp.scp_datalen)
			break;

		slp->sl_scp.scp_datalen -= count;
		slp->sl_scp.scp_data += count;
		return 0;

	case MKMSG_EXTEND(MSG_EXTEND_SYNCHLEN, MSG_EXTEND_SYNCHCODE):
		if (li == NULL)
			break;

		retry = scsi_low_synch(slp);
		if (retry != 0 || (ti->ti_emsgflags & SCSI_LOW_MSG_SYNCH) == 0)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_SYNCH, 0);

#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_ATTEN_CHECK, ti->ti_id))
		{
			scsi_low_test_atten(slp, ti, SCSI_LOW_MSG_SYNCH);
		}
#endif	/* SCSI_LOW_DEBUG */
		return 0;

	case MKMSG_EXTEND(MSG_EXTEND_WIDELEN, MSG_EXTEND_WIDECODE):
		if (li == NULL)
			break;

		retry = scsi_low_wide(slp);
		if (retry != 0 || (ti->ti_emsgflags & SCSI_LOW_MSG_WIDE) == 0)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_WIDE, 0);

		return 0;

	default:
		break;
	}

	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return EINVAL;
}

static int
scsi_low_msginfunc_parity(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;

	/* only I -> T, invalid! */
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	return 0;
}

static int
scsi_low_msginfunc_msg_reject(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti = slp->sl_Tnexus;
	struct scsi_low_msgout_data *mdp;
	u_int msgflags;

	if (ti->ti_emsgflags != 0)
	{
		printf("%s: msg flags [0x%x] rejected\n",
		       slp->sl_xname, ti->ti_emsgflags);
		msgflags = SCSI_LOW_MSG_REJECT;
		mdp = &scsi_low_msgout_data[0];
		for ( ; mdp->md_flags != SCSI_LOW_MSG_ALL; mdp ++)
		{
			if ((ti->ti_emsgflags & mdp->md_flags) != 0)
			{
				ti->ti_emsgflags &= ~mdp->md_flags;
				if (mdp->md_errfunc != NULL)
					(*mdp->md_errfunc) (slp, msgflags);
				break;
			}
		}
		return 0;
	}
	else
	{
		SCSI_LOW_INFO(slp, ti, "MSGIN: rejected msg not found");
		slp->sl_error |= MSGERR;
	}
	return EINVAL;
}

int
scsi_low_msgin(slp, ti, c)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int c;
{
	struct scsi_low_msgin_data *sdp;
	struct lun_info *li;
	u_int8_t msg;

#ifdef	SCSI_LOW_DIAGNOSTIC
	if (ti != slp->sl_Tnexus)
	{
		scsi_low_print(slp, NULL);
		panic("scsi_low_msgin: Target nexus inconsistent");
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	/*
	 * Phase changes, clear the pointer.
	 */
	if (ti->ti_ophase != ti->ti_phase)
	{
		MSGINPTR_CLR(ti);
		ti->ti_msgin_parity_error = 0;

		slp->sl_ph_count ++;
		if (slp->sl_ph_count > SCSI_LOW_MAX_PHCHANGES)
		{
			printf("%s: too many phase changes\n", slp->sl_xname);
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
		}
	}

	/*
	 * Store a current messages byte into buffer and 
	 * wait for the completion of the current msg.
	 */
	ti->ti_msgin[ti->ti_msginptr ++] = (u_int8_t) c;
	if (ti->ti_msginptr >= SCSI_LOW_MAX_MSGLEN)
	{
		ti->ti_msginptr = SCSI_LOW_MAX_MSGLEN - 1;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_REJECT, 0);
	}	

	/*
	 * Check parity errors.
	 */
	if ((c & SCSI_LOW_DATA_PE) != 0)
	{
		ti->ti_msgin_parity_error ++;
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_PARITY, 0);
		goto out;
	}

	if (ti->ti_msgin_parity_error != 0)
		goto out;

	/*
	 * Calculate messages length.
	 */
	msg = ti->ti_msgin[0];
	if (msg < MSGIN_DATA_LAST)
		sdp = &scsi_low_msgin_data[msg];
	else
		sdp = &scsi_low_msgin_data[MSGIN_DATA_LAST];

	if (ti->ti_msginlen == 0)
	{
		ti->ti_msginlen = sdp->md_len;
	}

	/*
	 * Check comletion.
	 */
	if (ti->ti_msginptr < ti->ti_msginlen)
		return EJUSTRETURN;

	/*
	 * Do process.
	 */
	if ((msg & MSG_IDENTIFY) == 0)
	{
		if (((*sdp->md_msgfunc) (slp)) == EJUSTRETURN)
			return EJUSTRETURN;
	}
	else
	{
		li = slp->sl_Lnexus;
		if (li == NULL)
		{
			li = scsi_low_alloc_li(ti, MSGCMD_LUN(msg), 0);
			if (li == NULL)
				goto badlun;
			slp->sl_Lnexus = li;
			(*slp->sl_funcs->scsi_low_establish_lun_nexus) (slp);
		}	
		else
		{
			if (MSGCMD_LUN(msg) != li->li_lun)
				goto badlun;
		}

		if (slp->sl_Qnexus == NULL && li->li_nqio == 0)
		{
			if (!scsi_low_establish_ccb(ti, li, SCSI_LOW_UNKTAG))
			{
#ifdef	SCSI_LOW_DEBUG
				if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_NEXUS_CHECK, ti->ti_id) != 0)
				{
					goto out;
				}
#endif	/* SCSI_LOW_DEBUG */
				goto badlun;
			}
		}
	}
	goto out;

	/*
	 * Msg process completed, reset msgin pointer and assert ATN if desired.
	 */
badlun:
	slp->sl_error |= FATALIO;
	scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 0);
	SCSI_LOW_INFO(slp, ti, "MSGIN: identify wrong");

out:
	if (ti->ti_msginptr < ti->ti_msginlen)
		return EJUSTRETURN;

#ifdef	SCSI_LOW_DIAGNOSTIC
	scsi_low_msg_log_write(&ti->ti_log_msgin,
			       &ti->ti_msgin[0], ti->ti_msginlen);
#endif	/* SCSI_LOW_DIAGNOSTIC */

	MSGINPTR_CLR(ti);
	return 0;
}

/**********************************************************
 * disconnect
 **********************************************************/
int
scsi_low_disconnected(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct slccb *cb = slp->sl_Qnexus;

	/* check phase completion */
	switch (slp->sl_msgphase)
	{
	case MSGPH_RESET:
		scsi_low_statusin(slp, slp->sl_Tnexus, ST_GOOD);
		scsi_low_msginfunc_cc(slp);
		scsi_low_reset_nexus_target(slp, slp->sl_Tnexus, 0);
		goto io_resume;

	case MSGPH_ABORT:
		scsi_low_statusin(slp, slp->sl_Tnexus, ST_GOOD);
		scsi_low_msginfunc_cc(slp);
		scsi_low_reset_nexus_lun(slp, slp->sl_Lnexus, 0);
		goto io_resume;

	case MSGPH_TERM:
		scsi_low_statusin(slp, slp->sl_Tnexus, ST_GOOD);
		scsi_low_msginfunc_cc(slp);
		goto io_resume;

	case MSGPH_DISC:
		if (cb != NULL)
		{
			struct lun_info *li;

			li = cb->li;
			TAILQ_INSERT_TAIL(&li->li_discq, cb, ccb_chain);
			cb->ccb_flags |= CCB_DISCQ;
			cb->ccb_error |= slp->sl_error;
			li->li_disc ++;
			ti->ti_disc ++;
			slp->sl_disc ++;
		}

#ifdef	SCSI_LOW_STATICS
		scsi_low_statics.nexus_disconnected ++;
#endif	/* SCSI_LOW_STATICS */

#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_DISC, ti->ti_id) != 0)
		{
			printf("## SCSI_LOW_DISCONNECTED ===============\n");
			scsi_low_print(slp, NULL);
		}
#endif	/* SCSI_LOW_DEBUG */
		break;

	case MSGPH_NULL:
		slp->sl_error |= FATALIO;
		if (ti->ti_phase == PH_SELSTART)
			slp->sl_error |= SELTIMEOUTIO;
		else
			slp->sl_error |= UBFERR;
		/* fall through */

	case MSGPH_LCTERM:
	case MSGPH_CMDC:
io_resume:
		if (cb == NULL)
			break;

#ifdef	SCSI_LOW_DEBUG
		if (SCSI_LOW_DEBUG_TEST_GO(SCSI_LOW_ATTEN_CHECK, ti->ti_id))
		{
			if (cb->ccb_omsgoutflag == SCSI_LOW_MSG_NOOP &&
			    (cb->ccb_msgoutflag != 0 ||
			     (ti->ti_msgflags & SCSI_LOW_MSG_NOOP)))
			{
				scsi_low_info(slp, ti, "ATTEN CHECK FAILED");
			}
		}
#endif	/* SCSI_LOW_DEBUG */

		cb->ccb_error |= slp->sl_error;
		if (scsi_low_done(slp, cb) == SCSI_LOW_DONE_RETRY)
		{
			cb->ccb_flags |= CCB_STARTQ;
			TAILQ_INSERT_HEAD(&slp->sl_start, cb, ccb_chain);
		}
		break;
	}

	scsi_low_bus_release(slp, ti);	
	scsi_low_start(slp);
	return 1;
}

/**********************************************************
 * TAG operations
 **********************************************************/
static int
scsi_low_alloc_qtag(cb)
	struct slccb *cb;
{
	struct lun_info *li = cb->li;
	scsi_low_tag_t etag;

	if (cb->ccb_otag != SCSI_LOW_UNKTAG)
		return 0;

#ifndef	SCSI_LOW_ALT_QTAG_ALLOCATE
	etag = ffs(li->li_qtagbits);
	if (etag == 0)
		return ENOSPC;

	li->li_qtagbits &= ~(1 << (etag - 1));
	cb->ccb_otag = etag;
	return 0;

#else	/* SCSI_LOW_ALT_QTAG_ALLOCATE */
	for (etag = li->li_qd ; li->li_qd < SCSI_LOW_MAXNEXUS; li->li_qd ++)
		if (li->li_qtagarray[li->li_qd] == 0)
			goto found;

	for (li->li_qd = 0; li->li_qd < etag; li->li_qd ++)
		if (li->li_qtagarray[li->li_qd] == 0)
			goto found;

	return ENOSPC;

found:
	li->li_qtagarray[li->li_qd] ++;
	cb->ccb_otag = (li->li_qd ++);
	return 0;
#endif	/* SCSI_LOW_ALT_QTAG_ALLOCATE */
}
	
static int
scsi_low_dealloc_qtag(cb)
	struct slccb *cb;
{
	struct lun_info *li = cb->li;
	scsi_low_tag_t etag;

	if (cb->ccb_otag == SCSI_LOW_UNKTAG)
		return 0;

#ifndef	SCSI_LOW_ALT_QTAG_ALLOCATE
	etag = cb->ccb_otag - 1;
#ifdef	SCSI_LOW_DIAGNOSTIC
	if (etag >= sizeof(li->li_qtagbits) * NBBY)
		panic("scsi_low_dealloc_tag: illegal tag");
#endif	/* SCSI_LOW_DIAGNOSTIC */
	li->li_qtagbits |= (1 << etag);

#else	/* SCSI_LOW_ALT_QTAG_ALLOCATE */
	etag = cb->ccb_otag;
#ifdef	SCSI_LOW_DIAGNOSTIC
	if (etag >= SCSI_LOW_MAXNEXUS)
		panic("scsi_low_dealloc_tag: illegal tag");
#endif	/* SCSI_LOW_DIAGNOSTIC */
	li->li_qtagarray[etag] --;
#endif	/* SCSI_LOW_ALT_QTAG_ALLOCATE */

	cb->ccb_otag = SCSI_LOW_UNKTAG;
	return 0;
}

static struct slccb *
scsi_low_revoke_ccb(slp, cb, fdone)
	struct scsi_low_softc *slp;
	struct slccb *cb;
	int fdone;
{
	struct targ_info *ti = cb->ti;
	struct lun_info *li = cb->li;

#ifdef	SCSI_LOW_DIAGNOSTIC
	if ((cb->ccb_flags & (CCB_STARTQ | CCB_DISCQ)) == 
	    (CCB_STARTQ | CCB_DISCQ))
	{
		panic("%s: ccb in both queue", slp->sl_xname);
	}
#endif	/* SCSI_LOW_DIAGNOSTIC */

	if ((cb->ccb_flags & CCB_STARTQ) != 0)
	{
		TAILQ_REMOVE(&slp->sl_start, cb, ccb_chain);
	}

	if ((cb->ccb_flags & CCB_DISCQ) != 0)
	{
		TAILQ_REMOVE(&li->li_discq, cb, ccb_chain);
		li->li_disc --;
		ti->ti_disc --;
		slp->sl_disc --;
	}

	cb->ccb_flags &= ~(CCB_STARTQ | CCB_DISCQ | 
			   CCB_SENSE | CCB_CLEARQ | CCB_INTERNAL);

	if (fdone != 0 &&
	    (cb->ccb_rcnt ++ >= slp->sl_max_retry || 
	     (cb->ccb_flags & CCB_NORETRY) != 0))
	{
		cb->ccb_error |= FATALIO;
		cb->ccb_flags &= ~CCB_AUTOSENSE;
		if (scsi_low_done(slp, cb) != SCSI_LOW_DONE_COMPLETE)
			panic("%s: done ccb retried", slp->sl_xname);
		return NULL;
	}
	else
	{
		cb->ccb_error |= PENDINGIO;
		scsi_low_deactivate_qtag(cb);
		scsi_low_ccb_message_retry(cb);
		cb->ccb_tc = cb->ccb_tcmax = SCSI_LOW_MIN_TOUT;
		return cb;
	}
}

static void
scsi_low_reset_nexus_lun(slp, li, fdone)
	struct scsi_low_softc *slp;
	struct lun_info *li;
	int fdone;
{
	struct slccb *cb, *ncb, *ecb;

	if (li == NULL)
		return;

	ecb = NULL;
	for (cb = TAILQ_FIRST(&li->li_discq); cb != NULL; cb = ncb)
	{
		ncb = TAILQ_NEXT(cb, ccb_chain);
		cb = scsi_low_revoke_ccb(slp, cb, fdone);
		if (cb != NULL)
		{
			/*
			 * presumely keep ordering of io
			 */
			cb->ccb_flags |= CCB_STARTQ;
			if (ecb == NULL)
			{
				TAILQ_INSERT_HEAD(&slp->sl_start,\
						  cb, ccb_chain);
			}
			else
			{
				TAILQ_INSERT_AFTER(&slp->sl_start,\
						   ecb, cb, ccb_chain);
			}
			ecb = cb;
		}
	}
}
	
/**************************************************************
 * Qurik setup
 **************************************************************/
static void
scsi_low_calcf_lun(li)
	struct lun_info *li;
{
	struct targ_info *ti = li->li_ti;
	struct scsi_low_softc *slp = ti->ti_sc;
	u_int cfgflags, diskflags;

	if (li->li_flags_valid == SCSI_LOW_LUN_FLAGS_ALL_VALID)
		cfgflags = li->li_cfgflags;
	else
		cfgflags = 0;

	diskflags = li->li_diskflags & li->li_quirks;

	/* disconnect */
	li->li_flags &= ~SCSI_LOW_DISC;
	if ((slp->sl_cfgflags & CFG_NODISC) == 0 &&
	    (diskflags & SCSI_LOW_DISK_DISC) != 0 &&
	    (cfgflags & SCSI_LOW_DISC) != 0)
		li->li_flags |= SCSI_LOW_DISC;

	/* parity */
	li->li_flags |= SCSI_LOW_NOPARITY;
	if ((slp->sl_cfgflags & CFG_NOPARITY) == 0 &&
	    (diskflags & SCSI_LOW_DISK_PARITY) != 0 &&
	    (cfgflags & SCSI_LOW_NOPARITY) == 0)
		li->li_flags &= ~SCSI_LOW_NOPARITY;

	/* qtag */
	if ((slp->sl_cfgflags & CFG_NOQTAG) == 0 &&
	    (cfgflags & SCSI_LOW_QTAG) != 0 &&
	    (diskflags & SCSI_LOW_DISK_QTAG) != 0)
	{
		li->li_flags |= SCSI_LOW_QTAG;
		li->li_maxnexus = SCSI_LOW_MAXNEXUS;
		li->li_maxnqio = li->li_maxnexus;
	}
	else
	{
		li->li_flags &= ~SCSI_LOW_QTAG;
		li->li_maxnexus = 0;
		li->li_maxnqio = li->li_maxnexus;
	}

	/* cmd link */
	li->li_flags &= ~SCSI_LOW_LINK;
	if ((cfgflags & SCSI_LOW_LINK) != 0 &&
	    (diskflags & SCSI_LOW_DISK_LINK) != 0)
		li->li_flags |= SCSI_LOW_LINK;

	/* compatible flags */
	li->li_flags &= ~SCSI_LOW_SYNC;
	if (ti->ti_maxsynch.offset > 0)
		li->li_flags |= SCSI_LOW_SYNC;

#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_CALCF, ti->ti_id) != 0)
	{
		scsi_low_calcf_show(li);
	}
#endif	/* SCSI_LOW_DEBUG */
}

static void
scsi_low_calcf_target(ti)
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = ti->ti_sc;
	u_int offset, period, diskflags;

	diskflags = ti->ti_diskflags & ti->ti_quirks;

	/* synch */
	if ((slp->sl_cfgflags & CFG_ASYNC) == 0 &&
	    (diskflags & SCSI_LOW_DISK_SYNC) != 0)
	{
		offset = ti->ti_maxsynch.offset;
		period = ti->ti_maxsynch.period;
		if (offset == 0 || period == 0)
			offset = period = 0;
	}
	else
	{
		offset = period = 0;
	}
	
	ti->ti_maxsynch.offset = offset;
	ti->ti_maxsynch.period = period;

	/* wide */
	if ((diskflags & SCSI_LOW_DISK_WIDE_32) == 0 &&
	     ti->ti_width > SCSI_LOW_BUS_WIDTH_16)
		ti->ti_width = SCSI_LOW_BUS_WIDTH_16;

	if ((diskflags & SCSI_LOW_DISK_WIDE_16) == 0 &&
	    ti->ti_width > SCSI_LOW_BUS_WIDTH_8)
		ti->ti_width = SCSI_LOW_BUS_WIDTH_8;

	if (ti->ti_flags_valid == SCSI_LOW_TARG_FLAGS_ALL_VALID)
	{
		if (ti->ti_maxsynch.offset != ti->ti_osynch.offset ||
		    ti->ti_maxsynch.period != ti->ti_osynch.period)
			ti->ti_setup_msg |= SCSI_LOW_MSG_SYNCH;
		if (ti->ti_width != ti->ti_owidth)
			ti->ti_setup_msg |= (SCSI_LOW_MSG_WIDE | SCSI_LOW_MSG_SYNCH);

		ti->ti_osynch = ti->ti_maxsynch;
		ti->ti_owidth = ti->ti_width;
	}

#ifdef	SCSI_LOW_DEBUG
	if (SCSI_LOW_DEBUG_GO(SCSI_LOW_DEBUG_CALCF, ti->ti_id) != 0)
	{
		printf("%s(%d:*): max period(%dns) offset(%d) width(%d)\n",
			slp->sl_xname, ti->ti_id,
			ti->ti_maxsynch.period * 4,
			ti->ti_maxsynch.offset,
			ti->ti_width);
	}
#endif	/* SCSI_LOW_DEBUG */
}

static void
scsi_low_calcf_show(li)
	struct lun_info *li;
{
	struct targ_info *ti = li->li_ti;
	struct scsi_low_softc *slp = ti->ti_sc;

	printf("%s(%d:%d): period(%d ns) offset(%d) width(%d) flags 0x%b\n",
		slp->sl_xname, ti->ti_id, li->li_lun,
		ti->ti_maxsynch.period * 4,
		ti->ti_maxsynch.offset,
		ti->ti_width,
		li->li_flags, SCSI_LOW_BITS);
}

#ifdef	SCSI_LOW_START_UP_CHECK
/**************************************************************
 * scsi world start up
 **************************************************************/
static int scsi_low_poll(struct scsi_low_softc *, struct slccb *);

static int
scsi_low_start_up(slp)
	struct scsi_low_softc *slp;
{
	struct targ_info *ti;
	struct lun_info *li;
	struct slccb *cb;
	int target, lun;

	printf("%s: scsi_low: probing all devices ....\n", slp->sl_xname);

	for (target = 0; target < slp->sl_ntargs; target ++)
	{
		if (target == slp->sl_hostid)
		{
			if ((slp->sl_show_result & SHOW_PROBE_RES) != 0)
			{
				printf("%s: scsi_low: target %d (host card)\n",
					slp->sl_xname, target);
			}
			continue;
		}

		if ((slp->sl_show_result & SHOW_PROBE_RES) != 0)
		{
			printf("%s: scsi_low: target %d lun ",
				slp->sl_xname, target);
		}

		ti = slp->sl_ti[target];
		for (lun = 0; lun < slp->sl_nluns; lun ++)
		{
			if ((cb = SCSI_LOW_ALLOC_CCB(1)) == NULL)
				break;

			cb->osdep = NULL;
			cb->bp = NULL;

			li = scsi_low_alloc_li(ti, lun, 1);

			scsi_low_enqueue(slp, ti, li, cb,
					 CCB_AUTOSENSE | CCB_POLLED, 0);

			scsi_low_poll(slp, cb);

			if (li->li_state != SCSI_LOW_LUN_OK)
				break;

			if ((slp->sl_show_result & SHOW_PROBE_RES) != 0)
			{
				printf("%d ", lun); 		
			}
		}

		if ((slp->sl_show_result & SHOW_PROBE_RES) != 0)
		{
			printf("\n");
		}
	}
	return 0;
}

static int
scsi_low_poll(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
	int tcount;

	tcount = 0;
	while (slp->sl_nio > 0)
	{
		SCSI_LOW_DELAY((1000 * 1000) / SCSI_LOW_POLL_HZ);

		(*slp->sl_funcs->scsi_low_poll) (slp);
		if (tcount ++ < SCSI_LOW_POLL_HZ / SCSI_LOW_TIMEOUT_HZ)
			continue;

		tcount = 0;
		scsi_low_timeout_check(slp);
	}

	return 0;
}
#endif	/* SCSI_LOW_START_UP_CHECK */

/**********************************************************
 * DEBUG SECTION
 **********************************************************/
#ifdef	SCSI_LOW_DEBUG
static void
scsi_low_test_abort(slp, ti, li)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	struct lun_info *li;
{
	struct slccb *acb;

	if (li->li_disc > 1)
	{
		acb = TAILQ_FIRST(&li->li_discq); 
		if (scsi_low_abort_ccb(slp, acb) == 0)
		{
			printf("%s: aborting ccb(0x%lx) start\n",
				slp->sl_xname, (u_long) acb);
		}
	}
}

static void
scsi_low_test_atten(slp, ti, msg)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_int msg;
{

	if (slp->sl_ph_count < SCSI_LOW_MAX_ATTEN_CHECK)
		scsi_low_assert_msg(slp, ti, msg, 0);
	else
		printf("%s: atten check OK\n", slp->sl_xname);
}

static void
scsi_low_test_cmdlnk(slp, cb)
	struct scsi_low_softc *slp;
	struct slccb *cb;
{
#define	SCSI_LOW_CMDLNK_NOK	(CCB_INTERNAL | CCB_SENSE | CCB_CLEARQ)

	if ((cb->ccb_flags & SCSI_LOW_CMDLNK_NOK) != 0)
		return;

	memcpy(cb->ccb_scsi_cmd, slp->sl_scp.scp_cmd,
	       slp->sl_scp.scp_cmdlen);
	cb->ccb_scsi_cmd[slp->sl_scp.scp_cmdlen - 1] |= 1;
	slp->sl_scp.scp_cmd = cb->ccb_scsi_cmd;
}
#endif	/* SCSI_LOW_DEBUG */

/* static */ void
scsi_low_info(slp, ti, s)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
	u_char *s;
{

	if (slp == NULL)
		slp = LIST_FIRST(&sl_tab);
	if (s == NULL)
		s = "no message";

	printf(">>>>> SCSI_LOW_INFO(0x%lx): %s\n", (u_long) slp->sl_Tnexus, s);
	if (ti == NULL)
	{
		for (ti = TAILQ_FIRST(&slp->sl_titab); ti != NULL;
		     ti = TAILQ_NEXT(ti, ti_chain))
		{
			scsi_low_print(slp, ti);
		}
	}
	else
	{
		scsi_low_print(slp, ti);
	}
}

static u_char *phase[] =
{
	"FREE", "ARBSTART", "SELSTART", "SELECTED",
	"CMDOUT", "DATA", "MSGIN", "MSGOUT", "STATIN", "DISC", "RESEL"
};

void
scsi_low_print(slp, ti)
	struct scsi_low_softc *slp;
	struct targ_info *ti;
{
	struct lun_info *li;
	struct slccb *cb;
	struct sc_p *sp;

	if (ti == NULL || ti == slp->sl_Tnexus)
	{
		ti = slp->sl_Tnexus;
		li = slp->sl_Lnexus;
		cb = slp->sl_Qnexus;
	}
	else
	{
		li = LIST_FIRST(&ti->ti_litab);
		cb = TAILQ_FIRST(&li->li_discq);
	}
 	sp = &slp->sl_scp;

	printf("%s: === NEXUS T(0x%lx) L(0x%lx) Q(0x%lx) NIO(%d) ===\n",
		slp->sl_xname, (u_long) ti, (u_long) li, (u_long) cb,
		slp->sl_nio);

	/* target stat */
	if (ti != NULL)
	{
		u_int flags = 0, maxnqio = 0, nqio = 0;
		int lun = -1;

		if (li != NULL)
		{
			lun = li->li_lun;
			flags = li->li_flags;
			maxnqio = li->li_maxnqio;
			nqio = li->li_nqio;
		}

		printf("%s(%d:%d) ph<%s> => ph<%s> DISC(%d) QIO(%d:%d)\n",
			slp->sl_xname,
		       ti->ti_id, lun, phase[(int) ti->ti_ophase], 
		       phase[(int) ti->ti_phase], ti->ti_disc,
		       nqio, maxnqio);

		if (cb != NULL)
		{
printf("CCB: cmd[0] 0x%x clen 0x%x dlen 0x%x<0x%x stat 0x%x err %b\n",
		       (u_int) cb->ccb_scp.scp_cmd[0],
		       cb->ccb_scp.scp_cmdlen, 
		       cb->ccb_datalen,
		       cb->ccb_scp.scp_datalen,
		       (u_int) cb->ccb_sscp.scp_status,
		       cb->ccb_error, SCSI_LOW_ERRORBITS);
		}

printf("MSGIN: ptr(%x) [%x][%x][%x][%x][%x] attention: %d\n",
	       (u_int) (ti->ti_msginptr), 
	       (u_int) (ti->ti_msgin[0]),
	       (u_int) (ti->ti_msgin[1]),
	       (u_int) (ti->ti_msgin[2]),
	       (u_int) (ti->ti_msgin[3]),
	       (u_int) (ti->ti_msgin[4]),
	       slp->sl_atten);

printf("MSGOUT: msgflags 0x%x [%x][%x][%x][%x][%x] msgoutlen %d C_FLAGS: %b\n",
		(u_int) ti->ti_msgflags,
		(u_int) (ti->ti_msgoutstr[0]), 
		(u_int) (ti->ti_msgoutstr[1]), 
		(u_int) (ti->ti_msgoutstr[2]), 
		(u_int) (ti->ti_msgoutstr[3]), 
		(u_int) (ti->ti_msgoutstr[4]), 
		ti->ti_msgoutlen,
		flags, SCSI_LOW_BITS);

#ifdef	SCSI_LOW_DIAGNOSTIC
		scsi_low_msg_log_show(&ti->ti_log_msgin, "MIN LOG ", 2);
		scsi_low_msg_log_show(&ti->ti_log_msgout, "MOUT LOG", 2);
#endif	/* SCSI_LOW_DIAGNOSTIC */

	}

	printf("SCB: daddr 0x%lx dlen 0x%x stat 0x%x err %b\n",
	       (u_long) sp->scp_data,
	       sp->scp_datalen,
	       (u_int) sp->scp_status,
	       slp->sl_error, SCSI_LOW_ERRORBITS);
}
