/* $FreeBSD$ */
/*	$NecBSD: ct.c,v 1.13 1999/07/23 20:54:00 honda Exp $	*/
/*	$NetBSD$	*/

#define	CT_DEBUG
#define	CT_USE_CCSEQ

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 * Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/disklabel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device_port.h>
#include <sys/errno.h>

#include <vm/vm.h>

#ifdef __NetBSD__
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <i386/Cbus/dev/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <i386/Cbus/dev/ct/ctvar.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/bus.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ct/ctvar.h>
#endif /* __FreeBSD__ */

/***************************************************
 * DEBUG
 ***************************************************/
#define	CT_NTARGETS	8
#define	CT_NLUNS	8
#define	CT_RESET_DEFAULT	2000

#ifndef DDB
#define Debugger() panic("should call debugger here (ct.c)")
#else /* ! DDB */
#ifdef __FreeBSD__
#define Debugger() Debugger("ct")
#endif /* __FreeBSD__ */
#endif

#ifdef	CT_DEBUG
int ct_debug;
#endif	/* CT_DEBUG */

/***************************************************
 * default data
 ***************************************************/
u_int8_t cthw_cmdlevel[256] = {
/*   0  1   2   3   4   5   6   7   8   9   A   B   C   E   D   F */
/*0*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,1  ,0  ,1  ,0  ,0  ,0  ,0  ,0  ,
/*1*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*2*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,1  ,0  ,1  ,0  ,0  ,0  ,0  ,0  ,
/*3*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*4*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*5*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*6*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*7*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*8*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*9*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*A*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*B*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*C*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*D*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*E*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*F*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
};

/* default synch data table */
/*  A  10    6.6   5.0   4.0   3.3   2.8   2.5   2.0  M/s */
/*  X  100   150   200   250   300   350   400   500  ns  */
static struct ct_synch_data ct_synch_data_20MHz[] = {
	{25, 0xa0}, {37, 0xb0}, {50, 0x20}, {62, 0xd0}, {75, 0x30},
	{87, 0xf0}, {100, 0x40}, {125, 0x50}, {0, 0}
};

extern unsigned int delaycount;

/*****************************************************************
 * Interface functions
 *****************************************************************/
static int ct_xfer __P((struct ct_softc *, u_int8_t *, int, int));
static void ct_io_xfer __P((struct ct_softc *));
static __inline int ct_reselected __P((struct ct_softc *));
static void ct_phase_error __P((struct ct_softc *, u_int8_t));
static int ct_start_selection __P((struct ct_softc *, struct slccb *));
static int ct_msg __P((struct ct_softc *, struct targ_info *, u_int));
static int ct_world_start __P((struct ct_softc *, int));
static __inline void cthw_phase_bypass __P((struct ct_softc *, u_int8_t));
static int cthw_chip_reset __P((bus_space_tag_t, bus_space_handle_t, int, int));
static void cthw_bus_reset __P((struct ct_softc *));
static int ct_nexus __P((struct ct_softc *, struct targ_info *));
static void cthw_attention __P((struct ct_softc *));
static int ct_targ_init __P((struct ct_softc *, struct targ_info *));

struct scsi_low_funcs ct_funcs = {
	SC_LOW_INIT_T ct_world_start,
	SC_LOW_BUSRST_T cthw_bus_reset,
	SC_LOW_TARG_INIT_T ct_targ_init,

	SC_LOW_SELECT_T ct_start_selection,
	SC_LOW_NEXUS_T ct_nexus,

	SC_LOW_ATTEN_T cthw_attention,
	SC_LOW_MSG_T ct_msg,

	SC_LOW_POLL_T ctintr,

	NULL,	/* SC_LOW_POWER_T cthw_power, */
};

/**************************************************
 * HW functions
 **************************************************/
static __inline void
cthw_phase_bypass(ct, ph)
	struct ct_softc *ct;
	u_int8_t ph;
{
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;

	ct_cr_write_1(bst, bsh, wd3s_cph, ph);
	ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_SELECT_ATN_TFR);
	ct->sc_satgo = CT_SAT_GOING;
}

static void
cthw_bus_reset(ct)
	struct ct_softc *ct;
{

	/*
	 * wd33c93 does not have bus reset function.
	 */
	if (ct->ct_bus_reset != NULL)
		((*ct->ct_bus_reset) (ct));
}

static int
cthw_chip_reset(bst, bsh, chipclk, hostid)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int chipclk, hostid;
{
#define	CT_SELTIMEOUT_20MHz_REGV	(0x80)
	u_int8_t aux, regv;
	u_int seltout;
	int wc;

	/* issue abort cmd */
	ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_ABORT);
	delay(1000);	/* 1ms wait */
	(void) ct_stat_read_1(bst, bsh);
	(void) ct_cr_read_1(bst, bsh, wd3s_stat);

	/* setup chip registers */
	regv = 0;
	seltout = CT_SELTIMEOUT_20MHz_REGV;
	switch (chipclk)
	{
	case 10:
		seltout = (seltout * chipclk) / 20;
		regv = 0;
		break;

	case 15:
		seltout = (seltout * chipclk) / 20;
		regv = IDR_FS_12_15;
		break;

	case 20:
		seltout = (seltout * chipclk) / 20;
		regv = IDR_FS_16_20;
		break;

	default:
		panic("ct: illegal chip clk rate\n");
		break;
	}
	regv |= IDR_EHP | hostid;
	ct_cr_write_1(bst, bsh, wd3s_oid, regv);

	ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_RESET);
	for (wc = CT_RESET_DEFAULT; wc > 0; wc --)
	{
		aux = ct_stat_read_1(bst, bsh);
		if (aux != 0xff && (aux & STR_INT))
		{
			if (ct_cr_read_1(bst, bsh, wd3s_stat) == 0)
				break;

			ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_RESET);
		}
		delay(1);
	}
	if (wc == 0)
		return ENXIO;

	ct_cr_write_1(bst, bsh, wd3s_tout, seltout);
	ct_cr_write_1(bst, bsh, wd3s_sid, SIDR_RESEL);
	ct_cr_write_1(bst, bsh, wd3s_ctrl, CR_DEFAULT);
	ct_cr_write_1(bst, bsh, wd3s_synch, 0);

	(void) ct_stat_read_1(bst, bsh);
	(void) ct_cr_read_1(bst, bsh, wd3s_stat);
	return 0;
}

/**************************************************
 * Attach & Probe
 **************************************************/
int
ctprobesubr(bst, bsh, dvcfg, hsid, chipclk)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int dvcfg, chipclk;
	int hsid;
{

#if	0
	if ((ct_stat_read_1(bst, bsh) & STR_BUSY) != 0)
		return 0;
#endif
	if (cthw_chip_reset(bst, bsh, chipclk, hsid) != 0)
		return 0;
	return 1;
}

int
ctprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
ctattachsubr(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;

	ct->sc_wc = delaycount * 2000;	/* 2 sec */
	slp->sl_funcs = &ct_funcs;
	(void) scsi_low_attach(slp, 2, CT_NTARGETS, CT_NLUNS,
			       sizeof(struct ct_targ_info));
}

/**************************************************
 * SCSI LOW interface functions
 **************************************************/
static void
cthw_attention(ct)
	struct ct_softc *ct;
{
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;

	if ((ct_stat_read_1(bst, bsh) & STR_BUSY) != 0)
	{
		ct->sc_atten = 1;
		return;
	}

	ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_ASSERT_ATN);
	delay(10);
	if ((ct_stat_read_1(bst, bsh) & STR_LCI) != 0)
	{
		ct->sc_atten = 1;
		return;
	}
	ct->sc_atten = 0;
}

static int
ct_targ_init(ct, ti)
	struct ct_softc *ct;
	struct targ_info *ti;
{
	struct ct_targ_info *cti = (void *) ti;

	if (ct->sc_chiprev == CT_WD33C93_A)
	{
		ti->ti_maxsynch.period = 200 / 4;	/* 5MHz */
		ti->ti_maxsynch.offset = 8;
	}
	else
	{
		ti->ti_maxsynch.period = 100 / 4;	/* 10MHz */
		ti->ti_maxsynch.offset = 12;
	}

	cti->cti_syncreg = 0;
	return 0;
}	

static int
ct_world_start(ct, fdone)
	struct ct_softc *ct;
	int fdone;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	intrmask_t s;

	if (ct->sc_sdp == NULL)
		ct->sc_sdp = &ct_synch_data_20MHz[0];

	slp->sl_cfgflags |= CFG_MSGUNIFY;
	if (slp->sl_cfgflags & CFG_NOPARITY)
		ct->sc_creg = CR_DEFAULT;
	else
		ct->sc_creg = CR_DEFAULT_HP;

	if (ct->sc_dma & CT_DMA_DMASTART)
		(*ct->ct_dma_xfer_stop) (ct);
	if (ct->sc_dma & CT_DMA_PIOSTART)
		(*ct->ct_pio_xfer_stop) (ct);
	ct->sc_dma = 0;
	ct->sc_atten = 0;

	s = splcam();
	cthw_chip_reset(bst, bsh, ct->sc_chipclk, slp->sl_hostid);
	scsi_low_bus_reset(slp);
	cthw_chip_reset(bst, bsh, ct->sc_chipclk, slp->sl_hostid);
	splx(s);

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

static int
ct_start_selection(ct, cb)
	struct ct_softc *ct;
	struct slccb *cb;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	struct targ_info *ti = slp->sl_nexus;
	struct lun_info *li = ti->ti_li;
	int s;
	u_int8_t cmd;

	ct->sc_atten = 0;
	if (cthw_cmdlevel[slp->sl_scp.scp_cmd[0]] != 0)
	{
		/*
		 * This completely violates scsi protocols,
		 * however some old devices do not work
		 * properly with scsi attentions.
		 */
		if ((li->li_flags & SCSI_LOW_DISC) != 0)
			cmd = WD3S_SELECT_ATN_TFR;
		else
			cmd = WD3S_SELECT_NO_ATN_TFR;
		ct->sc_satgo = CT_SAT_GOING;
	}
	else
	{
		cmd = WD3S_SELECT_ATN;
		ct->sc_satgo = 0;
	}

	if ((ct_stat_read_1(bst, bsh) & STR_BUSY) != 0)
		return SCSI_LOW_START_FAIL;

	scsi_low_cmd(slp, ti);

	if ((ct->sc_satgo & CT_SAT_GOING) != 0)
		ct_write_cmds(bst, bsh,
			      slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);

	s = splhigh();
	if ((ct_stat_read_1(bst, bsh) & STR_BUSY) == 0)
	{
		/* XXX: 
		 * Reload a lun again here.
		 */
		ct_cr_write_1(bst, bsh, wd3s_lun, li->li_lun);
		ct_cr_write_1(bst, bsh, wd3s_cmd, cmd);
		if ((ct_stat_read_1(bst, bsh) & STR_LCI) == 0)
		{
			splx(s);
			SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
			return SCSI_LOW_START_OK;
		}
	}
	splx(s);
	return SCSI_LOW_START_FAIL;
}

static int
ct_msg(ct, ti, msg)
	struct ct_softc *ct;
	struct targ_info *ti;
	u_int msg;
{
	struct lun_info *li = ti->ti_li;
	struct ct_targ_info *cti = (void *) ti;
	struct ct_synch_data *csp = ct->sc_sdp;
	u_int offset, period;

	if (msg != SCSI_LOW_MSG_SYNCH)
		return 0;

	offset = ti->ti_maxsynch.offset;
	period = ti->ti_maxsynch.period;
	for ( ; csp->cs_period != 0; csp ++)
	{
		if (period == csp->cs_period)
			break;
	}

	if (ti->ti_maxsynch.period != 0 && csp->cs_period == 0)
	{
		ti->ti_maxsynch.period = 0;
		ti->ti_maxsynch.offset = 0;
		cti->cti_syncreg = 0;
		return EINVAL;
	}

	cti->cti_syncreg = ((offset & 0x0f) | csp->cs_syncr);
	if (ct->ct_synch_setup != 0)
		(*ct->ct_synch_setup) (ct, li);
	return 0;
}

/*************************************************
 * <DATA PHASE>
 *************************************************/
static int
ct_xfer(ct, data, len, direction)
	struct ct_softc *ct;
	u_int8_t *data;
	int len, direction;
{
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	int wc;
	register u_int8_t aux;

	if (len == 1)
	{
		ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_SBT | WD3S_TFR_INFO);
	}
	else
	{
		cthw_set_count(bst, bsh, len);
		ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_TFR_INFO);
	}

	aux = ct_stat_read_1(bst, bsh);
	if ((aux & STR_LCI) != 0)
	{
		cthw_set_count(bst, bsh, 0);
		return len;
	}

	for (wc = ct->sc_wc ; wc > 0; wc --)
	{
		/* check data ready */
		if ((aux & (STR_BSY | STR_DBR)) == (STR_BSY | STR_DBR))
		{
			if (direction == SCSI_LOW_READ)
				*data = ct_cr_read_1(bst, bsh, wd3s_data);
			else
				ct_cr_write_1(bst, bsh, wd3s_data, *data);
			len --;
			if (len <= 0)
				break;
			data ++;
		}

		/* check phase miss */
		aux = ct_stat_read_1(bst, bsh);
		if ((aux & STR_INT) != 0)
			break;
	}
	return len;
}

static void
ct_io_xfer(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	struct sc_p *sp = &slp->sl_scp;
	u_int dummy;
	int len;

	/* io polling mode */
	ct_cr_write_1(bst, bsh, wd3s_ctrl, ct->sc_creg);

	if (sp->scp_datalen <= 0)
	{
		slp->sl_error |= PDMAERR;
		dummy = 0;
		len = ct_xfer(ct, (u_int8_t *) &dummy, 1, sp->scp_direction);
	}
	else
		len = ct_xfer(ct, sp->scp_data, sp->scp_datalen,
			      sp->scp_direction);

	sp->scp_data += (sp->scp_datalen - len);
	sp->scp_datalen = len;
}

/**************************************************
 * <PHASE ERROR>
 **************************************************/
struct ct_err {
	u_char	*pe_msg;
	u_int	pe_err;
	u_int	pe_errmsg;
	int	pe_done;
};

struct ct_err ct_cmderr[] = {
/*0*/	{ "illegal cmd", FATALIO, SCSI_LOW_MSG_ABORT, 1},
/*1*/	{ "unexpected bus free", FATALIO, 0, 1},
/*2*/	{ NULL, SELTIMEOUTIO, 0, 1},
/*3*/	{ "scsi bus parity error", PARITYERR, SCSI_LOW_MSG_ERROR, 0},
/*4*/	{ "scsi bus parity error", PARITYERR, SCSI_LOW_MSG_ERROR, 0},
/*5*/	{ "unknown" , FATALIO, SCSI_LOW_MSG_ABORT, 1},
/*6*/	{ "miss reselection (target mode)", FATALIO, SCSI_LOW_MSG_ABORT, 0},
/*7*/	{ "wrong status byte", PARITYERR, SCSI_LOW_MSG_ERROR, 0},
};

static void
ct_phase_error(ct, scsi_status)
	struct ct_softc *ct;
	u_int8_t scsi_status;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct targ_info *ti = slp->sl_nexus;
	struct ct_err *pep;
	u_int msg = 0;

	if ((scsi_status & BSR_CM) == BSR_CMDERR &&
	    (scsi_status & BSR_PHVALID) == 0)
	{
		pep = &ct_cmderr[scsi_status & BSR_PM];
		slp->sl_error |= pep->pe_err;
		if ((pep->pe_err & PARITYERR) != 0)
		{
			if (ti->ti_phase == PH_MSGIN)
				msg = SCSI_LOW_MSG_PARITY;
			else
				msg = SCSI_LOW_MSG_ERROR;
		}
		else
			msg = pep->pe_errmsg;	

		if (msg != 0)
			scsi_low_assert_msg(slp, slp->sl_nexus, msg, 1);

		if (pep->pe_msg != NULL)
		{
			printf("%s: phase error: %s",
				slp->sl_xname, pep->pe_msg);
			scsi_low_print(slp, slp->sl_nexus);
		}

		if (pep->pe_done != 0)
			scsi_low_disconnected(slp, ti);
	}
	else
	{
		slp->sl_error |= FATALIO;
		scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, "phase error");
	}
}

/**************************************************
 * ### SCSI PHASE SEQUENCER ###
 **************************************************/
static __inline int
ct_reselected(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	struct targ_info *ti;
	u_int sid;

	ct->sc_atten = 0;
	sid = (ct_cr_read_1(bst, bsh, wd3s_sid) & SIDR_IDM);
	if ((ti = scsi_low_reselected(slp, sid)) == NULL)
		return EJUSTRETURN;

	ct_cr_write_1(bst, bsh, wd3s_did, sid);
	ct_cr_write_1(bst, bsh, wd3s_lun, 0);	/* temp */
	ct_cr_write_1(bst, bsh, wd3s_ctrl, ct->sc_creg | CR_DMA);
	cthw_set_count(bst, bsh, 0);
	return EJUSTRETURN;
}

static int
ct_nexus(ct, ti)
	struct ct_softc *ct;
	struct targ_info *ti;
{
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	struct lun_info *li = ti->ti_li;
	struct ct_targ_info *cti = (void *) ti;

	if ((li->li_flags & SCSI_LOW_NOPARITY) != 0)
		ct->sc_creg = CR_DEFAULT;
	else
		ct->sc_creg = CR_DEFAULT_HP;

	ct_cr_write_1(bst, bsh, wd3s_did, ti->ti_id);
	ct_cr_write_1(bst, bsh, wd3s_lun, li->li_lun);
	ct_cr_write_1(bst, bsh, wd3s_ctrl, ct->sc_creg | CR_DMA);
	ct_cr_write_1(bst, bsh, wd3s_cph, 0);
	ct_cr_write_1(bst, bsh, wd3s_synch, cti->cti_syncreg);
	cthw_set_count(bst, bsh, 0);
	ct_cr_write_1(bst, bsh, wd3s_lun, li->li_lun);	/* XXX */
	return 0;
}

int
ctintr(arg)
	void *arg;
{
	struct ct_softc *ct = arg;
	struct scsi_low_softc *slp = &ct->sc_sclow;
	bus_space_tag_t bst = ct->sc_iot;
	bus_space_handle_t bsh = ct->sc_ioh;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	int len, satgo;
	u_int8_t scsi_status, regv;

	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	/**************************************************
	 * Get status & bus phase
	 **************************************************/
	if ((ct_stat_read_1(bst, bsh) & STR_INT) == 0)
		return 0;

	scsi_status = ct_cr_read_1(bst, bsh, wd3s_stat);
	if (scsi_status == ((u_int8_t) -1))
		return 1;

	/**************************************************
	 * Check reselection, or nexus
	 **************************************************/
	if (scsi_status == BSR_RESEL)
	{
		if (ct_reselected(ct) == EJUSTRETURN)
			return 1;
	}

	if ((ti = slp->sl_nexus) == NULL)
		return 1;

	/**************************************************
	 * Debug section
	 **************************************************/
#ifdef	CT_DEBUG
	if (ct_debug > 0)
	{
		scsi_low_print(slp, NULL);
		printf("%s: scsi_status 0x%x\n\n", slp->sl_xname, 
		       (u_int) scsi_status);
		if (ct_debug > 1)
			Debugger();
	}
#endif	/* CT_DEBUG */

	/**************************************************
	 * Internal scsi phase
	 **************************************************/
	satgo = ct->sc_satgo;
	ct->sc_satgo = 0;

	switch (ti->ti_phase)
	{
	case PH_SELSTART:
		if ((satgo & CT_SAT_GOING) == 0)
		{
			if (scsi_status != BSR_SELECTED)
			{
				ct_phase_error(ct, scsi_status);
				return 1;
			}
			scsi_low_arbit_win(slp, ti);
			SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_IDENTIFY, 0);
			return 1;
		}
		else
		{
			scsi_low_arbit_win(slp, ti);
			SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		}
		break;

	case PH_RESEL:
	    	if ((scsi_status & BSR_PHVALID) == 0 ||
		    (scsi_status & BSR_PM) != BSR_MSGIN)
		{
			scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, 
				 "phase miss after reselect");
			return 1;
		}
		break;

	default:
		if (slp->sl_flags & HW_PDMASTART)
		{
			slp->sl_flags &= ~HW_PDMASTART;
			if (ct->sc_dma & CT_DMA_DMASTART)
			{
				(*ct->ct_dma_xfer_stop) (ct);
				ct->sc_dma &= ~CT_DMA_DMASTART;
			}
			else 
			{
				(*ct->ct_pio_xfer_stop) (ct);
				ct->sc_dma &= ~CT_DMA_PIOSTART;
			}
		}
		break;
	}

	/**************************************************
	 * parse scsi phase
	 **************************************************/
	if (scsi_status & BSR_PHVALID)
	{
		/**************************************************
		 * Normal SCSI phase.
		 **************************************************/
		if ((scsi_status & BSR_CM) == BSR_CMDABT)
		{
			ct_phase_error(ct, scsi_status);
			return 1;
		}

		switch (scsi_status & BSR_PM)
		{
		case BSR_DATAOUT:
			SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
			if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
				return 1;
			goto common_data_phase;

		case BSR_DATAIN:
			SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
			if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
				return 1;

common_data_phase:
			if (slp->sl_scp.scp_datalen <= 0)
			{
				ct_io_xfer(ct);
				return 1;
			}

			slp->sl_flags |= HW_PDMASTART;
			if ((ct->sc_xmode & CT_XMODE_PIO) != 0 &&
			    (slp->sl_scp.scp_datalen % DEV_BSIZE) == 0)
			{
				pp = physio_proc_enter(bp);
				ct->sc_dma |= CT_DMA_PIOSTART;
				(*ct->ct_pio_xfer_start) (ct);
				physio_proc_leave(pp);
				return 1;
			}
			else
			{
				ct->sc_dma |= CT_DMA_DMASTART;
				(*ct->ct_dma_xfer_start) (ct);
				ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_TFR_INFO);
			}
			return 1;

		case BSR_CMDOUT:
			SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
			if (scsi_low_cmd(slp, ti) != 0)
				break;

			if (ct_xfer(ct, 
				    slp->sl_scp.scp_cmd,
				    slp->sl_scp.scp_cmdlen,
				    SCSI_LOW_WRITE) != 0)
			{
				printf("%s: scsi cmd xfer short\n",
					slp->sl_xname);
			}
			return 1;

		case BSR_STATIN:
			SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
#ifdef CT_USE_CCSEQ
			if (scsi_low_is_msgout_continue(ti) != 0 ||
			    ct->sc_atten != 0)
			{
				ct_xfer(ct, &ti->ti_status, 1, SCSI_LOW_READ);
			}
			else
			{
				cthw_set_count(bst, bsh, 0);
				cthw_phase_bypass(ct, 0x41);
			}
#else	/* !CT_USE_CCSEQ */
			ct_xfer(ct, &ti->ti_status, 1, SCSI_LOW_READ);
#endif	/* !CT_USE_CCSEQ */
			return 1;

		case BSR_UNSPINFO0:
		case BSR_UNSPINFO1:
			printf("%s: illegal bus phase (0x%x)\n", slp->sl_xname,
				(u_int) scsi_status);
			scsi_low_print(slp, ti);
			return 1;

		case BSR_MSGOUT:
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
			len = scsi_low_msgout(slp, ti);
			if (ct_xfer(ct, ti->ti_msgoutstr, len, SCSI_LOW_WRITE))
			{
				printf("%s: scsi msgout xfer short\n",
					slp->sl_xname);
				scsi_low_assert_msg(slp, ti, 
					SCSI_LOW_MSG_ABORT, 1);
			}
			return 1;

		case BSR_MSGIN:/* msg in */
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
			ct_xfer(ct, &regv, 1, SCSI_LOW_READ);
			scsi_low_msgin(slp, ti, regv);
			return 1;
		}
	}
	else
	{
		/**************************************************
		 * Special SCSI phase
		 **************************************************/
		switch (scsi_status)
		{
		case BSR_SATSDP: /* SAT with save data pointer */
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
			scsi_low_msgin(slp, ti, MSG_SAVESP);
			cthw_phase_bypass(ct, 0x41);
			return 1;

		case BSR_SATFIN: /* SAT COMPLETE */
			/*
			 * emulate statusin => msgin
			 */
			ti->ti_status = ct_cr_read_1(bst, bsh, wd3s_lun);
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
			SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_CMDC);
			scsi_low_disconnected(slp, ti);
			return 1;

		case BSR_ACKREQ: /* negate ACK */
			if (ct->sc_atten != 0)
				cthw_attention(ct);

			ct_cr_write_1(bst, bsh, wd3s_cmd, WD3S_NEGATE_ACK);
			return 1;

		case BSR_DISC: /* disconnect */
			if (slp->sl_msgphase == MSGPH_NULL &&
			    (satgo & CT_SAT_GOING) != 0)
			{
				/*
				 * emulate disconnect msg
				 */
				SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
				SCSI_LOW_SETUP_MSGPHASE(slp, MSGPH_DISC);
			}	
			scsi_low_disconnected(slp, ti);
			return 1;

		default:
			break;
		}
	}

	ct_phase_error(ct, scsi_status);
	return 1;
}
