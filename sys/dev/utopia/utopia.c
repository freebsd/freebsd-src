/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <dev/utopia/idtphy.h>
#include <dev/utopia/utopia.h>

#define READREGS(UTOPIA, REG, VALP, NP)				\
    (UTOPIA)->methods->readregs((UTOPIA)->ifatm, REG, VALP, NP)
#define WRITEREG(UTOPIA, REG, MASK, VAL)			\
    (UTOPIA)->methods->writereg((UTOPIA)->ifatm, REG, MASK, VAL)

/*
 * Global list of all registered interfaces
 */
static struct mtx utopia_list_mtx;
static LIST_HEAD(, utopia) utopia_list = LIST_HEAD_INITIALIZER(utopia_list);

#define UTP_RLOCK_LIST()	mtx_lock(&utopia_list_mtx)
#define UTP_RUNLOCK_LIST()	mtx_unlock(&utopia_list_mtx)
#define UTP_WLOCK_LIST()	mtx_lock(&utopia_list_mtx)
#define UTP_WUNLOCK_LIST()	mtx_unlock(&utopia_list_mtx)

#define UTP_LOCK(UTP)		mtx_lock((UTP)->lock)
#define UTP_UNLOCK(UTP)		mtx_unlock((UTP)->lock)
#define UTP_LOCK_ASSERT(UTP)	mtx_assert((UTP)->lock, MA_OWNED)

static struct proc *utopia_kproc;

static void utopia_dump(struct utopia *) __unused;

/*
 * Statistics update inlines
 */
static uint32_t
utp_update(struct utopia *utp, u_int reg, u_int nreg, uint32_t mask)
{
	int err;
	u_int n;
	uint8_t regs[4];
	uint32_t val;

	n = nreg;
	if ((err = READREGS(utp, reg, regs, &n)) != 0) {
#ifdef DIAGNOSTIC
		printf("%s: register read error %s(%u,%u): %d\n", __func__,
		    utp->chip->name, reg, nreg, err);
#endif
		return (0);
	}
	if (n < nreg) {
#ifdef DIAGNOSTIC
		printf("%s: got only %u regs %s(%u,%u): %d\n", __func__, n,
		    utp->chip->name, reg, nreg, err);
#endif
		return (0);
	}
	val = 0;
	for (n = nreg; n > 0; n--) {
		val <<= 8;
		val |= regs[n - 1];
	}
	return (val & mask);
}

#define	UPDATE8(UTP, REG)	utp_update(UTP, REG, 1, 0xff)
#define	UPDATE12(UTP, REG)	utp_update(UTP, REG, 2, 0xfff)
#define	UPDATE16(UTP, REG)	utp_update(UTP, REG, 2, 0xffff)
#define	UPDATE19(UTP, REG)	utp_update(UTP, REG, 3, 0x7ffff)
#define	UPDATE20(UTP, REG)	utp_update(UTP, REG, 3, 0xfffff)
#define	UPDATE21(UTP, REG)	utp_update(UTP, REG, 3, 0x1fffff)

/*
 * Debugging - dump all registers.
 */
static void
utopia_dump(struct utopia *utp)
{
	uint8_t regs[256];
	u_int n = 256, i;
	int err;

	if ((err = READREGS(utp, SUNI_REGO_MRESET, regs, &n)) != 0) {
		printf("SUNI read error %d\n", err);
		return;
	}
	for (i = 0; i < n; i++) {
		if (i % 16 == 0)
			printf("%02x:", i);
		if (i % 16 == 8)
			printf(" ");
		printf(" %02x", regs[i]);
		if (i % 16 == 15)
			printf("\n");
	}
	if (i % 16 != 0)
		printf("\n");
}

/*
 * Update the carrier status
 */
static void
utopia_check_carrier(struct utopia *utp, u_int carr_ok)
{
	int old;

	old = utp->carrier;
	if (carr_ok) {
		/* carrier */
		utp->carrier = UTP_CARR_OK;
		if (old != UTP_CARR_OK) {
			if_printf(&utp->ifatm->ifnet, "carrier detected\n");
			ATMEV_SEND_IFSTATE_CHANGED(utp->ifatm, 1);
		}
	} else {
		/* no carrier */
		utp->carrier = UTP_CARR_LOST;
		if (old == UTP_CARR_OK) {
			if_printf(&utp->ifatm->ifnet, "carrier lost\n");
			ATMEV_SEND_IFSTATE_CHANGED(utp->ifatm, 0);
		}
	}
}

static int
utopia_update_carrier_default(struct utopia *utp)
{
	int err;
	uint8_t reg;
	u_int n = 1;

	if ((err = READREGS(utp, SUNI_REGO_RSOPSIS, &reg, &n)) != 0) {
		utp->carrier = UTP_CARR_UNKNOWN;
		return (err);
	}
	utopia_check_carrier(utp, !(reg & SUNI_REGM_RSOPSIS_LOSV));
	return (0);
}

/*
 * enable/disable scrambling
 */
static int
utopia_set_noscramb_default(struct utopia *utp, int noscramb)
{
	int err;

	if (noscramb) {
		err = WRITEREG(utp, SUNI_REGO_TACPCTRL,
		    SUNI_REGM_TACPCTRL_DSCR, SUNI_REGM_TACPCTRL_DSCR);
		if (err)
			return (err);
		err = WRITEREG(utp, SUNI_REGO_RACPCTRL,
		    SUNI_REGM_RACPCTRL_DDSCR, SUNI_REGM_RACPCTRL_DDSCR);
		if (err)
			return (err);
		utp->state |= UTP_ST_NOSCRAMB;
	} else {
		err = WRITEREG(utp, SUNI_REGO_TACPCTRL,
		    SUNI_REGM_TACPCTRL_DSCR, 0);
		if (err)
			return (err);
		err = WRITEREG(utp, SUNI_REGO_RACPCTRL,
		    SUNI_REGM_RACPCTRL_DDSCR, 0);
		if (err)
			return (err);
		utp->state &= ~UTP_ST_NOSCRAMB;
	}
	return (0);
}

/*
 * set SONET/SDH mode
 */
static int
utopia_set_sdh_default(struct utopia *utp, int sdh)
{
	int err;

	if (sdh)
		err = WRITEREG(utp, SUNI_REGO_TPOPAPTR + 1,
		    SUNI_REGM_TPOPAPTR_S,
		    SUNI_REGM_SDH << SUNI_REGS_TPOPAPTR_S);
	else
		err = WRITEREG(utp, SUNI_REGO_TPOPAPTR + 1,
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
utopia_set_unass_default(struct utopia *utp, int unass)
{
	int err;

	if (unass)
		err = WRITEREG(utp, SUNI_REGO_TACPIDLEH,
		    0xff, (0 << SUNI_REGS_TACPIDLEH_CLP));
	else
		err = WRITEREG(utp, SUNI_REGO_TACPIDLEH,
		    0xff, (1 << SUNI_REGS_TACPIDLEH_CLP));
	if (err != 0)
		return (err);

	utp->state &= ~UTP_ST_UNASS;
	if (unass)
		utp->state |= UTP_ST_UNASS;

	return (0);
}

/*
 * Set loopback mode for the Lite
 */
static int
utopia_set_loopback_lite(struct utopia *utp, u_int mode)
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

	err = WRITEREG(utp, SUNI_REGO_MCTRL,
	    SUNI_REGM_MCTRL_LLE | SUNI_REGM_MCTRL_DLE | SUNI_REGM_MCTRL_LOOPT,
	    val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Set loopback mode for the Ultra
 */
static int
utopia_set_loopback_ultra(struct utopia *utp, u_int mode)
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

	err = WRITEREG(utp, SUNI_REGO_MCTRL,
	    SUNI_REGM_MCTRL_LLE | SUNI_REGM_MCTRL_SDLE | SUNI_REGM_MCTRL_LOOPT |
	    SUNI_REGM_MCTRL_PDLE | SUNI_REGM_MCTRL_TPLE, val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Set loopback mode for the Ultra
 */
static int
utopia_set_loopback_622(struct utopia *utp, u_int mode)
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

	err = READREGS(utp, SUNI_REGO_MCONFIG, &config, &n);
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

	err = WRITEREG(utp, SUNI_REGO_MCTRLM,
	    SUNI_REGM_MCTRLM_LLE | SUNI_REGM_MCTRLM_DLE |
	    SUNI_REGM_MCTRLM_DPLE | SUNI_REGM_MCTRL_LOOPT, val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Set the SUNI chip to reflect the current state in utopia.
 * Assume, that the chip has been reset.
 */
static int
utopia_set_chip(struct utopia *utp)
{
	int err = 0;

	/* set sonet/sdh */
	err |= utopia_set_sdh(utp, utp->state & UTP_ST_SDH);

	/* unassigned or idle cells */
	err |= utopia_set_unass(utp, utp->state & UTP_ST_UNASS);
	err |= WRITEREG(utp, SUNI_REGO_TACPIDLEP, 0xff, 0x6a);

	/* loopback */
	err |= utopia_set_loopback(utp, utp->loopback);

	/* update carrier state */
	err |= utopia_update_carrier(utp);

	/* enable interrupts on LOS */
	err |= WRITEREG(utp, SUNI_REGO_RSOPCIE,
	    SUNI_REGM_RSOPCIE_LOSE, SUNI_REGM_RSOPCIE_LOSE);

	return (err ? EIO : 0);
}

/*
 * Reset the SUNI chip to reflect the current state of utopia.
 */
static int
utopia_reset_default(struct utopia *utp)
{
	int err = 0;

	if (!(utp->flags & UTP_FL_NORESET)) {
		err |= WRITEREG(utp, SUNI_REGO_MRESET, SUNI_REGM_MRESET_RESET,
		    SUNI_REGM_MRESET_RESET);
		err |= WRITEREG(utp, SUNI_REGO_MRESET, SUNI_REGM_MRESET_RESET,
		    0);
	}

	/* disable test mode */
	err |= WRITEREG(utp, SUNI_REGO_MTEST, 0xff, 0x00);

	err |= utopia_set_chip(utp);

	return (err ? EIO : 0);
}

/*
 * Reset the SUNI chip to reflect the current state of utopia.
 */
static int
utopia_reset_622(struct utopia *utp)
{
	int err = 0;

	if (!(utp->flags & UTP_FL_NORESET)) {
		err |= WRITEREG(utp, SUNI_REGO_MRESET, SUNI_REGM_MRESET_RESET,
		    SUNI_REGM_MRESET_RESET);
		err |= WRITEREG(utp, SUNI_REGO_MRESET, SUNI_REGM_MRESET_RESET,
		    0);
	}

	/* disable test mode */
	err |= WRITEREG(utp, SUNI_REGO_MTEST, 0xff,
	    SUNI_REGM_MTEST_DS27_53_622);

	err |= utopia_set_chip(utp);

	return (err ? EIO : 0);
}

/*
 * Handle interrupt on lite chip
 */
static void
utopia_intr_default(struct utopia *utp)
{
	uint8_t regs[SUNI_REGO_MTEST];
	u_int n = SUNI_REGO_MTEST;
	int err;

	/* Read all registers. This acks the interrupts */
	if ((err = READREGS(utp, SUNI_REGO_MRESET, regs, &n)) != 0) {
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

/*
 * Update statistics from a SUNI/LITE or SUNI/ULTRA
 */
static void
suni_lite_update_stats(struct utopia *utp)
{
	int err;

	/* write to the master if we can */
	if (!(utp->flags & UTP_FL_NORESET)) {
		err = WRITEREG(utp, SUNI_REGO_MRESET, 0, 0);
	} else {
		err = WRITEREG(utp, SUNI_REGO_RSOP_BIP8, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RLOPBIP8_24, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RPOPBIP8, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RACPCHCS, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_TACPCNT, 0, 0);

	}
	if (err) {
#ifdef DIAGNOSTIC
		printf("%s: register write error %s: %d\n", __func__,
		    utp->chip->name, err);
#endif
		return;
	}

	DELAY(8);

	utp->stats.rx_sbip += UPDATE16(utp, SUNI_REGO_RSOP_BIP8);
	utp->stats.rx_lbip += UPDATE20(utp, SUNI_REGO_RLOPBIP8_24);
	utp->stats.rx_lfebe += UPDATE20(utp, SUNI_REGO_RLOPFEBE);
	utp->stats.rx_pbip += UPDATE16(utp, SUNI_REGO_RPOPBIP8);
	utp->stats.rx_pfebe += UPDATE16(utp, SUNI_REGO_RPOPFEBE);
	utp->stats.rx_corr += UPDATE8(utp, SUNI_REGO_RACPCHCS);
	utp->stats.rx_uncorr += UPDATE8(utp, SUNI_REGO_RACPUHCS);
	utp->stats.rx_cells += UPDATE19(utp, SUNI_REGO_RACPCNT);
	utp->stats.tx_cells += UPDATE19(utp, SUNI_REGO_TACPCNT);
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
		err = WRITEREG(utp, SUNI_REGO_MRESET, 0, 0);
	} else {
		err = WRITEREG(utp, SUNI_REGO_RSOP_BIP8, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RLOPBIP8_24, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RPOPBIP8, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_RACPCHCS, 0, 0);
		err |= WRITEREG(utp, SUNI_REGO_TACPCNT, 0, 0);
	}
	if (err) {
#ifdef DIAGNOSTIC
		printf("%s: register write error %s: %d\n", __func__,
		    utp->chip->name, err);
#endif
		return;
	}

	DELAY(8);

	utp->stats.rx_sbip += UPDATE16(utp, SUNI_REGO_RSOP_BIP8);
	utp->stats.rx_lbip += UPDATE20(utp, SUNI_REGO_RLOPBIP8_24);
	utp->stats.rx_lfebe += UPDATE20(utp, SUNI_REGO_RLOPFEBE);
	utp->stats.rx_pbip += UPDATE16(utp, SUNI_REGO_RPOPBIP8);
	utp->stats.rx_pfebe += UPDATE16(utp, SUNI_REGO_RPOPFEBE);
	utp->stats.rx_corr += UPDATE12(utp, SUNI_REGO_RACPCHCS_622);
	utp->stats.rx_uncorr += UPDATE12(utp, SUNI_REGO_RACPUHCS_622);
	utp->stats.rx_cells += UPDATE21(utp, SUNI_REGO_RACPCNT_622);
	utp->stats.tx_cells += UPDATE21(utp, SUNI_REGO_TACPCNT);
}

static const struct utopia_chip chip_622 = {
	UTP_TYPE_SUNI_622,
	"Suni/622 (PMC-5355)",
	256,
	utopia_reset_622,
	utopia_set_sdh_default,
	utopia_set_unass_default,
	utopia_set_noscramb_default,
	utopia_update_carrier_default,
	utopia_set_loopback_622,
	utopia_intr_default,
	suni_622_update_stats,
};
static const struct utopia_chip chip_lite = {
	UTP_TYPE_SUNI_LITE,
	"Suni/Lite (PMC-5346)",
	256,
	utopia_reset_default,
	utopia_set_sdh_default,
	utopia_set_unass_default,
	utopia_set_noscramb_default,
	utopia_update_carrier_default,
	utopia_set_loopback_lite,
	utopia_intr_default,
	suni_lite_update_stats,
};
static const struct utopia_chip chip_ultra = {
	UTP_TYPE_SUNI_ULTRA,
	"Suni/Ultra (PMC-5350)",
	256,
	utopia_reset_default,
	utopia_set_sdh_default,
	utopia_set_unass_default,
	utopia_set_noscramb_default,
	utopia_update_carrier_default,
	utopia_set_loopback_ultra,
	utopia_intr_default,
	suni_lite_update_stats,
};

/*
 * Reset IDT77105. There is really no way to reset this thing by acessing
 * the registers. Load the registers with default values.
 */
static int
idt77105_reset(struct utopia *utp)
{
	int err = 0;
	u_int n;
	uint8_t val[2];

	err |= WRITEREG(utp, IDTPHY_REGO_MCR, 0xff,
	    IDTPHY_REGM_MCR_DRIC | IDTPHY_REGM_MCR_EI);
	n = 1;
	err |= READREGS(utp, IDTPHY_REGO_ISTAT, val, &n);
	err |= WRITEREG(utp, IDTPHY_REGO_DIAG, 0xff, 0);
	err |= WRITEREG(utp, IDTPHY_REGO_LHEC, 0xff, 0);

	err |= WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_SEC);
	n = 2;
	err |= READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_TX);
	n = 2;
	err |= READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_RX);
	n = 2;
	err |= READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_HECE);
	n = 2;
	err |= READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= WRITEREG(utp, IDTPHY_REGO_MCR, IDTPHY_REGM_MCR_DREC,
	    IDTPHY_REGM_MCR_DREC);
	err |= WRITEREG(utp, IDTPHY_REGO_DIAG, IDTPHY_REGM_DIAG_RFLUSH,
	    IDTPHY_REGM_DIAG_RFLUSH);

	/* loopback */
	err |= utopia_set_loopback(utp, utp->loopback);

	/* update carrier state */
	err |= utopia_update_carrier(utp);

	return (err ? EIO : 0);
}

static int
unknown_inval(struct utopia *utp, int what __unused)
{
	return (EINVAL);
}

static int
idt77105_update_carrier(struct utopia *utp)
{
	int err;
	uint8_t reg;
	u_int n = 1;

	if ((err = READREGS(utp, IDTPHY_REGO_ISTAT, &reg, &n)) != 0) {
		utp->carrier = UTP_CARR_UNKNOWN;
		return (err);
	}
	utopia_check_carrier(utp, reg & IDTPHY_REGM_ISTAT_GOOD);
	return (0);
}

static int
idt77105_set_loopback(struct utopia *utp, u_int mode)
{
	int err;

	switch (mode) {
	  case UTP_LOOP_NONE:
		err = WRITEREG(utp, IDTPHY_REGO_DIAG,
		    IDTPHY_REGM_DIAG_LOOP, IDTPHY_REGM_DIAG_LOOP_NONE);
		break;

	  case UTP_LOOP_DIAG:
		err = WRITEREG(utp, IDTPHY_REGO_DIAG,
		    IDTPHY_REGM_DIAG_LOOP, IDTPHY_REGM_DIAG_LOOP_PHY);
		break;

	  case UTP_LOOP_LINE:
		err = WRITEREG(utp, IDTPHY_REGO_DIAG,
		    IDTPHY_REGM_DIAG_LOOP, IDTPHY_REGM_DIAG_LOOP_LINE);
		break;

	  default:
		return (EINVAL);
	}
	if (err)
		return (err);
	utp->loopback = mode;
	return (0);
}

/*
 * Handle interrupt on IDT77105 chip
 */
static void
idt77105_intr(struct utopia *utp)
{
	uint8_t reg;
	u_int n = 1;
	int err;

	/* Interrupt status and ack the interrupt */
	if ((err = READREGS(utp, IDTPHY_REGO_ISTAT, &reg, &n)) != 0) {
		printf("IDT77105 read error %d\n", err);
		return;
	}
	/* check for signal condition */
	utopia_check_carrier(utp, reg & IDTPHY_REGM_ISTAT_GOOD);
}

static void
idt77105_update_stats(struct utopia *utp)
{
	int err = 0;
	uint8_t regs[2];
	u_int n;

#ifdef DIAGNOSTIC
#define UDIAG(F,A,B)	printf(F, A, B)
#else
#define	UDIAG(F,A,B)	do { } while (0)
#endif

#define	UPD(FIELD, CODE, N, MASK)					\
	err = WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, CODE);		\
	if (err != 0) {							\
		UDIAG("%s: cannot write CNTS: %d\n", __func__, err);	\
		return;							\
	}								\
	n = N;								\
	err = READREGS(utp, IDTPHY_REGO_CNT, regs, &n);			\
	if (err != 0) {							\
		UDIAG("%s: cannot read CNT: %d\n", __func__, err);	\
		return;							\
	}								\
	if (n != N) {							\
		UDIAG("%s: got only %u registers\n", __func__, n);	\
		return;							\
	}								\
	if (N == 1)							\
		utp->stats.FIELD += (regs[0] & MASK);			\
	else								\
		utp->stats.FIELD += (regs[0] | (regs[1] << 8)) & MASK;

	UPD(rx_symerr, IDTPHY_REGM_CNTS_SEC, 1, 0xff);
	UPD(tx_cells, IDTPHY_REGM_CNTS_TX, 2, 0xffff);
	UPD(rx_cells, IDTPHY_REGM_CNTS_RX, 2, 0xffff);
	UPD(rx_uncorr, IDTPHY_REGM_CNTS_HECE, 1, 0x1f);

#undef	UDIAG
#undef	UPD
}

static const struct utopia_chip chip_idt77105 = {
	UTP_TYPE_IDT77105,
	"IDT77105",
	7,
	idt77105_reset,
	unknown_inval,
	unknown_inval,
	unknown_inval,
	idt77105_update_carrier,
	idt77105_set_loopback,
	idt77105_intr,
	idt77105_update_stats,
};

/*
 * Update the carrier status
 */
static int
idt77155_update_carrier(struct utopia *utp)
{
	int err;
	uint8_t reg;
	u_int n = 1;

	if ((err = READREGS(utp, IDTPHY_REGO_RSOS, &reg, &n)) != 0) {
		utp->carrier = UTP_CARR_UNKNOWN;
		return (err);
	}
	utopia_check_carrier(utp, !(reg & IDTPHY_REGM_RSOS_LOS));
	return (0);
}

/*
 * Handle interrupt on IDT77155 chip
 */
static void
idt77155_intr(struct utopia *utp)
{
	uint8_t reg;
	u_int n = 1;
	int err;

	if ((err = READREGS(utp, IDTPHY_REGO_RSOS, &reg, &n)) != 0) {
		printf("IDT77105 read error %d\n", err);
		return;
	}
	utopia_check_carrier(utp, !(reg & IDTPHY_REGM_RSOS_LOS));
}

/*
 * set SONET/SDH mode
 */
static int
idt77155_set_sdh(struct utopia *utp, int sdh)
{
	int err;

	if (sdh)
		err = WRITEREG(utp, IDTPHY_REGO_PTRM,
		    IDTPHY_REGM_PTRM_SS, IDTPHY_REGM_PTRM_SDH);
	else
		err = WRITEREG(utp, IDTPHY_REGO_PTRM,
		    IDTPHY_REGM_PTRM_SS, IDTPHY_REGM_PTRM_SONET);
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
idt77155_set_unass(struct utopia *utp, int unass)
{
	int err;

	if (unass)
		err = WRITEREG(utp, IDTPHY_REGO_TCHP, 0xff, 0);
	else
		err = WRITEREG(utp, IDTPHY_REGO_TCHP, 0xff, 1);
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
idt77155_set_noscramb(struct utopia *utp, int noscramb)
{
	int err;

	if (noscramb) {
		err = WRITEREG(utp, IDTPHY_REGO_TCC,
		    IDTPHY_REGM_TCC_DSCR, IDTPHY_REGM_TCC_DSCR);
		if (err)
			return (err);
		err = WRITEREG(utp, IDTPHY_REGO_RCC,
		    IDTPHY_REGM_RCC_DSCR, IDTPHY_REGM_RCC_DSCR);
		if (err)
			return (err);
		utp->state |= UTP_ST_NOSCRAMB;
	} else {
		err = WRITEREG(utp, IDTPHY_REGO_TCC,
		    IDTPHY_REGM_TCC_DSCR, 0);
		if (err)
			return (err);
		err = WRITEREG(utp, IDTPHY_REGO_RCC,
		    IDTPHY_REGM_RCC_DSCR, 0);
		if (err)
			return (err);
		utp->state &= ~UTP_ST_NOSCRAMB;
	}
	return (0);
}

/*
 * Set loopback mode for the 77155
 */
static int
idt77155_set_loopback(struct utopia *utp, u_int mode)
{
	int err;
	uint32_t val;
	u_int nmode;

	val = 0;
	nmode = mode;
	if (mode & UTP_LOOP_TIME) {
		nmode &= ~UTP_LOOP_TIME;
		val |= IDTPHY_REGM_MCTL_TLOOP;
	}
	if (mode & UTP_LOOP_DIAG) {
		nmode &= ~UTP_LOOP_DIAG;
		val |= IDTPHY_REGM_MCTL_DLOOP;
	}
	if (mode & UTP_LOOP_LINE) {
		nmode &= ~UTP_LOOP_LINE;
		val |= IDTPHY_REGM_MCTL_LLOOP;
	}
	if (nmode != 0)
		return (EINVAL);

	err = WRITEREG(utp, IDTPHY_REGO_MCTL, IDTPHY_REGM_MCTL_TLOOP |
	    IDTPHY_REGM_MCTL_DLOOP | IDTPHY_REGM_MCTL_LLOOP, val);
	if (err)
		return (err);
	utp->loopback = mode;

	return (0);
}

/*
 * Set the chip to reflect the current state in utopia.
 * Assume, that the chip has been reset.
 */
static int
idt77155_set_chip(struct utopia *utp)
{
	int err = 0;

	/* set sonet/sdh */
	err |= idt77155_set_sdh(utp, utp->state & UTP_ST_SDH);

	/* unassigned or idle cells */
	err |= idt77155_set_unass(utp, utp->state & UTP_ST_UNASS);

	/* loopback */
	err |= idt77155_set_loopback(utp, utp->loopback);

	/* update carrier state */
	err |= idt77155_update_carrier(utp);

	/* enable interrupts on LOS */
	err |= WRITEREG(utp, IDTPHY_REGO_INT,
	    IDTPHY_REGM_INT_RXSOHI, IDTPHY_REGM_INT_RXSOHI);
	err |= WRITEREG(utp, IDTPHY_REGO_RSOC,
	    IDTPHY_REGM_RSOC_LOSI, IDTPHY_REGM_RSOC_LOSI);

	return (err ? EIO : 0);
}

/*
 * Reset the chip to reflect the current state of utopia.
 */
static int
idt77155_reset(struct utopia *utp)
{
	int err = 0;

	if (!(utp->flags & UTP_FL_NORESET)) {
		err |= WRITEREG(utp, IDTPHY_REGO_MRID, IDTPHY_REGM_MRID_RESET,
		    IDTPHY_REGM_MRID_RESET);
		err |= WRITEREG(utp, IDTPHY_REGO_MRID, IDTPHY_REGM_MRID_RESET,
		    0);
	}

	err |= idt77155_set_chip(utp);

	return (err ? EIO : 0);
}

/*
 * Update statistics from a IDT77155
 * This appears to be the same as for the Suni/Lite and Ultra. IDT however
 * makes no assessment about the transfer time. Assume 7us.
 */
static void
idt77155_update_stats(struct utopia *utp)
{
	int err;

	/* write to the master if we can */
	if (!(utp->flags & UTP_FL_NORESET)) {
		err = WRITEREG(utp, IDTPHY_REGO_MRID, 0, 0);
	} else {
		err = WRITEREG(utp, IDTPHY_REGO_BIPC, 0, 0);
		err |= WRITEREG(utp, IDTPHY_REGO_B2EC, 0, 0);
		err |= WRITEREG(utp, IDTPHY_REGO_B3EC, 0, 0);
		err |= WRITEREG(utp, IDTPHY_REGO_CEC, 0, 0);
		err |= WRITEREG(utp, IDTPHY_REGO_TXCNT, 0, 0);

	}
	if (err) {
#ifdef DIAGNOSTIC
		printf("%s: register write error %s: %d\n", __func__,
		    utp->chip->name, err);
#endif
		return;
	}

	DELAY(8);

	utp->stats.rx_sbip += UPDATE16(utp, IDTPHY_REGO_BIPC);
	utp->stats.rx_lbip += UPDATE20(utp, IDTPHY_REGO_B2EC);
	utp->stats.rx_lfebe += UPDATE20(utp, IDTPHY_REGO_FEBEC);
	utp->stats.rx_pbip += UPDATE16(utp, IDTPHY_REGO_B3EC);
	utp->stats.rx_pfebe += UPDATE16(utp, IDTPHY_REGO_PFEBEC);
	utp->stats.rx_corr += UPDATE8(utp, IDTPHY_REGO_CEC);
	utp->stats.rx_uncorr += UPDATE8(utp, IDTPHY_REGO_UEC);
	utp->stats.rx_cells += UPDATE19(utp, IDTPHY_REGO_RCCNT);
	utp->stats.tx_cells += UPDATE19(utp, IDTPHY_REGO_TXCNT);
}


static const struct utopia_chip chip_idt77155 = {
	UTP_TYPE_IDT77155,
	"IDT77155",
	0x80,
	idt77155_reset,
	idt77155_set_sdh,
	idt77155_set_unass,
	idt77155_set_noscramb,
	idt77155_update_carrier,
	idt77155_set_loopback,
	idt77155_intr,
	idt77155_update_stats,
};

static int
unknown_reset(struct utopia *utp __unused)
{
	return (EIO);
}

static int
unknown_update_carrier(struct utopia *utp)
{
	utp->carrier = UTP_CARR_UNKNOWN;
	return (0);
}

static int
unknown_set_loopback(struct utopia *utp __unused, u_int mode __unused)
{
	return (EINVAL);
}

static void
unknown_intr(struct utopia *utp __unused)
{
}

static void
unknown_update_stats(struct utopia *utp __unused)
{
}

static const struct utopia_chip chip_unknown = {
	UTP_TYPE_UNKNOWN,
	"unknown",
	0,
	unknown_reset,
	unknown_inval,
	unknown_inval,
	unknown_inval,
	unknown_update_carrier,
	unknown_set_loopback,
	unknown_intr,
	unknown_update_stats,
};

/*
 * Callbacks for the ifmedia infrastructure.
 */
static int
utopia_media_change(struct ifnet *ifp)
{
	struct ifatm *ifatm = (struct ifatm *)ifp->if_softc;
	struct utopia *utp = ifatm->phy;
	int error = 0;

	UTP_LOCK(utp);
	if (utp->chip->type != UTP_TYPE_UNKNOWN && utp->state & UTP_ST_ACTIVE) {
		if (utp->media->ifm_media & IFM_ATM_SDH) {
			if (!(utp->state & UTP_ST_SDH))
				error = utopia_set_sdh(utp, 1);
		} else {
			if (utp->state & UTP_ST_SDH)
				error = utopia_set_sdh(utp, 0);
		}
		if (utp->media->ifm_media & IFM_ATM_UNASSIGNED) {
			if (!(utp->state & UTP_ST_UNASS))
				error = utopia_set_unass(utp, 1);
		} else {
			if (utp->state & UTP_ST_UNASS)
				error = utopia_set_unass(utp, 0);
		}
		if (utp->media->ifm_media & IFM_ATM_NOSCRAMB) {
			if (!(utp->state & UTP_ST_NOSCRAMB))
				error = utopia_set_noscramb(utp, 1);
		} else {
			if (utp->state & UTP_ST_NOSCRAMB)
				error = utopia_set_noscramb(utp, 0);
		}
	} else
		error = EIO;
	UTP_UNLOCK(utp);
	return (error);
}

/*
 * Look at the carrier status.
 */
static void
utopia_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct utopia *utp = ((struct ifatm *)ifp->if_softc)->phy;

	UTP_LOCK(utp);
	if (utp->chip->type != UTP_TYPE_UNKNOWN && utp->state & UTP_ST_ACTIVE) {
		ifmr->ifm_active = IFM_ATM | utp->ifatm->mib.media;

		switch (utp->carrier) {

		  case UTP_CARR_OK:
			ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
			break;

		  case UTP_CARR_LOST:
			ifmr->ifm_status = IFM_AVALID;
			break;

		  default:
			ifmr->ifm_status = 0;
			break;
		}
		if (utp->state & UTP_ST_SDH) {
			ifmr->ifm_active |= IFM_ATM_SDH;
			ifmr->ifm_current |= IFM_ATM_SDH;
		}
		if (utp->state & UTP_ST_UNASS) {
			ifmr->ifm_active |= IFM_ATM_UNASSIGNED;
			ifmr->ifm_current |= IFM_ATM_UNASSIGNED;
		}
		if (utp->state & UTP_ST_NOSCRAMB) {
			ifmr->ifm_active |= IFM_ATM_NOSCRAMB;
			ifmr->ifm_current |= IFM_ATM_NOSCRAMB;
		}
	} else {
		ifmr->ifm_active = 0;
		ifmr->ifm_status = 0;
	}
	UTP_UNLOCK(utp);
}

/*
 * Initialize media from the mib
 */
void
utopia_init_media(struct utopia *utp)
{

	ifmedia_removeall(utp->media);
	ifmedia_add(utp->media, IFM_ATM | utp->ifatm->mib.media, 0, NULL);
	ifmedia_set(utp->media, IFM_ATM | utp->ifatm->mib.media);
}

/*
 * Reset all media
 */
void
utopia_reset_media(struct utopia *utp)
{

	ifmedia_removeall(utp->media);
}

/*
 * This is called by the driver as soon as the SUNI registers are accessible.
 * This may be either in the attach routine or the init routine of the driver.
 */
int
utopia_start(struct utopia *utp)
{
	uint8_t reg;
	int err;
	u_int n = 1;

	if ((err = READREGS(utp, SUNI_REGO_MRESET, &reg, &n)) != 0)
		return (err);

	switch (reg & SUNI_REGM_MRESET_TYPE) {

	  case SUNI_REGM_MRESET_TYPE_622:
		utp->chip = &chip_622;
		break;

	  case SUNI_REGM_MRESET_TYPE_LITE:
		/* this may be either a SUNI LITE or a IDT77155 *
		 * Read register 0x70. The SUNI doesn't have it */
		n = 1;
		if ((err = READREGS(utp, IDTPHY_REGO_RBER, &reg, &n)) != 0)
			return (err);
		if ((reg & ~IDTPHY_REGM_RBER_RESV) ==
		    (IDTPHY_REGM_RBER_FAIL | IDTPHY_REGM_RBER_WARN))
			utp->chip = &chip_idt77155;
		else
			utp->chip = &chip_lite;
		break;

	  case SUNI_REGM_MRESET_TYPE_ULTRA:
		utp->chip = &chip_ultra;
		break;

	  default:
		if (reg == (IDTPHY_REGM_MCR_DRIC | IDTPHY_REGM_MCR_EI))
			utp->chip = &chip_idt77105;
		else {
			if_printf(&utp->ifatm->ifnet,
			    "unknown ATM-PHY chip %#x\n", reg);
			utp->chip = &chip_unknown;
		}
		break;
	}
	utp->state |= UTP_ST_ACTIVE;
	return (0);
}

/*
 * Stop the chip
 */
void
utopia_stop(struct utopia *utp)
{
	utp->state &= ~UTP_ST_ACTIVE;
}

/*
 * Handle the sysctls
 */
static int
utopia_sysctl_regs(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	int error;
	u_int n;
	uint8_t *val;
	uint8_t new[3];

	if ((n = utp->chip->nregs) == 0)
		return (EIO);
	val = malloc(sizeof(uint8_t) * n, M_TEMP, M_WAITOK);

	UTP_LOCK(utp);
	error = READREGS(utp, 0, val, &n);
	UTP_UNLOCK(utp);

	if (error) {
		free(val, M_TEMP);
		return (error);
	}

	error = SYSCTL_OUT(req, val, sizeof(uint8_t) * n);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, new, sizeof(new));
	if (error)
		return (error);

	UTP_LOCK(utp);
	error = WRITEREG(utp, new[0], new[1], new[2]);
	UTP_UNLOCK(utp);

	return (error);
}

static int
utopia_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	void *val;
	int error;

	val = malloc(sizeof(utp->stats), M_TEMP, M_WAITOK);

	UTP_LOCK(utp);
	bcopy(&utp->stats, val, sizeof(utp->stats));
	if (req->newptr != NULL)
		bzero((char *)&utp->stats + sizeof(utp->stats.version),
		    sizeof(utp->stats) - sizeof(utp->stats.version));
	UTP_UNLOCK(utp);

	error = SYSCTL_OUT(req, val, sizeof(utp->stats));
	free(val, M_TEMP);

	if (error && req->newptr != NULL)
		bcopy(val, &utp->stats, sizeof(utp->stats));

	/* ignore actual new value */

	return (error);
}

/*
 * Handle the loopback sysctl
 */
static int
utopia_sysctl_loopback(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	int error;
	u_int loopback;

	error = SYSCTL_OUT(req, &utp->loopback, sizeof(u_int));
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, &loopback, sizeof(u_int));
	if (error)
		return (error);

	UTP_LOCK(utp);
	error = utopia_set_loopback(utp, loopback);
	UTP_UNLOCK(utp);

	return (error);
}

/*
 * Handle the type sysctl
 */
static int
utopia_sysctl_type(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;

	return (SYSCTL_OUT(req, &utp->chip->type, sizeof(utp->chip->type)));
}

/*
 * Handle the name sysctl
 */
static int
utopia_sysctl_name(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;

	return (SYSCTL_OUT(req, utp->chip->name, strlen(utp->chip->name) + 1));
}

/*
 * Initialize the state. This is called from the drivers attach
 * function. The mutex must be already initialized.
 */
int
utopia_attach(struct utopia *utp, struct ifatm *ifatm, struct ifmedia *media,
    struct mtx *lock, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *children, const struct utopia_methods *m)
{

	bzero(utp, sizeof(*utp));
	utp->ifatm = ifatm;
	utp->methods = m;
	utp->media = media;
	utp->lock = lock;
	utp->chip = &chip_unknown;
	utp->stats.version = 1;

	ifmedia_init(media,
	    IFM_ATM_SDH | IFM_ATM_UNASSIGNED | IFM_ATM_NOSCRAMB,
	    utopia_media_change, utopia_media_status);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_regs",
	    CTLFLAG_RW | CTLTYPE_OPAQUE, utp, 0, utopia_sysctl_regs, "S",
	    "phy registers") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_loopback",
	    CTLFLAG_RW | CTLTYPE_UINT, utp, 0, utopia_sysctl_loopback, "IU",
	    "phy loopback mode") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_type",
	    CTLFLAG_RD | CTLTYPE_UINT, utp, 0, utopia_sysctl_type, "IU",
	    "phy type") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_name",
	    CTLFLAG_RD | CTLTYPE_STRING, utp, 0, utopia_sysctl_name, "A",
	    "phy name") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_stats",
	    CTLFLAG_RW | CTLTYPE_OPAQUE, utp, 0, utopia_sysctl_stats, "S",
	    "phy statistics") == NULL)
		return (-1);

	if (SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "phy_state",
	    CTLFLAG_RD, &utp->state, 0, "phy state") == NULL)
		return (-1);

	if (SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "phy_carrier",
	    CTLFLAG_RD, &utp->carrier, 0, "phy carrier") == NULL)
		return (-1);

	UTP_WLOCK_LIST();
	LIST_INSERT_HEAD(&utopia_list, utp, link);
	UTP_WUNLOCK_LIST();

	utp->state |= UTP_ST_ATTACHED;
	return (0);
}

/*
 * Detach. We set a flag here, wakeup the daemon and let him do it.
 * Here we need the lock for synchronisation with the daemon.
 */
void
utopia_detach(struct utopia *utp)
{

	UTP_LOCK_ASSERT(utp);
	if (utp->state & UTP_ST_ATTACHED) {
		utp->state |= UTP_ST_DETACH;
		while (utp->state & UTP_ST_DETACH) {
			wakeup(&utopia_list);
			msleep(utp, utp->lock, PZERO, "utopia_detach", hz);
		}
	}
}

/*
 * The carrier state kernel proc for those adapters that do not interrupt.
 *
 * We assume, that utopia_attach can safely add a new utopia while we are going
 * through the list without disturbing us (we lock the list while getting
 * the address of the first element, adding is always done at the head).
 * Removing is entirely handled here.
 */
static void
utopia_daemon(void *arg __unused)
{
	struct utopia *utp, *next;

	UTP_RLOCK_LIST();
	while (utopia_kproc != NULL) {
		utp = LIST_FIRST(&utopia_list);
		UTP_RUNLOCK_LIST();

		while (utp != NULL) {
			mtx_lock(&Giant);	/* XXX depend on MPSAFE */
			UTP_LOCK(utp);
			next = LIST_NEXT(utp, link);
			if (utp->state & UTP_ST_DETACH) {
				LIST_REMOVE(utp, link);
				utp->state &= ~UTP_ST_DETACH;
				wakeup_one(utp);
			} else if (utp->state & UTP_ST_ACTIVE) {
				if (utp->flags & UTP_FL_POLL_CARRIER)
					utopia_update_carrier(utp);
				utopia_update_stats(utp);
			}
			UTP_UNLOCK(utp);
			mtx_unlock(&Giant);	/* XXX depend on MPSAFE */
			utp = next;
		}

		UTP_RLOCK_LIST();
		msleep(&utopia_list, &utopia_list_mtx, PZERO, "*idle*", hz);
	}
	wakeup_one(&utopia_list);
	UTP_RUNLOCK_LIST();
	kthread_exit(0);
}

/*
 * Module initialisation
 */
static int
utopia_mod_init(module_t mod, int what, void *arg)
{
	int err;
	struct proc *kp;

	switch (what) {

	  case MOD_LOAD:
		mtx_init(&utopia_list_mtx, "utopia list mutex", NULL, MTX_DEF);
		err = kthread_create(utopia_daemon, NULL, &utopia_kproc,
		    RFHIGHPID, 0, "utopia");
		if (err != 0) {
			printf("cannot created utopia thread %d\n", err);
			return (err);
		}
		break;

	  case MOD_UNLOAD:
		UTP_WLOCK_LIST();
		if ((kp = utopia_kproc) != NULL) {
			utopia_kproc = NULL;
			wakeup_one(&utopia_list);
			PROC_LOCK(kp);
			UTP_WUNLOCK_LIST();
			msleep(kp, &kp->p_mtx, PWAIT, "utopia_destroy", 0);
			PROC_UNLOCK(kp);
		} else
			UTP_WUNLOCK_LIST();
		mtx_destroy(&utopia_list_mtx);
		break;
	  default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t utopia_mod = {
        "utopia",
        utopia_mod_init,
        0
};
                
DECLARE_MODULE(utopia, utopia_mod, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(utopia, 1);
