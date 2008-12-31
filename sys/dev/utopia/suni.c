/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/utopia/suni.c,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_atm.h>

#include <dev/utopia/suni.h>
#include <dev/utopia/utopia.h>
#include <dev/utopia/utopia_priv.h>

/*
 * set SONET/SDH mode
 */
static int
suni_set_sdh(struct utopia *utp, int sdh)
{
	int err;

	if (sdh)
		err = UTP_WRITEREG(utp, SUNI_REGO_TPOPAPTR + 1,
		    SUNI_REGM_TPOPAPTR_S,
		    SUNI_REGM_SDH << SUNI_REGS_TPOPAPTR_S);
	else
		err = UTP_WRITEREG(utp, SUNI_REGO_TPOPAPTR + 1,
		    SUNI_REGM_TPOPAPTR_S,
		    SUNI_REGM_SONET << SUNI_REGS_TPOPAPTR_S);
	if (err != 0)
		return (err);

	utp->state &= ~UTP_ST_SDH;
	if (sdh)
		utp->state |= UTP_ST_SDH;

	return (0);
}

/*
 * set idle/unassigned cells
 */
static int
suni_set_unass(struct utopia *utp, int unass)
{
	int err;

	if (unass)
		err = UTP_WRITEREG(utp, SUNI_REGO_TACPIDLEH,
		    0xff, (0 << SUNI_REGS_TACPIDLEH_CLP));
	else
		err = UTP_WRITEREG(utp, SUNI_REGO_TACPIDLEH,
		    0xff, (1 << SUNI_REGS_TACPIDLEH_CLP));
	if (err != 0)
		return (err);

	utp->state &= ~UTP_ST_UNASS;
	if (unass)
		utp->state |= UTP_ST_UNASS;

	return (0);
}

/*
 * enable/disable scrambling
 */
static int
suni_set_noscramb(struct utopia *utp, int noscramb)
{
	int err;

	if (noscramb) {
		err = UTP_WRITEREG(utp, SUNI_REGO_TACPCTRL,
		    SUNI_REGM_TACPCTRL_DSCR, SUNI_REGM_TACPCTRL_DSCR);
		if (err)
			return (err);
		err = UTP_WRITEREG(utp, SUNI_REGO_RACPCTRL,
		    SUNI_REGM_RACPCTRL_DDSCR, SUNI_REGM_RACPCTRL_DDSCR);
		if (err)
			return (err);
		utp->state |= UTP_ST_NOSCRAMB;
	} else {
		err = UTP_WRITEREG(utp, SUNI_REGO_TACPCTRL,
		    SUNI_REGM_TACPCTRL_DSCR, 0);
		if (err)
			return (err);
		err = UTP_WRITEREG(utp, SUNI_REGO_RACPCTRL,
		    SUNI_REGM_RACPCTRL_DDSCR, 0);
		if (err)
			return (err);
		utp->state &= ~UTP_ST_NOSCRAMB;
	}
	return (0);
}

/*
 * Get current carrier state
 */
static int
suni_update_carrier(struct utopia *utp)
{
	int err;
	uint8_t reg;
	u_int n = 1;

	if ((err = UTP_READREGS(utp, SUNI_REGO_RSOPSIS, &reg, &n)) != 0) {
		utp->carrier = UTP_CARR_UNKNOWN;
		return (err);
	}
	utopia_check_carrier(utp, !(reg & SUNI_REGM_RSOPSIS_LOSV));
	return (0);
}

/*
 * Set the SUNI chip to reflect the current state in utopia.
 * Assume, that the chip has been reset.
 */
static int
suni_set_chip(struct utopia *utp)
{
	int err = 0;

	/* set sonet/sdh */
	err |= utopia_set_sdh(utp, utp->state & UTP_ST_SDH);

	/* unassigned or idle cells */
	err |= utopia_set_unass(utp, utp->state & UTP_ST_UNASS);
	err |= UTP_WRITEREG(utp, SUNI_REGO_TACPIDLEP, 0xff, 0x6a);

	/* set scrambling */
	err |= utopia_set_noscramb(utp, utp->state & UTP_ST_NOSCRAMB);

	/* loopback */
	err |= utopia_set_loopback(utp, utp->loopback);

	/* update carrier state */
	err |= utopia_update_carrier(utp);

	/* enable interrupts on LOS */
	err |= UTP_WRITEREG(utp, SUNI_REGO_RSOPCIE,
	    SUNI_REGM_RSOPCIE_LOSE, SUNI_REGM_RSOPCIE_LOSE);

	return (err ? EIO : 0);
}

/*
 * Reset the SUNI chip to reflect the current state of utopia.
 */
static int
suni_reset_default(struct utopia *utp)
{
	int err = 0;

	if (!(utp->flags & UTP_FL_NORESET)) {
		err |= UTP_WRITEREG(utp, SUNI_REGO_MRESET,
		    SUNI_REGM_MRESET_RESET, SUNI_REGM_MRESET_RESET);
		err |= UTP_WRITEREG(utp, SUNI_REGO_MRESET,
		    SUNI_REGM_MRESET_RESET, 0);
	}

	/* disable test mode */
	err |= UTP_WRITEREG(utp, SUNI_REGO_MTEST, 0xff, 0x00);

	err |= suni_set_chip(utp);

	return (err ? EIO : 0);
}

/*
 * Set loopback mode for the Lite
 */
static int
suni_set_loopback_lite(struct utopia *utp, u_int mode)
{
	int err;
	uint32_t val;
	u_int nmode;

	val = 0;
	nmode = mode;
	if (mode & UTP_LOOP_TIME) {
		nmode &= ~UTP_LOOP_TIME;
		val |= SUNI_REGM_MCTRL_LOOPT;
	}
	if (mode & UTP_LOOP_DIAG) {
		nmode &= ~UTP_LOOP_DIAG;
		val |= SUNI_REGM_MCTRL_DLE;
	}
	if (mode & UTP_LOOP_LINE) {
		nmode &= ~UTP_LOOP_LINE;
		if (val & SUNI_REGM_MCTRL_DLE)
			return (EINVAL);
		val |= SUNI_REGM_MCTRL_LLE;
	}
	if (nmode != 0)
		return (EINVAL);

	err = UTP_WRITEREG(utp, SUNI_REGO_MCTRL,
	    SUNI_REGM_MCTRL_LLE | SUNI_REGM_MCTRL_DLE | SUNI_REGM_MCTRL_LOOPT,
	    val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Update statistics from a SUNI/LITE or SUNI/ULTRA
 */
static void
suni_lite_update_stats(struct utopia *utp)
{
	int err;

	/* write to the master if we can */
	if (!(utp->flags & UTP_FL_NORESET)) {
		err = UTP_WRITEREG(utp, SUNI_REGO_MRESET, 0, 0);
	} else {
		err = UTP_WRITEREG(utp, SUNI_REGO_RSOP_BIP8, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RLOPBIP8_24, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RPOPBIP8, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RACPCHCS, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_TACPCNT, 0, 0);

	}
	if (err) {
#ifdef DIAGNOSTIC
		printf("%s: register write error %s: %d\n", __func__,
		    utp->chip->name, err);
#endif
		return;
	}

	DELAY(8);

	utp->stats.rx_sbip += utopia_update(utp,
	    SUNI_REGO_RSOP_BIP8, 2, 0xffff);
	utp->stats.rx_lbip += utopia_update(utp,
	    SUNI_REGO_RLOPBIP8_24, 3, 0xfffff);
	utp->stats.rx_lfebe += utopia_update(utp,
	    SUNI_REGO_RLOPFEBE, 3, 0xfffff);
	utp->stats.rx_pbip += utopia_update(utp,
	    SUNI_REGO_RPOPBIP8, 2, 0xffff);
	utp->stats.rx_pfebe += utopia_update(utp,
	    SUNI_REGO_RPOPFEBE, 2, 0xffff);
	utp->stats.rx_corr += utopia_update(utp,
	    SUNI_REGO_RACPCHCS, 1, 0xff);
	utp->stats.rx_uncorr += utopia_update(utp,
	    SUNI_REGO_RACPUHCS, 1, 0xff);
	utp->stats.rx_cells += utopia_update(utp,
	    SUNI_REGO_RACPCNT, 3, 0x7ffff);
	utp->stats.tx_cells += utopia_update(utp,
	    SUNI_REGO_TACPCNT, 3, 0x7ffff);
}

/*
 * Handle interrupt on SUNI chip
 */
static void
suni_intr_default(struct utopia *utp)
{
	uint8_t regs[SUNI_REGO_MTEST];
	u_int n = SUNI_REGO_MTEST;
	int err;

	/* Read all registers. This acks the interrupts */
	if ((err = UTP_READREGS(utp, SUNI_REGO_MRESET, regs, &n)) != 0) {
		printf("SUNI read error %d\n", err);
		return;
	}
	if (n <= SUNI_REGO_RSOPSIS) {
		printf("%s: could not read RSOPSIS", __func__);
		return;
	}
	/* check for LOSI (loss of signal) */
	if ((regs[SUNI_REGO_MISTATUS] & SUNI_REGM_MISTATUS_RSOPI) &&
	    (regs[SUNI_REGO_RSOPSIS] & SUNI_REGM_RSOPSIS_LOSI))
		utopia_check_carrier(utp, !(regs[SUNI_REGO_RSOPSIS]
		    & SUNI_REGM_RSOPSIS_LOSV));
}

const struct utopia_chip utopia_chip_lite = {
	UTP_TYPE_SUNI_LITE,
	"Suni/Lite (PMC-5346)",
	256,
	suni_reset_default,
	suni_set_sdh,
	suni_set_unass,
	suni_set_noscramb,
	suni_update_carrier,
	suni_set_loopback_lite,
	suni_intr_default,
	suni_lite_update_stats,
};

/*
 * Set loopback mode for the Ultra
 */
static int
suni_set_loopback_ultra(struct utopia *utp, u_int mode)
{
	int err;
	uint32_t val;
	u_int nmode;

	val = 0;
	nmode = mode;
	if (mode & UTP_LOOP_TIME) {
		nmode &= ~UTP_LOOP_TIME;
		val |= SUNI_REGM_MCTRL_LOOPT;
	}
	if (mode & UTP_LOOP_DIAG) {
		nmode &= ~UTP_LOOP_DIAG;
		if (val & SUNI_REGM_MCTRL_LOOPT)
			return (EINVAL);
		val |= SUNI_REGM_MCTRL_SDLE;
	}
	if (mode & UTP_LOOP_LINE) {
		nmode &= ~UTP_LOOP_LINE;
		if (val & (SUNI_REGM_MCTRL_LOOPT | SUNI_REGM_MCTRL_SDLE))
			return (EINVAL);
		val |= SUNI_REGM_MCTRL_LLE;
	}
	if (mode & UTP_LOOP_PARAL) {
		nmode &= ~UTP_LOOP_PARAL;
		val |= SUNI_REGM_MCTRL_PDLE;
	}
	if (mode & UTP_LOOP_TWIST) {
		nmode &= ~UTP_LOOP_TWIST;
		val |= SUNI_REGM_MCTRL_TPLE;
	}
	if (nmode != 0)
		return (EINVAL);

	err = UTP_WRITEREG(utp, SUNI_REGO_MCTRL,
	    SUNI_REGM_MCTRL_LLE | SUNI_REGM_MCTRL_SDLE | SUNI_REGM_MCTRL_LOOPT |
	    SUNI_REGM_MCTRL_PDLE | SUNI_REGM_MCTRL_TPLE, val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

const struct utopia_chip utopia_chip_ultra = {
	UTP_TYPE_SUNI_ULTRA,
	"Suni/Ultra (PMC-5350)",
	256,
	suni_reset_default,
	suni_set_sdh,
	suni_set_unass,
	suni_set_noscramb,
	suni_update_carrier,
	suni_set_loopback_ultra,
	suni_intr_default,
	suni_lite_update_stats,
};

/*
 * Set loopback mode for the 622
 */
static int
suni_set_loopback_622(struct utopia *utp, u_int mode)
{
	int err;
	uint32_t val;
	uint8_t config;
	int smode;
	u_int nmode;
	u_int n = 1;

	val = 0;
	nmode = mode;
	if (mode & UTP_LOOP_PATH) {
		nmode &= ~UTP_LOOP_PATH;
		val |= SUNI_REGM_MCTRLM_DPLE;
	}

	err = UTP_READREGS(utp, SUNI_REGO_MCONFIG, &config, &n);
	if (err != 0)
		return (err);
	smode = ((config & SUNI_REGM_MCONFIG_TMODE_622) ==
	    SUNI_REGM_MCONFIG_TMODE_STS1_BIT &&
	    (config & SUNI_REGM_MCONFIG_RMODE_622) ==
	    SUNI_REGM_MCONFIG_RMODE_STS1_BIT);

	if (mode & UTP_LOOP_TIME) {
		if (!smode)
			return (EINVAL);
		nmode &= ~UTP_LOOP_TIME;
		val |= SUNI_REGM_MCTRLM_LOOPT;
	}
	if (mode & UTP_LOOP_DIAG) {
		nmode &= ~UTP_LOOP_DIAG;
		if (val & SUNI_REGM_MCTRLM_LOOPT)
			return (EINVAL);
		val |= SUNI_REGM_MCTRLM_DLE;
	}
	if (mode & UTP_LOOP_LINE) {
		nmode &= ~UTP_LOOP_LINE;
		if (val & (SUNI_REGM_MCTRLM_LOOPT | SUNI_REGM_MCTRLM_DLE))
			return (EINVAL);
		val |= SUNI_REGM_MCTRLM_LLE;
	}
	if (nmode != 0)
		return (EINVAL);

	err = UTP_WRITEREG(utp, SUNI_REGO_MCTRLM,
	    SUNI_REGM_MCTRLM_LLE | SUNI_REGM_MCTRLM_DLE |
	    SUNI_REGM_MCTRLM_DPLE | SUNI_REGM_MCTRL_LOOPT, val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Reset the SUNI chip to reflect the current state of utopia.
 */
static int
suni_reset_622(struct utopia *utp)
{
	int err = 0;

	if (!(utp->flags & UTP_FL_NORESET)) {
		err |= UTP_WRITEREG(utp, SUNI_REGO_MRESET,
		    SUNI_REGM_MRESET_RESET, SUNI_REGM_MRESET_RESET);
		err |= UTP_WRITEREG(utp, SUNI_REGO_MRESET,
		    SUNI_REGM_MRESET_RESET, 0);
	}

	/* disable test mode */
	err |= UTP_WRITEREG(utp, SUNI_REGO_MTEST, 0xff,
	    SUNI_REGM_MTEST_DS27_53_622);

	err |= suni_set_chip(utp);

	return (err ? EIO : 0);
}

/*
 * Update statistics from a SUNI/622
 */
static void
suni_622_update_stats(struct utopia *utp)
{
	int err;

	/* write to the master if we can */
	if (!(utp->flags & UTP_FL_NORESET)) {
		err = UTP_WRITEREG(utp, SUNI_REGO_MRESET, 0, 0);
	} else {
		err = UTP_WRITEREG(utp, SUNI_REGO_RSOP_BIP8, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RLOPBIP8_24, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RPOPBIP8, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_RACPCHCS, 0, 0);
		err |= UTP_WRITEREG(utp, SUNI_REGO_TACPCNT, 0, 0);
	}
	if (err) {
#ifdef DIAGNOSTIC
		printf("%s: register write error %s: %d\n", __func__,
		    utp->chip->name, err);
#endif
		return;
	}

	DELAY(8);

	utp->stats.rx_sbip += utopia_update(utp,
	    SUNI_REGO_RSOP_BIP8, 2, 0xffff);
	utp->stats.rx_lbip += utopia_update(utp,
	    SUNI_REGO_RLOPBIP8_24, 3, 0xfffff);
	utp->stats.rx_lfebe += utopia_update(utp,
	    SUNI_REGO_RLOPFEBE, 3, 0xfffff);
	utp->stats.rx_pbip += utopia_update(utp,
	    SUNI_REGO_RPOPBIP8, 2, 0xffff);
	utp->stats.rx_pfebe += utopia_update(utp,
	    SUNI_REGO_RPOPFEBE, 2, 0xffff);
	utp->stats.rx_corr += utopia_update(utp,
	    SUNI_REGO_RACPCHCS_622, 2, 0xfff);
	utp->stats.rx_uncorr += utopia_update(utp,
	    SUNI_REGO_RACPUHCS_622, 2, 0xfff);
	utp->stats.rx_cells += utopia_update(utp,
	    SUNI_REGO_RACPCNT_622, 3, 0x1fffff);
	utp->stats.tx_cells += utopia_update(utp,
	    SUNI_REGO_TACPCNT, 3, 0x1fffff);
}

const struct utopia_chip utopia_chip_622 = {
	UTP_TYPE_SUNI_622,
	"Suni/622 (PMC-5355)",
	256,
	suni_reset_622,
	suni_set_sdh,
	suni_set_unass,
	suni_set_noscramb,
	suni_update_carrier,
	suni_set_loopback_622,
	suni_intr_default,
	suni_622_update_stats,
};
