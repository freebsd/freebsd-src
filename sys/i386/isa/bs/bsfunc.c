/*	$NecBSD: bsfunc.c,v 1.2 1997/10/31 17:43:37 honda Exp $	*/
/*	$NetBSD$	*/
/* $FreeBSD$ */
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

#ifdef	__NetBSD__
#include <i386/Cbus/dev/bs/bsif.h>
#endif
#ifdef	__FreeBSD__
#include <i386/isa/bs/bsif.h>
#endif

#ifdef	BS_STATICS
struct bs_statics bs_statics[NTARGETS];
u_int bs_linkcmd_count[NTARGETS];
u_int bs_bounce_used[NTARGETS];
#endif	/* BS_STATICS */

#ifdef	BS_DEBUG
int bs_debug_flag = 0;
#endif	/* BS_DEBUG */

static void bs_print_syncmsg __P((struct targ_info *, char*));
static void bs_timeout_target __P((struct targ_info *));
static void bs_kill_msg __P((struct bsccb *cb));

static int bs_start_target __P((struct targ_info *));
static int bs_check_target __P((struct targ_info *));

/*************************************************************
 * CCB
 ************************************************************/
GENERIC_CCB_STATIC_ALLOC(bs, bsccb)
GENERIC_CCB(bs, bsccb, ccb_chain)

/*************************************************************
 * TIMEOUT
 ************************************************************/
static void
bs_timeout_target(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;

	ti->ti_error |= BSTIMEOUT;
	bsc->sc_flags |= BSRESET;

	if (ti->ti_herrcnt ++ >= HARDRETRIES)
	{
		bs_printf(ti, "timeout", "async transfer!");
		ti->ti_syncmax.period = ti->ti_syncmax.offset = 0;
	}
}

void
bstimeout(arg)
	void *arg;
{
	struct bs_softc *bsc = (struct bs_softc *) arg;
	struct targ_info *ti;
	struct bsccb *cb;
	int s;

	s = splcam();
	bsc->sc_flags &= ~BSSTARTTIMEOUT;

	/* check */
	if ((ti = bsc->sc_nexus) && (cb = ti->ti_ctab.tqh_first))
	{
		if ((cb->tc -= BS_TIMEOUT_CHECK_INTERVAL) < 0)
			bs_timeout_target(ti);
	}
	else for (ti = bsc->sc_titab.tqh_first; ti; ti = ti->ti_tchain.tqe_next)
	{
		if (bsc->sc_dtgnum && ti->ti_phase < DISCONNECTED)
			continue;

		cb = ti->ti_ctab.tqh_first;
		if (cb && ((cb->tc -= BS_TIMEOUT_CHECK_INTERVAL) < 0))
			bs_timeout_target(ti);
	}

	/* try to recover */
	if (bsc->sc_flags & BSRESET)
	{
		bs_debug_print_all(bsc);
		bs_printf(ti, "timeout", "bus hang up");
		bs_reset_nexus(bsc);
	}

	bs_start_timeout(bsc);
	splx(s);
}

/**************************************************
 * MAKE CCB & MSG CCB
 *************************************************/
static u_int8_t cmd_unit_ready[6];

struct bsccb *
bs_make_internal_ccb(ti, lun, cmd, cmdlen, data, datalen, flags, timeout)
	struct targ_info *ti;
	u_int lun;
	u_int8_t *cmd;
	u_int cmdlen;
	u_int8_t *data;
	u_int datalen;
	u_int flags;
	int timeout;
{
	struct bsccb *cb;

	if ((cb = bs_get_ccb()) == NULL)
		bs_panic(ti->ti_bsc, "can not get ccb mem");

	cb->ccb = NULL;
	cb->lun = lun;
	cb->cmd = (cmd ? cmd : cmd_unit_ready);
	cb->cmdlen = (cmd ? cmdlen : sizeof(cmd_unit_ready));
	cb->data = data;
	cb->datalen = (data ? datalen : 0);
	cb->msgoutlen = 0;
	cb->bsccb_flags = flags & BSCFLAGSMASK;
	bs_targ_flags(ti, cb);
	cb->rcnt = 0;
	cb->tcmax = (timeout > BS_DEFAULT_TIMEOUT_SECOND ? timeout :
				BS_DEFAULT_TIMEOUT_SECOND);

	TAILQ_INSERT_HEAD(&ti->ti_ctab, cb, ccb_chain);

	return cb;
}

struct bsccb *
bs_make_msg_ccb(ti, lun, cb, msg, timex)
	struct targ_info *ti;
	u_int lun;
	struct bsccb *cb;
	struct msgbase *msg;
	u_int timex;
{
	u_int flags;

	flags = BSFORCEIOPOLL | msg->flag;
	if (cb == NULL)
		cb = bs_make_internal_ccb(ti, lun, NULL, 0, NULL, 0,
					   flags, timex);
	else
		cb->bsccb_flags |= flags & BSCFLAGSMASK;

	cb->msgoutlen = msg->msglen;
	bcopy(msg->msg, cb->msgout, msg->msglen);
	return cb;
}

int
bs_send_msg(ti, lun, msg, timex)
	struct targ_info *ti;
	u_int lun;
	struct msgbase *msg;
	int timex;
{
	struct bsccb *cb;

	cb = bs_make_msg_ccb(ti, lun, NULL, msg, timex);
	bscmdstart(ti, BSCMDSTART);
	return bs_scsi_cmd_poll(ti, cb);
}

static void
bs_kill_msg(cb)
	struct bsccb *cb;
{
	cb->msgoutlen = 0;
}

/**************************************************
 * MAKE SENSE CCB
 **************************************************/
struct bsccb *
bs_request_sense(ti)
	struct targ_info *ti;
{
	struct bsccb *cb;

	bzero(ti->scsi_cmd, sizeof(struct scsi_sense));
	bzero(&ti->sense, sizeof(struct scsi_sense_data));
	ti->scsi_cmd[0] = REQUEST_SENSE;
	ti->scsi_cmd[1] = (ti->ti_lun << 5);
	ti->scsi_cmd[4] = sizeof(struct scsi_sense_data);
	cb = bs_make_internal_ccb(ti, ti->ti_lun, ti->scsi_cmd,
				     sizeof(struct scsi_sense),
				     (u_int8_t *) & ti->sense,
				     sizeof(struct scsi_sense_data),
				     BSFORCEIOPOLL,
				     BS_DEFAULT_TIMEOUT_SECOND);
	cb->bsccb_flags |= BSSENSECCB;
	return cb;
}

/**************************************************
 * SYNC MSG
 *************************************************/
/* sync neg */
int
bs_start_syncmsg(ti, cb, flag)
	struct targ_info *ti;
	struct bsccb *cb;
	int flag;
{
	struct syncdata *negp, *maxp;
	struct msgbase msg;
	u_int lun;

	negp = &ti->ti_syncnow;
	maxp = &ti->ti_syncmax;

	ti->ti_state = BS_TARG_SYNCH;

	if (flag == BS_SYNCMSG_REQUESTED)
	{
		if (negp->offset > maxp->offset)
			negp->offset = maxp->offset;
		if (negp->offset != 0 && negp->period < maxp->period)
			negp->period = maxp->period;

		msg.flag = 0;
		lun = ti->ti_lun;
		if (cb == NULL)
			cb = ti->ti_ctab.tqh_first;
	}
	else if (ti->ti_cfgflags & BS_SCSI_SYNC)
	{
		negp->offset = maxp->offset;
		negp->period = maxp->period;

		msg.flag = BSERROROK;
		lun = 0;
	}
	else
	{
		ti->ti_state = BS_TARG_RDY;
		return COMPLETE;
	}

	BS_SETUP_SYNCSTATE(flag);
	msg.msg[0] = MSG_EXTEND;
	msg.msg[1] = MSG_EXTEND_SYNCHLEN;
	msg.msg[2] = MSG_EXTEND_SYNCHCODE;
	msg.msg[3] = negp->period;
	msg.msg[4] = negp->offset;
	msg.msglen = MSG_EXTEND_SYNCHLEN + 2;

	bs_make_msg_ccb(ti, lun, cb, &msg, BS_SYNC_TIMEOUT);
	return COMPLETE;
}

static void
bs_print_syncmsg(ti, s)
	struct targ_info *ti;
	char *s;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct syncdata *negp;
	u_int speed;

	negp = &ti->ti_syncnow;
	speed = (negp->offset && negp->period) ?
		(2500 / ((u_int) negp->period)) : 0;

	printf("%s(%d:%d): <%s> ", bsc->sc_dvname, ti->ti_id, ti->ti_lun, s);
	printf("period 0x%x offset %d chip (0x%x)", negp->period, negp->offset,
		ti->ti_sync);
	if (speed)
		printf(" %d.%d M/s", speed / 10, speed % 10);
	printf("\n");
}

int
bs_analyze_syncmsg(ti, cb)
	struct targ_info *ti;
	struct bsccb *cb;
{
	struct bs_softc *bsc = ti->ti_bsc;
	u_int8_t ans = ti->ti_syncnow.state;
	struct syncdata *negp, *maxp;
	struct syncdata bdata;
	char *s = NULL;
	u_int8_t period;

	negp = &ti->ti_syncnow;
	bdata = *negp;
	maxp = &ti->ti_syncmax;

	switch(ans)
	{
	case BS_SYNCMSG_REJECT:
		period = 0;
		s = "msg reject";
		break;

	case BS_SYNCMSG_ASSERT:
		period = 0;
		s = "no msg";
		break;

	default:
		if (negp->offset != 0 && negp->period < maxp->period)
		{
			period = 0xff;
			s = "illegal(period)";
		}
		else if (negp->offset > maxp->offset)
		{
			period = 0xff;
			s = "illegal(offset)";
		}
		else
			period = negp->offset ? negp->period : 0;
		break;
	}

	if (s == NULL)
	{
		bshw_adj_syncdata(negp);
		*maxp = *negp;

		if (ans == BS_SYNCMSG_REQUESTED)
			s = "requested";
		else
			s = negp->offset ? "synchronous" : "async";
	}
	else
	{
		negp->offset = maxp->offset = 0;
		bshw_adj_syncdata(negp);
		bshw_adj_syncdata(maxp);
	}

	/* really setup hardware */
	bshw_set_synchronous(bsc, ti);
	if (cb == NULL || (period >= negp->period && period <= negp->period + 2))
	{
		bs_print_syncmsg(ti, s);
		BS_SETUP_TARGSTATE(BS_TARG_RDY);
		BS_SETUP_SYNCSTATE(BS_SYNCMSG_NULL);
		if (cb)
			bs_kill_msg(cb);

		return 0;
	}
	else
	{
		bs_printf(ti, "bs_analyze_syncmsg",
			  "sync(period) mismatch, retry neg...");
		printf("expect(%d:0x%x) => reply(%d:0x%x)\n",
			bdata.offset, bdata.period, negp->offset, negp->period);

		bs_start_syncmsg(ti, cb, BS_SYNCMSG_ASSERT);
		return EINVAL;
	}
}

/**************************************************
 * ABORT AND RESET MSG
 **************************************************/
/* send device reset msg and wait */
void
bs_reset_device(ti)
	struct targ_info *ti;
{
	struct msgbase msg;

	msg.msglen = 1;
	msg.msg[0] = MSG_RESET;
	msg.flag = 0;

	bs_send_msg(ti, 0, &msg, 0);

	delay(ti->ti_bsc->sc_RSTdelay);
	bs_check_target(ti);
}

/* send abort msg */
struct bsccb *
bs_force_abort(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct msgbase msg;
	struct bsccb *cb = ti->ti_ctab.tqh_first;
	u_int lun;

	if (cb)
	{
		lun = cb->lun;
		cb->rcnt++;
	}
	else
		lun = 0;

	msg.msglen = 1;
	msg.msg[0] = MSG_ABORT;
	msg.flag = 0;

	cb = bs_make_msg_ccb(ti, lun, NULL, &msg, 0);
	bscmdstart(ti, BSCMDSTART);

	if (bsc->sc_nexus == ti)
		BS_LOAD_SDP

	return cb;
}

/**************************************************
 * COMPLETE SCSI BUS RESET
 *************************************************/
/*
 * XXX:
 * 1) reset scsi bus (ie. all target reseted).
 * 2) chip reset.
 * 3) check target status.
 * 4) sync neg with all targets.
 * 5) setup sync reg in host.
 * 6) recover previous nexus.
 */
void
bs_scsibus_start(bsc)
	struct bs_softc *bsc;
{
	struct targ_info *ti, *nextti = NULL;
	int error = HASERROR;
	u_int querm, bits, skip = 0;

	querm = (bsc->sc_hstate == BSC_BOOTUP);
	bsc->sc_hstate = BSC_TARG_CHECK;

	/* target check */
	do
	{
		if (error != COMPLETE)
		{
			printf("%s: scsi bus reset and try to restart ...",
			       bsc->sc_dvname);
			bshw_smitabort(bsc);
			bshw_dmaabort(bsc, NULL);
			bshw_chip_reset(bsc);
			bshw_bus_reset(bsc);
			bshw_chip_reset(bsc);
			printf(" done. scsi bus ready.\n");
			nextti = bsc->sc_titab.tqh_first;
			error = COMPLETE;
		}

		if ((ti = nextti) == NULL)
			break;
		nextti = ti->ti_tchain.tqe_next;

		bits = (1 << ti->ti_id);
		if (skip & bits)
			continue;

		if ((error = bs_check_target(ti)) != COMPLETE)
		{
			if (querm)
			{
				TAILQ_REMOVE(&bsc->sc_titab, ti, ti_tchain);
				bsc->sc_openf &= ~bits;
			}

			if (error == NOTARGET)
				error = COMPLETE;

			skip |= bits;
		}
	}
	while (1);

	/* ok now ready */
	bsc->sc_hstate = BSC_RDY;

	/* recover */
	for (ti = bsc->sc_titab.tqh_first; ti; ti = ti->ti_tchain.tqe_next)
	{
		ti->ti_ctab = ti->ti_bctab;
		TAILQ_INIT(&ti->ti_bctab);
		if (ti->ti_ctab.tqh_first)
			bscmdstart(ti, BSCMDSTART);
	}
}

void
bs_reset_nexus(bsc)
	struct bs_softc *bsc;
{
	struct targ_info *ti;
	struct bsccb *cb;

	bsc->sc_flags &= ~(BSRESET | BSUNDERRESET);
	if (bsc->sc_poll)
	{
		bsc->sc_flags |= BSUNDERRESET;
		return;
	}

	/* host state clear */
	BS_HOST_TERMINATE
	BS_SETUP_MSGPHASE(FREE)
	bsc->sc_dtgnum = 0;

	/* target state clear */
	for (ti = bsc->sc_titab.tqh_first; ti; ti = ti->ti_tchain.tqe_next)
	{
		if (ti->ti_state == BS_TARG_SYNCH)
			bs_analyze_syncmsg(ti, NULL);
		if (ti->ti_state > BS_TARG_START)
			BS_SETUP_TARGSTATE(BS_TARG_START);

		BS_SETUP_PHASE(UNDEF)
		bs_hostque_delete(bsc, ti);
		if ((cb = ti->ti_ctab.tqh_first) != NULL)
		{
			if (bsc->sc_hstate == BSC_TARG_CHECK)
			{
				ti->ti_error |= BSFATALIO;
				bscmddone(ti);
			}
			else if (cb->rcnt >= bsc->sc_retry)
			{
				ti->ti_error |= BSABNORMAL;
				bscmddone(ti);
			}
			else if (ti->ti_error)
				cb->rcnt++;
		}

		/* target state clear */
		BS_SETUP_PHASE(FREE)
		BS_SETUP_SYNCSTATE(BS_SYNCMSG_NULL);
		ti->ti_flags &= ~BSCFLAGSMASK;
		ti->ti_msgout = 0;
#ifdef	BS_DIAG
		ti->ti_flags &= ~BSNEXUS;
#endif	/* BS_DIAG */

		for ( ; cb; cb = cb->ccb_chain.tqe_next)
		{
			bs_kill_msg(cb);
			cb->bsccb_flags &= ~(BSITSDONE | BSCASTAT);
			cb->error = 0;
		}

		if (bsc->sc_hstate != BSC_TARG_CHECK &&
		    ti->ti_bctab.tqh_first == NULL)
			ti->ti_bctab = ti->ti_ctab;

		TAILQ_INIT(&ti->ti_ctab);
	}

	if (bsc->sc_hstate != BSC_TARG_CHECK)
		bs_scsibus_start(bsc);
}

/**************************************************
 * CHECK TARGETS AND START TARGETS
 *************************************************/
static int
bs_start_target(ti)
	struct targ_info *ti;
{
	struct bsccb *cb;
	struct scsi_start_stop_unit cmd;

	bzero(&cmd, sizeof(struct scsi_start_stop_unit));
	cmd.opcode = START_STOP;
	cmd.how = SSS_START;
	ti->ti_lun = 0;
	cb = bs_make_internal_ccb(ti, 0, (u_int8_t *) &cmd,
				   sizeof(struct scsi_start_stop_unit),
				   NULL, 0, BSFORCEIOPOLL, BS_MOTOR_TIMEOUT);
	bscmdstart(ti, BSCMDSTART);
	return bs_scsi_cmd_poll(ti, cb);
}

/* test unit ready and check ATN msgout response */
static int
bs_check_target(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct scsi_inquiry scsi_cmd;
	struct scsi_inquiry_data scsi_inquiry_data;
	struct bsccb *cb;
	int count, retry = bsc->sc_retry;
	int s, error = COMPLETE;

	ti->ti_lun = 0;
	bsc->sc_retry = 2;
	s = splcam();

	/* inquiry */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = INQUIRY;
	scsi_cmd.length = (u_int8_t) sizeof(struct scsi_inquiry_data);
	cb = bs_make_internal_ccb(ti, 0,
				   (u_int8_t *) &scsi_cmd, sizeof(scsi_cmd),
				   (u_int8_t *) &scsi_inquiry_data,
				   sizeof(scsi_inquiry_data),
				   BSFORCEIOPOLL, BS_STARTUP_TIMEOUT);
	bscmdstart(ti, BSCMDSTART);
	error = bs_scsi_cmd_poll(ti, cb);
	if (error != COMPLETE || (ti->ti_error & BSSELTIMEOUT))
		goto done;
	ti->targ_type = scsi_inquiry_data.device;
	ti->targ_support = scsi_inquiry_data.flags;

	/* test unit ready twice */
	for (count = 0; count < 2; count++)
	{
		cb = bs_make_internal_ccb(ti, 0, NULL, 0, NULL, 0,
					 BSFORCEIOPOLL, BS_STARTUP_TIMEOUT);
		bscmdstart(ti, BSCMDSTART);
		error = bs_scsi_cmd_poll(ti, cb);
		if (error != COMPLETE || (ti->ti_error & BSSELTIMEOUT))
			goto done;
	}

	if (cb->bsccb_flags & BSCASTAT)
		bs_printf(ti, "check", "could not clear CA state");
	ti->ti_error = 0;

done:
	bsc->sc_retry = retry;

	if (ti->ti_error & BSSELTIMEOUT)
		error = NOTARGET;

	if (error == COMPLETE)
		error = bs_start_target(ti);

	splx(s);
	return error;
}

/**************************************************
 * TARGET CONTROL
 **************************************************/
struct targ_info *
bs_init_target_info(bsc, target)
	struct bs_softc *bsc;
	int target;
{
	struct targ_info *ti;

	ti = malloc(sizeof(struct targ_info), M_DEVBUF, M_NOWAIT);
	if (ti == NULL)
	{
		bs_printf(NULL, "bs_init_targ_info", "no target info memory");
		return ti;
	}

	bzero(ti, sizeof(*ti));

	ti->ti_bsc = bsc;
	ti->ti_id = target;
	ti->sm_offset = 0;
	ti->ti_cfgflags = BS_SCSI_NOPARITY | BS_SCSI_NOSAT;
	ti->ti_mflags = ~(BSSAT | BSDISC | BSSMIT | BSLINK);
	BS_SETUP_TARGSTATE(BS_TARG_CTRL);

	TAILQ_INIT(&ti->ti_ctab);

	bs_alloc_buf(ti);
	if (ti->bounce_addr == NULL)
	{
		free(ti, M_DEVBUF);
		return NULL;
	}

	TAILQ_INSERT_TAIL(&bsc->sc_titab, ti, ti_tchain);
	bsc->sc_ti[target] = ti;
	bsc->sc_openf |= (1 << target);

	return ti;
}

void
bs_setup_ctrl(ti, quirks, flags)
	struct targ_info *ti;
	u_int quirks;
	u_int flags;
{
	struct bs_softc *bsc = ti->ti_bsc;
	u_int offset, period, maxperiod;

	if (ti->ti_state == BS_TARG_CTRL)
	{
		ti->ti_cfgflags = BS_SCSI_POSITIVE;
		ti->ti_syncmax.offset = BSHW_MAX_OFFSET;
		BS_SETUP_TARGSTATE(BS_TARG_START);
	}
	else
		flags |= ti->ti_cfgflags & BS_SCSI_NEGATIVE;

#ifdef	BS_TARG_SAFEMODE
	if (ti->targ_type != 0)
	{
		flags &= ~(BS_SCSI_DISC | BS_SCSI_SYNC);
		flags |= BS_SCSI_NOPARITY;
	}
#endif

#ifdef	SDEV_NODISC
	if (quirks & SDEV_NODISC)
		flags &= ~BS_SCSI_DISC;
#endif
#ifdef	SDEV_NOPARITY
	if (quirks & SDEV_NOPARITY)
		flags |= BS_SCSI_NOPARITY;
#endif
#ifdef	SDEV_NOCMDLNK
	if (quirks & SDEV_NOCMDLNK)
		flags &= ~BS_SCSI_LINK;
#endif
#ifdef	SDEV_ASYNC
	if (quirks & SDEV_ASYNC)
		flags &= ~BS_SCSI_SYNC;
#endif
#ifdef	SDEV_AUTOSAVE
	if (quirks & SDEV_AUTOSAVE)
		flags |= BS_SCSI_SAVESP;
#endif
#ifdef	SD_Q_NO_SYNC
	if (quirks & SD_Q_NO_SYNC)
		flags &= ~BS_SCSI_SYNC;
#endif

	if ((flags & BS_SCSI_DISC) == 0 ||
	    (ti->targ_support & SID_Linked) == 0)
		flags &= ~BS_SCSI_LINK;

	ti->sm_offset = (flags & BS_SCSI_NOSMIT) ?  0 : bsc->sm_offset;
	if (ti->sm_offset == 0)
		flags |= BS_SCSI_NOSMIT;
	else if (bsc->sc_cfgflags & BSC_SMITSAT_DISEN)
		flags |= BS_SCSI_NOSAT;

	flags &= (ti->ti_cfgflags & BS_SCSI_POSITIVE) | (~BS_SCSI_POSITIVE);
	ti->ti_cfgflags = flags;

	/* calculate synch setup */
	period = BS_SCSI_PERIOD(flags);
	offset = (flags & BS_SCSI_SYNC) ? BS_SCSI_OFFSET(flags) : 0;

	maxperiod = (bsc->sc_cspeed & IDR_FS_16_20) ? 100 : 50;
	if (period > maxperiod)
		period = maxperiod;

	if (period)
		period = 2500 / period;

	if (ti->ti_syncmax.offset > offset)
		ti->ti_syncmax.offset = offset;
	if (ti->ti_syncmax.period < period)
		ti->ti_syncmax.period = period;

	bshw_adj_syncdata(&ti->ti_syncmax);

	/* finally report our info */
	printf("%s(%d:%d): {%d:0x%x:0x%x:%s} flags 0x%b\n",
		bsc->sc_dvname, ti->ti_id, ti->ti_lun,
	       (u_int) ti->targ_type,
	       (u_int) ti->targ_support,
	       (u_int) ti->bounce_size,
	       (flags & BS_SCSI_NOSMIT) ? "dma" : "pdma",
		flags, BS_SCSI_BITS);

	/* internal representation */
	ti->ti_mflags = ~0;
	if ((ti->ti_cfgflags & BS_SCSI_DISC) == 0)
		ti->ti_mflags &= ~BSDISC;
	if ((ti->ti_cfgflags & BS_SCSI_LINK) == 0)
		ti->ti_mflags &= ~BSLINK;
	if (ti->ti_cfgflags & BS_SCSI_NOSAT)
		ti->ti_mflags &= ~BSSAT;
	if (ti->ti_cfgflags & BS_SCSI_NOSMIT)
		ti->ti_mflags &= ~BSSMIT;
}

/**************************************************
 * MISC
 **************************************************/
void
bs_printf(ti, ph, c)
	struct targ_info *ti;
	char *ph;
	char *c;
{

	if (ti)
		printf("%s(%d:%d): <%s> %s\n",
		       ti->ti_bsc->sc_dvname, ti->ti_id, ti->ti_lun, ph, c);
	else
		printf("bs*(*:*): <%s> %s\n", ph, c);
}

void
bs_panic(bsc, c)
	struct bs_softc *bsc;
	u_char *c;
{

	panic("%s %s\n", bsc->sc_dvname, c);
}

/**************************************************
 * DEBUG FUNC
 **************************************************/
#ifdef	BS_DEBUG_ROUTINE
u_int
bsr(addr)
	u_int addr;
{

	outb(0xcc0, addr);
	return inb(0xcc2);
}

u_int
bsw(addr, data)
	u_int addr;
	int data;
{

	outb(0xcc0, addr);
	outb(0xcc2, data);
	return 0;
}
#endif	/* BS_DEBUG_ROUTINE */

void
bs_debug_print_all(bsc)
	struct bs_softc *bsc;
{
	struct targ_info *ti;

	for (ti = bsc->sc_titab.tqh_first; ti; ti = ti->ti_tchain.tqe_next)
		bs_debug_print(bsc, ti);
}

static u_char *phase[] =
{
	"FREE", "HOSTQUE", "DISC", "COMPMSG", "ATN", "DISCMSG", "SELECT",
	"SELECTED", "RESELECTED", "MSGIN", "MSGOUT", "STATIN", "CMDOUT",
	"DATA", "SATSEL", "SATRESEL", "SATSDP", "SATCOMPSEQ", "UNDEF",
};

void
bs_debug_print(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{
	struct bsccb *cb;

	/* host stat */
	printf("%s <DEBUG INFO> nexus %lx bs %lx bus status %lx \n",
	       bsc->sc_dvname, (u_long) ti, (u_long) bsc->sc_nexus, (u_long) bsc->sc_busstat);

	/* target stat */
	if (ti)
	{
		struct sc_p *sp = &bsc->sc_p;

		printf("%s(%d:%d) ph<%s> ", bsc->sc_dvname, ti->ti_id,
		       ti->ti_lun, phase[(int) ti->ti_phase]);
		printf("msgptr %x msg[0] %x status %x tqh %lx fl %x\n",
		       (u_int) (ti->ti_msginptr), (u_int) (ti->ti_msgin[0]),
		       ti->ti_status, (u_long) (cb = ti->ti_ctab.tqh_first),
		       ti->ti_flags);
		if (cb)
			printf("cmdlen %x cmdaddr %lx cmd[0] %x\n",
			       cb->cmdlen, (u_long) cb->cmd, (int) cb->cmd[0]);
		printf("datalen %x dataaddr %lx seglen %x ",
		       sp->datalen, (u_long) sp->data, sp->seglen);
		if (cb)
			printf("odatalen %x flags %x\n",
				cb->datalen, cb->bsccb_flags);
		else
			printf("\n");
		printf("error flags %b\n", ti->ti_error, BSERRORBITS);
	}
}
