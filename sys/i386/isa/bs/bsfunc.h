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
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

/**************************************************
 * FUNC
 **************************************************/
/* timeout */
void bstimeout __P((void *));

/* ctrl setup */
void bs_setup_ctrl __P((struct targ_info *, u_int, u_int));
struct targ_info *bs_init_target_info __P((struct bs_softc *, int));

/* msg op */
int bs_send_msg __P((struct targ_info *, u_int, struct msgbase *, int));
struct ccb *bs_request_sense __P((struct targ_info *));

/* sync msg op */
int bs_start_syncmsg __P((struct targ_info *, struct ccb *, int));
int bs_send_syncmsg __P((struct targ_info *));
int bs_analyze_syncmsg __P((struct targ_info *, struct ccb *));

/* reset device */
void bs_scsibus_start __P((struct bs_softc *));
void bs_reset_nexus __P((struct bs_softc *));
struct ccb *bs_force_abort __P((struct targ_info *));
void bs_reset_device __P((struct targ_info *));

/* ccb */
struct ccb *bs_make_internal_ccb __P((struct targ_info *, u_int, u_int8_t *, u_int, u_int8_t *, u_int, u_int, int));
struct ccb *bs_make_msg_ccb __P((struct targ_info *, u_int, struct ccb *, struct msgbase *, u_int));

/* misc funcs */
void bs_printf __P((struct targ_info *, char *, char *));
void bs_panic __P((struct bs_softc *, u_char *));

/* misc debug */
u_int bsr __P((u_int));
u_int bsw __P((u_int, int));
void bs_debug_print_all __P((struct bs_softc *));
void bs_debug_print __P((struct bs_softc *, struct targ_info *));

/**************************************************
 * TARG FLAGS
 *************************************************/
static BS_INLINE int bs_check_sat __P((struct targ_info *));
static BS_INLINE int bs_check_smit __P((struct targ_info *));
static BS_INLINE int bs_check_disc __P((struct targ_info *));
static BS_INLINE int bs_check_link __P((struct targ_info *, struct ccb *));
static BS_INLINE u_int8_t bs_identify_msg __P((struct targ_info *));
static BS_INLINE void bs_targ_flags __P((struct targ_info *, struct ccb *));

static BS_INLINE int
bs_check_disc(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSDISC);
}

static BS_INLINE int
bs_check_sat(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSSAT);
}

static BS_INLINE int
bs_check_smit(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSSMIT);
}

static BS_INLINE int
bs_check_link(ti, cb)
	struct targ_info *ti;
	struct ccb *cb;
{
	struct ccb *nextcb;

	return ((ti->ti_flags & BSLINK) &&
		(nextcb = cb->ccb_chain.tqe_next) &&
		(nextcb->flags & BSLINK));
}

static BS_INLINE u_int8_t
bs_identify_msg(ti)
	struct targ_info *ti;
{

	return ((bs_check_disc(ti) ? 0xc0 : 0x80) | ti->ti_lun);
}

static BS_INLINE void
bs_targ_flags(ti, cb)
	struct targ_info *ti;
	struct ccb *cb;
{
	u_int cmf = (u_int) bshw_cmd[cb->cmd[0]];

	cb->flags |= ((cmf & (BSSAT | BSSMIT | BSLINK)) | BSDISC);
	cb->flags &= ti->ti_mflags;

	if (cb->datalen < DEV_BSIZE)
		cb->flags &= ~BSSMIT;
	if (cb->flags & BSFORCEIOPOLL)
		cb->flags &= ~(BSLINK | BSSMIT | BSSAT | BSDISC);
}

/**************************************************
 * QUEUE OP
 **************************************************/
static BS_INLINE void bs_hostque_init __P((struct bs_softc *));
static BS_INLINE void bs_hostque_head __P((struct bs_softc *, struct targ_info *));
static BS_INLINE void bs_hostque_tail __P((struct bs_softc *, struct targ_info *));
static BS_INLINE void bs_hostque_delete __P((struct bs_softc *, struct targ_info *));

static BS_INLINE void
bs_hostque_init(bsc)
	struct bs_softc *bsc;
{

	TAILQ_INIT(&bsc->sc_sttab);
	TAILQ_INIT(&bsc->sc_titab);
}

static BS_INLINE void
bs_hostque_head(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED)
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain)
	else
		ti->ti_flags |= BSQUEUED;

	TAILQ_INSERT_HEAD(&bsc->sc_sttab, ti, ti_wchain);
}

static BS_INLINE void
bs_hostque_tail(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED)
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain)
	else
		ti->ti_flags |= BSQUEUED;

	TAILQ_INSERT_TAIL(&bsc->sc_sttab, ti, ti_wchain)
}

static BS_INLINE void
bs_hostque_delete(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED)
	{
		ti->ti_flags &= ~BSQUEUED;
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain)
	}
}

/*************************************************************
 * TIMEOUT
 ************************************************************/
static BS_INLINE void bs_start_timeout __P((struct bs_softc *));
static BS_INLINE void bs_terminate_timeout __P((struct bs_softc *));

static BS_INLINE void
bs_start_timeout(bsc)
	struct bs_softc *bsc;
{

	if ((bsc->sc_flags & BSSTARTTIMEOUT) == 0)
	{
		bsc->sc_flags |= BSSTARTTIMEOUT;
		timeout(bstimeout, bsc, BS_TIMEOUT_INTERVAL);
	}
}

static BS_INLINE void
bs_terminate_timeout(bsc)
	struct bs_softc *bsc;
{

	if (bsc->sc_flags & BSSTARTTIMEOUT)
	{
		untimeout(bstimeout, bsc);
		bsc->sc_flags &= ~BSSTARTTIMEOUT;
	}
}
