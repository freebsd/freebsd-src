/*
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * Copyright (c) LAN Media Corporation 1998, 1999.
 * Copyright (c) 2000 Stephen Kiernan (sk-ports@vegamuse.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_lmc_media.c,v 1.23 1999/03/01 15:12:24 explorer Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * For lack of a better place, put the SSI cable stuff here.
 */
char *lmc_ssi_cables[] = {
	"V.10/RS423", "EIA530A", "reserved", "X.21", "V.35",
	"EIA449/EIA530/V.36", "V.28/EIA232", "none", NULL
};

/*
 * protocol independent method.
 */
static void	lmc_set_protocol(lmc_softc_t * const, lmc_ctl_t *);

/*
 * media independent methods to check on media status, link, light LEDs,
 * etc.
 */
static void	lmc_ds3_init(lmc_softc_t * const);
static void	lmc_ds3_default(lmc_softc_t * const);
static void	lmc_ds3_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_ds3_set_100ft(lmc_softc_t * const, int);
static int	lmc_ds3_get_link_status(lmc_softc_t * const);
static void	lmc_ds3_set_crc_length(lmc_softc_t * const, int);
static void	lmc_ds3_set_scram(lmc_softc_t * const, int);

static void	lmc_hssi_init(lmc_softc_t * const);
static void	lmc_hssi_default(lmc_softc_t * const);
static void	lmc_hssi_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_hssi_set_clock(lmc_softc_t * const, int);
static int	lmc_hssi_get_link_status(lmc_softc_t * const);
static void	lmc_hssi_set_link_status(lmc_softc_t * const, int);
static void	lmc_hssi_set_crc_length(lmc_softc_t * const, int);

static void	lmc_ssi_init(lmc_softc_t * const);
static void	lmc_ssi_default(lmc_softc_t * const);
static void	lmc_ssi_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_ssi_set_clock(lmc_softc_t * const, int);
static void	lmc_ssi_set_speed(lmc_softc_t * const, lmc_ctl_t *);
static int	lmc_ssi_get_link_status(lmc_softc_t * const);
static void	lmc_ssi_set_link_status(lmc_softc_t * const, int);
static void	lmc_ssi_set_crc_length(lmc_softc_t * const, int);

static void     lmc_t1_init(lmc_softc_t * const);
static void     lmc_t1_default(lmc_softc_t * const);
static void     lmc_t1_set_status(lmc_softc_t * const, lmc_ctl_t *);
static int      lmc_t1_get_link_status(lmc_softc_t * const);
static void     lmc_t1_set_circuit_type(lmc_softc_t * const, int);
static void     lmc_t1_set_crc_length(lmc_softc_t * const, int);

static void	lmc_dummy_set_1(lmc_softc_t * const, int);
static void	lmc_dummy_set2_1(lmc_softc_t * const, lmc_ctl_t *);

static void write_av9110_bit(lmc_softc_t *, int);
static void	write_av9110(lmc_softc_t *, u_int32_t, u_int32_t, u_int32_t,
			     u_int32_t, u_int32_t);

lmc_media_t lmc_ds3_media = {
	lmc_ds3_init,			/* special media init stuff */
	lmc_ds3_default,		/* reset to default state */
	lmc_ds3_set_status,		/* reset status to state provided */
	lmc_dummy_set_1,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_ds3_set_100ft,		/* set cable length */
	lmc_ds3_set_scram,		/* set scrambler */
	lmc_ds3_get_link_status,	/* get link status */
	lmc_dummy_set_1,		/* set link status */
	lmc_ds3_set_crc_length,		/* set CRC length */
	lmc_dummy_set_1
};

lmc_media_t lmc_hssi_media = {
	lmc_hssi_init,			/* special media init stuff */
	lmc_hssi_default,		/* reset to default state */
	lmc_hssi_set_status,		/* reset status to state provided */
	lmc_hssi_set_clock,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_hssi_get_link_status,	/* get link status */
	lmc_hssi_set_link_status,	/* set link status */
	lmc_hssi_set_crc_length,	/* set CRC length */
	lmc_dummy_set_1
};

lmc_media_t lmc_ssi_media = {
	lmc_ssi_init,			/* special media init stuff */
	lmc_ssi_default,			/* reset to default state */
	lmc_ssi_set_status,		/* reset status to state provided */
	lmc_ssi_set_clock,		/* set clock source */
	lmc_ssi_set_speed,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_ssi_get_link_status,		/* get link status */
	lmc_ssi_set_link_status,		/* set link status */
	lmc_ssi_set_crc_length,		/* set CRC length */
	lmc_dummy_set_1
};

lmc_media_t lmc_t1_media = {
        lmc_t1_init,                   /* special media init stuff */
        lmc_t1_default,                        /* reset to default state */
        lmc_t1_set_status,             /* reset status to state provided */
        lmc_dummy_set_1,                /* set clock source */
        lmc_dummy_set2_1,                       /* set line speed */
        lmc_dummy_set_1,                /* set cable length */
        lmc_dummy_set_1,                /* set scrambler */
        lmc_t1_get_link_status,                /* get link status */
        lmc_dummy_set_1,                /* set link status */
        lmc_t1_set_crc_length,         /* set CRC length */
        lmc_t1_set_circuit_type /* set T1 or E1 circuit type */
};


static void
lmc_dummy_set_1(lmc_softc_t * const sc, int a)
{
}

static void
lmc_dummy_set2_1(lmc_softc_t * const sc, lmc_ctl_t *a)
{
}

/*
 *  HSSI methods
 */

static void
lmc_hssi_init(lmc_softc_t * const sc)
{
	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5200;

	lmc_gpio_mkoutput(sc, LMC_GEP_HSSI_CLOCK);
}

static void
lmc_hssi_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_hssi_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source && !sc->ictl.clock_source)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
	else if (!ctl->clock_source && sc->ictl.clock_source)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_hssi_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio |= LMC_GEP_HSSI_CLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio &= ~(LMC_GEP_HSSI_CLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_hssi_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_HSSI_CA) == LMC_MII16_HSSI_CA)
		return 1;
	else
		return 0;
}

static void
lmc_hssi_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state)
		sc->lmc_miireg16 |= LMC_MII16_HSSI_TA;
	else
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_TA;

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_hssi_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}


/*
 *  DS3 methods
 */

/*
 * Set cable length
 */
static void
lmc_ds3_set_100ft(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CABLE_LENGTH_GT_100FT) {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_GT_100FT;
	} else if (ie == LMC_CTL_CABLE_LENGTH_LT_100FT) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_LT_100FT;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_ds3_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_cable_length(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);
	sc->lmc_media->set_scrambler(sc, LMC_CTL_OFF);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_ds3_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_cable_length(sc, sc->ictl.cable_length);
		sc->lmc_media->set_scrambler(sc, sc->ictl.scrambler_onoff);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in cable length setting
	 */
	if (ctl->cable_length && !sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_GT_100FT);
	else if (!ctl->cable_length && sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);

	/*
	 * Check for change in scrambler setting (requires reset)
	 */
	if (ctl->scrambler_onoff && !sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_ON);
	else if (!ctl->scrambler_onoff && sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_OFF);

	lmc_set_protocol(sc, ctl);
}

static void
lmc_ds3_init(lmc_softc_t * const sc)
{
	int i;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5245;

	/* writes zeros everywhere */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_writereg(sc, 0, 18, 0);
	}

	/* set some essential bits */
	lmc_mii_writereg(sc, 0, 17, 1);
	lmc_mii_writereg(sc, 0, 18, 0x05);	/* ser, xtx */

	lmc_mii_writereg(sc, 0, 17, 5);
	lmc_mii_writereg(sc, 0, 18, 0x80);	/* emode */

	lmc_mii_writereg(sc, 0, 17, 14);
	lmc_mii_writereg(sc, 0, 18, 0x30);	/* rcgen, tcgen */

	/* clear counters and latched bits */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_readreg(sc, 0, 18);
	}
}

/*
 * 1 == DS3 payload scrambled, 0 == not scrambled
 */
static void
lmc_ds3_set_scram(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_ON) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_ON;
	} else {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_OFF;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_ds3_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	lmc_mii_writereg(sc, 0, 17, 7);
	link_status = lmc_mii_readreg(sc, 0, 18);

	if ((link_status & LMC_FRAMER_REG0_DLOS) == 0)
		return 1;
	else
		return 0;
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_ds3_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}


/*
 *  SSI methods
 */

static void
lmc_ssi_init(lmc_softc_t * const sc)
{
	u_int16_t mii17;
	int cable;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1000;

	mii17 = lmc_mii_readreg(sc, 0, 17);

	cable = (mii17 & LMC_MII17_SSI_CABLE_MASK) >> LMC_MII17_SSI_CABLE_SHIFT;
	sc->ictl.cable_type = cable;

	lmc_gpio_mkoutput(sc, LMC_GEP_SSI_TXCLOCK);
}

static void
lmc_ssi_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	/*
	 * make TXCLOCK always be an output
	 */
	lmc_gpio_mkoutput(sc, LMC_GEP_SSI_TXCLOCK);

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_speed(sc, NULL);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_ssi_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		sc->lmc_media->set_speed(sc, &sc->ictl);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_INT
	    && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_EXT)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
	else if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_EXT
		 && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_INT)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);

	if (ctl->clock_rate != sc->ictl.clock_rate)
		sc->lmc_media->set_speed(sc, ctl);

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_ssi_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio &= ~(LMC_GEP_SSI_TXCLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio |= LMC_GEP_SSI_TXCLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

static void
lmc_ssi_set_speed(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	lmc_ctl_t *ictl = &sc->ictl;
	lmc_av9110_t *av;

	if (ctl == NULL) {
		av = &ictl->cardspec.ssi;
		ictl->clock_rate = 100000;
		av->f = ictl->clock_rate;
		av->n = 8;
		av->m = 25;
		av->v = 0;
		av->x = 0;
		av->r = 2;

		write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
		return;
	}

	av = &ctl->cardspec.ssi;

	if (av->f == 0)
		return;

	ictl->clock_rate = av->f;  /* really, this is the rate we are */
	ictl->cardspec.ssi = *av;

	write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_ssi_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	/*
	 * missing CTS?  Hmm.  If we require CTS on, we may never get the
	 * link to come up, so omit it in this test.
	 *
	 * Also, it seems that with a loopback cable, DCD isn't asserted,
	 * so just check for things like this:
	 *	DSR _must_ be asserted.
	 *	One of DCD or CTS must be asserted.
	 */
	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_SSI_DSR) == 0)
		return (0);

	if ((link_status & (LMC_MII16_SSI_CTS | LMC_MII16_SSI_DCD)) == 0)
		return (0);

	return (1);
}

static void
lmc_ssi_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state) {
		sc->lmc_miireg16 |= (LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
		printf(LMC_PRINTF_FMT ": asserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_miireg16 &= ~(LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
		printf(LMC_PRINTF_FMT ": deasserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_ssi_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_SSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_SSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * These are bits to program the SSI frequency generator
 */
static void
write_av9110_bit(lmc_softc_t *sc, int c)
{
	/*
	 * set the data bit as we need it.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	if (c & 0x01)
		sc->lmc_gpio |= LMC_GEP_SERIAL;
	else
		sc->lmc_gpio &= ~(LMC_GEP_SERIAL);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to high
	 */
	sc->lmc_gpio |= LMC_GEP_SERIALCLK;
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to low again.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
}

static void
write_av9110(lmc_softc_t *sc, u_int32_t n, u_int32_t m, u_int32_t v,
	     u_int32_t x, u_int32_t r)
{
	int i;

#if 0
	printf(LMC_PRINTF_FMT ": speed %u, %d %d %d %d %d\n",
	       LMC_PRINTF_ARGS, sc->ictl.clock_rate,
	       n, m, v, x, r);
#endif

	sc->lmc_gpio |= LMC_GEP_SSI_GENERATOR;
	sc->lmc_gpio &= ~(LMC_GEP_SERIAL | LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * Set the TXCLOCK, GENERATOR, SERIAL, and SERIALCLK
	 * as outputs.
	 */
	lmc_gpio_mkoutput(sc, (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			       | LMC_GEP_SSI_GENERATOR));

	sc->lmc_gpio &= ~(LMC_GEP_SSI_GENERATOR);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * a shifting we will go...
	 */
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, n >> i);
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, m >> i);
	for (i = 0 ; i < 1 ; i++)
		write_av9110_bit(sc, v >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, x >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, r >> i);
	for (i = 0 ; i < 5 ; i++)
		write_av9110_bit(sc, 0x17 >> i);

	/*
	 * stop driving serial-related signals
	 */
	lmc_gpio_mkinput(sc,
			 (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			  | LMC_GEP_SSI_GENERATOR));
}

static void
lmc_set_protocol(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == 0) {
		sc->ictl.keepalive_onoff = LMC_CTL_ON;

		return;
	}
}

/*
 *  T1 methods
 */

/*
 * The framer regs are multiplexed through MII regs 17 & 18
 *  write the register address to MII reg 17 and the *  data to MII reg 18. */

static void lmc_t1_write(lmc_softc_t * const sc, int a, int d)
{
       lmc_mii_writereg(sc, 0, 17, a);
       lmc_mii_writereg(sc, 0, 18, d);
}

#if 0
/* XXX future to be integtrated with if_lmc.c for alarms */

static int lmc_t1_read(lmc_softc_t * const sc, int a)
{
       lmc_mii_writereg(sc, 0, 17, a);
       return lmc_mii_readreg(sc, 0, 18);
}
#endif

static void
   lmc_t1_init(lmc_softc_t * const sc)
{
        u_int16_t mii16;
        int     i;

        sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1200;
        mii16 = lmc_mii_readreg(sc, 0, 16);

        /* reset 8370 */
        mii16 &= ~LMC_MII16_T1_RST;
        lmc_mii_writereg(sc, 0, 16, mii16 | LMC_MII16_T1_RST);
        lmc_mii_writereg(sc, 0, 16, mii16);

        /* set T1 or E1 line impedance */
        /* mii16 &= ~LMC_MII16_T1_Z; */
        mii16 |= LMC_MII16_T1_Z;
        lmc_mii_writereg(sc, 0, 16, mii16);

        lmc_t1_write(sc, 0x01, 0x1B);  /* CR0     - primary control          */
        lmc_t1_write(sc, 0x02, 0x42);  /* JAT_CR  - jitter atten config      */
        lmc_t1_write(sc, 0x14, 0x00);  /* LOOP    - loopback config          */
        lmc_t1_write(sc, 0x15, 0x00);  /* DL3_TS  - xtrnl datalink timeslot  */
        lmc_t1_write(sc, 0x18, 0xFF);  /* PIO     - programmable I/O         */
        lmc_t1_write(sc, 0x19, 0x30);  /* POE     - programmable OE          */
        lmc_t1_write(sc, 0x1A, 0x0F);  /* CMUX    - clock input mux          */
        lmc_t1_write(sc, 0x20, 0x41);  /* LIU_CR  - RX LIU config            */
        lmc_t1_write(sc, 0x22, 0x76);  /* RLIU_CR - RX LIU config            */
        lmc_t1_write(sc, 0x40, 0x03);  /* RCR0    - RX config                */
        lmc_t1_write(sc, 0x45, 0x00);  /* RALM    - RX alarm config          */
        lmc_t1_write(sc, 0x46, 0x05);  /* LATCH   - RX alarm/err/cntr latch  */
        lmc_t1_write(sc, 0x68, 0x40);  /* TLIU_CR - TX LIU config            */
        lmc_t1_write(sc, 0x70, 0x0D);  /* TCR0    - TX framer config         */
        lmc_t1_write(sc, 0x71, 0x05);  /* TCR1    - TX config                */
        lmc_t1_write(sc, 0x72, 0x0B);  /* TFRM    - TX frame format          */
        lmc_t1_write(sc, 0x73, 0x00);  /* TERROR  - TX error insert          */
        lmc_t1_write(sc, 0x74, 0x00);  /* TMAN    - TX manual Sa/FEBE config */
        lmc_t1_write(sc, 0x75, 0x00);  /* TALM    - TX alarm signal config   */
        lmc_t1_write(sc, 0x76, 0x00);  /* TPATT   - TX test pattern config   */
        lmc_t1_write(sc, 0x77, 0x00);  /* TLB     - TX inband loopback confg */
        lmc_t1_write(sc, 0x90, 0x05);  /* CLAD_CR - clock rate adapter confg */
        lmc_t1_write(sc, 0x91, 0x05);  /* CSEL    - clad freq sel            */
        lmc_t1_write(sc, 0xA6, 0x00);  /* DL1_CTL - DL1 control              */
        lmc_t1_write(sc, 0xB1, 0x00);  /* DL2_CTL - DL2 control              */
        lmc_t1_write(sc, 0xD0, 0x47);  /* SBI_CR  - sys bus iface config     */
        lmc_t1_write(sc, 0xD1, 0x70);  /* RSB_CR  - RX sys bus config        */
        lmc_t1_write(sc, 0xD4, 0x30);  /* TSB_CR  - TX sys bus config        */
        for (i=0; i<32; i++)
        {
                lmc_t1_write(sc, 0x0E0+i, 0x00); /*SBCn sysbus perchannel ctl */
                lmc_t1_write(sc, 0x100+i, 0x00); /* TPCn - TX per-channel ctl */
                lmc_t1_write(sc, 0x180+i, 0x00); /* RPCn - RX per-channel ctl */
        }
        for (i=1; i<25; i++)
	{                lmc_t1_write(sc, 0x0E0+i, 0x0D);
				/* SBCn - sys bus per-channel ctl    */
	}
	/* Turn on the transmiter */
        mii16 |= LMC_MII16_T1_XOE;
        lmc_mii_writereg(sc, 0, 16, mii16);
        sc->lmc_miireg16 = mii16;  
}

static void   lmc_t1_default(lmc_softc_t * const sc)
{
        sc->lmc_miireg16 = LMC_MII16_LED_ALL;
        sc->lmc_media->set_link_status(sc, 0);
        sc->lmc_media->set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_T1);
        sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */

static void
lmc_t1_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl){
	if (ctl == NULL) {
		sc->lmc_media->set_circuit_type(sc, sc->ictl.circuit_type);
		lmc_set_protocol(sc, NULL);

		return;
	}

        /*
         * check for change in circuit type
	 */

	if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_T1
		&& sc->ictl.circuit_type == LMC_CTL_CIRCUIT_TYPE_E1)
		sc->lmc_media->set_circuit_type(sc,LMC_CTL_CIRCUIT_TYPE_E1 );
	else if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_E1
		&& sc->ictl.circuit_type == LMC_CTL_CIRCUIT_TYPE_T1)
		sc->lmc_media->set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_T1);
	lmc_set_protocol(sc, ctl);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */

static int
lmc_t1_get_link_status(lmc_softc_t * const sc){
	u_int16_t link_status;
	lmc_mii_writereg(sc, 0, 17, T1FRAMER_ALARM1_STATUS );
	link_status = lmc_mii_readreg(sc, 0, 18) & 0x00FF; /* Make sure it's 8 bits only */

        /*
	 * LMC 1200 LED definitions
         * led0 yellow = far-end adapter is in Red alarm condition
  	 * led1 blue = received an Alarm Indication signal (upstream failure)
         * led2 Green = power to adapter, Gate Array loaded & driver attached
         * led3 red = Loss of Signal (LOS) or out of frame (OOF) conditions
	 * detected on T3 receive signal
         */

        /* detect a change in Blue alarm indication signal */
       if( link_status & T1F_RAIS )
       {                        /* turn on blue LED */
                 lmc_led_on(sc, LMC_DS3_LED1);
       }
       else
       {                        /* turn off blue LED */
		lmc_led_off(sc, LMC_DS3_LED1);
       }       
	
        if( (sc->t1_alarm1_status & T1F_RAIS) != (link_status & T1F_RAIS) )
        {
                if( link_status & T1F_RAIS )
                {                        /* turn on blue LED */
                        printf(" link status: RAIS turn ON Blue %x\n", link_status ); /* DEBUG */
                        lmc_led_on(sc, LMC_DS3_LED1);
                }
                else
                {                        /* turn off blue LED */
                        printf(" link status: RAIS turn OFF Blue %x\n", link_status ); /* DEBUG */
			lmc_led_off(sc, LMC_DS3_LED1);
               }       
	}
        /*
	 * T1F_RYEL wiggles quite a bit,
	 *  taking it out until I understand why -baz 6/22/99
         */
         if(link_status & T1F_RMYEL)
         {
		/* turn on yellow LED */
                lmc_led_on(sc, LMC_DS3_LED0);
         }
         else
         {
                lmc_led_off(sc, LMC_DS3_LED0);
         }
               /* Yellow alarm indication */
                if( (sc->t1_alarm1_status &  T1F_RMYEL) !=
                        (link_status & T1F_RMYEL) )
                {
		if((link_status & T1F_RMYEL) == 0 )
                        {
       		                printf(" link status: RYEL turn OFF Yellow %x\n", link_status ); /* DEBUG */
                        }
                        else
                        {
                                printf(" link status: RYEL turn ON Yellow %x\n", link_status ); /* DEBUG */
                        }
                }

        if (link_status & (T1F_RLOF | T1F_RLOS))
	{
		lmc_led_on(sc, LMC_DS3_LED3);
	}
	else
	{
		lmc_led_off(sc, LMC_DS3_LED3);
	}
        sc->t1_alarm1_status = link_status;

        lmc_mii_writereg(sc, 0, 17, T1FRAMER_ALARM2_STATUS );
        sc->t1_alarm2_status = lmc_mii_readreg(sc, 0, 18);

        /* link status based upon T1 receive loss of frame or
         * loss of signal - RED alarm indication */
        if ((link_status & (T1F_RLOF | T1F_RLOS)) == 0)
                return 1;
        else
                return 0;
}

/*
 * 1 == T1 Circuit Type , 0 == E1 Circuit Type
 */
static void
   lmc_t1_set_circuit_type(lmc_softc_t * const sc, int ie)
{
        if (ie == LMC_CTL_CIRCUIT_TYPE_T1)
        {
                sc->lmc_miireg16 |= LMC_MII16_T1_Z;
                sc->ictl.circuit_type = LMC_CTL_CIRCUIT_TYPE_T1;
        } else {                sc->lmc_miireg16 &= ~LMC_MII16_T1_Z;
                sc->ictl.scrambler_onoff = LMC_CTL_CIRCUIT_TYPE_E1;
        }
        lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit */
static void
   lmc_t1_set_crc_length(lmc_softc_t * const sc, int state)
{
        if (state == LMC_CTL_CRC_LENGTH_32) {
                /* 32 bit */
                sc->lmc_miireg16 |= LMC_MII16_T1_CRC;
                sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
                sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_4;

        } else {
                /* 16 bit */                sc->lmc_miireg16 &= ~LMC_MII16_T1_CRC;
                sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
                sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_2;

        }

        lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}
