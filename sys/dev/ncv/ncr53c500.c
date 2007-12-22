/*	$NecBSD: ncr53c500.c,v 1.30.12.3 2001/06/26 07:31:41 honda Exp $	*/
/*	$NetBSD$	*/

#define	NCV_DEBUG
#define	NCV_STATICS
#define	NCV_IO_CONTROL_FLAGS	(0)

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
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
#if defined(__FreeBSD__) && __FreeBSD_version >= 500001
#include <sys/bio.h>
#endif	/* __FreeBSD__ */
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

#include <i386/Cbus/dev/ncr53c500reg.h>
#include <i386/Cbus/dev/ncr53c500hw.h>
#include <i386/Cbus/dev/ncr53c500var.h>

#include <i386/Cbus/dev/ncr53c500hwtab.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/cpu.h>
#include <machine/bus.h>

#include <compat/netbsd/dvcfg.h>
#include <compat/netbsd/physio_proc.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ncv/ncr53c500reg.h>
#include <dev/ncv/ncr53c500hw.h>
#include <dev/ncv/ncr53c500var.h>

#include <dev/ncv/ncr53c500hwtab.h>
#endif /* __FreeBSD__ */

#define	NCV_MAX_DATA_SIZE	(64 * 1024)
#define	NCV_DELAY_MAX		(2 * 1000 * 1000)
#define	NCV_DELAY_INTERVAL	(1)
#define	NCV_PADDING_SIZE	(32)

/***************************************************
 * IO control
 ***************************************************/
#define	NCV_READ_INTERRUPTS_DRIVEN	0x0001
#define	NCV_WRITE_INTERRUPTS_DRIVEN	0x0002
#define	NCV_ENABLE_FAST_SCSI		0x0010
#define	NCV_FAST_INTERRUPTS		0x0100

u_int ncv_io_control = NCV_IO_CONTROL_FLAGS;
int ncv_data_read_bytes = 4096;
int ncv_data_write_bytes = 4096;

/***************************************************
 * DEBUG
 ***************************************************/
#ifdef	NCV_DEBUG
static int ncv_debug;
#endif	/* NCV_DEBUG */

#ifdef	NCV_STATICS
static struct ncv_statics {
	int disconnect;
	int reselect;
} ncv_statics;
#endif	/* NCV_STATICS */

/***************************************************
 * DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver ncv_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
/* static */
static void ncv_pio_read(struct ncv_softc *, u_int8_t *, u_int);
static void ncv_pio_write(struct ncv_softc *, u_int8_t *, u_int);
static int ncv_msg(struct ncv_softc *, struct targ_info *, u_int);
static int ncv_reselected(struct ncv_softc *);
static int ncv_disconnected(struct ncv_softc *, struct targ_info *);

static __inline void ncvhw_set_count(bus_space_tag_t, bus_space_handle_t, int);
static __inline u_int ncvhw_get_count(bus_space_tag_t, bus_space_handle_t);
static __inline void ncvhw_select_register_0(bus_space_tag_t, bus_space_handle_t, struct ncv_hw *);
static __inline void ncvhw_select_register_1(bus_space_tag_t, bus_space_handle_t, struct ncv_hw *);
static __inline void ncvhw_fpush(bus_space_tag_t, bus_space_handle_t, u_int8_t *, int);

static void ncv_pdma_end(struct ncv_softc *sc, struct targ_info *);
static int ncv_world_start(struct ncv_softc *, int);
static void ncvhw_bus_reset(struct ncv_softc *);
static void ncvhw_reset(bus_space_tag_t, bus_space_handle_t, struct ncv_hw *);
static int ncvhw_check(bus_space_tag_t, bus_space_handle_t, struct ncv_hw *);
static void ncvhw_init(bus_space_tag_t, bus_space_handle_t, struct ncv_hw *);
static int ncvhw_start_selection(struct ncv_softc *sc, struct slccb *);
static void ncvhw_attention(struct ncv_softc *);
static int ncv_ccb_nexus_establish(struct ncv_softc *);
static int ncv_lun_nexus_establish(struct ncv_softc *);
static int ncv_target_nexus_establish(struct ncv_softc *);
static int ncv_targ_init(struct ncv_softc *, struct targ_info *, int);
static int ncv_catch_intr(struct ncv_softc *);
#ifdef	NCV_POWER_CONTROL
static int ncvhw_power(struct ncv_softc *, u_int);
#endif	/* NCV_POWER_CONTROL */
static __inline void ncv_setup_and_start_pio(struct ncv_softc *, u_int);

struct scsi_low_funcs ncv_funcs = {
	SC_LOW_INIT_T ncv_world_start,
	SC_LOW_BUSRST_T ncvhw_bus_reset,
	SC_LOW_TARG_INIT_T ncv_targ_init,
	SC_LOW_LUN_INIT_T NULL,

	SC_LOW_SELECT_T ncvhw_start_selection,
	SC_LOW_NEXUS_T ncv_lun_nexus_establish,
	SC_LOW_NEXUS_T ncv_ccb_nexus_establish,

	SC_LOW_ATTEN_T ncvhw_attention,
	SC_LOW_MSG_T ncv_msg,

	SC_LOW_TIMEOUT_T NULL,
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

	bus_space_write_1(iot, ioh, cr0_cfg4, hw->hw_cfg4);
}

static __inline void
ncvhw_select_register_1(iot, ioh, hw)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct ncv_hw *hw;
{

	bus_space_write_1(iot, ioh, cr1_cfg5, hw->hw_cfg5);
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
	SCSI_LOW_DELAY(100 * 1000);

	/* check response */
	bus_space_read_1(iot, ioh, cr0_stat);
	stat = bus_space_read_1(iot, ioh, cr0_istat);
	SCSI_LOW_DELAY(1000);

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
	bus_space_write_1(iot, ioh, cr0_clk, hw->hw_clk);
	bus_space_write_1(iot, ioh, cr0_srtout, SEL_TOUT);
	bus_space_write_1(iot, ioh, cr0_period, 0);
	bus_space_write_1(iot, ioh, cr0_offs, 0);

	bus_space_write_1(iot, ioh, cr0_cfg1, hw->hw_cfg1);
	bus_space_write_1(iot, ioh, cr0_cfg2, hw->hw_cfg2);
	bus_space_write_1(iot, ioh, cr0_cfg3, hw->hw_cfg3);
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
	SCSI_LOW_DELAY(10);
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
	int s, len;
	u_int flags;
	u_int8_t cmd;

	sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	sc->sc_compseq = 0;
	if (scsi_low_is_msgout_continue(ti, SCSI_LOW_MSG_IDENTIFY) == 0)
	{
		cmd = CMD_SELATN;
		sc->sc_selstop = 0;
		flags = SCSI_LOW_MSGOUT_UNIFY | SCSI_LOW_MSGOUT_INIT;
	}
	else if (scsi_low_is_msgout_continue(ti, 
			SCSI_LOW_MSG_IDENTIFY | SCSI_LOW_MSG_SIMPLE_QTAG) == 0)
	{
		cmd = CMD_SELATN3;
		sc->sc_selstop = 0;
		flags = SCSI_LOW_MSGOUT_UNIFY | SCSI_LOW_MSGOUT_INIT;
	}	
	else
	{
		cmd = CMD_SELATNS;
		sc->sc_selstop = 1;
		flags = SCSI_LOW_MSGOUT_INIT;
	}

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	if ((bus_space_read_1(iot, ioh, cr0_stat) & STAT_INT) != 0)
		return SCSI_LOW_START_FAIL;

	ncv_target_nexus_establish(sc);

	len = scsi_low_msgout(slp, ti, flags);
	if (sc->sc_selstop == 0)
		scsi_low_cmd(slp, ti);

	s = splhigh();
	if ((bus_space_read_1(iot, ioh, cr0_stat) & STAT_INT) != 0)
	{
		splx(s);
		return SCSI_LOW_START_FAIL;
	}

	bus_space_write_1(iot, ioh, cr0_dstid, ti->ti_id);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	ncvhw_fpush(iot, ioh, ti->ti_msgoutstr, len);
	if (sc->sc_selstop == 0)
	{
		ncvhw_fpush(iot, ioh,
			    slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen);
	}
	bus_space_write_1(iot, ioh, cr0_cmd, cmd);
	splx(s);

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

	if ((slp->sl_cfgflags & CFG_NOPARITY) == 0)
		sc->sc_hw.hw_cfg1 |= C1_PARENB;
	else
		sc->sc_hw.hw_cfg1 &= ~C1_PARENB;

	ncvhw_reset(iot, ioh, &sc->sc_hw);
	ncvhw_init(iot, ioh, &sc->sc_hw);

	scsi_low_bus_reset(slp);

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_stat);
	stat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, cr0_istat);
	SCSI_LOW_DELAY(1000);

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
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ncv_targ_info *nti = (void *) ti;
	u_int hwcycle, period;

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
	hwcycle = (sc->sc_hw.hw_clk == 0) ? 40 : (5 * sc->sc_hw.hw_clk);
	hwcycle = 1000 / hwcycle;

	if (period < 200 / 4 && period >= 100 / 4)
		nti->nti_reg_cfg3 |= sc->sc_hw.hw_cfg3_fscsi;
	else
		nti->nti_reg_cfg3 &= ~sc->sc_hw.hw_cfg3_fscsi;

	period = ((period * 40 / hwcycle) + 5) / 10;
	nti->nti_reg_period = period & 0x1f;
	nti->nti_reg_offset = ti->ti_maxsynch.offset;

	bus_space_write_1(iot, ioh, cr0_period, nti->nti_reg_period);
	bus_space_write_1(iot, ioh, cr0_offs, nti->nti_reg_offset);
	bus_space_write_1(iot, ioh, cr0_cfg3, nti->nti_reg_cfg3);
	return 0;
}

static int
ncv_targ_init(sc, ti, action)
	struct ncv_softc *sc;
	struct targ_info *ti;
	int action;
{
	struct ncv_targ_info *nti = (void *) ti;

	if (action == SCSI_LOW_INFO_ALLOC || action == SCSI_LOW_INFO_REVOKE)
	{
		ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
		ti->ti_maxsynch.period = sc->sc_hw.hw_mperiod;
		ti->ti_maxsynch.offset = sc->sc_hw.hw_moffset;

		nti->nti_reg_cfg3 = sc->sc_hw.hw_cfg3;
		nti->nti_reg_period = 0;
		nti->nti_reg_offset = 0;
	}
	return 0;
}	

/**************************************************************
 * General probe attach
 **************************************************************/
static int ncv_setup_img(struct ncv_hw *, u_int, int);

static int
ncv_setup_img(hw, dvcfg, hostid)
	struct ncv_hw *hw;
	u_int dvcfg;
	int hostid;
{

	if (NCV_CLKFACTOR(dvcfg) > CLK_35M_F)
	{
		printf("ncv: invalid dvcfg flags\n");
		return EINVAL;
	}

	if (NCV_C5IMG(dvcfg) != 0)
	{
		hw->hw_cfg5 = NCV_C5IMG(dvcfg);
		hw->hw_clk = NCV_CLKFACTOR(dvcfg);

		if ((ncv_io_control & NCV_ENABLE_FAST_SCSI) != 0 &&
		    (NCV_SPECIAL(dvcfg) & NCVHWCFG_MAX10M) != 0)
			hw->hw_mperiod = 100 / 4;

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_FIFOBUG)
			hw->hw_cfg3_fclk = 0x04;

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_SCSI1)
			hw->hw_cfg2 &= ~C2_SCSI2;

		if (NCV_SPECIAL(dvcfg) & NCVHWCFG_SLOW)
			hw->hw_cfg1 |= C1_SLOW;
	}

	/* setup configuration image 3 */
	if (hw->hw_clk != CLK_40M_F && hw->hw_clk <= CLK_25M_F)
		hw->hw_cfg3 &= ~hw->hw_cfg3_fclk;
	else
		hw->hw_cfg3 |= hw->hw_cfg3_fclk;

	/* setup configuration image 1 */
	hw->hw_cfg1 = (hw->hw_cfg1 & 0xf0) | hostid;
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
	slp->sl_flags |= HW_READ_PADDING;
	sc->sc_tmaxcnt = SCSI_LOW_MIN_TOUT * 1000 * 1000; /* default */

	(void) scsi_low_attach(slp, 0, NCV_NTARGETS, NCV_NLUNS,
			       sizeof(struct ncv_targ_info), 0);
}

/**************************************************************
 * PDMA
 **************************************************************/
static __inline void
ncv_setup_and_start_pio(sc, reqlen)
	struct ncv_softc *sc;
	u_int reqlen;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	ncvhw_set_count(iot, ioh, reqlen);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS | CMD_DMA);

	ncvhw_select_register_1(iot, ioh, &sc->sc_hw);
	bus_space_write_1(iot, ioh, cr1_fstat, FIFO_EN);
}

static void
ncv_pdma_end(sc, ti)
	struct ncv_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int len;

	slp->sl_flags &= ~HW_PDMASTART;
	if (slp->sl_Qnexus == NULL)
	{
		slp->sl_error |= PDMAERR;
		goto out;
	}

	if (ti->ti_phase == PH_DATA)
	{
		len = ncvhw_get_count(sc->sc_iot, sc->sc_ioh);
		if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
			len += (bus_space_read_1(sc->sc_iot, sc->sc_ioh,
				cr0_sffl) & CR0_SFFLR_BMASK);

		if ((u_int) len <= (u_int) sc->sc_sdatalen)
		{
			if ((slp->sl_scp.scp_direction == SCSI_LOW_READ) &&
			    sc->sc_tdatalen != len)
				goto bad;

			len = sc->sc_sdatalen - len;
			if ((u_int) len > (u_int) slp->sl_scp.scp_datalen)
				goto bad;

			slp->sl_scp.scp_data += len;
			slp->sl_scp.scp_datalen -= len;
		}
		else
		{
bad:
			if ((slp->sl_error & PDMAERR) == 0)
			{
				printf("%s: stragne cnt hw 0x%x soft 0x%x\n",
					slp->sl_xname, len,
					slp->sl_scp.scp_datalen);
			}
			slp->sl_error |= PDMAERR;
		}
		scsi_low_data_finish(slp);
	}
	else
	{
		printf("%s: data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}

out:
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
	int tout;
	register u_int8_t fstat;

	ncv_setup_and_start_pio(sc, reqlen);
	slp->sl_flags |= HW_PDMASTART;
	sc->sc_sdatalen = reqlen;
	tout = sc->sc_tmaxcnt;

	while (reqlen >= FIFO_F_SZ && tout -- > 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat == (u_int8_t) -1)
			goto out;
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
		}
		else 
		{
			if (fstat & FIFO_BRK)
				break;

			SCSI_LOW_DELAY(1);
		}
	}

	while (reqlen > 0 && tout -- > 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if ((fstat & FIFO_E) == 0)
		{
			*buf++ = bus_space_read_1(iot, ioh, cr1_fdata);
			reqlen --;
		}
		else
		{
			 if (fstat & FIFO_BRK)
				break;

			SCSI_LOW_DELAY(1);
		}
	}

out:
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	sc->sc_tdatalen = reqlen;
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
	int tout;
	register u_int8_t fstat;

	ncv_setup_and_start_pio(sc, reqlen);
	sc->sc_sdatalen = reqlen;
	tout = sc->sc_tmaxcnt;
	slp->sl_flags |= HW_PDMASTART;

	while (reqlen >= FIFO_F_SZ && tout -- > 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_BRK)
			goto done;

		if ((fstat & FIFO_E) != 0)
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
		else
		{
			SCSI_LOW_DELAY(1);
		}
	}

	while (reqlen > 0 && tout -- > 0)
	{
		fstat = bus_space_read_1(iot, ioh, cr1_fstat);
		if (fstat & FIFO_BRK)
			break;

		if ((fstat & FIFO_F) == 0) /* fifo not full */
		{
			bus_space_write_1(iot, ioh, cr1_fdata, *buf++);
			reqlen --;
		}
		else
		{
			SCSI_LOW_DELAY(1);
		}
	}

done:
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
}

/**************************************************************
 * disconnect & reselect (HW low)
 **************************************************************/
static int
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
	sid &= ~(1 << slp->sl_hostid);
	sid = ffs(sid) - 1;
	ti = scsi_low_reselected((struct scsi_low_softc *) sc, sid);
	if (ti == NULL)
		return EJUSTRETURN;

#ifdef	NCV_STATICS
	ncv_statics.reselect ++;
#endif	/* NCV_STATICS */
	bus_space_write_1(iot, ioh, cr0_dstid, sid);
	return 0;
}

static int
ncv_disconnected(sc, ti)
	struct ncv_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
	bus_space_write_1(iot, ioh, cr0_cmd, CMD_ENSEL);

#ifdef	NCV_STATICS
	ncv_statics.disconnect ++;
#endif	/* NCV_STATICS */

	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static int
ncv_target_nexus_establish(sc)
	struct ncv_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct targ_info *ti = slp->sl_Tnexus;
	struct ncv_targ_info *nti = (void *) ti;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, cr0_period, nti->nti_reg_period);
	bus_space_write_1(iot, ioh, cr0_offs, nti->nti_reg_offset);
	bus_space_write_1(iot, ioh, cr0_cfg3, nti->nti_reg_cfg3);
	return 0;
}

static int
ncv_lun_nexus_establish(sc)
	struct ncv_softc *sc;
{

	return 0;
}

static int
ncv_ccb_nexus_establish(sc)
	struct ncv_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = slp->sl_Qnexus;

	sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	return 0;
}

static int
ncv_catch_intr(sc)
	struct ncv_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int wc;
	register u_int8_t status;

	for (wc = 0; wc < NCV_DELAY_MAX / NCV_DELAY_INTERVAL; wc ++)
	{
		status = bus_space_read_1(iot, ioh, cr0_stat);
		if ((status & STAT_INT) != 0)
			return 0;

		SCSI_LOW_DELAY(NCV_DELAY_INTERVAL);
	}
	return EJUSTRETURN;
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
	u_int derror, flags;
	int len;
	u_int8_t regv, status, ireason;

again:
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	/********************************************
	 * Status
	 ********************************************/
	ncvhw_select_register_0(iot, ioh, &sc->sc_hw);
	status = bus_space_read_1(iot, ioh, cr0_stat);
	if ((status & STAT_INT) == 0 || status == (u_int8_t) -1)
		return 0;

	ireason = bus_space_read_1(iot, ioh, cr0_istat);
	if ((ireason & INTR_SBR) != 0)
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
#ifdef	KDB
		if (ncv_debug > 1)
			SCSI_LOW_DEBUGGER("ncv");
#endif	/* KDB */
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
	if ((ti = slp->sl_Tnexus) == NULL)
		return 0;

	derror = 0;
	if ((status & (STAT_PE | STAT_GE)) != 0)
	{
		slp->sl_error |= PARITYERR;
		if ((status & PHASE_MASK) == MESSAGE_IN_PHASE)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_PARITY, 0);
		else
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ERROR, 1);
		derror = SCSI_LOW_DATA_PE;
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
		scsi_low_arbit_win(slp);
		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);

		if (sc->sc_selstop == 0)
		{
			/* XXX:
		 	 * Here scsi phases expected are
			 * DATA PHASE: 
		 	 * MSGIN     : target wants to disconnect the host.
			 * STATUSIN  : immediate command completed.
			 * CMD PHASE : command out failed
			 * MSGOUT    : identify command failed.
			 */
			if ((status & PHASE_MASK) != MESSAGE_OUT_PHASE)
				break;
		}
		else
		{
			if ((status & PHASE_MASK) != MESSAGE_OUT_PHASE)
				break;
			if ((ireason & INTR_FC) != 0) 
			{
				SCSI_LOW_ASSERT_ATN(slp);
			}
		}
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
		break;

	case PH_RESEL:
		ncv_target_nexus_establish(sc);
		if ((status & PHASE_MASK) != MESSAGE_IN_PHASE)
		{
			printf("%s: unexpected phase after reselect\n",
				slp->sl_xname);
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			return 1;
		}
		break;

	default:
		if ((slp->sl_flags & HW_PDMASTART) != 0)
		{
			ncv_pdma_end(sc, ti);
		}
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
		{
			scsi_low_attention(slp);
		}

		pp = physio_proc_enter(bp);
		if (slp->sl_scp.scp_datalen <= 0)
		{
			if ((ireason & INTR_BS) == 0)
				break;

			if ((slp->sl_error & PDMAERR) == 0)
				printf("%s: data underrun\n", slp->sl_xname);
			slp->sl_error |= PDMAERR;

			if ((slp->sl_flags & HW_WRITE_PADDING) != 0)
			{
				u_int8_t padding[NCV_PADDING_SIZE];

				SCSI_LOW_BZERO(padding, sizeof(padding));
				ncv_pio_write(sc, padding, sizeof(padding));
			}
			else
			{
				printf("%s: write padding required\n",
					slp->sl_xname);
			}
		}
		else
		{
			len = slp->sl_scp.scp_datalen;
			if ((ncv_io_control & NCV_WRITE_INTERRUPTS_DRIVEN) != 0)
			{
				if (len > ncv_data_write_bytes)
					len = ncv_data_write_bytes;
			}
			ncv_pio_write(sc, slp->sl_scp.scp_data, len);
		}
		physio_proc_leave(pp);
		break;

	case DATA_IN_PHASE: /* data in */
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
		{
			scsi_low_attention(slp);
		}

		pp = physio_proc_enter(bp);
		if (slp->sl_scp.scp_datalen <= 0)
		{
			if ((ireason & INTR_BS) == 0)
				break;

			if ((slp->sl_error & PDMAERR) == 0)
				printf("%s: data overrun\n", slp->sl_xname);
			slp->sl_error |= PDMAERR;

			if ((slp->sl_flags & HW_READ_PADDING) != 0)
			{
				u_int8_t padding[NCV_PADDING_SIZE];

				ncv_pio_read(sc, padding, sizeof(padding));
			}
			else
			{
				printf("%s: read padding required\n",
					slp->sl_xname);
				break;
			}
		}
		else
		{
			len = slp->sl_scp.scp_datalen;
			if ((ncv_io_control & NCV_READ_INTERRUPTS_DRIVEN) != 0)
			{
				if (len > ncv_data_read_bytes)
					len = ncv_data_read_bytes;
			}
			ncv_pio_read(sc, slp->sl_scp.scp_data, len);
		}
		physio_proc_leave(pp);
		break;

	case COMMAND_PHASE: /* cmd out */
		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
		{
			scsi_low_attention(slp);
		}

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

		flags = SCSI_LOW_MSGOUT_UNIFY;
		if (ti->ti_ophase != ti->ti_phase)
			flags |= SCSI_LOW_MSGOUT_INIT;
		len = scsi_low_msgout(slp, ti, flags);

		if (len > 1 && slp->sl_atten == 0)
		{
			scsi_low_attention(slp);
		}

		ncvhw_fpush(iot, ioh, ti->ti_msgoutstr, len);
		bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS);
		SCSI_LOW_DEASSERT_ATN(slp);
		break;

	case MESSAGE_IN_PHASE: /* msg in */
		SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

		len = bus_space_read_1(iot, ioh, cr0_sffl) & CR0_SFFLR_BMASK;
		if (sc->sc_compseq != 0)
		{
			sc->sc_compseq = 0;
			if ((ireason & INTR_FC) && len == 2)
			{
				regv = bus_space_read_1(iot, ioh, cr0_sfifo);
				scsi_low_statusin(slp, ti, regv | derror);
				len --;
			}
			else
			{
				slp->sl_error |= FATALIO;
				scsi_low_assert_msg(slp, ti,
						    SCSI_LOW_MSG_ABORT, 1);
				bus_space_write_1(sc->sc_iot, sc->sc_ioh,
						  cr0_cmd, CMD_MSGOK);
				break;
			}
		}
		else if (ireason & INTR_BS)
		{
			bus_space_write_1(iot, ioh, cr0_cmd, CMD_FLUSH);
			bus_space_write_1(iot, ioh, cr0_cmd, CMD_TRANS);
			if ((ncv_io_control & NCV_FAST_INTERRUPTS) != 0)
			{
				if (ncv_catch_intr(sc) == 0)
					goto again;
			}
			break;
		}

		if ((ireason & INTR_FC) && len == 1)
		{
			regv = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
					        cr0_sfifo);
			if (scsi_low_msgin(slp, ti, regv | derror) == 0)
			{
				if (scsi_low_is_msgout_continue(ti, 0) != 0)
				{
					scsi_low_attention(slp);
				}
			}
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, cr0_cmd,
				CMD_MSGOK);
			if ((ncv_io_control & NCV_FAST_INTERRUPTS) != 0)
			{
				/* XXX: 
				 * clear a pending interrupt and sync with
				 * a next interrupt!
				 */
				ncv_catch_intr(sc);
			}
		}
		else
		{
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, cr0_cmd,
				CMD_MSGOK);
		}
		break;
	}

	return 1;
}
