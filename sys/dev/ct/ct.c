/*	$NecBSD: ct.c,v 1.13.12.5 2001/06/26 07:31:53 honda Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/ct/ct.c,v 1.10 2005/01/06 01:42:33 imp Exp $");
/*	$NetBSD$	*/

#define	CT_DEBUG
#define	CT_IO_CONTROL_FLAGS	(CT_USE_CCSEQ | CT_FAST_INTR)

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__) && __FreeBSD_version > 500001
#include <sys/bio.h>
#endif	/* __ FreeBSD__ */
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#ifdef __NetBSD__
#include <sys/device.h>

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
#include <i386/Cbus/dev/ct/ct_machdep.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/bus.h>

#include <compat/netbsd/dvcfg.h>
#include <compat/netbsd/physio_proc.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ct/ctvar.h>
#include <dev/ct/ct_machdep.h>
#endif /* __FreeBSD__ */

#define	CT_NTARGETS		8
#define	CT_NLUNS		8
#define	CT_RESET_DEFAULT	2000
#define	CT_DELAY_MAX		(2 * 1000 * 1000)
#define	CT_DELAY_INTERVAL	(1)

/***************************************************
 * DEBUG
 ***************************************************/
#ifdef	CT_DEBUG
int ct_debug;
#endif	/* CT_DEBUG */

/***************************************************
 * IO control
 ***************************************************/
#define	CT_USE_CCSEQ	0x0100
#define	CT_FAST_INTR	0x0200

u_int ct_io_control = CT_IO_CONTROL_FLAGS;

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

#if	0
/* default synch data table */
/*  A  10    6.6   5.0   4.0   3.3   2.8   2.5   2.0  M/s */
/*  X  100   150   200   250   300   350   400   500  ns  */
static struct ct_synch_data ct_synch_data_FSCSI[] = {
	{25, 0xa0}, {37, 0xb0}, {50, 0x20}, {62, 0xd0}, {75, 0x30},
	{87, 0xf0}, {100, 0x40}, {125, 0x50}, {0, 0}
};

static struct ct_synch_data ct_synch_data_SCSI[] = {
	{50, 0x20}, {75, 0x30}, {100, 0x40}, {125, 0x50}, {0, 0}
};
#endif
/***************************************************
 * DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver ct_cd;

/*****************************************************************
 * Interface functions
 *****************************************************************/
static int ct_xfer(struct ct_softc *, u_int8_t *, int, int, u_int *);
static void ct_io_xfer(struct ct_softc *);
static int ct_reselected(struct ct_softc *, u_int8_t);
static void ct_phase_error(struct ct_softc *, u_int8_t);
static int ct_start_selection(struct ct_softc *, struct slccb *);
static int ct_msg(struct ct_softc *, struct targ_info *, u_int);
static int ct_world_start(struct ct_softc *, int);
static __inline void cthw_phase_bypass(struct ct_softc *, u_int8_t);
static int cthw_chip_reset(struct ct_bus_access_handle *, int *, int, int);
static void cthw_bus_reset(struct ct_softc *);
static int ct_ccb_nexus_establish(struct ct_softc *);
static int ct_lun_nexus_establish(struct ct_softc *);
static int ct_target_nexus_establish(struct ct_softc *, int, int);
static void cthw_attention(struct ct_softc *);
static int ct_targ_init(struct ct_softc *, struct targ_info *, int);
static int ct_unbusy(struct ct_softc *);
static void ct_attention(struct ct_softc *);
static struct ct_synch_data *ct_make_synch_table(struct ct_softc *);
static int ct_catch_intr(struct ct_softc *);

struct scsi_low_funcs ct_funcs = {
	SC_LOW_INIT_T ct_world_start,
	SC_LOW_BUSRST_T cthw_bus_reset,
	SC_LOW_TARG_INIT_T ct_targ_init,
	SC_LOW_LUN_INIT_T NULL,

	SC_LOW_SELECT_T ct_start_selection,
	SC_LOW_NEXUS_T ct_lun_nexus_establish,
	SC_LOW_NEXUS_T ct_ccb_nexus_establish,

	SC_LOW_ATTEN_T cthw_attention,
	SC_LOW_MSG_T ct_msg,

	SC_LOW_TIMEOUT_T NULL,
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
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	ct_cr_write_1(chp, wd3s_cph, ph);
	ct_cr_write_1(chp, wd3s_cmd, WD3S_SELECT_ATN_TFR);
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
cthw_chip_reset(chp, chiprevp, chipclk, hostid)
	struct ct_bus_access_handle *chp;
	int *chiprevp;
	int chipclk, hostid;
{
#define	CT_SELTIMEOUT_20MHz_REGV	(0x80)
	u_int8_t aux, regv;
	u_int seltout;
	int wc;

	/* issue abort cmd */
	ct_cr_write_1(chp, wd3s_cmd, WD3S_ABORT);
	SCSI_LOW_DELAY(1000);	/* 1ms wait */
	(void) ct_stat_read_1(chp);
	(void) ct_cr_read_1(chp, wd3s_stat);

	/* setup chip registers */
	regv = 0;
	seltout = CT_SELTIMEOUT_20MHz_REGV;
	switch (chipclk)
	{
	case 8:
	case 10:
		seltout = (seltout * chipclk) / 20;
		regv = IDR_FS_8_10;
		break;

	case 12:
	case 15:
		seltout = (seltout * chipclk) / 20;
		regv = IDR_FS_12_15;
		break;

	case 16:
	case 20:
		seltout = (seltout * chipclk) / 20;
		regv = IDR_FS_16_20;
		break;

	default:
		panic("ct: illegal chip clk rate");
		break;
	}

	regv |= IDR_EHP | hostid | IDR_RAF | IDR_EAF;
	ct_cr_write_1(chp, wd3s_oid, regv);

	ct_cr_write_1(chp, wd3s_cmd, WD3S_RESET);
	for (wc = CT_RESET_DEFAULT; wc > 0; wc --)
	{
		aux = ct_stat_read_1(chp);
		if (aux != 0xff && (aux & STR_INT))
		{
			regv = ct_cr_read_1(chp, wd3s_stat);
			if (regv == BSR_RESET || regv == BSR_AFM_RESET)
				break;

			ct_cr_write_1(chp, wd3s_cmd, WD3S_RESET);
		}
		SCSI_LOW_DELAY(1);
	}
	if (wc == 0)
		return ENXIO;

	ct_cr_write_1(chp, wd3s_tout, seltout);
	ct_cr_write_1(chp, wd3s_sid, SIDR_RESEL);
	ct_cr_write_1(chp, wd3s_ctrl, CR_DEFAULT);
	ct_cr_write_1(chp, wd3s_synch, 0);
	if (chiprevp != NULL)
	{
		*chiprevp = CT_WD33C93;
		if (regv == BSR_RESET)
			goto out;

		*chiprevp = CT_WD33C93_A;
		ct_cr_write_1(chp, wd3s_qtag, 0xaa);
		if (ct_cr_read_1(chp, wd3s_qtag) != 0xaa)
		{
			ct_cr_write_1(chp, wd3s_qtag, 0x0);
			goto out;
		}
		ct_cr_write_1(chp, wd3s_qtag, 0x55);
		if (ct_cr_read_1(chp, wd3s_qtag) != 0x55)
		{
			ct_cr_write_1(chp, wd3s_qtag, 0x0);
			goto out;
		}
		ct_cr_write_1(chp, wd3s_qtag, 0x0);
		*chiprevp = CT_WD33C93_B;
	}

out:
	(void) ct_stat_read_1(chp);
	(void) ct_cr_read_1(chp, wd3s_stat);
	return 0;
}

static struct ct_synch_data *
ct_make_synch_table(ct)
	struct ct_softc *ct;
{
	struct ct_synch_data *sdtp, *sdp;
	u_int base, i, period;

	sdtp = sdp = &ct->sc_default_sdt[0];

	if ((ct->sc_chipclk % 5) == 0)
		base = 1000 / (5 * 2);	/* 5 MHz type */
	else
		base = 1000 / (4 * 2);	/* 4 MHz type */

	if (ct->sc_chiprev >= CT_WD33C93_B)
	{
		/* fast scsi */
		for (i = 2; i < 8; i ++, sdp ++)
		{
			period = (base * i) / 2;
			if (period >= 200)	/* 5 MHz */
				break;
			sdp->cs_period = period / 4;
			sdp->cs_syncr = (i * 0x10) | 0x80;
		}
	}

	for (i = 2; i < 8; i ++, sdp ++)
	{
		period = (base * i);
		if (period > 500)		/* 2 MHz */
			break;
		sdp->cs_period = period / 4;
		sdp->cs_syncr = (i * 0x10);
	}

	sdp->cs_period = 0;
	sdp->cs_syncr = 0;
	return sdtp;
}

/**************************************************
 * Attach & Probe
 **************************************************/
int
ctprobesubr(chp, dvcfg, hsid, chipclk, chiprevp)
	struct ct_bus_access_handle *chp;
	u_int dvcfg, chipclk;
	int hsid;
	int *chiprevp;
{

#if	0
	if ((ct_stat_read_1(chp) & STR_BSY) != 0)
		return 0;
#endif
	if (cthw_chip_reset(chp, chiprevp, chipclk, hsid) != 0)
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

	ct->sc_tmaxcnt = SCSI_LOW_MIN_TOUT * 1000 * 1000; /* default */
	slp->sl_funcs = &ct_funcs;
	slp->sl_flags |= HW_READ_PADDING;
	(void) scsi_low_attach(slp, 0, CT_NTARGETS, CT_NLUNS,
			       sizeof(struct ct_targ_info), 0);
}

/**************************************************
 * SCSI LOW interface functions
 **************************************************/
static void
cthw_attention(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	ct->sc_atten = 1;
	if ((ct_stat_read_1(chp) & (STR_BSY | STR_CIP)) != 0)
		return;

	ct_cr_write_1(chp, wd3s_cmd, WD3S_ASSERT_ATN);
	SCSI_LOW_DELAY(10);
	if ((ct_stat_read_1(chp) & STR_LCI) == 0)
		ct->sc_atten = 0;
	ct_unbusy(ct);
	return;
}

static void
ct_attention(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;

	if (slp->sl_atten == 0)
	{
		ct_unbusy(ct);
		scsi_low_attention(slp);
	}
	else if (ct->sc_atten != 0)
	{
		ct_unbusy(ct);
		cthw_attention(ct);
	}
}

static int
ct_targ_init(ct, ti, action)
	struct ct_softc *ct;
	struct targ_info *ti;
	int action;
{
	struct ct_targ_info *cti = (void *) ti;

	if (action == SCSI_LOW_INFO_ALLOC || action == SCSI_LOW_INFO_REVOKE)
	{
		if (ct->sc_sdp == NULL)
		{
			ct->sc_sdp = ct_make_synch_table(ct);
		}

		switch (ct->sc_chiprev)
		{
		default:
			ti->ti_maxsynch.offset = 5;
			break;

		case CT_WD33C93_A:
		case CT_AM33C93_A:
			ti->ti_maxsynch.offset = 12;
			break;

		case CT_WD33C93_B:
		case CT_WD33C93_C:
			ti->ti_maxsynch.offset = 12;
			break;
		}

		ti->ti_maxsynch.period = ct->sc_sdp[0].cs_period;
		ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
		cti->cti_syncreg = 0;
	}

	return 0;
}	

static int
ct_world_start(ct, fdone)
	struct ct_softc *ct;
	int fdone;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	if (ct->sc_sdp == NULL)
	{
		ct->sc_sdp = ct_make_synch_table(ct);
	}

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

	cthw_chip_reset(chp, NULL, ct->sc_chipclk, slp->sl_hostid);
	scsi_low_bus_reset(slp);
	cthw_chip_reset(chp, NULL, ct->sc_chipclk, slp->sl_hostid);

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

static int
ct_start_selection(ct, cb)
	struct ct_softc *ct;
	struct slccb *cb;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	struct targ_info *ti = slp->sl_Tnexus;
	struct lun_info *li = slp->sl_Lnexus;
	int s, satok;
	u_int8_t cmd;

	ct->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	ct->sc_atten = 0;
	satok = 0;

	if (scsi_low_is_disconnect_ok(cb) != 0)
	{	
		if (ct->sc_chiprev >= CT_WD33C93_A)
			satok = 1;
		else if (cthw_cmdlevel[slp->sl_scp.scp_cmd[0]] != 0)
			satok = 1;
	}

	if (satok != 0 &&
	    scsi_low_is_msgout_continue(ti, SCSI_LOW_MSG_IDENTIFY) == 0)
	{
		cmd = WD3S_SELECT_ATN_TFR;
		ct->sc_satgo = CT_SAT_GOING;
	}
	else
	{
		cmd = WD3S_SELECT_ATN;
		ct->sc_satgo = 0;
	}

	if ((ct_stat_read_1(chp) & (STR_BSY | STR_INT | STR_CIP)) != 0)
		return SCSI_LOW_START_FAIL;

	if ((ct->sc_satgo & CT_SAT_GOING) != 0)
	{
		(void) scsi_low_msgout(slp, ti, SCSI_LOW_MSGOUT_INIT);
		scsi_low_cmd(slp, ti);
		ct_cr_write_1(chp, wd3s_oid, slp->sl_scp.scp_cmdlen);
		ct_write_cmds(chp, slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);
	}
	else
	{
		/* anyway attention assert */
		SCSI_LOW_ASSERT_ATN(slp);
	}

	ct_target_nexus_establish(ct, li->li_lun, slp->sl_scp.scp_direction);

	s = splhigh();
	if ((ct_stat_read_1(chp) & (STR_BSY | STR_INT | STR_CIP)) == 0)
	{
		/* XXX: 
		 * Reload a lun again here.
		 */
		ct_cr_write_1(chp, wd3s_lun, li->li_lun);
		ct_cr_write_1(chp, wd3s_cmd, cmd);
		if ((ct_stat_read_1(chp) & STR_LCI) == 0)
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
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct ct_targ_info *cti = (void *) ti;
	struct ct_synch_data *csp = ct->sc_sdp;
	u_int offset, period;
	int error;

	if ((msg & SCSI_LOW_MSG_WIDE) != 0)
	{
		if (ti->ti_width != SCSI_LOW_BUS_WIDTH_8)
		{
			ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
			return EINVAL;
		}
		return 0;
	}

	if ((msg & SCSI_LOW_MSG_SYNCH) == 0)
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
		error = EINVAL;
	}
	else
	{
		cti->cti_syncreg = ((offset & 0x0f) | csp->cs_syncr);
		error = 0;
	}

	if (ct->ct_synch_setup != 0)
		(*ct->ct_synch_setup) (ct, ti);
	ct_cr_write_1(chp, wd3s_synch, cti->cti_syncreg);
	return error;
}

/*************************************************
 * <DATA PHASE>
 *************************************************/
static int
ct_xfer(ct, data, len, direction, statp)
	struct ct_softc *ct;
	u_int8_t *data;
	int len, direction;
	u_int *statp;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	int wc;
	register u_int8_t aux;

	*statp = 0;
	if (len == 1)
	{
		ct_cr_write_1(chp, wd3s_cmd, WD3S_SBT | WD3S_TFR_INFO);
	}
	else
	{
		cthw_set_count(chp, len);
		ct_cr_write_1(chp, wd3s_cmd, WD3S_TFR_INFO);
	}

	aux = ct_stat_read_1(chp);
	if ((aux & STR_LCI) != 0)
	{
		cthw_set_count(chp, 0);
		return len;
	}

	for (wc = 0; wc < ct->sc_tmaxcnt; wc ++)
	{
		/* check data ready */
		if ((aux & (STR_BSY | STR_DBR)) == (STR_BSY | STR_DBR))
		{
			if (direction == SCSI_LOW_READ)
			{
				*data = ct_cr_read_1(chp, wd3s_data);
				if ((aux & STR_PE) != 0)
					*statp |= SCSI_LOW_DATA_PE;
			}
			else
			{
				ct_cr_write_1(chp, wd3s_data, *data);
			}
			len --;
			if (len <= 0)
				break;
			data ++;
		}
		else
		{
			SCSI_LOW_DELAY(1);
		}

		/* check phase miss */
		aux = ct_stat_read_1(chp);
		if ((aux & STR_INT) != 0)
			break;
	}
	return len;
}

#define	CT_PADDING_BUF_SIZE 32

static void
ct_io_xfer(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct sc_p *sp = &slp->sl_scp;
	u_int stat;
	int len;
	u_int8_t pbuf[CT_PADDING_BUF_SIZE];

	/* polling mode */
	ct_cr_write_1(chp, wd3s_ctrl, ct->sc_creg);

	if (sp->scp_datalen <= 0)
	{
		slp->sl_error |= PDMAERR;

		if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
			SCSI_LOW_BZERO(pbuf, CT_PADDING_BUF_SIZE);
		ct_xfer(ct, pbuf, CT_PADDING_BUF_SIZE, 
			sp->scp_direction, &stat);
	}
	else
	{
		len = ct_xfer(ct, sp->scp_data, sp->scp_datalen,
			      sp->scp_direction, &stat);
		sp->scp_data += (sp->scp_datalen - len);
		sp->scp_datalen = len;
	}
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
	struct targ_info *ti = slp->sl_Tnexus;
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
			scsi_low_assert_msg(slp, slp->sl_Tnexus, msg, 1);

		if (pep->pe_msg != NULL)
		{
			printf("%s: phase error: %s",
				slp->sl_xname, pep->pe_msg);
			scsi_low_print(slp, slp->sl_Tnexus);
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
static int
ct_reselected(ct, scsi_status)
	struct ct_softc *ct;
	u_int8_t scsi_status;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct targ_info *ti;
	u_int sid;
	u_int8_t regv;

	ct->sc_atten = 0;
	ct->sc_satgo &= ~CT_SAT_GOING;
	regv = ct_cr_read_1(chp, wd3s_sid);
	if ((regv & SIDR_VALID) == 0)
		return EJUSTRETURN;

	sid = regv & SIDR_IDM;
	if ((ti = scsi_low_reselected(slp, sid)) == NULL)
		return EJUSTRETURN;

	ct_target_nexus_establish(ct, 0, SCSI_LOW_READ);
	if (scsi_status != BSR_AFM_RESEL)
		return EJUSTRETURN;

	SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
	regv = ct_cr_read_1(chp, wd3s_data);
	if (scsi_low_msgin(slp, ti, (u_int) regv) == 0)
	{
		if (scsi_low_is_msgout_continue(ti, 0) != 0)
		{
			/* XXX: scsi_low_attetion */
			scsi_low_attention(slp);
		}
	}

	if (ct->sc_atten != 0)
	{
		ct_attention(ct);
	}

	ct_cr_write_1(chp, wd3s_cmd, WD3S_NEGATE_ACK);
	return EJUSTRETURN;
}

static int
ct_target_nexus_establish(ct, lun, dir)
	struct ct_softc *ct;
	int lun, dir;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct targ_info *ti = slp->sl_Tnexus;
	struct ct_targ_info *cti = (void *) ti;

	if (dir == SCSI_LOW_WRITE)
		ct_cr_write_1(chp, wd3s_did, ti->ti_id);
	else
		ct_cr_write_1(chp, wd3s_did, ti->ti_id | DIDR_DPD); 
	ct_cr_write_1(chp, wd3s_lun, lun);
	ct_cr_write_1(chp, wd3s_ctrl, ct->sc_creg | CR_DMA);
	ct_cr_write_1(chp, wd3s_cph, 0);
	ct_cr_write_1(chp, wd3s_synch, cti->cti_syncreg);
	cthw_set_count(chp, 0);
	return 0;
}

static int
ct_lun_nexus_establish(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct lun_info *li = slp->sl_Lnexus;

	ct_cr_write_1(chp, wd3s_lun, li->li_lun);
	return 0;
}

static int
ct_ccb_nexus_establish(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct lun_info *li = slp->sl_Lnexus;
	struct targ_info *ti = slp->sl_Tnexus;
	struct ct_targ_info *cti = (void *) ti;
	struct slccb *cb = slp->sl_Qnexus;

	ct->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;

	if ((ct->sc_satgo & CT_SAT_GOING) != 0)
	{
		ct_cr_write_1(chp, wd3s_oid, slp->sl_scp.scp_cmdlen);
		ct_write_cmds(chp, slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);
	}
	if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
		ct_cr_write_1(chp, wd3s_did, ti->ti_id);
	else
		ct_cr_write_1(chp, wd3s_did, ti->ti_id | DIDR_DPD);
	ct_cr_write_1(chp, wd3s_lun, li->li_lun);
	ct_cr_write_1(chp, wd3s_synch, cti->cti_syncreg);
	return 0;
}

static int
ct_unbusy(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	int wc;
	register u_int8_t regv;

	for (wc = 0; wc < CT_DELAY_MAX / CT_DELAY_INTERVAL; wc ++)
	{
		regv = ct_stat_read_1(chp);
		if ((regv & (STR_BSY | STR_CIP)) == 0)
			return 0;
		if (regv == (u_int8_t) -1)
			return EIO;

		SCSI_LOW_DELAY(CT_DELAY_INTERVAL);
	}

	printf("%s: unbusy timeout\n", slp->sl_xname);
	return EBUSY;
}
	
static int
ct_catch_intr(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	int wc;
	register u_int8_t regv;

	for (wc = 0; wc < CT_DELAY_MAX / CT_DELAY_INTERVAL; wc ++)
	{
		regv = ct_stat_read_1(chp);
		if ((regv & (STR_INT | STR_BSY | STR_CIP)) == STR_INT)
			return 0;

		SCSI_LOW_DELAY(CT_DELAY_INTERVAL);
	}
	return EJUSTRETURN;
}

int
ctintr(arg)
	void *arg;
{
	struct ct_softc *ct = arg;
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	u_int derror, flags;
	int len, satgo, error;
	u_int8_t scsi_status, regv;

again:
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	/**************************************************
	 * Get status & bus phase
	 **************************************************/
	if ((ct_stat_read_1(chp) & STR_INT) == 0)
		return 0;

	scsi_status = ct_cr_read_1(chp, wd3s_stat);
	if (scsi_status == ((u_int8_t) -1))
		return 1;

	/**************************************************
	 * Check reselection, or nexus
	 **************************************************/
	if (scsi_status == BSR_RESEL || scsi_status == BSR_AFM_RESEL)
	{
		if (ct_reselected(ct, scsi_status) == EJUSTRETURN)
			return 1;
	}

	if ((ti = slp->sl_Tnexus) == NULL)
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
#ifdef	KDB
		if (ct_debug > 1)
			SCSI_LOW_DEBUGGER("ct");
#endif	/* KDB */
	}
#endif	/* CT_DEBUG */

	/**************************************************
	 * Internal scsi phase
	 **************************************************/
	satgo = ct->sc_satgo;
	ct->sc_satgo &= ~CT_SAT_GOING;

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
			scsi_low_arbit_win(slp);
			SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
			return 1;
		}
		else
		{
			scsi_low_arbit_win(slp);
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT); /* XXX */
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
			else if (ct->sc_dma & CT_DMA_PIOSTART)
			{
				(*ct->ct_pio_xfer_stop) (ct);
				ct->sc_dma &= ~CT_DMA_PIOSTART;
			}
			else
			{
				scsi_low_data_finish(slp);
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
			{
				ct_attention(ct);
			}
			goto common_data_phase;

		case BSR_DATAIN:
			SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
			if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
			{
				ct_attention(ct);
			}

common_data_phase:
			if (slp->sl_scp.scp_datalen > 0)
			{
				slp->sl_flags |= HW_PDMASTART;
				if ((ct->sc_xmode & CT_XMODE_PIO) != 0)
				{
					pp = physio_proc_enter(bp);
					error = (*ct->ct_pio_xfer_start) (ct);
					physio_proc_leave(pp);
					if (error == 0)
					{
						ct->sc_dma |= CT_DMA_PIOSTART;
						return 1;
					}
				}

				if ((ct->sc_xmode & CT_XMODE_DMA) != 0)
				{
					error = (*ct->ct_dma_xfer_start) (ct);
					if (error == 0)
					{
						ct->sc_dma |= CT_DMA_DMASTART;
						return 1;
					}
				}
			}
			else
			{	
				if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
				{
					if (!(slp->sl_flags & HW_READ_PADDING))
					{
						printf("%s: read padding required\n", slp->sl_xname);
						return 1;
					}
				}
				else
				{
					if (!(slp->sl_flags & HW_WRITE_PADDING))
					{
						printf("%s: write padding required\n", slp->sl_xname);
						return 1;
					}
				}
				slp->sl_flags |= HW_PDMASTART;
			}

			ct_io_xfer(ct);
			return 1;

		case BSR_CMDOUT:
			SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
			if (scsi_low_cmd(slp, ti) != 0)
			{
				ct_attention(ct);
			}

			if (ct_xfer(ct, slp->sl_scp.scp_cmd,
				    slp->sl_scp.scp_cmdlen,
				    SCSI_LOW_WRITE, &derror) != 0)
			{
				printf("%s: scsi cmd xfer short\n",
					slp->sl_xname);
			}
			return 1;

		case BSR_STATIN:
			SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
			if ((ct_io_control & CT_USE_CCSEQ) != 0)
			{
				if (scsi_low_is_msgout_continue(ti, 0) != 0 ||
				    ct->sc_atten != 0)
				{
					ct_xfer(ct, &regv, 1, SCSI_LOW_READ,
						&derror);
					scsi_low_statusin(slp, ti,
						  	  regv | derror);
				}
				else
				{
					ct->sc_satgo |= CT_SAT_GOING;
					cthw_set_count(chp, 0);
					cthw_phase_bypass(ct, 0x41);
				}
			}
			else
			{
				ct_xfer(ct, &regv, 1, SCSI_LOW_READ, &derror);
				scsi_low_statusin(slp, ti, regv | derror);
			}
			return 1;

		case BSR_UNSPINFO0:
		case BSR_UNSPINFO1:
			printf("%s: illegal bus phase (0x%x)\n", slp->sl_xname,
				(u_int) scsi_status);
			scsi_low_print(slp, ti);
			return 1;

		case BSR_MSGOUT:
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
			flags = SCSI_LOW_MSGOUT_UNIFY;
		        if (ti->ti_ophase != ti->ti_phase)
				flags |= SCSI_LOW_MSGOUT_INIT;
			len = scsi_low_msgout(slp, ti, flags);

			if (len > 1 && slp->sl_atten == 0)
			{
				ct_attention(ct);
			}

			if (ct_xfer(ct, ti->ti_msgoutstr, len, 
				    SCSI_LOW_WRITE, &derror) != 0)
			{
				printf("%s: scsi msgout xfer short\n",
					slp->sl_xname);
			}
			SCSI_LOW_DEASSERT_ATN(slp);
			ct->sc_atten = 0;
			return 1;

		case BSR_MSGIN:/* msg in */
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

			ct_xfer(ct, &regv, 1, SCSI_LOW_READ, &derror);
			if (scsi_low_msgin(slp, ti, regv | derror) == 0)
			{
				if (scsi_low_is_msgout_continue(ti, 0) != 0)
				{
					/* XXX: scsi_low_attetion */
					scsi_low_attention(slp);
				}
			}

			if ((ct_io_control & CT_FAST_INTR) != 0)
			{
				if (ct_catch_intr(ct) == 0)
					goto again;
			}
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
			ct->sc_satgo |= CT_SAT_GOING;
			scsi_low_msgin(slp, ti, MSG_SAVESP);
			cthw_phase_bypass(ct, 0x41);
			return 1;

		case BSR_SATFIN: /* SAT COMPLETE */
			/*
			 * emulate statusin => msgin
			 */
			SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
			scsi_low_statusin(slp, ti, ct_cr_read_1(chp, wd3s_lun));

			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
			scsi_low_msgin(slp, ti, MSG_COMP);

			scsi_low_disconnected(slp, ti);
			return 1;

		case BSR_ACKREQ: /* negate ACK */
			if (ct->sc_atten != 0)
			{
				ct_attention(ct);
			}

			ct_cr_write_1(chp, wd3s_cmd, WD3S_NEGATE_ACK);
			if ((ct_io_control & CT_FAST_INTR) != 0)
			{
				/* XXX:
				 * Should clear a pending interrupt and
				 * sync with a next interrupt!
				 */
				ct_catch_intr(ct);
			}
			return 1;

		case BSR_DISC: /* disconnect */
			if (slp->sl_msgphase == MSGPH_NULL &&
			    (satgo & CT_SAT_GOING) != 0)
			{
				/*
				 * emulate disconnect msg
				 */
				SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);
				scsi_low_msgin(slp, ti, MSG_DISCON);
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
