/*	$FreeBSD$	*/
/*	$NecBSD: ncr53c500.c,v 1.30 1999/07/23 21:00:04 honda Exp $	*/
/*	$NetBSD$	*/

#define	NCV_DEBUG
#define	NCV_STATICS

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
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

#include <i386/Cbus/dev/ncr53c500reg.h>
#include <i386/Cbus/dev/ncr53c500hw.h>
#include <i386/Cbus/dev/ncr53c500var.h>

#include <i386/Cbus/dev/ncr53c500hwtab.h>
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

#include <dev/ncv/ncr53c500reg.h>
#include <dev/ncv/ncr53c500hw.h>
#include <dev/ncv/ncr53c500var.h>

#include <dev/ncv/ncr53c500hwtab.h>

#if __FreeBSD_version < 400001
#include "ncv.h"
struct ncv_softc *ncvdata[NNCV];
#endif
#endif /* __FreeBSD__ */

/***************************************************
 * DEBUG
 ***************************************************/
#ifndef DDB
#define Debugger() panic("should call debugger here (ncr53c500.c)")
#else /* ! DDB */
#ifdef __FreeBSD__
#define Debugger() Debugger("ncv")
#endif /* __FreeBSD__ */
#endif

#ifdef	NCV_DEBUG
int ncv_debug;
#endif	/* NCV_DEBUG */

#ifdef	NCV_STATICS
struct ncv_statics {
	int disconnect;
	int reselect;
} ncv_statics[NCV_NTARGETS];
#endif	/* NCV_STATICS */

/***************************************************
 * ISA DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver ncv_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
/* static */
static void ncv_pio_read __P((struct ncv_softc *, u_int8_t *, u_int));
static void ncv_pio_write __P((struct ncv_softc *, u_int8_t *, u_int));
static int ncv_msg __P((struct ncv_softc *, struct targ_info *, u_int));
static __inline int ncv_reselected __P((struct ncv_softc *));
static __inline int ncv_disconnected __P((struct ncv_softc *, struct targ_info *));
static __inline void ncv_pdma_end __P((struct ncv_softc *sc, struct targ_info *));

static __inline void ncvhw_set_count __P((bus_space_tag_t, bus_space_handle_t, int));
static __inline u_int ncvhw_get_count __P((bus_space_tag_t, bus_space_handle_t));
static __inline void ncvhw_select_register_0 __P((bus_space_tag_t, bus_space_handle_t, struct ncv_hw *));
static __inline void ncvhw_select_register_1 __P((bus_space_tag_t, bus_space_handle_t, struct ncv_hw *));
static __inline void ncvhw_fpush __P((bus_space_tag_t, bus_space_handle_t, u_int8_t *, int));

static int ncv_world_start __P((struct ncv_softc *, int));
static void ncvhw_bus_reset __P((struct ncv_softc *));
static void ncvhw_reset __P((bus_space_tag_t, bus_space_handle_t, struct ncv_hw *));
static int ncvhw_check __P((bus_space_tag_t, bus_space_handle_t, struct ncv_hw *));
static void ncvhw_init __P((bus_space_tag_t, bus_space_handle_t, struct ncv_hw *));
static int ncvhw_start_selection __P((struct ncv_softc *sc, struct slccb *));
static void ncvhw_attention __P((struct ncv_softc *));
static int ncv_nexus __P((struct ncv_softc *, struct targ_info *));
#ifdef	NCV_POWER_CONTROL
static int ncvhw_power __P((struct ncv_softc *, u_int));
#endif
static int ncv_lun_init __P((struct ncv_softc *, struct targ_info *, struct lun_info *));
static void settimeout __P((void *));

struct scsi_low_funcs ncv_funcs = {
	SC_LOW_INIT_T ncv_world_start,
	SC_LOW_BUSRST_T ncvhw_bus_reset,
	SC_LOW_LUN_INIT_T ncv_lun_init,

	SC_LOW_SELECT_T ncvhw_start_selection,
	SC_LOW_NEXUS_T ncv_nexus,

	SC_LOW_ATTEN_T ncvhw_attention,
	SC_LOW_MSG_T ncv_msg,

	SC_LOW_POLL_T ncvintr,

	NULL,	/* SC_LOW_POWER_T ncvhw_power, */
};

/**************************************************************
 * hwfuncs
 **************************************************************/
static __inline void
ncvhw_select_register_0(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{

	bus_space_write_1(iot, ioh, cr0_cfg4, hw->cfg4);
}

static __inline void
ncvhw_select_register_1(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{

	bus_space_write_1(iot, ioh, cr1_cfg5, hw->cfg5);
}

static __inline void
ncvhw_fpush(iot, ioh, buf, len)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t *buf;
	int len;
{
	int ptr;

	for (ptr = 0; ptr < len; ptr ++)
		bus_space_write_1(iot, ioh, cr0_sfifo, buf[ptr]);
}

static int
ncvhw_check(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{
	u_int8_t stat;

	ncvhw_select_register_0(iot, ioh, hw);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP | CMD_DMA);
	if (bus_space_read_1(iot, ioh, cr0_cmd) != (CMD_NOP | CMD_DMA))
	{
#ifdef	NCV_DEBUG
		printf("ncv: cr0_cmd CMD_NOP|CMD_DMA failed\n");
#endif	/* NCV_DEBUG */
		return ENODEV;
	}

	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP);
	if (bus_space_read_1(iot, ioh, cr0_cmd) != CMD_NOP)
	{
#ifdef	NCV_DEBUG
		printf("ncv: cr0_cmd CMD_NOP failed\n");
#endif	/* NCV_DEBUG */
		return ENODEV;
	}

	/* hardware reset */
	ncvhw_reset(iot, ioh, hw);
	ncvhw_init(iot, ioh, hw);

	/* bus reset */
	ncvhw_select_register_0(iot, ioh, hw);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_RSTSCSI);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP | CMD_DMA);
	delay(100 * 1000);

	/* check response */
	bus_space_read_1(iot, ioh, cr0_stat);
	stat = bus_space_read_1(iot, ioh, cr0_istat);
	delay(1000);

	if (((stat & INTR_SBR) == 0) ||
	    (bus_space_read_1(iot, ioh, cr0_istat) & INTR_SBR))
	{
#ifdef	NCV_DEBUG
		printf("ncv: cr0_istat SCSI BUS RESET failed\n");
#endif	/* NCV_DEBUG */
		return ENODEV;
	}

	return 0;
}

static void
ncvhw_reset(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{

	ncvhw_select_register_0(iot, ioh, hw);

	/* dummy cmd twice */
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP);

	/* chip reset */
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_RSTCHIP);

	/* again dummy cmd twice */
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP);
}

static void
ncvhw_init(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{

	ncvhw_select_register_0(iot, ioh, hw);
	bus_space_write_1(iot, ioh, cr0_clk, hw->clk);
	bus_space_write_1(iot, ioh, cr0_srtout, SEL_TOUT);
	bus_space_write_1(iot, ioh, cr0_period, 0);
	bus_space_write_1(iot, ioh, cr0_offs, 0);

	bus_space_write_1(iot, ioh, cr0_cfg1, hw->cfg1);
	bus_space_write_1(iot, ioh, cr0_cfg2, hw->cfg2);
	bus_space_write_1(iot, ioh, cr0_cfg3, hw->cfg3);
	bus_space_write_1(iot, ioh, cr0_tchsb, 0);

	ncvhw_select_register_1(iot, ioh, hw);
	bus_space_write_1(iot, ioh, cr1_fstat, 0x0);
	bus_space_write_1(iot, ioh, cr1_pflag, 0x0);
	bus_space_write_1(iot, ioh, cr1_atacmd, ATACMD_ENGAGE);

	ncvhw_select_register_0(iot, ioh, hw);
}

#ifdef	NCV_POWER_CONTROL
static int
ncvhw_power(sc, flags)
	struct ncv_softc *sc;
	u_int flags;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (flags == SCSI_LOW_POWDOWN)
	{
		printf("%s power down\n", slp->sl_xname);
		ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
		bus_space_write_1(iot, ioh, cr1_atacmd, ATACMD_POWDOWN);
	}
	else
	{
		switch (sc->sc_rstep)
		{
		case 0:
			printf("%s resume step O\n", slp->sl_xname);
			ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
			bus_space_write_1(iot, ioh, cr1_atacmd, ATACMD_ENGAGE);
			break;

		case 1:
			printf("%s resume step I\n", slp->sl_xname);
			ncvhw_reset(iot, ioh, &sc->sc_hw);
			ncvhw_init(iot, ioh, &sc->sc_hw);
			break;
		}
	}

	return 0;
}
#endif	/* NCV_POWER_CONTROL */

/**************************************************************
 * scsi low interface
 **************************************************************/
static void
ncvhw_attention(sc)
	struct ncv_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, cr0_cmd, CMD_SETATN);
	delay(10);
}

static void
ncvhw_bus_reset(sc)
	struct ncv_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_RSTSCSI);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_NOP | CMD_DMA);
}

static int
ncvhw_start_selection(sc, cb)
	struct ncv_softc *sc;
	struct slccb *cb;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti = cb->ti;
	int s;
	u_int8_t msg;

	msg = ID_MSG_SETUP(ti);
	sc->sc_compseq = 0;
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);

	s = splhigh();

	if (slp->sl_disc > 0 &&
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_stat) & STAT_INT))
	{
		splx(s);
		return SCSI_LOW_START_FAIL;
	}

	bus_space_write_1(iot, ioh, cr0_dstid, ti->ti_id);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	bus_space_write_1(iot, ioh, cr0_sfifo, msg);

	if (scsi_low_is_msgout_continue(ti) != 0)
	{
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_SELATNS);
		sc->sc_selstop = 1;
	}
	else
	{
		/* XXX:
		 * emulate nexus call because ncv bypasses CMD phase.
		 */
		scsi_low_cmd(slp, ti);
		ncvhw_fpush(iot, ioh,
			    slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_SELATN);
		sc->sc_selstop = 0;
	}
	splx(s);

	SCSI_LOW_TARGET_ASSERT_ATN(ti);
	SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
	return SCSI_LOW_START_OK;
}

static int
ncv_world_start(sc, fdone)
	struct ncv_softc *sc;
	int fdone;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t stat;
	intrmask_t s;

	ncvhw_reset(iot, ioh, &sc->sc_hw);
	ncvhw_init(iot, ioh, &sc->sc_hw);

	s = splcam();
	scsi_low_bus_reset((struct scsi_low_softc *) sc);

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_stat);
	stat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_istat);
	splx(s);
	delay(1000);

	if (((stat & INTR_SBR) == 0) ||
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_istat) & INTR_SBR))
		return ENODEV;

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

static int
ncv_msg(sc, ti, msg)
	struct ncv_softc *sc;
	struct targ_info *ti;
	u_int msg;
{
	struct lun_info *li = ti->ti_li;
	struct ncv_lun_info *nli = (void *) li;
	u_int hwcycle, period;

	if ((msg & SCSI_LOW_MSG_SYNCH) == 0)
		return 0;

	period = li->li_maxsynch.period;
	hwcycle = 1000 / ((sc->sc_hw.clk == 0) ? 40 : (5 * sc->sc_hw.clk));

	if (period < 200 / 4 && period >= 100 / 4)
		nli->nli_reg_cfg3 |= C3_FSCSI;
	else
		nli->nli_reg_cfg3 &= ~C3_FSCSI;

	period = ((period * 40 / hwcycle) + 5) / 10;
	nli->nli_reg_period = period & 0x1f;
	nli->nli_reg_offset = li->li_maxsynch.offset;
	return 0;
}

static int
ncv_lun_init(sc, ti, li)
	struct ncv_softc *sc;
	struct targ_info *ti;
	struct lun_info *li;
{
	struct ncv_lun_info *nli = (void *) li;

	li->li_maxsynch.period = sc->sc_hw.mperiod;
	li->li_maxsynch.offset = sc->sc_hw.moffset;

	nli->nli_reg_cfg3 = sc->sc_hw.cfg3;
	nli->nli_reg_period = 0;
	nli->nli_reg_offset = 0;
	return 0;
}	

/**************************************************************
 * General probe attach
 **************************************************************/
static int ncv_setup_img __P((struct ncv_hw *, u_int, int));

static int
ncv_setup_img(hw, dvcfg, hsid)
	struct ncv_hw *hw;
	u_int dvcfg;
	int hsid;
{

	if (NCV_CLKFACTOR(dvcfg) > CLK_35M_F)
	{
		printf("ncv: invalid dvcfg flags\n");
		return EINVAL;
	}

	if (NCV_C5IMG(dvcfg) != 0)
	{
		hw->cfg5 = NCV_C5IMG(dvcfg);
		hw->clk = NCV_CLKFACTOR(dvcfg);

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_MAX10M)
			hw->mperiod = 100 / 4;

		/* XXX:	
		 * RATOC scsi cards have fatal fifo asic bug.
		 * To avoid it, currently make sync offset 0 (async)!
		 */
		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_FIFOBUG)
		{
			hw->mperiod = 0;
			hw->moffset = 0;
		}

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_SCSI1)
			hw->cfg2 &= ~C2_SCSI2;

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_SLOW)
			hw->cfg1 |= C1_SLOW;
	}

	/* setup configuration image 3 */
	if (hw->clk != CLK_40M_F && hw->clk <= CLK_25M_F)
		hw->cfg3 &= ~C3_FCLK;

	/* setup configuration image 1 */
	hw->cfg1 = (hw->cfg1 & 0xf0) | hsid;
	return 0;
}

int
ncvprobesubr(iot, ioh, dvcfg, hsid)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int dvcfg;
	int hsid;
{
	struct ncv_hw hwtab;

	hwtab = ncv_template;
	if (ncv_setup_img(&hwtab, dvcfg, hsid))
		return 0;
	if (ncvhw_check(iot, ioh, &hwtab) != 0)
		return 0;

	return 1;
}

int
ncvprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
ncvattachsubr(sc)
	struct ncv_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	printf("\n");
	sc->sc_hw = ncv_template;
	ncv_setup_img(&sc->sc_hw, slp->sl_cfgflags, slp->sl_hostid);
	slp->sl_funcs = &ncv_funcs;
	(void) scsi_low_attach(slp, 2, NCV_NTARGETS, NCV_NLUNS,
			       sizeof(struct ncv_lun_info));
}

/**************************************************************
 * PDMA
 **************************************************************/
static __inline void
ncvhw_set_count(iot, ioh, count)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int count;
{

	bus_space_write_1(iot, ioh, cr0_tclsb, (u_int8_t) count);
	bus_space_write_1(iot, ioh, cr0_tcmsb, (u_int8_t) (count >> NBBY));
	bus_space_write_1(iot, ioh, cr0_tchsb, (u_int8_t) (count >> (NBBY * 2)));
}

static __inline u_int
ncvhw_get_count(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_int count;

	count = (u_int) bus_space_read_1(iot, ioh, cr0_tclsb);
	count |= ((u_int) bus_space_read_1(iot, ioh, cr0_tcmsb)) << NBBY;
	count |= ((u_int) bus_space_read_1(iot, ioh, cr0_tchsb)) << (NBBY * 2);
	return count;
}

static __inline void
ncv_pdma_end(sc, ti)
	struct ncv_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int len;

	slp->sl_flags &= ~HW_PDMASTART;
	if (ti->ti_phase == PH_DATA)
	{
		len = ncvhw_get_count(sc->sc_iot, sc->sc_ioh);
		if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
			len += (bus_space_read_1(sc->sc_iot, sc->sc_ioh,
				cr0_sffl) & CR0_SFFLR_BMASK);

		if ((u_int) len <= (u_int) slp->sl_scp.scp_datalen)
		{
			slp->sl_scp.scp_data += (slp->sl_scp.scp_datalen - len);
			slp->sl_scp.scp_datalen = len;
			if ((slp->sl_scp.scp_direction == SCSI_LOW_READ) &&
			    sc->sc_tdatalen != len)
				goto bad;
		}
		else
		{
bad:
			slp->sl_error |= PDMAERR;
			printf("%s stragne count hw 0x%x soft 0x%x tlen 0x%x\n",
				slp->sl_xname, len, slp->sl_scp.scp_datalen,
				sc->sc_tdatalen);
		}
	}
	else
	{
		printf("%s data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_fstat, 0);
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
}

static void
ncv_pio_read(sc, buf, reqlen)
	struct ncv_softc *sc;
	u_int8_t *buf;
	u_int reqlen;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;
	int tout = 0;
	register u_int8_t fstat;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_pflag, 0);

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	ncvhw_set_count(iot, ioh, reqlen);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS | CMD_DMA);

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_fstat, FIFO_EN);
	slp->sl_flags |= HW_PDMASTART;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (reqlen >= FIFO_F_SZ && tout == 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_F)
		{
#define	NCV_FAST32_ACCESS
#ifdef	NCV_FAST32_ACCESS
			bus_space_read_multi_4(iot, ioh, cr1_fdata, 
				(u_int32_t *) buf, FIFO_F_SZ / 4);
#else	/* !NCV_FAST32_ACCESS */
			bus_space_read_multi_2(iot, ioh, cr1_fdata, 
				(u_int16_t *) buf, FIFO_F_SZ / 2);
#endif	/* !NCV_FAST32_ACCESS */
			buf += FIFO_F_SZ;
			reqlen -= FIFO_F_SZ;
			continue;
		}
		else if (fstat & FIFO_BRK)
			break;

	}

	if (reqlen >= FIFO_2_SZ)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_2)
		{
#ifdef	NCV_FAST32_ACCESS
			bus_space_read_multi_4(iot, ioh, cr1_fdata, 
				(u_int32_t *) buf, FIFO_2_SZ / 4);
#else	/* !NCV_FAST32_ACCESS */
			bus_space_read_multi_2(iot, ioh, cr1_fdata, 
				(u_int16_t *) buf, FIFO_2_SZ / 2);
#endif	/* !NCV_FAST32_ACCESS */
			buf += FIFO_2_SZ;
			reqlen -= FIFO_2_SZ;
		}
	}

	while (reqlen > 0 && tout == 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if ((fstat & FIFO_E) == 0)
		{
			*buf++ = bus_space_read_1(iot, ioh, cr1_fdata);
			reqlen --;
			continue;
		}
		else if (fstat & FIFO_BRK)
			break;

	}

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	sc->sc_tdatalen = reqlen;

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
ncv_pio_write(sc, buf, reqlen)
	struct ncv_softc *sc;
	u_int8_t *buf;
	u_int reqlen;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;
	int tout = 0;
	register u_int8_t fstat;
#ifdef __FreeBSD__
	struct callout_handle ch;
#endif

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_pflag, 0);

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	ncvhw_set_count(iot, ioh, reqlen);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS | CMD_DMA);

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_fstat, FIFO_EN);
	slp->sl_flags |= HW_PDMASTART;

#ifdef __FreeBSD__
	ch = timeout(settimeout, &tout, 2 * hz);
#else
	timeout(settimeout, &tout, 2 * hz);
#endif
	while (reqlen >= FIFO_F_SZ && tout == 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_BRK)
			goto done;

		if (fstat & FIFO_E)
		{
#ifdef	NCV_FAST32_ACCESS
			bus_space_write_multi_4(iot, ioh, cr1_fdata, 
				(u_int32_t *) buf, FIFO_F_SZ / 4);
#else	/* !NCV_FAST32_ACCESS */
			bus_space_write_multi_2(iot, ioh, cr1_fdata, 
				(u_int16_t *) buf, FIFO_F_SZ / 2);
#endif	/* !NCV_FAST32_ACCESS */
			buf += FIFO_F_SZ;
			reqlen -= FIFO_F_SZ;
		}
	}

	while (reqlen > 0 && tout == 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_BRK)
			break;

		if ((fstat & FIFO_F) == 0) /* fifo not full */
		{
			bus_space_write_1(iot, ioh, cr1_fdata, *buf++);
			reqlen --;
		}
	}

done:
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);

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

/**************************************************************
 * disconnect & reselect (HW low)
 **************************************************************/
static __inline int
ncv_reselected(sc)
	struct ncv_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti;
	u_int sid;

	if ((bus_space_read_1(iot, ioh, cr0_sffl) & CR0_SFFLR_BMASK) != 2)
	{
		printf("%s illegal fifo bytes\n", slp->sl_xname);
		scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, "chip confused");
		return EJUSTRETURN;
	}

	sid = (u_int) bus_space_read_1(iot, ioh, cr0_sfifo);
	sid = ffs(sid) - 1;
	ti = scsi_low_reselected((struct scsi_low_softc *) sc, sid);
	if (ti == NULL)
		return EJUSTRETURN;

#ifdef	NCV_STATICS
	ncv_statics[sid].reselect ++;
#endif	/* NCV_STATICS */
	bus_space_write_1(iot, ioh, cr0_dstid, sid);
	return 0;
}

static __inline int
ncv_disconnected(sc, ti)
	struct ncv_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	bus_space_write_1(iot, ioh, cr0_cfg1, sc->sc_hw.cfg1);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_ENSEL);

#ifdef	NCV_STATICS
	if (slp->sl_msgphase == MSGPH_DISC)
		ncv_statics[ti->ti_id].disconnect ++;
#endif	/* NCV_STATICS */

	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static int
ncv_nexus(sc, ti)
	struct ncv_softc *sc;
	struct targ_info *ti;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct lun_info *li = ti->ti_li;
	struct ncv_lun_info *nli = (void *) li;

	if (li->li_flags & SCSI_LOW_NOPARITY)
		bus_space_write_1(iot, ioh, cr0_cfg1, sc->sc_hw.cfg1);
	else
		bus_space_write_1(iot, ioh, cr0_cfg1, sc->sc_hw.cfg1 | C1_PARENB);
	bus_space_write_1(iot, ioh, cr0_period, nli->nli_reg_period);
	bus_space_write_1(iot, ioh, cr0_offs, nli->nli_reg_offset);
	bus_space_write_1(iot, ioh, cr0_cfg3, nli->nli_reg_cfg3);
	return 0;
}

int
ncvintr(arg)
	void *arg;
{
	struct ncv_softc *sc = arg;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	int len, identify;
	u_int8_t regv, status, ireason;

	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	/********************************************
	 * Status
	 ********************************************/
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	status = bus_space_read_1(iot, ioh, cr0_stat);
	if ((status & STAT_INT) == 0)
		return 0;

	ireason = bus_space_read_1(iot, ioh, cr0_istat);
	if (ireason & INTR_SBR)
	{
		u_int8_t val;

		/* avoid power off hangup */
		val = bus_space_read_1(iot, ioh, cr0_cfg1);
		bus_space_write_1(iot, ioh, cr0_cfg1, val | C1_SRR);

		/* status init */
		scsi_low_restart(slp, SCSI_LOW_RESTART_SOFT, 
				 "bus reset (power off?)");
		return 1;
	}

	/********************************************
	 * Debug section
	 ********************************************/
#ifdef	NCV_DEBUG
	if (ncv_debug)
	{
		scsi_low_print(slp, NULL);
		printf("%s st %x ist %x\n\n", slp->sl_xname,
			status, ireason);
		if (ncv_debug > 1)
			Debugger();
	}
#endif	/* NCV_DEBUG */

	/********************************************
	 * Reselect or Disconnect or Nexus check
	 ********************************************/
	/* (I) reselect */
	if (ireason == INTR_RESELECT)
	{
		if (ncv_reselected(sc) == EJUSTRETURN)
			return 1;
	}

	/* (II) nexus */
	if ((ti = slp->sl_nexus) == NULL)
		return 0;

	if ((status & (STAT_PE | STAT_GE)) != 0)
	{
		slp->sl_error |= PARITYERR;
		if (ti->ti_phase == PH_MSGIN)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_PARITY, 1);
		else
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ERROR, 1);
	}

	if ((ireason & (INTR_DIS | INTR_ILL)) != 0)
	{
		if ((ireason & INTR_ILL) == 0)
			return ncv_disconnected(sc, ti);

		slp->sl_error |= FATALIO;
		scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, "illegal cmd");
		return 1;
	}

	/********************************************
	 * Internal scsi phase
	 ********************************************/
	switch (ti->ti_phase)
	{
	case PH_SELSTART:
		scsi_low_arbit_win(slp, ti);
		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		identify = 0;

		if (sc->sc_selstop == 0)
		{
			/* XXX:
		 	 * Here scsi phases expected are
			 * DATA PHASE: 
		 	 * MSGIN     : target wants to disconnect the host.
			 * STATUSIN  : immediate command completed.
			 * MSGOUT    : identify command failed.
			 */
			if ((status & PHASE_MASK) != MESSAGE_OUT_PHASE)
				break;
			identify = 1;
		}
		else
		{
		 	 /* XXX:
			  * Here scsi phase should be MSGOUT. 
			  * The driver NEVER supports devices
			  * which neglect ATN singal.
			  */
			if ((status & PHASE_MASK) != MESSAGE_OUT_PHASE)
			{
				slp->sl_error |= FATALIO;
				scsi_low_restart(slp, SCSI_LOW_RESTART_HARD,
						 "msgout error");
				return 1;
			}

			if ((ireason & INTR_FC) == 0)
				identify = 1;
		}

		if (identify != 0)
		{
			printf("%s msg identify failed\n", slp->sl_xname);
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_IDENTIFY, 0);
		}
		break;

	case PH_RESEL:
		if ((status & PHASE_MASK) != MESSAGE_IN_PHASE)
		{
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			return 1;
		}
		break;

	default:
		if (slp->sl_flags & HW_PDMASTART)
			ncv_pdma_end(sc, ti);
		break;
	}

	/********************************************
	 * Scsi phase sequencer
	 ********************************************/
	switch (status & PHASE_MASK)
	{
	case DATA_OUT_PHASE: /* data out */
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
			break;

		pp = physio_proc_enter(bp);
		ncv_pio_write(sc, slp->sl_scp.scp_data, slp->sl_scp.scp_datalen);
		physio_proc_leave(pp);
		break;

	case DATA_IN_PHASE: /* data in */
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
			break;

		pp = physio_proc_enter(bp);
		ncv_pio_read(sc, slp->sl_scp.scp_data, slp->sl_scp.scp_datalen);
		physio_proc_leave(pp);
		break;

	case COMMAND_PHASE: /* cmd out */
		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
			break;

		bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
		ncvhw_fpush(iot, ioh,
			    slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS);
		break;

	case STATUS_PHASE: /* status in */
		SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_ICCS);
		sc->sc_compseq = 1;
		break;

	default:
		break;

	case MESSAGE_OUT_PHASE: /* msg out */
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);

		len = scsi_low_msgout(slp, ti);
		ncvhw_fpush(iot, ioh, ti->ti_msgoutstr, len);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS);
		if (scsi_low_is_msgout_continue(ti) == 0)
			bus_space_write_1(iot, ioh, cr0_cmd, CMD_RSTATN);
		break;

	case MESSAGE_IN_PHASE: /* msg in */
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

		len = bus_space_read_1(iot, ioh, cr0_sffl) & CR0_SFFLR_BMASK;
		if (sc->sc_compseq != 0)
		{
			sc->sc_compseq = 0;
			if ((ireason & INTR_FC) && len == 2)
			{
				ti->ti_status =
					bus_space_read_1(iot, ioh, cr0_sfifo);
				len --;
			}
			else
			{
				scsi_low_restart(slp, SCSI_LOW_RESTART_HARD,
						 "compseq error");
				break;
			}
		}
		else if (ireason & INTR_BS)
		{
			bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
			bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS);
			break;
		}

		if ((ireason & INTR_FC) && len == 1)
		{
			regv = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
					        cr0_sfifo);
			scsi_low_msgin(slp, ti, regv);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, cr0_cmd,
				CMD_MSGOK);
		}
		else
		{
			slp->sl_error |= MSGERR;
			printf("%s st %x ist %x\n\n", slp->sl_xname,
				status, ireason);
			scsi_low_restart(slp, SCSI_LOW_RESTART_HARD, 
					 "hw msgin error");
		}
		break;
	}

	return 1;
}
