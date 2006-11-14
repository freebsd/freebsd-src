/*	$NecBSD: nsp.c,v 1.21.12.6 2001/06/29 06:27:52 honda Exp $	*/
/*	$NetBSD$	*/

#define	NSP_DEBUG
#define	NSP_STATICS
#define	NSP_IO_CONTROL_FLAGS \
	(NSP_READ_SUSPEND_IO | NSP_WRITE_SUSPEND_IO | \
	 NSP_READ_FIFO_INTERRUPTS | NSP_WRITE_FIFO_INTERRUPTS | \
	 NSP_USE_MEMIO | NSP_WAIT_FOR_SELECT)

/*-
 *  Copyright (c) 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1998, 1999, 2000, 2001
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <i386/Cbus/dev/nspreg.h>
#include <i386/Cbus/dev/nspvar.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/bus.h>

#include <compat/netbsd/dvcfg.h>
#include <compat/netbsd/physio_proc.h>

#include <cam/scsi/scsi_low.h>
#include <dev/nsp/nspreg.h>
#include <dev/nsp/nspvar.h>
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

#define	NSP_MAX_DATA_SIZE	(64 * 1024)
#define	NSP_SELTIMEOUT		(200)
#define	NSP_DELAY_MAX		(2 * 1000 * 1000)
#define	NSP_DELAY_INTERVAL	(1)
#define	NSP_TIMER_1MS		(1000 / 51)

/***************************************************
 * DEBUG
 ***************************************************/
#ifdef	NSP_DEBUG
int nsp_debug;
#endif	/* NSP_DEBUG */

#ifdef	NSP_STATICS
struct nsp_statics {
	int arbit_conflict_1;
	int arbit_conflict_2;
	int device_data_write;
	int device_busy;
	int disconnect;
	int reselect;
	int data_phase_bypass;
} nsp_statics;
#endif	/* NSP_STATICS */

/***************************************************
 * IO control
 ***************************************************/
#define	NSP_READ_SUSPEND_IO		0x0001
#define	NSP_WRITE_SUSPEND_IO		0x0002
#define	NSP_USE_MEMIO			0x0004
#define	NSP_READ_FIFO_INTERRUPTS	0x0010
#define	NSP_WRITE_FIFO_INTERRUPTS	0x0020
#define	NSP_WAIT_FOR_SELECT		0x0100

u_int nsp_io_control = NSP_IO_CONTROL_FLAGS;
int nsp_read_suspend_bytes = DEV_BSIZE;
int nsp_write_suspend_bytes = DEV_BSIZE;
int nsp_read_interrupt_bytes = 4096;
int nsp_write_interrupt_bytes = 4096;

/***************************************************
 * DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver nsp_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
#define	NSP_FIFO_ON	1
#define	NSP_FIFO_OFF	0
static void nsp_pio_read(struct nsp_softc *, int);
static void nsp_pio_write(struct nsp_softc *, int);
static int nsp_xfer(struct nsp_softc *, u_int8_t *, int, int, int);
static int nsp_msg(struct nsp_softc *, struct targ_info *, u_int);
static int nsp_reselected(struct nsp_softc *);
static int nsp_disconnected(struct nsp_softc *, struct targ_info *);
static void nsp_pdma_end(struct nsp_softc *, struct targ_info *);
static void nsphw_init(struct nsp_softc *);
static int nsp_target_nexus_establish(struct nsp_softc *);
static int nsp_lun_nexus_establish(struct nsp_softc *);
static int nsp_ccb_nexus_establish(struct nsp_softc *);
static int nsp_world_start(struct nsp_softc *, int);
static int nsphw_start_selection(struct nsp_softc *sc, struct slccb *);
static void nsphw_bus_reset(struct nsp_softc *);
static void nsphw_attention(struct nsp_softc *);
static u_int nsp_fifo_count(struct nsp_softc *);
static u_int nsp_request_count(struct nsp_softc *);
static int nsp_negate_signal(struct nsp_softc *, u_int8_t, u_char *);
static int nsp_expect_signal(struct nsp_softc *, u_int8_t, u_int8_t);
static void nsp_start_timer(struct nsp_softc *, int);
static void nsp_setup_fifo(struct nsp_softc *, int, int, int);
static int nsp_targ_init(struct nsp_softc *, struct targ_info *, int);
static void nsphw_selection_done_and_expect_msgout(struct nsp_softc *);
static void nsp_data_padding(struct nsp_softc *, int, u_int);
static int nsp_timeout(struct nsp_softc *);
static int nsp_read_fifo(struct nsp_softc *, int);
static int nsp_write_fifo(struct nsp_softc *, int);
static int nsp_phase_match(struct nsp_softc *, u_int8_t, u_int8_t);
static int nsp_wait_interrupt(struct nsp_softc *);

struct scsi_low_funcs nspfuncs = {
	SC_LOW_INIT_T nsp_world_start,
	SC_LOW_BUSRST_T nsphw_bus_reset,
	SC_LOW_TARG_INIT_T nsp_targ_init,
	SC_LOW_LUN_INIT_T NULL,

	SC_LOW_SELECT_T nsphw_start_selection,
	SC_LOW_NEXUS_T nsp_lun_nexus_establish,
	SC_LOW_NEXUS_T nsp_ccb_nexus_establish,

	SC_LOW_ATTEN_T nsphw_attention,
	SC_LOW_MSG_T nsp_msg,

	SC_LOW_TIMEOUT_T nsp_timeout,
	SC_LOW_POLL_T nspintr,

	NULL,
};

/****************************************************
 * hwfuncs
 ****************************************************/
static __inline u_int8_t nsp_cr_read_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ofs);
static __inline void nsp_cr_write_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_addr_t ofs, u_int8_t va);

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
	int wc;
	u_int8_t ph, isrc;

	for (wc = 0; wc < NSP_DELAY_MAX / NSP_DELAY_INTERVAL; wc ++)
	{
		ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (ph == (u_int8_t) -1)
			return -1;

		isrc = bus_space_read_1(bst, bsh, nsp_irqsr);
		if (isrc & IRQSR_SCSI)
			return 0;

		if ((ph & mask) != 0 && (ph & SCBUSMON_PHMASK) == curphase)
			return 1;

		SCSI_LOW_DELAY(NSP_DELAY_INTERVAL);
	}

	printf("%s: nsp_expect_signal timeout\n", slp->sl_xname);
	return -1;
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
	nsp_cr_write_1(bst, bsh, NSPR_PARITYR, sc->sc_parr);
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
	nsp_cr_write_1(bst, bsh, NSPR_SCIENR, sc->sc_icr);
	bus_space_write_1(bst, bsh, nsp_irqcr, IRQSR_MASK);

	nsp_setup_fifo(sc, NSP_FIFO_OFF, SCSI_LOW_READ, 0);
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
	SCSI_LOW_DELAY(10);
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
	SCSI_LOW_DELAY(100 * 1000);	/* 100ms */
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, 0);
	for (i = 0; i < 5; i ++)
		(void) nsp_cr_read_1(bst, bsh, NSPR_IRQPHS);

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQSR_MASK);
}

static void
nsphw_selection_done_and_expect_msgout(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;

	/* clear ack counter */
	sc->sc_cnt = 0;
	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_PT | PTCLRR_ACK |
			PTCLRR_REQ | PTCLRR_HOST);

	/* deassert sel and assert atten */
	sc->sc_seltout = 0;
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, sc->sc_busc);
	SCSI_LOW_DELAY(1);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, 
			sc->sc_busc | SCBUSCR_ADIR | SCBUSCR_ACKEN);
	SCSI_LOW_ASSERT_ATN(slp);
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
	int s, wc;

	wc = sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	sc->sc_dataout_timeout = 0;

	/* check bus free */
	s = splhigh();
	ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
	if (ph != SCBUSMON_FREE)
	{
		splx(s);
#ifdef	NSP_STATICS
		nsp_statics.arbit_conflict_1 ++;
#endif	/* NSP_STATICS */
		return SCSI_LOW_START_FAIL;
	}

	/* start arbitration */
	nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_EXEC);
	splx(s);

	SCSI_LOW_SETUP_PHASE(ti, PH_ARBSTART);
	do 
	{
		/* XXX: what a stupid chip! */
		arbs = nsp_cr_read_1(bst, bsh, NSPR_ARBITS);
		SCSI_LOW_DELAY(1);
	} 
	while ((arbs & (ARBITS_WIN | ARBITS_FAIL)) == 0 && wc -- > 0);

	if ((arbs & ARBITS_WIN) == 0)
	{
		nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_CLR);
#ifdef	NSP_STATICS
		nsp_statics.arbit_conflict_2 ++;
#endif	/* NSP_STATICS */
		return SCSI_LOW_START_FAIL;
	}

	/* assert select line */
	SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
	scsi_low_arbit_win(slp);

	s = splhigh();
	SCSI_LOW_DELAY(3);
	nsp_cr_write_1(bst, bsh, NSPR_DATA,
		       sc->sc_idbit | (1 << ti->ti_id));
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
			SCBUSCR_SEL | SCBUSCR_BSY | sc->sc_busc);
	SCSI_LOW_DELAY(3);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, SCBUSCR_SEL |
			SCBUSCR_BSY | SCBUSCR_DOUT | sc->sc_busc);
	nsp_cr_write_1(bst, bsh, NSPR_ARBITS, ARBITS_CLR);
	SCSI_LOW_DELAY(3);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
		       SCBUSCR_SEL | SCBUSCR_DOUT | sc->sc_busc);
	SCSI_LOW_DELAY(1);

	if ((nsp_io_control & NSP_WAIT_FOR_SELECT) != 0)
	{
#define	NSP_FIRST_SEL_WAIT	300
#define	NSP_SEL_CHECK_INTERVAL	10

		/* wait for a selection response */
		for (wc = 0; wc < NSP_FIRST_SEL_WAIT / NSP_SEL_CHECK_INTERVAL;
		     wc ++)
		{
			ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
			if ((ph & SCBUSMON_BSY) == 0)
			{
				SCSI_LOW_DELAY(NSP_SEL_CHECK_INTERVAL);
				continue;
			}

			SCSI_LOW_DELAY(1);
			ph = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
			if ((ph & SCBUSMON_BSY) != 0)
			{
				nsphw_selection_done_and_expect_msgout(sc);
				splx(s);

				SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
				return SCSI_LOW_START_OK;
			}
		}
	}
	splx(s);

	/* check a selection timeout */
	nsp_start_timer(sc, NSP_TIMER_1MS);
	sc->sc_seltout = 1;
	return SCSI_LOW_START_OK;
}

static int
nsp_world_start(sc, fdone)
	struct nsp_softc *sc;
	int fdone;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	sc->sc_cnt = 0;
	sc->sc_seltout = 0;

	if ((slp->sl_cfgflags & CFG_NOATTEN) == 0)
		sc->sc_busc = SCBUSCR_ATN;
	else
		sc->sc_busc = 0;

	if ((slp->sl_cfgflags & CFG_NOPARITY) == 0)
		sc->sc_parr = PARITYR_ENABLE | PARITYR_CLEAR;
	else
		sc->sc_parr = 0;

	sc->sc_icr = (SCIENR_SCCHG | SCIENR_RESEL | SCIENR_RST);

	nsphw_init(sc);
	scsi_low_bus_reset(slp);

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
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct ncp_synch_data *sdp;
	struct nsp_targ_info *nti = (void *) ti;
	u_int period, offset;
	int i, error;

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

	period = ti->ti_maxsynch.period;
	offset = ti->ti_maxsynch.offset;
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
		ti->ti_maxsynch.period = 0;
		ti->ti_maxsynch.offset = 0;
		nti->nti_reg_syncr = 0;
		nti->nti_reg_ackwidth = 0;
		error = EINVAL;
	}
	else
	{
		nti->nti_reg_syncr = (sdp->chip_period << SYNCR_PERS) |
				      (offset & SYNCR_OFFM);
		nti->nti_reg_ackwidth = sdp->ack_width;
		error = 0;
	}

	nsp_cr_write_1(bst, bsh, NSPR_SYNCR, nti->nti_reg_syncr);
	nsp_cr_write_1(bst, bsh, NSPR_ACKWIDTH, nti->nti_reg_ackwidth);
	return error;
}

static int
nsp_targ_init(sc, ti, action)
	struct nsp_softc *sc;
	struct targ_info *ti;
	int action;
{
	struct nsp_targ_info *nti = (void *) ti;

	if (action == SCSI_LOW_INFO_ALLOC || action == SCSI_LOW_INFO_REVOKE)
	{
		ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
		ti->ti_maxsynch.period = 100 / 4;
		ti->ti_maxsynch.offset = 15;
		nti->nti_reg_syncr = 0;
		nti->nti_reg_ackwidth = 0;
	}
	return 0;
}	

static void
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
	slp->sl_flags |= HW_READ_PADDING;
	slp->sl_funcs = &nspfuncs;
	sc->sc_tmaxcnt = SCSI_LOW_MIN_TOUT * 1000 * 1000; /* default */

	(void) scsi_low_attach(slp, 0, NSP_NTARGETS, NSP_NLUNS,
			       sizeof(struct nsp_targ_info), 0);
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

	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_RSS_ACK | PTCLRR_PT);
	count = bus_space_read_1(bst, bsh, nsp_datar);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 8);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 16);
	return count;
}

static u_int
nsp_request_count(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int count;

	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_RSS_REQ | PTCLRR_PT);
	count = bus_space_read_1(bst, bsh, nsp_datar);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 8);
	count += (((u_int) bus_space_read_1(bst, bsh, nsp_datar)) << 16);
	return count;
}

static void
nsp_setup_fifo(sc, on, direction, datalen)
	struct nsp_softc *sc;
	int on;
	int direction;
	int datalen;
{
	u_int8_t xfermode;

	sc->sc_suspendio = 0;
	if (on == NSP_FIFO_OFF)
	{
		xfermode = XFERMR_IO8;
		goto out;
	}

	/* check if suspend io OK ? */
	if (datalen > 0)
	{
		if (direction == SCSI_LOW_READ)
		{
			if ((nsp_io_control & NSP_READ_SUSPEND_IO) != 0 &&
			    (datalen % nsp_read_suspend_bytes) == 0)
				sc->sc_suspendio = nsp_read_suspend_bytes;
		}
		else
		{
			if ((nsp_io_control & NSP_WRITE_SUSPEND_IO) != 0 &&
			    (datalen % nsp_write_suspend_bytes) == 0)
				sc->sc_suspendio = nsp_write_suspend_bytes;
		}
	}

	/* determine a transfer type */
	if (datalen < DEV_BSIZE || (datalen & 3) != 0)
	{
		if (sc->sc_memh != 0 &&
		    (nsp_io_control & NSP_USE_MEMIO) != 0)
			xfermode = XFERMR_XEN | XFERMR_MEM8;
		else
			xfermode = XFERMR_XEN | XFERMR_IO8;
	}
	else
	{
		if (sc->sc_memh != 0 &&
		    (nsp_io_control & NSP_USE_MEMIO) != 0)
			xfermode = XFERMR_XEN | XFERMR_MEM32;
		else
			xfermode = XFERMR_XEN | XFERMR_IO32;

		if (sc->sc_suspendio > 0)
			xfermode |= XFERMR_FIFOEN;
	}

out:
	sc->sc_xfermr = xfermode;
	nsp_cr_write_1(sc->sc_iot, sc->sc_ioh, NSPR_XFERMR, sc->sc_xfermr);
}

static void
nsp_pdma_end(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = slp->sl_Qnexus;
	u_int len = 0, cnt;

	sc->sc_dataout_timeout = 0;
	slp->sl_flags &= ~HW_PDMASTART;
	nsp_setup_fifo(sc, NSP_FIFO_OFF, SCSI_LOW_READ, 0);
	if ((sc->sc_icr & SCIENR_FIFO) != 0)
	{
		sc->sc_icr &= ~SCIENR_FIFO;
		nsp_cr_write_1(sc->sc_iot, sc->sc_ioh, NSPR_SCIENR, sc->sc_icr);
	}

	if (cb == NULL)
	{
		slp->sl_error |= PDMAERR;
		return;
	}

	if (ti->ti_phase == PH_DATA)
	{
		cnt = nsp_fifo_count(sc);
		if (slp->sl_scp.scp_direction  == SCSI_LOW_WRITE)
		{
			len = sc->sc_cnt - cnt;
			if (sc->sc_cnt >= cnt &&
			    slp->sl_scp.scp_datalen + len <= 
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
			if (sc->sc_cnt != cnt ||
			    sc->sc_cnt > cb->ccb_scp.scp_datalen)
			{
				slp->sl_error |= PDMAERR;
				printf("%s: data read count error %x != %x (%x)\n",
					slp->sl_xname, sc->sc_cnt, cnt,
					cb->ccb_scp.scp_datalen);
			}
		}
		sc->sc_cnt = cnt;
		scsi_low_data_finish(slp);
	}
	else
	{

		printf("%s data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}
}

#define	RFIFO_CRIT	64
#define	WFIFO_CRIT	32

static void
nsp_data_padding(sc, direction, count)
	struct nsp_softc *sc;
	int direction;
	u_int count;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;

	if (count > NSP_MAX_DATA_SIZE)
		count = NSP_MAX_DATA_SIZE;

	nsp_cr_write_1(bst, bsh, NSPR_XFERMR, XFERMR_XEN | XFERMR_IO8);
	if (direction == SCSI_LOW_READ)
	{
		while (count -- > 0)
			(void) bus_space_read_1(bst, bsh, nsp_fifodr);
	}
	else
	{
		while (count -- > 0)
			(void) bus_space_write_1(bst, bsh, nsp_fifodr, 0);
	}
	nsp_cr_write_1(bst, bsh, NSPR_XFERMR, sc->sc_xfermr);
}

static int
nsp_read_fifo(sc, suspendio)
	struct nsp_softc *sc;
	int suspendio;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int res;

	res = nsp_fifo_count(sc);
	if (res == sc->sc_cnt)
		return 0;

#ifdef	NSP_DEBUG
	if (res < sc->sc_cnt || res == (u_int) -1)
	{
		printf("%s: strange fifo ack count 0x%x < 0x%x\n", 
			slp->sl_xname, res, sc->sc_cnt);
		return 0;
	}
#endif	/* NSP_DEBUG */

	res = res - sc->sc_cnt;
	if (res > slp->sl_scp.scp_datalen)
	{
		if ((slp->sl_error & PDMAERR) == 0)
		{
			printf("%s: data overrun 0x%x > 0x%x\n",
				slp->sl_xname, res, slp->sl_scp.scp_datalen);
		}

		slp->sl_error |= PDMAERR;
		slp->sl_scp.scp_datalen = 0;

		if ((slp->sl_flags & HW_READ_PADDING) == 0)
		{
			printf("%s: read padding required\n", slp->sl_xname);
			return 0;
		}

		nsp_data_padding(sc, SCSI_LOW_READ, res);
		sc->sc_cnt += res;
		return 1;	/* padding start */
	}

	if (suspendio > 0 && slp->sl_scp.scp_datalen >= suspendio)
		res = suspendio;

	if ((sc->sc_xfermr & (XFERMR_MEM32 | XFERMR_MEM8)) != 0)
	{
		if ((sc->sc_xfermr & XFERMR_MEM32) != 0)
		{
			res &= ~3;
			bus_space_read_region_4(sc->sc_memt, sc->sc_memh, 0, 
				(u_int32_t *) slp->sl_scp.scp_data, res >> 2);
		}
		else
		{
			bus_space_read_region_1(sc->sc_memt, sc->sc_memh, 0, 
				(u_int8_t *) slp->sl_scp.scp_data, res);
		}
	}
	else
	{
		if ((sc->sc_xfermr & XFERMR_IO32) != 0)
		{
			res &= ~3;
			bus_space_read_multi_4(bst, bsh, nsp_fifodr,
				(u_int32_t *) slp->sl_scp.scp_data, res >> 2);
		}
		else 
		{
			bus_space_read_multi_1(bst, bsh, nsp_fifodr,
				(u_int8_t *) slp->sl_scp.scp_data, res);
		}
	}

	if (nsp_cr_read_1(bst, bsh, NSPR_PARITYR) & PARITYR_PE)
	{
		nsp_cr_write_1(bst, bsh, NSPR_PARITYR, 
			       PARITYR_ENABLE | PARITYR_CLEAR);
		scsi_low_assert_msg(slp, slp->sl_Tnexus, SCSI_LOW_MSG_ERROR, 1);
	}

	slp->sl_scp.scp_data += res;
	slp->sl_scp.scp_datalen -= res;
	sc->sc_cnt += res;
	return 0;
}

static int
nsp_write_fifo(sc, suspendio)
	struct nsp_softc *sc;
	int suspendio;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int res;
	register u_int8_t stat;

	if (suspendio > 0)
	{
#ifdef	NSP_DEBUG
		if ((slp->sl_scp.scp_datalen % WFIFO_CRIT) != 0)
		{
			printf("%s: strange write length 0x%x\n",
				slp->sl_xname, slp->sl_scp.scp_datalen);
		}
#endif	/* NSP_DEBUG */
		res = slp->sl_scp.scp_datalen % suspendio;
		if (res == 0)
		{
			res = suspendio;
		}
	}
	else
	{
		res = WFIFO_CRIT;
	}

	if (res > slp->sl_scp.scp_datalen)
		res = slp->sl_scp.scp_datalen;

	/* XXX: reconfirm! */
	stat = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON) & SCBUSMON_PHMASK;
	if (stat != PHASE_DATAOUT)
		return 0;

	if ((sc->sc_xfermr & (XFERMR_MEM32 | XFERMR_MEM8)) != 0)
	{
		if ((sc->sc_xfermr & XFERMR_MEM32) != 0)
		{
			bus_space_write_region_4(sc->sc_memt, sc->sc_memh, 0,
				(u_int32_t *) slp->sl_scp.scp_data, res >> 2);
		}
		else
		{
			bus_space_write_region_1(sc->sc_memt, sc->sc_memh, 0,
				(u_int8_t *) slp->sl_scp.scp_data, res);
		}
	}
	else
	{
		if ((sc->sc_xfermr & XFERMR_IO32) != 0)
		{
			bus_space_write_multi_4(bst, bsh, nsp_fifodr,
				(u_int32_t *) slp->sl_scp.scp_data, res >> 2);
		}
		else
		{
			bus_space_write_multi_1(bst, bsh, nsp_fifodr,
				(u_int8_t *) slp->sl_scp.scp_data, res);
		}
	}

	slp->sl_scp.scp_datalen -= res;
	slp->sl_scp.scp_data += res;
	sc->sc_cnt += res;
	return 0;
}

static int
nsp_wait_interrupt(sc)
	struct nsp_softc *sc;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int tout;
	register u_int8_t isrc;

	for (tout = 0; tout < DEV_BSIZE / 10; tout ++)
	{
		isrc = bus_space_read_1(bst, bsh, nsp_irqsr);
		if ((isrc & (IRQSR_SCSI | IRQSR_FIFO)) != 0)
		{
			if ((isrc & IRQSR_FIFO) != 0)
			{
				bus_space_write_1(bst, bsh,
					nsp_irqcr, IRQCR_FIFOCL);
			}
			return 1;
		}
		SCSI_LOW_DELAY(1);
	}
	return 0;
}

static void
nsp_pio_read(sc, suspendio)
	struct nsp_softc *sc;
	int suspendio;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int tout, padding, datalen;
	register u_int8_t stat, fstat;

	padding = 0;
	tout = sc->sc_tmaxcnt;
	slp->sl_flags |= HW_PDMASTART;
	datalen = slp->sl_scp.scp_datalen;

ReadLoop:
	while (1)
	{
		stat = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (stat == (u_int8_t) -1)
			return;

		/* out of data phase */
		if ((stat & SCBUSMON_PHMASK) != PHASE_DATAIN)
		{
			nsp_read_fifo(sc, 0);
			return;
		}

		/* data phase */
		fstat = bus_space_read_1(bst, bsh, nsp_fifosr);
		if ((fstat & FIFOSR_FULLEMP) != 0)
		{
			if ((sc->sc_icr & SCIENR_FIFO) != 0)
			{
				bus_space_write_1(bst, bsh, nsp_irqcr, 
						  IRQCR_FIFOCL);
			}

			if (suspendio > 0)
			{
				padding |= nsp_read_fifo(sc, suspendio);
			}
			else
			{
				padding |= nsp_read_fifo(sc, 0);
			}

			if ((sc->sc_icr & SCIENR_FIFO) != 0)
				break;
		}
		else
		{
			if (padding == 0 && slp->sl_scp.scp_datalen <= 0)
				return;

			if ((sc->sc_icr & SCIENR_FIFO) != 0)
				break;

			SCSI_LOW_DELAY(1);
		}

		if ((-- tout) <= 0)
		{
			printf("%s: nsp_pio_read: timeout\n", slp->sl_xname);
			return;
		}
	}


	if (slp->sl_scp.scp_datalen > 0 &&
	    slp->sl_scp.scp_datalen > datalen - nsp_read_interrupt_bytes)
	{
		if (nsp_wait_interrupt(sc) != 0)
			goto ReadLoop;
	}
}

static void
nsp_pio_write(sc, suspendio)
	struct nsp_softc *sc;
	int suspendio;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	u_int rcount, acount;
	int tout, datalen;
	register u_int8_t stat, fstat;

	tout = sc->sc_tmaxcnt;
	slp->sl_flags |= HW_PDMASTART;
	datalen = slp->sl_scp.scp_datalen;

WriteLoop:
	while (1)
	{
		stat = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON) & SCBUSMON_PHMASK;
		if (stat != PHASE_DATAOUT)
			return;

		if (slp->sl_scp.scp_datalen <= 0)
		{
			if (sc->sc_dataout_timeout == 0)
				sc->sc_dataout_timeout = SCSI_LOW_TIMEOUT_HZ;
			return;
		}

		fstat = bus_space_read_1(bst, bsh, nsp_fifosr);
		if ((fstat & FIFOSR_FULLEMP) != 0)
		{
			if ((sc->sc_icr & SCIENR_FIFO) != 0)
			{
				bus_space_write_1(bst, bsh, nsp_irqcr,
						  IRQCR_FIFOCL);
			}

			if (suspendio > 0)
			{
				/* XXX:IMPORTANT:
				 * To avoid timeout of pcmcia bus
				 * (not scsi bus!), we should check
				 * the scsi device sends us request
				 * signals, which means the scsi device
				 * is ready to recieve data without
				 * heavy delays. 
				 */
				if ((slp->sl_scp.scp_datalen % suspendio) == 0)
				{
					/* Step I:
					 * fill the nsp fifo, and waiting for
					 * the fifo empty.
					 */
					nsp_write_fifo(sc, 0);
				}
				else
				{
					/* Step II:
					 * check the request singals.
					 */
					acount = nsp_fifo_count(sc);
					rcount = nsp_request_count(sc);
					if (rcount <= acount)
					{
						nsp_write_fifo(sc, 0);
#ifdef	NSP_STATICS
						nsp_statics.device_busy ++;
#endif	/* NSP_STATICS */
					}
					else
					{
						nsp_write_fifo(sc, suspendio);
#ifdef	NSP_STATICS
						nsp_statics.device_data_write ++;
#endif	/* NSP_STATICS */
					}
				}
			}
			else
			{
				nsp_write_fifo(sc, 0);
			}

			if ((sc->sc_icr & SCIENR_FIFO) != 0)
				break;
		}
		else
		{
			if ((sc->sc_icr & SCIENR_FIFO) != 0)
				break;

			SCSI_LOW_DELAY(1);
		}

		if ((-- tout) <= 0)
		{
			printf("%s: nsp_pio_write: timeout\n", slp->sl_xname);
			return;
		}
	}

	if (slp->sl_scp.scp_datalen > 0 &&
	    slp->sl_scp.scp_datalen > datalen - nsp_write_interrupt_bytes)
	{
		if (nsp_wait_interrupt(sc) != 0)
			goto WriteLoop;
	}
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
	int wc;
	u_int8_t regv;

	for (wc = 0; wc < NSP_DELAY_MAX / NSP_DELAY_INTERVAL; wc ++)
	{
		regv = nsp_cr_read_1(bst, bsh, NSPR_SCBUSMON);
		if (regv == (u_int8_t) -1)
			return -1;
		if ((regv & mask) == 0)
			return 1;
		SCSI_LOW_DELAY(NSP_DELAY_INTERVAL);
	}

	printf("%s: %s nsp_negate_signal timeout\n", slp->sl_xname, s);
	return -1;
}

static int
nsp_xfer(sc, buf, len, phase, clear_atn)
	struct nsp_softc *sc;
	u_int8_t *buf;
	int len;
	int phase;
	int clear_atn;
{
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int ptr, rv;

	for (ptr = 0; len > 0; len --, ptr ++)
	{
		rv = nsp_expect_signal(sc, phase, SCBUSMON_REQ);
		if (rv <= 0)
			goto out;

		if (len == 1 && clear_atn != 0)
		{
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR,
				       SCBUSCR_ADIR | SCBUSCR_ACKEN);
			SCSI_LOW_DEASSERT_ATN(&sc->sc_sclow);
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

	cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR);
	cr &= ~(SCBUSCR_BSY | SCBUSCR_ATN);
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);
	cr |= SCBUSCR_ADIR | SCBUSCR_ACKEN;
	nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);

#ifdef	NSP_STATICS
	nsp_statics.reselect ++;
#endif	/* NSP_STATCIS */
	return EJUSTRETURN;
}

static int
nsp_disconnected(sc, ti)
	struct nsp_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;

	nsp_cr_write_1(bst, bsh, NSPR_PTCLRR, PTCLRR_PT | PTCLRR_ACK |
			PTCLRR_REQ | PTCLRR_HOST);
	if ((sc->sc_icr & SCIENR_FIFO) != 0)
	{
		sc->sc_icr &= ~SCIENR_FIFO;
		nsp_cr_write_1(bst, bsh, NSPR_SCIENR, sc->sc_icr);
	}
	sc->sc_cnt = 0;
	sc->sc_dataout_timeout = 0;
#ifdef	NSP_STATICS
	nsp_statics.disconnect ++;
#endif	/* NSP_STATICS */
	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static void nsp_error(struct nsp_softc *, u_char *, u_int8_t, u_int8_t, u_int8_t);

static void
nsp_error(sc, s, isrc, ph, irqphs)
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
nsp_target_nexus_establish(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	struct targ_info *ti = slp->sl_Tnexus;
	struct nsp_targ_info *nti = (void *) ti;

	/* setup synch transfer registers */
	nsp_cr_write_1(bst, bsh, NSPR_SYNCR, nti->nti_reg_syncr);
	nsp_cr_write_1(bst, bsh, NSPR_ACKWIDTH, nti->nti_reg_ackwidth);

	/* setup pdma fifo (minimum) */
	nsp_setup_fifo(sc, NSP_FIFO_ON, SCSI_LOW_READ, 0);
	return 0;
}

static int
nsp_lun_nexus_establish(sc)
	struct nsp_softc *sc;
{

	return 0;
}

static int
nsp_ccb_nexus_establish(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = slp->sl_Qnexus;

	sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;

	/* setup pdma fifo */
	nsp_setup_fifo(sc, NSP_FIFO_ON, 
		       slp->sl_scp.scp_direction, slp->sl_scp.scp_datalen);

	if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
	{
		if (sc->sc_suspendio > 0 &&
		    (nsp_io_control & NSP_READ_FIFO_INTERRUPTS) != 0)
		{
			sc->sc_icr |= SCIENR_FIFO;
			nsp_cr_write_1(sc->sc_iot, sc->sc_ioh,
				       NSPR_SCIENR, sc->sc_icr);
		}
	}
	else 
	{
		if (sc->sc_suspendio > 0 &&
		    (nsp_io_control & NSP_WRITE_FIFO_INTERRUPTS) != 0)
		{
			sc->sc_icr |= SCIENR_FIFO;
			nsp_cr_write_1(sc->sc_iot, sc->sc_ioh,
				       NSPR_SCIENR, sc->sc_icr);
		}
	}
	return 0;
}

static int
nsp_phase_match(sc, phase, stat)
	struct nsp_softc *sc;
	u_int8_t phase;
	u_int8_t stat;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	if ((stat & SCBUSMON_PHMASK) != phase)
	{
		printf("%s: phase mismatch 0x%x != 0x%x\n",
			slp->sl_xname, (u_int) phase, (u_int) stat);
		return EINVAL;
	}

	if ((stat & SCBUSMON_REQ) == 0)
		return EINVAL;

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
	u_int derror, flags;
	int len, rv;
	u_int8_t isrc, ph, irqphs, cr, regv;

	/*******************************************
	 * interrupt check
	 *******************************************/
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_IRQDIS);
	isrc = bus_space_read_1(bst, bsh, nsp_irqsr);
	if (isrc == (u_int8_t) -1 || (isrc & IRQSR_MASK) == 0)
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
	
	/* check a timer interrupt */
	regv = 0;
	if ((isrc & IRQSR_TIMER) != 0)
	{
		if ((isrc & IRQSR_MASK) == IRQSR_TIMER && sc->sc_seltout == 0)
		{
			bus_space_write_1(bst, bsh, nsp_irqcr, IRQCR_TIMERCL);
			return 1;
		}
		regv |= IRQCR_TIMERCL;
	}

	/* check a fifo interrupt */
	if ((isrc & IRQSR_FIFO) != 0)
	{
		regv |= IRQCR_FIFOCL;
	}

	/* OK. enable all interrupts */
	bus_space_write_1(bst, bsh, nsp_irqcr, regv);

	/*******************************************
	 * debug section
	 *******************************************/
#ifdef	NSP_DEBUG
	if (nsp_debug)
	{
		nsp_error(sc, "current status", isrc, ph, irqphs);
		scsi_low_print(slp, NULL);
#ifdef	KDB
		if (nsp_debug > 1)
			SCSI_LOW_DEBUGGER("nsp");
#endif	/* KDB */
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
	if ((ti = slp->sl_Tnexus) == NULL)
	{
		/* unknown scsi phase changes */
		nsp_error(sc, "unknown scsi phase changes", isrc, ph, irqphs);
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
			nsp_start_timer(sc, NSP_TIMER_1MS);
			return 1;
		}

		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		nsphw_selection_done_and_expect_msgout(sc);
		return 1;

	case PH_SELECTED:
		if ((isrc & IRQSR_SCSI) == 0)
			return 1;

		nsp_target_nexus_establish(sc);
		break;

	case PH_RESEL:
		if ((isrc & IRQSR_SCSI) == 0)
			return 1;

		nsp_target_nexus_establish(sc);
		if ((ph & SCBUSMON_PHMASK) != PHASE_MSGIN)
		{
			printf("%s: unexpected phase after reselect\n",
			       slp->sl_xname);
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			return 1;
		}
		break;

	case PH_DATA:
		if ((isrc & IRQSR_SCSI) != 0)
			break;
		if ((isrc & IRQSR_FIFO) != 0)
		{
			if (NSP_IS_PHASE_DATA(ph) == 0)
				return 1;
			irqphs = (ph & IRQPHS_PHMASK);
			break;
		}
		return 1;

	default:
		if ((isrc & IRQSR_SCSI) == 0)
			return 1;
		break;
	}

	/*******************************************
	 * data phase control 
	 *******************************************/
	if (slp->sl_flags & HW_PDMASTART)
	{
		if ((isrc & IRQSR_SCSI) != 0 &&
		     NSP_IS_IRQPHS_DATA(irqphs) == 0)
		{
			if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
				nsp_pio_read(sc, 0);
			nsp_pdma_end(sc, ti);
		}
	}

	/*******************************************
	 * scsi seq
	 *******************************************/
	if (slp->sl_msgphase != 0 && (irqphs & IRQPHS_LBF) != 0)
		return nsp_disconnected(sc, ti);

	/* check unexpected bus free state */
	if (ph == 0)
	{
		nsp_error(sc, "unexpected bus free", isrc, ph, irqphs);
		return nsp_disconnected(sc, ti);
	}
		
	/* check normal scsi phase */
	switch (irqphs & IRQPHS_PHMASK)
	{
	case IRQPHS_CMD:
		if (nsp_phase_match(sc, PHASE_CMD, ph) != 0)
			return 1;

		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
		{
			scsi_low_attention(slp);
		}

		nsp_cr_write_1(bst, bsh, NSPR_CMDCR, CMDCR_PTCLR);
		for (len = 0; len < slp->sl_scp.scp_cmdlen; len ++)
			nsp_cr_write_1(bst, bsh, NSPR_CMDDR,
				       slp->sl_scp.scp_cmd[len]);

		nsp_cr_write_1(bst, bsh, NSPR_CMDCR, CMDCR_PTCLR | CMDCR_EXEC);
		break;

	case IRQPHS_DATAOUT:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
		{
			scsi_low_attention(slp);
		}
	
		pp = physio_proc_enter(bp);
		nsp_pio_write(sc, sc->sc_suspendio);
		physio_proc_leave(pp);
		break;

	case IRQPHS_DATAIN:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
		{
			scsi_low_attention(slp);
		}

		pp = physio_proc_enter(bp);
		nsp_pio_read(sc, sc->sc_suspendio);
		physio_proc_leave(pp);
		break;

	case IRQPHS_STATUS:
		if (nsp_phase_match(sc, PHASE_STATUS, ph) != 0)
			return 1;

		SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
		regv = nsp_cr_read_1(bst, bsh, NSPR_DATA);
		if (nsp_cr_read_1(bst, bsh, NSPR_PARITYR) & PARITYR_PE)
		{
			nsp_cr_write_1(bst, bsh, NSPR_PARITYR, 
				       PARITYR_ENABLE | PARITYR_CLEAR);
			derror = SCSI_LOW_DATA_PE;
		}
		else
			derror = 0;

		/* assert ACK */
		cr = SCBUSCR_ACK | nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR);
		nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);

		if (scsi_low_statusin(slp, ti, derror | regv) != 0)
		{
			scsi_low_attention(slp);
		}

		/* check REQ nagated */
		nsp_negate_signal(sc, SCBUSMON_REQ, "statin<REQ>");

		/* deassert ACK */
		cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR) & (~SCBUSCR_ACK);
		nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);
		break;

	case IRQPHS_MSGOUT:
		if (nsp_phase_match(sc, PHASE_MSGOUT, ph) != 0)
			return 1;

#ifdef	NSP_MSGOUT_SERIALIZE
		/*
		 * XXX: NSP QUIRK
		 * NSP invoke interrupts only in the case of scsi phase changes,
		 * therefore we should poll the scsi phase here to catch 
		 * the next "msg out" if exists (no scsi phase changes).
		 */
		rv = len = 16;
		do {
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
			flags = (ti->ti_ophase != ti->ti_phase) ? 
					SCSI_LOW_MSGOUT_INIT : 0;
			len = scsi_low_msgout(slp, ti, flags);

			if (len > 1 && slp->sl_atten == 0)
			{
				scsi_low_attention(slp);
			}

			if (nsp_xfer(sc, ti->ti_msgoutstr, len, PHASE_MSGOUT,
			             slp->sl_clear_atten) != 0)
			{
				slp->sl_error |= FATALIO;
				nsp_error(sc, "MSGOUT: xfer short",
						    isrc, ph, irqphs);
			}

			/* catch a next signal */
			rv = nsp_expect_signal(sc, PHASE_MSGOUT, SCBUSMON_REQ);
		}
		while (rv > 0 && len -- > 0);

#else	/* !NSP_MSGOUT_SERIALIZE */
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
		flags = SCSI_LOW_MSGOUT_UNIFY;
		if (ti->ti_ophase != ti->ti_phase)
			flags |= SCSI_LOW_MSGOUT_INIT;
		len = scsi_low_msgout(slp, ti, flags);

		if (len > 1 && slp->sl_atten == 0)
		{
			scsi_low_attention(slp);
		}

		if (nsp_xfer(sc, ti->ti_msgoutstr, len, PHASE_MSGOUT,
			     slp->sl_clear_atten) != 0)
		{
			nsp_error(sc, "MSGOUT: xfer short", isrc, ph, irqphs);
		}

#endif	/* !NSP_MSGOUT_SERIALIZE */
		break;

	case IRQPHS_MSGIN:
		if (nsp_phase_match(sc, PHASE_MSGIN, ph) != 0)
			return 1;

		/*
		 * XXX: NSP QUIRK
		 * NSP invoke interrupts only in the case of scsi phase changes,
		 * therefore we should poll the scsi phase here to catch 
		 * the next "msg in" if exists (no scsi phase changes).
		 */
		rv = len = 16;
		do {
			SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

			/* read a data */
			regv = nsp_cr_read_1(bst, bsh, NSPR_DATA);
			if (nsp_cr_read_1(bst, bsh, NSPR_PARITYR) & PARITYR_PE)
			{
				nsp_cr_write_1(bst, bsh,
					       NSPR_PARITYR, 
					       PARITYR_ENABLE | PARITYR_CLEAR);
				derror = SCSI_LOW_DATA_PE;
			}
			else
			{
				derror = 0;
			}

			/* assert ack */
			cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR) | SCBUSCR_ACK;
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);

			if (scsi_low_msgin(slp, ti, regv | derror) == 0)
			{
				if (scsi_low_is_msgout_continue(ti, 0) != 0)
				{
					scsi_low_attention(slp);
				}
			}

			/* check REQ nagated */
			nsp_negate_signal(sc, SCBUSMON_REQ, "msgin<REQ>");

			/* deassert ack */
			cr = nsp_cr_read_1(bst, bsh, NSPR_SCBUSCR) & (~SCBUSCR_ACK);
			nsp_cr_write_1(bst, bsh, NSPR_SCBUSCR, cr);

			/* catch a next signal */
			rv = nsp_expect_signal(sc, PHASE_MSGIN, SCBUSMON_REQ);
		} 
		while (rv > 0 && len -- > 0);
		break;

	default:
		slp->sl_error |= FATALIO;
		nsp_error(sc, "unknown scsi phase", isrc, ph, irqphs);
		break;
	}

	return 1;

#if	0
timerout:
	nsp_start_timer(sc, NSP_TIMER_1MS);
	return 0;
#endif
}

static int
nsp_timeout(sc)
	struct nsp_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int tout;
	u_int8_t ph, regv;

	if (slp->sl_Tnexus == NULL)
		return 0;

	ph = nsp_cr_read_1(iot, ioh, NSPR_SCBUSMON);
	switch (ph & SCBUSMON_PHMASK)
	{
	case PHASE_DATAOUT:
		if (sc->sc_dataout_timeout == 0)
			break;

		/* check a fifo empty */
		regv = bus_space_read_1(iot, ioh, nsp_fifosr);
		if ((regv & FIFOSR_FULLEMP) == 0)
			break;
		bus_space_write_1(iot, ioh, nsp_irqcr, IRQCR_FIFOCL);

		/* check still requested */
		ph = nsp_cr_read_1(iot, ioh, NSPR_SCBUSMON);
		if ((ph & SCBUSMON_REQ) == 0)
			break;
		/* check timeout */
		if ((-- sc->sc_dataout_timeout) > 0)
			break;	

	        slp->sl_error |= PDMAERR;
		if ((slp->sl_flags & HW_WRITE_PADDING) == 0)
		{
			printf("%s: write padding required\n", slp->sl_xname);
			break;
		}

		tout = NSP_DELAY_MAX;
		while (tout -- > 0)
		{
			ph = nsp_cr_read_1(iot, ioh, NSPR_SCBUSMON);
			if ((ph & SCBUSMON_PHMASK) != PHASE_DATAOUT)
				break;
			regv = bus_space_read_1(iot, ioh, nsp_fifosr);
			if ((regv & FIFOSR_FULLEMP) == 0)
			{
				SCSI_LOW_DELAY(1);
				continue;
			}

			bus_space_write_1(iot, ioh, nsp_irqcr, IRQCR_FIFOCL);
			nsp_data_padding(sc, SCSI_LOW_WRITE, 32);
		}
		ph = nsp_cr_read_1(iot, ioh, NSPR_SCBUSMON);
		if ((ph & SCBUSMON_PHMASK) == PHASE_DATAOUT)
			sc->sc_dataout_timeout = SCSI_LOW_TIMEOUT_HZ;
		break;

	default:
		break;
	}
	return 0;
}
