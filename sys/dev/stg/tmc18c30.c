/*	$FreeBSD$	*/
/*	$NecBSD: tmc18c30.c,v 1.28 1999/07/23 21:00:06 honda Exp $	*/
/*	$NetBSD$	*/

#define	STG_DEBUG
#define	STG_STATICS

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	Kouichi Matsuda. All rights reserved.
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
#if defined(__FreeBSD__) && __FreeBSD_version >= 500001
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
#include <i386/Cbus/dev/tmc18c30reg.h>
#include <i386/Cbus/dev/tmc18c30var.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/clock.h>
#define delay(time) DELAY(time)

#include <machine/cpu.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <cam/scsi/scsi_low.h>
#include <dev/stg/tmc18c30reg.h>
#include <dev/stg/tmc18c30var.h>

#if __FreeBSD_version < 400001
#include "stg.h"
struct	stg_softc *stgdata[NSTG];
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
/* #define	STG_SYNC_SUPPORT */	/* NOT YET but easy */

/* For the 512 fifo type: change below */
#define	TMC18C30_FIFOSZ	0x800
#define	TMC18C30_FCB	1

#define	TMC18C50_FIFOSZ	0x2000
#define	TMC18C50_FCB	2

/***************************************************
 * PARAMS
 ***************************************************/
#define	STG_NTARGETS	8
#define	STG_NLUNS	8

/***************************************************
 * DEBUG
 ***************************************************/
#ifndef DDB
#define Debugger() panic("should call debugger here (tmc18c30.c)")
#else /* ! DDB */
#ifdef __FreeBSD__
#define Debugger() Debugger("stg")
#endif /* __FreeBSD__ */
#endif

#ifdef	STG_DEBUG
int stg_debug;
#endif	/* STG_DEBUG */

#ifdef	STG_STATICS
struct stg_statics {
	int disconnect;
	int reselect;
	int sprious_arbit_fail_0;
	int sprious_arbit_fail_1;
	int sprious_arbit_fail_2;
} stg_statics[STG_NTARGETS];
#endif	/* STG_STATICS */

/***************************************************
 * ISA DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver stg_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
/* static */
static void stg_pio_read __P((struct stg_softc *, struct targ_info *));
static void stg_pio_write __P((struct stg_softc *, struct targ_info *));
static int stg_xfer __P((struct stg_softc *, u_int8_t *, int, int));
static int stg_msg __P((struct stg_softc *, struct targ_info *, u_int));
static int stg_reselected __P((struct stg_softc *));
static __inline int stg_disconnected __P((struct stg_softc *, struct targ_info *));
static __inline void stg_pdma_end __P((struct stg_softc *, struct targ_info *));
static int stghw_select_targ_wait __P((struct stg_softc *, int));
static int stghw_check __P((struct stg_softc *));
static void stghw_init __P((struct stg_softc *));
static int stg_negate_signal __P((struct stg_softc *, u_int8_t, u_char *));
static int stg_expect_signal __P((struct stg_softc *, u_int8_t, u_int8_t));
static int stg_world_start __P((struct stg_softc *, int));
static int stghw_start_selection __P((struct stg_softc *sc, struct slccb *));
static void stghw_bus_reset __P((struct stg_softc *));
static void stghw_attention __P((struct stg_softc *));
static int stg_nexus __P((struct stg_softc *, struct targ_info *));
static int stg_lun_init __P((struct stg_softc *, struct targ_info *, struct lun_info *));
static __inline void stghw_bcr_write_1 __P((struct stg_softc *, u_int8_t));
static void settimeout __P((void *));

struct scsi_low_funcs stgfuncs = {
	SC_LOW_INIT_T stg_world_start,
	SC_LOW_BUSRST_T stghw_bus_reset,
	SC_LOW_LUN_INIT_T stg_lun_init,

	SC_LOW_SELECT_T stghw_start_selection,
	SC_LOW_NEXUS_T stg_nexus,

	SC_LOW_ATTEN_T stghw_attention,
	SC_LOW_MSG_T stg_msg,

	SC_LOW_POLL_T stgintr,

	NULL,
};

/****************************************************
 * hwfuncs
 ****************************************************/
static int
stghw_check(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t lsb, msb;

	sc->sc_chip = TMCCHIP_UNK;
	sc->sc_fsz = TMC18C50_FIFOSZ;
	sc->sc_fcb = TMC18C50_FCB;
	sc->sc_fcsp = 0;

	sc->sc_fcRinit = FCTL_INTEN;
	sc->sc_fcWinit = FCTL_PARENB | FCTL_INTEN;

	if (slp->sl_cfgflags & CFG_NOATTEN)
		sc->sc_imsg = 0;
	else
		sc->sc_imsg = BCTL_ATN;
	sc->sc_busc = BCTL_BUSEN;

	lsb = bus_space_read_1(iot, ioh, tmc_idlsb);
	msb = bus_space_read_1(iot, ioh, tmc_idmsb);
	switch (msb << 8 | lsb)
	{
		case 0x6127:
			/* TMCCHIP_1800 not supported. (it's my policy) */
			sc->sc_chip = TMCCHIP_1800;
			return EINVAL;

		case 0x60e9:
			sc->sc_chip = TMCCHIP_18C50;
			sc->sc_fcsp |= FCTL_CLRINT;
			if (bus_space_read_1(iot, ioh, tmc_cfg2) & 0x02)
			{
				sc->sc_chip = TMCCHIP_18C30;
				sc->sc_fsz = TMC18C30_FIFOSZ;
				sc->sc_fcb = TMC18C30_FCB;
			}
			break;

		default:
			return ENODEV;
	}

	sc->sc_icinit = ICTL_ALLINT | sc->sc_fcb;
	return 0;
}

static void
stghw_init(sc)
	struct stg_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, tmc_ictl, 0);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcsp | sc->sc_fcRinit |
					  FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	bus_space_write_1(iot, ioh, tmc_ictl, sc->sc_icinit);

	bus_space_write_1(iot, ioh, tmc_ssctl, 0);
}

static int
stg_lun_init(sc, ti, li)
	struct stg_softc *sc;
	struct targ_info *ti;
	struct lun_info *li;
{
	struct stg_lun_info *sli = (void *) li;

	li->li_maxsynch.period = 0;
	li->li_maxsynch.offset = 8;
	sli->sli_reg_synch = 0;
	return 0;
}	

/****************************************************
 * scsi low interface
 ****************************************************/
static __inline void 
stghw_bcr_write_1(sc, bcv)
	struct stg_softc *sc;
	u_int8_t bcv;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, tmc_bctl, bcv);
	sc->sc_busimg = bcv;
}

static void
stghw_attention(sc)
	struct stg_softc *sc;
{

	sc->sc_busc |= BCTL_ATN;
	sc->sc_busimg |= BCTL_ATN;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, tmc_bctl, sc->sc_busimg);
}

static void
stghw_bus_reset(sc)
	struct stg_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, tmc_ictl, 0);
	bus_space_write_1(iot, ioh, tmc_fctl, 0);
	stghw_bcr_write_1(sc, BCTL_RST);
	delay(100000);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
}

static int
stghw_start_selection(sc, cb)
	struct stg_softc *sc;
	struct slccb *cb;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti = cb->ti;
	struct lun_info *li = ti->ti_li;
	register u_int8_t stat;
	int s;

	if (li->li_flags & SCSI_LOW_NOPARITY)
		sc->sc_fcRinit &= ~FCTL_PARENB;
	else
		sc->sc_fcRinit |= FCTL_PARENB;

	stghw_bcr_write_1(sc, BCTL_BUSFREE);

	s = splhigh();
	if (slp->sl_disc > 0)
	{
		stat = bus_space_read_1(iot, ioh, tmc_bstat);
		if (stat & (BSTAT_BSY | BSTAT_SEL | BSTAT_IO))
		{
			splx(s);
			return SCSI_LOW_START_FAIL;
		}
	}

	bus_space_write_1(iot, ioh, tmc_scsiid, sc->sc_idbit);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_ARBIT);
	splx(s);

	SCSI_LOW_SETUP_PHASE(ti, PH_ARBSTART);
	return SCSI_LOW_START_OK;
}

static int
stg_world_start(sc, fdone)
	struct stg_softc *sc;
	int fdone;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	int error;
	intrmask_t s;

	if ((error = stghw_check(sc)) != 0)
		return error;

	s = splcam();
	stghw_init(sc);
	scsi_low_bus_reset(slp);
	stghw_init(sc);
	splx(s);

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

static int
stg_msg(sc, ti, msg)
	struct stg_softc *sc;
	struct targ_info *ti;
	u_int msg;
{
	struct lun_info *li = ti->ti_li;
	struct stg_lun_info *sli = (void *) li;
	u_int period, offset;

	if (msg != SCSI_LOW_MSG_SYNCH)
		return EINVAL;

 	period = li->li_maxsynch.period;
	offset = li->li_maxsynch.offset;
	period = period << 2;
	if (period >= 200)
	{
		sli->sli_reg_synch = (period - 200) / 50;
		if (period % 50)
			sli->sli_reg_synch ++;
		sli->sli_reg_synch |= SSCTL_SYNCHEN;
	}
	else if (period >= 100)
	{
		sli->sli_reg_synch = (period - 100) / 50;
		if (period % 50)
			sli->sli_reg_synch ++;
		sli->sli_reg_synch |= SSCTL_SYNCHEN | SSCTL_FSYNCHEN;
	}
	return 0;
}

/**************************************************************
 * General probe attach
 **************************************************************/
int
stgprobesubr(iot, ioh, dvcfg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int dvcfg;
{
	u_int16_t lsb, msb;

	lsb = bus_space_read_1(iot, ioh, tmc_idlsb);
	msb = bus_space_read_1(iot, ioh, tmc_idmsb);
	switch (msb << 8 | lsb)
	{
		default:
			return 0;
		case 0x6127:
			/* not support! */
			return 0;
		case 0x60e9:
			return 1;
	}
	return 0;
}

int
stgprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
stgattachsubr(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	printf("\n");

	sc->sc_idbit = (1 << slp->sl_hostid); 
	slp->sl_funcs = &stgfuncs;

	slp->sl_cfgflags |= CFG_ASYNC;	/* XXX */

	if (stghw_check(sc) != 0)
	{
		printf("stg: hardware missing\n");
		return;
	}

	(void) scsi_low_attach(slp, 2, STG_NTARGETS, STG_NLUNS,
				sizeof(struct stg_lun_info));
}

/**************************************************************
 * PDMA functions
 **************************************************************/
static __inline void
stg_pdma_end(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct slccb *cb = ti->ti_nexus;
	u_int len, tres;

	slp->sl_flags &= ~HW_PDMASTART;

	if (ti->ti_phase == PH_DATA)
	{
		len = bus_space_read_2(iot, ioh, tmc_fdcnt);
		if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
		{
			if (len != 0)
			{
				tres = len + slp->sl_scp.scp_datalen;
				if (tres <= (u_int) cb->ccb_scp.scp_datalen)
				{
					slp->sl_scp.scp_data -= len;
					slp->sl_scp.scp_datalen = tres;
				}
				else
				{
					slp->sl_error |= PDMAERR;
					printf("%s len %x >= datalen %x\n",
						slp->sl_xname,
						len, slp->sl_scp.scp_datalen);
				}
			}
		}
		else if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
		{
			if (len != 0)
			{
				slp->sl_error |= PDMAERR;
				printf("%s: len %x left in fifo\n",
					slp->sl_xname, len);
			}
		}
	}
	else
	{

		printf("%s data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}

	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
}

static void
stg_pio_read(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct sc_p *sp = &slp->sl_scp;
	int s;
	int tout = 0;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif
	u_int res;
	u_int8_t stat;

	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_FIFOEN);

	slp->sl_flags |= HW_PDMASTART;
#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (sp->scp_datalen > 0 && tout == 0)
	{
		res = bus_space_read_2(iot, ioh, tmc_fdcnt);
		if (res == 0)
		{
			stat = bus_space_read_1(iot, ioh, tmc_bstat);
			if ((stat & BSTAT_PHMASK) == BSTAT_IO)
				continue;
			break; /* phase mismatch */
		}

		/* XXX */
		if (res > sp->scp_datalen)
		{
			slp->sl_error |= PDMAERR;
			break;
		}

		sp->scp_datalen -= res;
		if (res & 1)
		{
			*sp->scp_data = bus_space_read_1(iot, ioh, tmc_rfifo);
			sp->scp_data ++;
			res --;
		}

		bus_space_read_multi_2(iot, ioh, tmc_rfifo,
				       (u_int16_t *) sp->scp_data, res >> 1);
		sp->scp_data += res;
	}

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

#define	WFIFO_CRIT	0x100

static void
stg_pio_write(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct sc_p *sp = &slp->sl_scp;
	u_int res;
	int s;
	int tout = 0;
	register u_int8_t stat;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif

	stat = sc->sc_fcWinit | FCTL_FIFOEN | FCTL_FIFOW;
	bus_space_write_1(iot, ioh, tmc_fctl, stat | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, stat);

	slp->sl_flags |= HW_PDMASTART;
#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (sp->scp_datalen > 0 && tout == 0)
	{
		stat = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((stat & BSTAT_PHMASK) != 0)
			break;

		if (bus_space_read_2(iot, ioh, tmc_fdcnt) >= WFIFO_CRIT)
			continue;

		res = (sp->scp_datalen > WFIFO_CRIT) ?
			WFIFO_CRIT : sp->scp_datalen;
		sp->scp_datalen -= res;
		if ((res & 0x1) != 0)
		{
			bus_space_write_1(iot, ioh, tmc_wfifo, *sp->scp_data);
			sp->scp_data ++;
			res --;
		}

		bus_space_write_multi_2(iot, ioh, tmc_wfifo, 
					(u_int16_t *) sp->scp_data, res >> 1);
		sp->scp_data += res;
	}

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

static int
stg_negate_signal(sc, mask, s)
	struct stg_softc *sc;
	u_int8_t mask;
	u_char *s;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int s;
	int tout = 0;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif
	u_int8_t regv;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	do 
	{
		regv = bus_space_read_1(bst, bsh, tmc_bstat);
		if (regv == 0xff) {
			s = splhigh();
			if (tout == 0) {
#ifdef __FreeBSD__
				untimeout(settimeout, &tout, ch);
#else
				untimeout(settimeout, &tout);
#endif
			}
			splx(s);
			return EIO;
		}
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
		printf("%s: %s singal off timeout \n", slp->sl_xname, s);
		return EIO;
	}
	return 0;
}

static void
settimeout(arg)
	void *arg;
{
	int *tout = arg;

	*tout = 1;
}

static int
stg_expect_signal(sc, phase, mask)
	struct stg_softc *sc;
	u_int8_t phase, mask;
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
	u_int8_t ph;

	phase &= BSTAT_PHMASK;
#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, hz/2);
#else
	timeout(settimeout, &tout, hz/2);
#endif
	do 
	{
		ph = bus_space_read_1(bst, bsh, tmc_bstat);
		if (ph == 0xff) {
			rv = -1;
			break;
		}
		if ((ph & BSTAT_PHMASK) != phase) {
			rv = 0;
			break;
		}
		if ((ph & mask) != 0) {
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
		printf("%s: stg_expect_signal timeout\n", slp->sl_xname);
		rv = -1;
	}
	return rv;
}

static int
stg_xfer(sc, buf, len, phase)
	struct stg_softc *sc;
	u_int8_t *buf;
	int len;
	int phase;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int rv, ptr, atn;

	atn = (scsi_low_is_msgout_continue(slp->sl_nexus) != 0);
	if (phase & BSTAT_IO)
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	else
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit);

	for (ptr = 0; len > 0; len --)
	{
		rv = stg_expect_signal(sc, phase, BSTAT_REQ);
		if (rv <= 0)
			goto bad;

		if (len == 1 && atn == 0)
		{
			sc->sc_busc &= ~BCTL_ATN;
			stghw_bcr_write_1(sc, sc->sc_busc);
		}

		if (phase & BSTAT_IO)
		{
			buf[ptr ++] = bus_space_read_1(iot, ioh, tmc_rdata);
		}
		else
		{
			bus_space_write_1(iot, ioh, tmc_wdata, buf[ptr ++]);
		}
		stg_negate_signal(sc, BSTAT_ACK, "xfer<ACK>");
	}

bad:
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	return len;
}

/**************************************************************
 * disconnect & reselect (HW low)
 **************************************************************/
static int
stg_reselected(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti;
	u_int sid;

	if (slp->sl_selid != NULL)
	{
		/* XXX:
		 * Selection vs Reselection conflicts.
		 */
#ifdef	STG_STATICS
		stg_statics[slp->sl_selid->ti_id].sprious_arbit_fail_0 ++;
#endif	/* STG_STATICS */
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		stghw_bcr_write_1(sc, BCTL_BUSFREE);
	}

	/* XXX:
	 * We should ack the reselection as soon as possible,
	 * becuase the target would abort the current reselection seq 
  	 * due to reselection timeout.
	 */
	sid = (u_int) bus_space_read_1(iot, ioh, tmc_scsiid);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcsp |
				    sc->sc_fcRinit | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	stghw_bcr_write_1(sc, sc->sc_busc | BCTL_BSY);

	sid &= ~sc->sc_idbit;
	sid = ffs(sid) - 1;
	if ((ti = scsi_low_reselected(slp, sid)) == NULL)
		return EJUSTRETURN;

#ifdef	STG_STATICS
	stg_statics[sid].reselect ++;
#endif	/* STG_STATICS */
	return EJUSTRETURN;
}

static __inline int
stg_disconnected(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* clear bus status & fifo */
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
	sc->sc_fcRinit &= ~FCTL_PARENB;
	sc->sc_busc &= ~BCTL_ATN;

#ifdef	STG_STATICS
	if (slp->sl_msgphase == MSGPH_DISC)
		stg_statics[ti->ti_id].disconnect ++;
#endif	/* STG_STATICS */
	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static int
stg_nexus(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct lun_info *li = ti->ti_li;
	struct stg_lun_info *sli = (void *) ti->ti_li;

	if (li->li_flags & SCSI_LOW_NOPARITY)
		sc->sc_fcRinit &= ~FCTL_PARENB;
	else
		sc->sc_fcRinit |= FCTL_PARENB;

	bus_space_write_1(iot, ioh, tmc_ssctl, sli->sli_reg_synch);
	return 0;
}

static int
stghw_select_targ_wait(sc, id)
	struct stg_softc *sc;
	int id;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wc, error = EIO;

	bus_space_write_1(iot, ioh, tmc_scsiid, sc->sc_idbit | (1 << id));
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit & (~FCTL_INTEN));
	stghw_bcr_write_1(sc, sc->sc_busc | sc->sc_imsg | BCTL_SEL);

	for (wc = 50000; wc; wc--)
	{
		if (bus_space_read_1(iot, ioh, tmc_bstat) & BSTAT_BSY)
		{
			error = 0;
			break;
		}

		delay(1);
	}

	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	return error;
}

int
stgintr(arg)
	void *arg;
{
	struct stg_softc *sc = arg;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	int len;
	u_int8_t status, astatus, regv;

	/*******************************************
	 * interrupt check
	 *******************************************/
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	astatus = bus_space_read_1(iot, ioh, tmc_astat);
	status = bus_space_read_1(iot, ioh, tmc_bstat);

	if ((astatus & ASTAT_STATMASK) == 0)
		return 0;

	if (astatus & ASTAT_SCSIRST)
	{
		bus_space_write_1(iot, ioh, tmc_fctl,
				  sc->sc_fcRinit | FCTL_CLRFIFO);
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);

		scsi_low_restart(slp, SCSI_LOW_RESTART_SOFT, 
				 "bus reset (power off?)");
		return 1;
	}

	/*******************************************
	 * debug section
	 *******************************************/
#ifdef	STG_DEBUG
	if (stg_debug)
	{
		scsi_low_print(slp, NULL);
		printf("%s st %x ist %x\n\n", slp->sl_xname,
		       status, astatus);
		if (stg_debug > 1)
			Debugger();
	}
#endif	/* STG_DEBUG */

	/*******************************************
	 * reselection & nexus
	 *******************************************/
	if ((status & RESEL_PHASE_MASK)== PHASE_RESELECTED)
	{
		if (stg_reselected(sc) == EJUSTRETURN)
			return 1;
	}

	if ((ti = slp->sl_nexus) == NULL)
	{
		status = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((status & PHASE_MASK) != MESSAGE_IN_PHASE)
			return 1;

		/* XXX:
		 * Some scsi devices overrun scsi phase.
		 */
		if (stg_reselected(sc) == EJUSTRETURN)
		{
#ifdef	STG_STATICS
			if ((ti = slp->sl_nexus) != NULL)
				stg_statics[ti->ti_id].sprious_arbit_fail_1 ++;
#endif	/* STG_STATICS */
			return 1;
		}
	}

	if ((astatus & ASTAT_PARERR) != 0 && ti->ti_phase != PH_ARBSTART &&
	    (sc->sc_fcRinit & FCTL_PARENB) != 0)
	{
		slp->sl_error |= PARITYERR;
		if (ti->ti_phase == PH_MSGIN)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_PARITY, 1);
		else
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ERROR, 1);
	}

	/*******************************************
	 * aribitration & selection
	 *******************************************/
	switch (ti->ti_phase)
	{
	case PH_ARBSTART:
		if ((astatus & ASTAT_ARBIT) == 0)
			goto arb_fail;

		status = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((status & BSTAT_IO) != 0)
		{
			/* XXX:
			 * Selection vs Reselection conflicts.
			 */
#ifdef	STG_STATICS
			stg_statics[ti->ti_id].sprious_arbit_fail_2 ++;
#endif	/* STG_STATICS */
arb_fail:
			bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
			stghw_bcr_write_1(sc, BCTL_BUSFREE);
			SCSI_LOW_SETUP_PHASE(ti, PH_NULL);
			scsi_low_clear_nexus(slp, ti);
			return 1;
		}

		/*
		 * selection assert start.
		 */
		SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
		scsi_low_arbit_win(slp, ti);
#ifdef	STG_ALT_SELECTION
		bus_space_write_1(iot, ioh, tmc_scsiid,
				  sc->sc_idbit | (1 << ti->ti_id));
		/* assert busy */
		stghw_bcr_write_1(sc, sc->sc_imsg | BCTL_BSY | sc->sc_busc);
		/* arb flag clear */
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit);
		/* assert sel */
		stghw_bcr_write_1(sc, sc->sc_imsg | BCTL_BSY | sc->sc_busc | BCTL_SEL);
		delay(3);
		/* deassert busy */
		stghw_bcr_write_1(sc, sc->sc_imsg | sc->sc_busc | BCTL_SEL);
#else	/* !STG_ALT_SELECTION */
		bus_space_write_1(iot, ioh, tmc_scsiid,
				  sc->sc_idbit | (1 << ti->ti_id));
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit);
		stghw_bcr_write_1(sc, sc->sc_imsg | sc->sc_busc | BCTL_SEL);
#endif	/* !STG_ALT_SELECTION */
		return 1;

	case PH_SELSTART:
		if ((status & BSTAT_BSY) == 0)
		{
			if (stghw_select_targ_wait(sc, ti->ti_id) != 0)
			{
				return stg_disconnected(sc, ti);
			}
		}

		/*
		 * attention assert.
		 */
		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit |
				  FCTL_CLRFIFO);
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		stghw_bcr_write_1(sc, sc->sc_imsg | sc->sc_busc);
		SCSI_LOW_TARGET_ASSERT_ATN(ti);
		scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_IDENTIFY, 0);
		return 1;

	case PH_RESEL:
		/* clear a busy line */
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		stghw_bcr_write_1(sc, sc->sc_busc);
		if ((status & PHASE_MASK) != MESSAGE_IN_PHASE)
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			return 1;
		}
		break;
	}

	/*******************************************
	 * scsi seq
	 *******************************************/
	if (slp->sl_flags & HW_PDMASTART)
		stg_pdma_end(sc, ti);

	switch (status & PHASE_MASK)
	{
	case COMMAND_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
			break;

		if (stg_xfer(sc, slp->sl_scp.scp_cmd,
		   	     slp->sl_scp.scp_cmdlen, COMMAND_PHASE) != 0)
		{
			printf("%s: MSGOUT short\n", slp->sl_xname);
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_RESET, 0);
		}
		break;

	case DATA_OUT_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
			break;

		pp = physio_proc_enter(bp);
		stg_pio_write(sc, ti);
		physio_proc_leave(pp);
		break;

	case DATA_IN_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
			break;

		pp = physio_proc_enter(bp);
		stg_pio_read(sc, ti);
		physio_proc_leave(pp);
		break;

	case STATUS_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
		ti->ti_status = bus_space_read_1(iot, ioh, tmc_rdata);
		break;

	case MESSAGE_OUT_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
		len = scsi_low_msgout(slp, ti);
		if (stg_xfer(sc, ti->ti_msgoutstr, len, MESSAGE_OUT_PHASE))
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_RESET, 0);
			printf("%s: MSGOUT short\n", slp->sl_xname);
		}
		break;

	case MESSAGE_IN_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

		/* confirm REQ signal */
		regv = stg_expect_signal(sc, MESSAGE_IN_PHASE, BSTAT_REQ);
		if (regv <= 0)
		{
			printf("%s: MSGIN: no req\n", slp->sl_xname);
			break;
		}
		/* read data with NOACK */
		regv = bus_space_read_1(sc->sc_iot, sc->sc_ioh, tmc_sdna);

		scsi_low_msgin(slp, ti, regv);

		/* read data with ACK */
		if (regv != bus_space_read_1(sc->sc_iot, sc->sc_ioh, tmc_rdata))
		{
			printf("%s: MSGIN: data mismatch\n", slp->sl_xname);
		}

		if (slp->sl_msgphase != 0)
		{
			stg_negate_signal(sc, BSTAT_ACK, "discon<ACK>");
			return stg_disconnected(sc, ti);
		}
		break;

	case BUSFREE_PHASE:
		printf("%s unexpected disconnection\n", slp->sl_xname);
		return stg_disconnected(sc, ti);

	default:
		printf("%s unknown phase bus %x intr %x\n",
			slp->sl_xname, status, astatus);
		break;
	}

	return 1;
}
