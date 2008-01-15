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
__FBSDID("$FreeBSD: src/sys/dev/utopia/idtphy.c,v 1.1 2005/02/24 16:56:36 harti Exp $");

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

#include <dev/utopia/idtphy.h>
#include <dev/utopia/utopia.h>
#include <dev/utopia/utopia_priv.h>

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

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_MCR, 0xff,
	    IDTPHY_REGM_MCR_DRIC | IDTPHY_REGM_MCR_EI);
	n = 1;
	err |= UTP_READREGS(utp, IDTPHY_REGO_ISTAT, val, &n);
	err |= UTP_WRITEREG(utp, IDTPHY_REGO_DIAG, 0xff, 0);
	err |= UTP_WRITEREG(utp, IDTPHY_REGO_LHEC, 0xff, 0);

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_SEC);
	n = 2;
	err |= UTP_READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_TX);
	n = 2;
	err |= UTP_READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_RX);
	n = 2;
	err |= UTP_READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, IDTPHY_REGM_CNTS_HECE);
	n = 2;
	err |= UTP_READREGS(utp, IDTPHY_REGO_CNT, val, &n);

	err |= UTP_WRITEREG(utp, IDTPHY_REGO_MCR, IDTPHY_REGM_MCR_DREC,
	    IDTPHY_REGM_MCR_DREC);
	err |= UTP_WRITEREG(utp, IDTPHY_REGO_DIAG, IDTPHY_REGM_DIAG_RFLUSH,
	    IDTPHY_REGM_DIAG_RFLUSH);

	/* loopback */
	err |= utopia_set_loopback(utp, utp->loopback);

	/* update carrier state */
	err |= utopia_update_carrier(utp);

	return (err ? EIO : 0);
}

static int
idt77105_inval(struct utopia *utp, int what __unused)
{

	return (EINVAL);
}

static int
idt77105_update_carrier(struct utopia *utp)
{
	int err;
	uint8_t reg;
	u_int n = 1;

	if ((err = UTP_READREGS(utp, IDTPHY_REGO_ISTAT, &reg, &n)) != 0) {
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
		err = UTP_WRITEREG(utp, IDTPHY_REGO_DIAG,
		    IDTPHY_REGM_DIAG_LOOP, IDTPHY_REGM_DIAG_LOOP_NONE);
		break;

	  case UTP_LOOP_DIAG:
		err = UTP_WRITEREG(utp, IDTPHY_REGO_DIAG,
		    IDTPHY_REGM_DIAG_LOOP, IDTPHY_REGM_DIAG_LOOP_PHY);
		break;

	  case UTP_LOOP_LINE:
		err = UTP_WRITEREG(utp, IDTPHY_REGO_DIAG,
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
	if ((err = UTP_READREGS(utp, IDTPHY_REGO_ISTAT, &reg, &n)) != 0) {
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
	err = UTP_WRITEREG(utp, IDTPHY_REGO_CNTS, 0xff, CODE);		\
	if (err != 0) {							\
		UDIAG("%s: cannot write CNTS: %d\n", __func__, err);	\
		return;							\
	}								\
	n = N;								\
	err = UTP_READREGS(utp, IDTPHY_REGO_CNT, regs, &n);		\
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

struct utopia_chip utopia_chip_idt77105 = {
	UTP_TYPE_IDT77105,
	"IDT77105",
	7,
	idt77105_reset,
	idt77105_inval,
	idt77105_inval,
	idt77105_inval,
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

	if ((err = UTP_READREGS(utp, IDTPHY_REGO_RSOS, &reg, &n)) != 0) {
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

	if ((err = UTP_READREGS(utp, IDTPHY_REGO_RSOS, &reg, &n)) != 0) {
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
		err = UTP_WRITEREG(utp, IDTPHY_REGO_PTRM,
		    IDTPHY_REGM_PTRM_SS, IDTPHY_REGM_PTRM_SDH);
	else
		err = UTP_WRITEREG(utp, IDTPHY_REGO_PTRM,
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
		err = UTP_WRITEREG(utp, IDTPHY_REGO_TCHP, 0xff, 0);
	else
		err = UTP_WRITEREG(utp, IDTPHY_REGO_TCHP, 0xff, 1);
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
		err = UTP_WRITEREG(utp, IDTPHY_REGO_TCC,
		    IDTPHY_REGM_TCC_DSCR, IDTPHY_REGM_TCC_DSCR);
		if (err)
			return (err);
		err = UTP_WRITEREG(utp, IDTPHY_REGO_RCC,
		    IDTPHY_REGM_RCC_DSCR, IDTPHY_REGM_RCC_DSCR);
		if (err)
			return (err);
		utp->state |= UTP_ST_NOSCRAMB;
	} else {
		err = UTP_WRITEREG(utp, IDTPHY_REGO_TCC,
		    IDTPHY_REGM_TCC_DSCR, 0);
		if (err)
			return (err);
		err = UTP_WRITEREG(utp, IDTPHY_REGO_RCC,
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

	err = UTP_WRITEREG(utp, IDTPHY_REGO_MCTL, IDTPHY_REGM_MCTL_TLOOP |
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
	err |= UTP_WRITEREG(utp, IDTPHY_REGO_INT,
	    IDTPHY_REGM_INT_RXSOHI, IDTPHY_REGM_INT_RXSOHI);
	err |= UTP_WRITEREG(utp, IDTPHY_REGO_RSOC,
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
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_MRID,
		    IDTPHY_REGM_MRID_RESET, IDTPHY_REGM_MRID_RESET);
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_MRID,
		    IDTPHY_REGM_MRID_RESET, 0);
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
		err = UTP_WRITEREG(utp, IDTPHY_REGO_MRID, 0, 0);
	} else {
		err = UTP_WRITEREG(utp, IDTPHY_REGO_BIPC, 0, 0);
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_B2EC, 0, 0);
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_B3EC, 0, 0);
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_CEC, 0, 0);
		err |= UTP_WRITEREG(utp, IDTPHY_REGO_TXCNT, 0, 0);

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
	    IDTPHY_REGO_BIPC, 2, 0xffff);
	utp->stats.rx_lbip += utopia_update(utp,
	    IDTPHY_REGO_B2EC, 3, 0xfffff);
	utp->stats.rx_lfebe += utopia_update(utp,
	    IDTPHY_REGO_FEBEC, 3, 0xfffff);
	utp->stats.rx_pbip += utopia_update(utp,
	    IDTPHY_REGO_B3EC, 2, 0xffff);
	utp->stats.rx_pfebe += utopia_update(utp,
	    IDTPHY_REGO_PFEBEC, 2, 0xffff);
	utp->stats.rx_corr += utopia_update(utp,
	    IDTPHY_REGO_CEC, 1, 0xff);
	utp->stats.rx_uncorr += utopia_update(utp,
	    IDTPHY_REGO_UEC, 1, 0xff);
	utp->stats.rx_cells += utopia_update(utp,
	    IDTPHY_REGO_RCCNT, 3, 0x7ffff);
	utp->stats.tx_cells += utopia_update(utp,
	    IDTPHY_REGO_TXCNT, 3, 0x7ffff);
}

const struct utopia_chip utopia_chip_idt77155 = {
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
