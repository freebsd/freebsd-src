/*	$FreeBSD$	*/
/*	$NecBSD: nsp.c,v 1.21 1999/07/23 21:00:05 honda Exp $	*/
/*	$NetBSD$	*/

#define	NSP_DEBUG
#define	NSP_STATICS

/*
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
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
#if defined(__FreeBSD__) && __FreeBSD_version > 500001
#include <sys/bio.h>
#endif
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
#include <i386/Cbus/dev/nspreg.h>
#include <i386/Cbus/dev/nspvar.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/clock.h>
#define delay(time) DELAY(time)

#include <machine/cpu.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <cam/scsi/scsi_low.h>
#include <dev/nsp/nspreg.h>
#include <dev/nsp/nspvar.h>

#if __FreeBSD_version < 400001
#include "nsp.h"
struct nsp_softc *nspdata[NNSP];
#endif
#endif /* __FreeBSD__ */

/***************************************************
 * USER SETTINGS
 ***************************************************/
/* DEVICE CONFIGURATION FLAGS (MINOR)
 *
 * 0x01   DISCONECT OFF
 * 0x02   PARITY LINE OFF
 * 0x04   IDENTIFY MSG OFF ( = single lun)
 * 0x08   SYNC TRANSFER OFF
 */

/***************************************************
 * PARAMS
 ***************************************************/
#define	NSP_NTARGETS	8
#define	NSP_NLUNS	8

#define	NSP_SELTIMEOUT	200

/***************************************************
 * DEBUG
 ***************************************************/
#ifndef DDB
#define Debugger() panic("should call debugger here (nsp.c)")
#else /* ! DDB */
#ifdef __FreeBSD__
#define Debugger() Debugger("nsp")
#endif /* __FreeBSD__ */
#endif

#ifdef	NSP_DEBUG
int nsp_debug;
#endif	/* NSP_DEBUG */

#ifdef	NSP_STATICS
struct nsp_statics {
	int disconnect;
	int reselect;
	int data_phase_bypass;
} nsp_statics[NSP_NTARGETS];
#endif	/* NSP_STATICS */

/***************************************************
 * ISA DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver nsp_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
/* static */
static void nsp_pio_read __P((struct nsp_softc *, struct targ_info *));
static void nsp_pio_write __P((struct nsp_softc *, struct targ_info *));
static int nsp_xfer __P((struct nsp_softc *, u_int8_t *, int, int));
static int nsp_msg __P((struct nsp_softc *, struct targ_info *, u_int));
static int nsp_reselected __P((struct nsp_softc *));
static __inline int nsp_disconnected __P((struct nsp_softc *, struct targ_info *));
static __inline void nsp_pdma_end __P((struct nsp_softc *, struct targ_info *));
static void nsphw_init __P((struct nsp_softc *));
static int nsp_nexus __P((struct nsp_softc *, struct targ_info *));
static int nsp_world_start __P((struct nsp_softc *, int));
static int nsphw_start_selection __P((struct nsp_softc *sc, struct slccb *));
static void nsphw_bus_reset __P((struct nsp_softc *));
static void nsphw_attention __P((struct nsp_softc *));
static u_int nsp_fifo_count __P((struct nsp_softc *));
static int nsp_negate_signal __P((struct nsp_softc *, u_int8_t, u_char *));
static int nsp_expect_signal __P((struct nsp_softc *, u_int8_t, u_int8_t));
static __inline void nsp_start_timer __P((struct nsp_softc *, int));
static int nsp_dataphase_bypass __P((struct nsp_softc *, struct targ_info *));
static void nsp_setup_fifo __P((struct nsp_softc *, int));
static int nsp_lun_init __P((struct nsp_softc *, struct targ_info *, struct lun_info *));
static void settimeout __P((void *));

struct scsi_low_funcs nspfuncs = {
	SC_LOW_INIT_T nsp_world_start,
	SC_LOW_BUSRST_T nsphw_bus_reset,
	SC_LOW_LUN_INIT_T nsp_lun_init,

	SC_LOW_SELECT_T nsphw_start_selection,
	SC_LOW_NEXUS_T nsp_nexus,

	SC_LOW_ATTEN_T nsphw_attention,
	SC_LOW_MSG_T nsp_msg,

	SC_LOW_POLL_T nspintr,

	NULL,
};

/****************************************************
 * hwfuncs
 ****************************************************/
static __inline u_int8_t nsp_cr_read_1 __P((bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ofs));
static __inline void nsp_cr_write_1 __P((bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ofs, u_int8_t va));

static __inline u_int8_t
nsp_cr_read_1(bst, bsh, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_addr_t ofs;
{
	
	bus_space_write_1(bst, bsh, nsp_idxr, ofs);
	return bus_space_read_1(bst, bsh, nsp_datar);
}

static __inline void 
nsp_cr_write_1(bst, bsh, ofs, va)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_addr_t ofs;
	u_int8_t va;
{

	bus_space_write_1(bst, bsh, nsp_idxr, ofs);
	bus_space_write_1(bst, bsh, nsp_datar, va);
}
	
static int
nsp_expect_signal(sc, curphase, mask)
	struct nsp_softc *sc;
	u_int8_t curphase, mask;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int rv = -1;
	int s;
	int tout = 0;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif
	u_int8_t ph, isrc;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, hz/2);
#else
	timeout(settimeout, &tout, hz/2);
#endif
	do 
	{
		ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (ph == 0xff) {
			rv = -1;
			break;
		}
		isrc = bus_space_read_1(bst, bsh, nsp_irqsr);
		if (isrc & IRQSR_SCSI) {
			rv = 0;
			break;
		}
		if ((ph & mask) != 0 && (ph & SCBUSMON_PHMASK) == curphase) {
			rv = 1;
			break;
		}
	}
	while (tout == 0);

	s = splhigh();
	if (tout == 0) {
#ifdef __FreeBSD__
		untimeout(settimeout, &tout, ch);
#else
		untimeout(settimeout, &tout);
#endif
		splx(s);
	} else {
		splx(s);
		printf("%s: nsp_expect_signal timeout\n", slp->sl_xname);
		rv = -1;
	}

	return rv;
}

static void
nsphw_init(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;

	/* block all interrupts */
	bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_ALLMASK);

	/* setup SCSI interface */
	bus_space_write_1(bst, bsh, nsp_ifselr, IFSELR_IFSEL);

	nsp_cr_write_1(bst, bsh, NSPR_SCIENR, 0);

	nsp_cr_write_1(bst, bsh, NSPR_XFERMR, XFERMR_IO8);
	nsp_cr_write_1(bst, bsh, NSPR_CLKDIVR, sc->sc_iclkdiv);

	nsp_cr_write_1(bst, bsh, NSPR_SCIENR, sc->sc_icr);
	nsp_cr_write_1(bst, bsh, NSPR_PARITYR, 0);
	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR,
		       PTCLRR_ACK | PTCLRR_REQ | PTCLRR_HOST | PTCLRR_RSS);

	/* setup fifo asic */
	bus_space_write_1(bst, bsh, nsp_ifselr, IFSELR_REGSEL);
	nsp_cr_write_1(bst, bsh, NSPR_TERMPWRC, 0);
	if ((nsp_cr_read_1(bst, bsh, NSPR_OCR) & OCR_TERMPWRS) == 0)
		nsp_cr_write_1(bst, bsh, NSPR_TERMPWRC, TERMPWRC_POWON);

	nsp_cr_write_1(bst, bsh, NSPR_XFERMR, XFERMR_IO8);
	nsp_cr_write_1(bst, bsh, NSPR_CLKDIVR, sc->sc_clkdiv);
	nsp_cr_write_1(bst, bsh, NSPR_TIMERCNT, 0);
	nsp_cr_write_1(bst, bsh, NSPR_TIMERCNT, 0);

	nsp_cr_write_1(bst, bsh, NSPR_SYNCR, 0);
	nsp_cr_write_1(bst, bsh, NSPR_ACKWIDTH, 0);

	/* enable interrupts and ack them */
	nsp_cr_write_1(bst, bsh, NSPR_SCIENR, SCIENR_SCCHG | SCIENR_RESEL | SCIENR_RST);
	bus_space_write_1(bst, bsh, nsp_irqcr, IRQSR_MASK);

	nsp_setup_fifo(sc, 0);
}

/****************************************************
 * scsi low interface
 ****************************************************/
static void
nsphw_attention(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int8_t cr;

	cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR)/*  & ~SCBUSCR_ACK */;
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr | SCBUSCR_ATN);
}

static void
nsphw_bus_reset(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int i;

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_ALLMASK);

	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, SCBUSCR_RST);
	delay(100 * 1000);	/* 100ms */
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, 0);
	for (i = 0; i < 5; i ++)
		(void) nsp_cr_read_1(bst, bsh, NSPR_IRQPHS);

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQSR_MASK);
}

static int
nsphw_start_selection(sc, cb)
	struct nsp_softc *sc;
	struct slccb *cb;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct targ_info *ti = cb->ti;
	register u_int8_t arbs, ph;
	int s;
	int tout = 0;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif

	/* check bus free */
	if (slp->sl_disc > 0)
	{
		s = splhigh();
		ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (ph != SCBUSMON_FREE)
		{
			splx(s);
			return SCSI_LOW_START_FAIL;
		}
		splx(s);
	}

	/* start arbitration */
	SCSI_LOW_SETUP_PHASE(ti, PH_ARBSTART);
	nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_EXEC);
#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	do 
	{
		/* XXX: what a stupid chip! */
		arbs = nsp_cr_read_1(bst, bsh, NSPR_ARBITS);
		delay(1);
	} 
	while ((arbs & (ARBITS_WIN | ARBITS_FAIL)) == 0 && tout == 0);

	s = splhigh();
	if (tout == 0) {
#ifdef __FreeBSD__
		untimeout(settimeout, &tout, ch);
#else
		untimeout(settimeout, &tout);
#endif
	}
	splx(s);

	if ((arbs & ARBITS_WIN) == 0)
	{
		nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_CLR);
		return SCSI_LOW_START_FAIL;
	}

	/* assert select line */
	SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
	scsi_low_arbit_win(slp, ti);
	delay(3);
	nsp_cr_write_1(bst, bsh, NSPR_DATA,
		       sc->sc_idbit | (1 << ti->ti_id));
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
			SCBUSCR_SEL | SCBUSCR_BSY | sc->sc_busc);
	delay(3);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, SCBUSCR_SEL |
			SCBUSCR_BSY | SCBUSCR_DOUT | sc->sc_busc);
	nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_CLR);
	delay(3);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
		       SCBUSCR_SEL | SCBUSCR_DOUT | sc->sc_busc);

	/* check selection timeout */
	nsp_start_timer(sc, 1000 / 51);
	sc->sc_seltout = 1;

	return SCSI_LOW_START_OK;
}

static int
nsp_world_start(sc, fdone)
	struct nsp_softc *sc;
	int fdone;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	intrmask_t s;

	s = splcam();
	sc->sc_cnt = 0;
	sc->sc_seltout = 0;
	if ((slp->sl_cfgflags & CFG_NOATTEN) == 0)
		sc->sc_busc = SCBUSCR_ATN;
	else
		sc->sc_busc = 0;
	sc->sc_icr = (SCIENR_SCCHG | SCIENR_RESEL | SCIENR_RST);

	nsphw_init(sc);
	scsi_low_bus_reset(slp);
	splx(s);

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

struct ncp_synch_data {
	u_int min_period;
	u_int max_period;
	u_int chip_period;
	u_int ack_width;
};

static struct ncp_synch_data ncp_sync_data_40M[] = {
	{0x0c,0x0c,0x1,0},	/* 20MB  50ns*/
	{0x19,0x19,0x3,1}, 	/* 10MB  100ns*/ 
	{0x1a,0x25,0x5,2},	/* 7.5MB 150ns*/ 
	{0x26,0x32,0x7,3},	/* 5MB   200ns*/
	{0x0, 0, 0, 0}
};

static struct ncp_synch_data ncp_sync_data_20M[] = {
	{0x19,0x19,0x1,0}, 	/* 10MB  100ns*/ 
	{0x1a,0x25,0x2,0},	/* 7.5MB 150ns*/ 
	{0x26,0x32,0x3,1},	/* 5MB   200ns*/
	{0x0, 0, 0, 0}
};

static int
nsp_msg(sc, ti, msg)
	struct nsp_softc *sc;
	struct targ_info *ti;
	u_int msg;
{
	struct ncp_synch_data *sdp;
	struct lun_info *li = ti->ti_li;
	struct nsp_lun_info *nli = (void *) li;
	u_int period, offset;
	int i;

	if ((msg & SCSI_LOW_MSG_SYNCH) == 0)
		return 0;

	period = li->li_maxsynch.period;
	offset = li->li_maxsynch.offset;
	if (sc->sc_iclkdiv == CLKDIVR_20M)
		sdp = &ncp_sync_data_20M[0];
	else
		sdp = &ncp_sync_data_40M[0];

	for (i = 0; sdp->max_period != 0; i ++, sdp ++)
	{
		if (period >= sdp->min_period && period <= sdp->max_period)
			break;
	}

	if (period != 0 && sdp->max_period == 0)
	{
		/*
		 * NO proper period/offset found,
		 * Retry neg with the target.
		 */
		li->li_maxsynch.period = 0;
		li->li_maxsynch.offset = 0;
		nli->nli_reg_syncr = 0;
		nli->nli_reg_ackwidth = 0;
		return EINVAL;
	}

	nli->nli_reg_syncr = (sdp->chip_period << SYNCR_PERS) |
			      (offset & SYNCR_OFFM);
	nli->nli_reg_ackwidth = sdp->ack_width;
	return 0;
}

static int
nsp_lun_init(sc, ti, li)
	struct nsp_softc *sc;
	struct targ_info *ti;
	struct lun_info *li;
{
	struct nsp_lun_info *nli = (void *) li;

	li->li_maxsynch.period = 200 / 4;
	li->li_maxsynch.offset = 15;
	nli->nli_reg_syncr = 0;
	nli->nli_reg_ackwidth = 0;
	return 0;
}	

static __inline void
nsp_start_timer(sc, time)
	struct nsp_softc *sc;
	int time;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;

	sc->sc_timer = time;
	nsp_cr_write_1(bst, bsh, NSPR_TIMERCNT, time);
}

/**************************************************************
 * General probe attach
 **************************************************************/
int
nspprobesubr(iot, ioh, dvcfg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int dvcfg;
{
	u_int8_t regv;

	regv = bus_space_read_1(iot, ioh, nsp_fifosr);
	if (regv < 0x11 || regv >= 0x20)
		return 0;
	return 1;
}

int
nspprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
nspattachsubr(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	printf("\n");

	sc->sc_idbit = (1 << slp->sl_hostid);
	slp->sl_funcs = &nspfuncs;
	if (sc->sc_memh != NULL)
		sc->sc_xmode = NSP_MID_SMIT;
	else
		sc->sc_xmode = NSP_PIO;

	(void) scsi_low_attach(slp, 2, NSP_NTARGETS, NSP_NLUNS,
			       sizeof(struct nsp_lun_info));
}

/**************************************************************
 * PDMA functions
 **************************************************************/
static u_int
nsp_fifo_count(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int count;

	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_PT);
	count = bus_space_read_1(bst, bsh, nsp_datar);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 8);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 16);
	return count;
}

static void
nsp_setup_fifo(sc, on)
	struct nsp_softc *sc;
	int on;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int8_t xfermode;

	if (on != 0)
		xfermode = XFERMR_XEN | XFERMR_FIFOEN;
	else
		xfermode = 0;

	if ((slp->sl_scp.scp_datalen % DEV_BSIZE) != 0)
	{
		sc->sc_mask = 0;
		xfermode |= XFERMR_IO8;
	}
	else
	{
		sc->sc_mask = 3;
		if (sc->sc_xmode == NSP_MID_SMIT)
			xfermode |= XFERMR_MEM32;
		else
			xfermode |= XFERMR_IO32;
	}

	sc->sc_xfermr = xfermode;
	nsp_cr_write_1(bst, bsh, NSPR_XFERMR, sc->sc_xfermr);
}

static __inline void
nsp_pdma_end(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = ti->ti_nexus;
	u_int len = 0, cnt;

	slp->sl_flags &= ~HW_PDMASTART;
	nsp_setup_fifo(sc, 0);

	if (ti->ti_phase == PH_DATA)
	{
		cnt = nsp_fifo_count(sc);
		if (slp->sl_scp.scp_direction  == SCSI_LOW_WRITE)
		{
			len = sc->sc_cnt - cnt;
			if (slp->sl_scp.scp_datalen + len <= 
			    cb->ccb_scp.scp_datalen)
			{
				slp->sl_scp.scp_data -= len;
				slp->sl_scp.scp_datalen += len;
			}
			else
			{
				slp->sl_error |= PDMAERR;
				printf("%s len %x >= datalen %x\n",
					slp->sl_xname,
					len, slp->sl_scp.scp_datalen);
			}
		}
		else if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
		{
			if (sc->sc_cnt != cnt)
			{
				slp->sl_error |= PDMAERR;
				printf("%s: data read count error %x != %x\n",
					slp->sl_xname, sc->sc_cnt, cnt);
			}
		}
		sc->sc_cnt = cnt;
	}
	else
	{

		printf("%s data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}
}

#define	RFIFO_CRIT	64
#define	WFIFO_CRIT	64

static void
nsp_pio_read(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int s;
	int tout = 0;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif
	u_int res, ocount, mask = sc->sc_mask;
	u_int8_t stat, fstat;

	slp->sl_flags |= HW_PDMASTART;
	ocount = sc->sc_cnt;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (slp->sl_scp.scp_datalen > 0 && tout == 0)
	{
		stat = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		stat &= SCBUSMON_PHMASK;
		res = nsp_fifo_count(sc) - ocount;
		if (res == 0)
		{
			if (stat == PHASE_DATAIN)
				continue;
			break;
		}

		fstat = bus_space_read_1(bst, bsh, nsp_fifosr);
		if ((fstat & FIFOSR_FULLEMP) == 0 && stat == PHASE_DATAIN)
			continue;

		if (res > slp->sl_scp.scp_datalen)
			break;

		if (res >= NSP_BUFFER_SIZE)
			res = NSP_BUFFER_SIZE;
		else
			res &= ~mask;

		if (sc->sc_xfermr & XFERMR_MEM32)
		{
			bus_space_read_region_4(sc->sc_memt, 
						sc->sc_memh,
						0, 
					(u_int32_t *) slp->sl_scp.scp_data, 
						res >> 2);
		}
		else
		{
			if (mask != 0)
				bus_space_read_multi_4(bst, bsh, nsp_fifodr,
					(u_int32_t *) slp->sl_scp.scp_data,
						       res >> 2);
			else 
				bus_space_read_multi_1(bst, bsh, nsp_fifodr,
					(u_int8_t *) slp->sl_scp.scp_data,
						       res);
		}

		slp->sl_scp.scp_data += res;
		slp->sl_scp.scp_datalen -= res;
		ocount += res;
	}

	sc->sc_cnt = ocount;
	s = splhigh();
	if (tout == 0) {
#ifdef __FreeBSD__
		untimeout(settimeout, &tout, ch);
#else
		untimeout(settimeout, &tout);
#endif
		splx(s);
	} else {
		splx(s);
		printf("%s pio read timeout\n", slp->sl_xname);
	}
}

static void
nsp_pio_write(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int res, ocount, mask = sc->sc_mask;
	int s;
	int tout = 0;
	register u_int8_t stat;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif

	ocount = sc->sc_cnt;
	slp->sl_flags |= HW_PDMASTART;
#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (slp->sl_scp.scp_datalen > 0 && tout == 0)
	{
		stat = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		stat &= SCBUSMON_PHMASK;
		if (stat != PHASE_DATAOUT)
			break;

		res = ocount - nsp_fifo_count(sc);
		if (res > 0)
			continue;

		res = (slp->sl_scp.scp_datalen > WFIFO_CRIT) ? WFIFO_CRIT :
						  slp->sl_scp.scp_datalen;

		if (sc->sc_xfermr & XFERMR_MEM32)
		{
			bus_space_write_region_4(sc->sc_memt, 
						 sc->sc_memh,
						 0,
					(u_int32_t *) slp->sl_scp.scp_data, 
						 res >> 2);
		}
		else
		{
			if (mask != 0)
				bus_space_write_multi_4(bst, bsh, nsp_fifodr,
				(u_int32_t *) slp->sl_scp.scp_data, res >> 2);
			else
				bus_space_write_multi_1(bst, bsh, nsp_fifodr,
				(u_int8_t *) slp->sl_scp.scp_data, res);
		}

		slp->sl_scp.scp_datalen -= res;
		slp->sl_scp.scp_data += res;
		ocount += res;
	}

	sc->sc_cnt = ocount;
	s = splhigh();
	if (tout == 0) {
#ifdef __FreeBSD__
		untimeout(settimeout, &tout, ch);
#else
		untimeout(settimeout, &tout);
#endif
		splx(s);
	} else {
		splx(s);
		printf("%s pio write timeout\n", slp->sl_xname);
	}
}

static void
settimeout(arg)
	void *arg;
{
	int *tout = arg;

	*tout = 1;
}

static int
nsp_negate_signal(sc, mask, s)
	struct nsp_softc *sc;
	u_int8_t mask;
	u_char *s;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int tout = 0;
	int s;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif
	u_int8_t regv;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, hz/2);
#else
	timeout(settimeout, &tout, hz/2);
#endif
	do 
	{
		regv = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (regv == 0xff)
			break;
	}
	while ((regv & mask) != 0 && tout == 0);

	s = splhigh();
	if (tout == 0) {
#ifdef __FreeBSD__
		untimeout(settimeout, &tout, ch);
#else
		untimeout(settimeout, &tout);
#endif
		splx(s);
	} else {
		splx(s);
		printf("%s: %s singla off timeout \n", slp->sl_xname, s);
	}

	return 0;
}

static int
nsp_xfer(sc, buf, len, phase)
	struct nsp_softc *sc;
	u_int8_t *buf;
	int len;
	int phase;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int ptr, rv, atn;

	atn = (scsi_low_is_msgout_continue(slp->sl_nexus) != 0);
	for (ptr = 0; len > 0; len --, ptr ++)
	{
		rv = nsp_expect_signal(sc, phase, SCBUSMON_REQ);
		if (rv <= 0)
			goto out;

		if (len == 1 && atn == 0)
		{
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
				       SCBUSCR_ADIR | SCBUSCR_ACKEN);
		}

		if (phase & SCBUSMON_IO)
		{
			buf[ptr] = nsp_cr_read_1(bst, bsh, NSPR_DATAACK);
		}
		else
		{
			nsp_cr_write_1(bst, bsh, NSPR_DATAACK, buf[ptr]);
		}
		nsp_negate_signal(sc, SCBUSMON_ACK, "xfer<ACK>");
	}

out:
	return len;
}

static int
nsp_dataphase_bypass(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = ti->ti_nexus;
	u_int cnt;

	if (slp->sl_scp.scp_direction != SCSI_LOW_READ ||
	    (slp->sl_scp.scp_datalen % DEV_BSIZE) == 0)
		return 0;

	cnt = nsp_fifo_count(sc);
	if (sc->sc_cnt == cnt)
		return 0;
	if (cnt >= DEV_BSIZE)
		return EINVAL;

	if (cb == NULL)
		return 0;

	/*
	 * XXX: NSP_QUIRK
	 * Data phase skip only occures in case of SCSI_LOW_READ.
	 */
	SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
	nsp_pio_read(sc, ti);
	nsp_pdma_end(sc, ti);
#ifdef	NSP_STATICS
	nsp_statics[ti->ti_id].data_phase_bypass ++;
#endif	/* NSP_STATICS */
	return 0;
}	
	    
/**************************************************************
 * disconnect & reselect (HW low)
 **************************************************************/
static int
nsp_reselected(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct targ_info *ti;
	u_int sid;
	u_int8_t cr;

	sid = (u_int) nsp_cr_read_1(bst, bsh, NSPR_RESELR);
	sid &= ~sc->sc_idbit;
	sid = ffs(sid) - 1;
	if ((ti = scsi_low_reselected(slp, sid)) == NULL)
		return EJUSTRETURN;

	nsp_negate_signal(sc, SCBUSMON_SEL, "reselect<SEL>");

	cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR) & ~(SCBUSCR_BSY | SCBUSCR_ATN);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr | SCBUSCR_ADIR | SCBUSCR_ACKEN);

#ifdef	NSP_STATICS
	nsp_statics[sid].reselect ++;
#endif	/* NSP_STATCIS */
	return EJUSTRETURN;
}

static __inline int
nsp_disconnected(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

#ifdef	NSP_STATICS
	if (slp->sl_msgphase == MSGPH_DISC)
		nsp_statics[ti->ti_id].disconnect ++;
#endif	/* NSP_STATICS */

	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static void nspmsg __P((struct nsp_softc *, u_char *, u_int8_t, u_int8_t, u_int8_t));

static void
nspmsg(sc, s, isrc, ph, irqphs)
	struct nsp_softc *sc;
	u_char *s;
	u_int8_t isrc, ph, irqphs;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	printf("%s: %s\n", slp->sl_xname, s);
	printf("%s: isrc 0x%x scmon 0x%x irqphs 0x%x\n",
	       slp->sl_xname, (u_int) isrc, (u_int) ph, (u_int) irqphs);
}

static int
nsp_nexus(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct nsp_lun_info *nli = (void *) ti->ti_li;

	/* setup synch transfer registers */
	nsp_cr_write_1(bst, bsh, NSPR_SYNCR, nli->nli_reg_syncr);
	nsp_cr_write_1(bst, bsh, NSPR_ACKWIDTH, nli->nli_reg_ackwidth);

	/* setup pdma fifo */
	nsp_setup_fifo(sc, 1);

	/* clear ack counter */
	sc->sc_cnt = 0;
	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_PT | PTCLRR_ACK |
			PTCLRR_REQ | PTCLRR_HOST);
	return 0;
}

int
nspintr(arg)
	void *arg;
{
	struct nsp_softc *sc = arg;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	int len, rv;
	u_int8_t isrc, ph, irqphs, cr, regv;

	/*******************************************
	 * interrupt check
	 *******************************************/
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_IRQDIS);
	isrc = bus_space_read_1(bst, bsh, nsp_irqsr);
	if (isrc == 0xff || (isrc & IRQSR_MASK) == 0)
	{
		bus_space_write_1(bst, bsh, nsp_irqcr, 0);
		return 0;
	}

	/* XXX: IMPORTANT
 	 * Do not read an irqphs register if no scsi phase interrupt.
	 * Unless, you should lose a scsi phase interrupt.
	 */
	ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
	if ((isrc & IRQSR_SCSI) != 0)
	{
		irqphs = nsp_cr_read_1(bst, bsh, NSPR_IRQPHS);
	}
	else
		irqphs = 0;

	/*
 	 * timer interrupt handler (scsi vs timer interrupts)
	 */
	if (sc->sc_timer != 0)
	{
		nsp_cr_write_1(bst, bsh, NSPR_TIMERCNT, 0);
		nsp_cr_write_1(bst, bsh, NSPR_TIMERCNT, 0);
		sc->sc_timer = 0;
	}
	
	if ((isrc & IRQSR_MASK) == IRQSR_TIMER && sc->sc_seltout == 0)
	{
		bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_TIMERCL);
		return 1;
	}

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_TIMERCL | IRQCR_FIFOCL);

	/*******************************************
	 * debug section
	 *******************************************/
#ifdef	NSP_DEBUG
	if (nsp_debug)
	{
		nspmsg(sc, "current status", isrc, ph, irqphs);
		scsi_low_print(slp, NULL);
		if (nsp_debug > 1)
			Debugger();
	}
#endif	/* NSP_DEBUG */

	/*******************************************
	 * Parse hardware SCSI irq reasons register
	 *******************************************/
	if ((isrc & IRQSR_SCSI) != 0)
	{
 		if ((irqphs & IRQPHS_RST) != 0)
		{
			scsi_low_restart(slp, SCSI_LOW_RESTART_SOFT, 
					 "bus reset (power off?)");
			return 1;
		}

		if ((irqphs & IRQPHS_RSEL) != 0)
		{
			bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_RESCL);
			if (nsp_reselected(sc) == EJUSTRETURN)
				return 1;
		}

		if ((irqphs & (IRQPHS_PCHG | IRQPHS_LBF)) == 0)
			return 1; 
	}

	/*******************************************
	 * nexus check
	 *******************************************/
	if ((ti = slp->sl_nexus) == NULL)
	{
		/* unknown scsi phase changes */
		nspmsg(sc, "unknown scsi phase changes", isrc, ph, irqphs);
		return 0;
	}

	/*******************************************
	 * aribitration & selection
	 *******************************************/
	switch (ti->ti_phase)
	{
	case PH_SELSTART:
		if ((ph & SCBUSMON_BSY) == 0)
		{
			if (sc->sc_seltout >= NSP_SELTIMEOUT)
			{
				sc->sc_seltout = 0;
				nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, 0);
				return nsp_disconnected(sc, ti);
			}
			sc->sc_seltout ++;
			nsp_start_timer(sc, 1000 / 51);
			return 1;
		}

		/* attention assert */
		sc->sc_seltout = 0;
		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, sc->sc_busc);
		delay(1);
		nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, 
				sc->sc_busc | SCBUSCR_ADIR | SCBUSCR_ACKEN);

		SCSI_LOW_TARGET_ASSERT_ATN(ti);
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_IDENTIFY, 0);
		return 1;

	case PH_RESEL:
		if ((ph & SCBUSMON_PHMASK) != PHASE_MSGIN)
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			return 1;
		}
		/* fall */

	default:
		if ((isrc & (IRQSR_SCSI | IRQSR_FIFO)) == 0)
			return 1;
		break;
	}

	/*******************************************
	 * scsi seq
	 *******************************************/
	if (slp->sl_flags & HW_PDMASTART)
		nsp_pdma_end(sc, ti);

	/* normal disconnect */
	if (slp->sl_msgphase != 0 && (irqphs & IRQPHS_LBF) != 0)
		return nsp_disconnected(sc, ti);

	/* check unexpected bus free state */
	if (ph == 0)
	{
		nspmsg(sc, "unexpected bus free", isrc, ph, irqphs);
		return nsp_disconnected(sc, ti);
	}
		
	/* check normal scsi phase */
	switch (ph & SCBUSMON_PHMASK)
	{
	case PHASE_CMD:
		if ((ph & SCBUSMON_REQ) == 0)
			return 1;

		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
			break;

		nsp_cr_write_1(bst, bsh, NSPR_CMDCR, CMDCR_PTCLR);
		for (len = 0; len < slp->sl_scp.scp_cmdlen; len ++)
			nsp_cr_write_1(bst, bsh, NSPR_CMDDR,
				       slp->sl_scp.scp_cmd[len]);

		nsp_cr_write_1(bst, bsh, NSPR_CMDCR, CMDCR_PTCLR | CMDCR_EXEC);
		break;

	case PHASE_DATAOUT:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
			break;
	
		pp = physio_proc_enter(bp);
		nsp_pio_write(sc, ti);
		physio_proc_leave(pp);
		break;

	case PHASE_DATAIN:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
			break;

		pp = physio_proc_enter(bp);
		nsp_pio_read(sc, ti);
		physio_proc_leave(pp);
		break;

	case PHASE_STATUS:
		nsp_dataphase_bypass(sc, ti);
		if ((ph & SCBUSMON_REQ) == 0)
			return 1;

		SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
		ti->ti_status = nsp_cr_read_1(bst, bsh, NSPR_DATAACK);
		break;

	case PHASE_MSGOUT:
		if ((ph & SCBUSMON_REQ) == 0)
			goto timerout;

		/*
		 * XXX: NSP QUIRK
		 * NSP invoke interrupts only in the case of scsi phase changes,
		 * therefore we should poll the scsi phase here to catch 
		 * the next "msg out" if exists (no scsi phase changes).
		 */
		rv = len = 16;
		do {
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);

			len = scsi_low_msgout(slp, ti);
			if (nsp_xfer(sc, ti->ti_msgoutstr, len, PHASE_MSGOUT))
			{
				scsi_low_assert_msg(slp, ti, 
						    SCSI_LOW_MSG_RESET, 0);
				nspmsg(sc, "MSGOUT: xfer short",
						    isrc, ph, irqphs);
			}

			/* catch a next signal */
			rv = nsp_expect_signal(sc, PHASE_MSGOUT, SCBUSMON_REQ);
		}
		while (rv > 0 && len -- > 0);
		break;

	case PHASE_MSGIN:
		nsp_dataphase_bypass(sc, ti);
		if ((ph & SCBUSMON_REQ) == 0)
			goto timerout;

		SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

		/*
		 * XXX: NSP QUIRK
		 * NSP invoke interrupts only in the case of scsi phase changes,
		 * therefore we should poll the scsi phase here to catch 
		 * the next "msg in" if exists (no scsi phase changes).
		 */
		rv = len = 16;
		do {
			/* read a data */
			regv = nsp_cr_read_1(bst, bsh, NSPR_DATA);

			/* assert ack */
			cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR);
			cr |= SCBUSCR_ACK;
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);
			nsp_negate_signal(sc, SCBUSMON_REQ, "msgin<REQ>");

			scsi_low_msgin(slp, ti, regv);

			/* deassert ack */
			cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR);
			cr &= ~SCBUSCR_ACK;
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);

			/* catch a next signal */
			rv = nsp_expect_signal(sc, PHASE_MSGIN, SCBUSMON_REQ);
		} 
		while (rv > 0 && len -- > 0);
		break;

	case PHASE_SEL:
	default:
		nspmsg(sc, "unknown scsi phase", isrc, ph, irqphs);
		break;
	}

	return 1;

timerout:
	nsp_start_timer(sc, 1000 / 102);
	return 0;
}
