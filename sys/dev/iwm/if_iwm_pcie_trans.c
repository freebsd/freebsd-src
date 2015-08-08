/*	$OpenBSD: if_iwm.c,v 1.39 2015/03/23 00:35:19 jsg Exp $	*/

/*
 * Copyright (c) 2014 genua mbh <info@genua.de>
 * Copyright (c) 2014 Fixup Software Ltd.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 * Driver version we are currently based off of is
 * Linux 3.14.3 (tag id a2df521e42b1d9a23f620ac79dbfe8655a8391dd)
 *
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/linker.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/bpf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/iwm/if_iwmreg.h>
#include <dev/iwm/if_iwmvar.h>
#include <dev/iwm/if_iwm_debug.h>
#include <dev/iwm/if_iwm_pcie_trans.h>

/*
 * This is a subset of what's in linux iwlwifi/pcie/trans.c.
 * The rest can be migrated out into here once they're no longer in
 * if_iwm.c.
 */

/*
 * basic device access
 */

uint32_t
iwm_read_prph(struct iwm_softc *sc, uint32_t addr)
{
	IWM_WRITE(sc,
	    IWM_HBUS_TARG_PRPH_RADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWM_BARRIER_READ_WRITE(sc);
	return IWM_READ(sc, IWM_HBUS_TARG_PRPH_RDAT);
}

void
iwm_write_prph(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
	IWM_WRITE(sc,
	    IWM_HBUS_TARG_PRPH_WADDR, ((addr & 0x000fffff) | (3 << 24)));
	IWM_BARRIER_WRITE(sc);
	IWM_WRITE(sc, IWM_HBUS_TARG_PRPH_WDAT, val);
}

#ifdef IWM_DEBUG
/* iwlwifi: pcie/trans.c */
int
iwm_read_mem(struct iwm_softc *sc, uint32_t addr, void *buf, int dwords)
{
	int offs, ret = 0;
	uint32_t *vals = buf;

	if (iwm_nic_lock(sc)) {
		IWM_WRITE(sc, IWM_HBUS_TARG_MEM_RADDR, addr);
		for (offs = 0; offs < dwords; offs++)
			vals[offs] = IWM_READ(sc, IWM_HBUS_TARG_MEM_RDAT);
		iwm_nic_unlock(sc);
	} else {
		ret = EBUSY;
	}
	return ret;
}
#endif

/* iwlwifi: pcie/trans.c */
int
iwm_write_mem(struct iwm_softc *sc, uint32_t addr, const void *buf, int dwords)
{
	int offs;
	const uint32_t *vals = buf;

	if (iwm_nic_lock(sc)) {
		IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WADDR, addr);
		/* WADDR auto-increments */
		for (offs = 0; offs < dwords; offs++) {
			uint32_t val = vals ? vals[offs] : 0;
			IWM_WRITE(sc, IWM_HBUS_TARG_MEM_WDAT, val);
		}
		iwm_nic_unlock(sc);
	} else {
		IWM_DPRINTF(sc, IWM_DEBUG_TRANS,
		    "%s: write_mem failed\n", __func__);
		return EBUSY;
	}
	return 0;
}

int
iwm_write_mem32(struct iwm_softc *sc, uint32_t addr, uint32_t val)
{
	return iwm_write_mem(sc, addr, &val, 1);
}

int
iwm_poll_bit(struct iwm_softc *sc, int reg,
	uint32_t bits, uint32_t mask, int timo)
{
	for (;;) {
		if ((IWM_READ(sc, reg) & mask) == (bits & mask)) {
			return 1;
		}
		if (timo < 10) {
			return 0;
		}
		timo -= 10;
		DELAY(10);
	}
}

int
iwm_nic_lock(struct iwm_softc *sc)
{
	int rv = 0;

	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	if (iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY
	     | IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP, 15000)) {
	    	rv = 1;
	} else {
		/* jolt */
		IWM_WRITE(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_FORCE_NMI);
	}

	return rv;
}

void
iwm_nic_unlock(struct iwm_softc *sc)
{
	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
}

void
iwm_set_bits_mask_prph(struct iwm_softc *sc,
	uint32_t reg, uint32_t bits, uint32_t mask)
{
	uint32_t val;

	/* XXX: no error path? */
	if (iwm_nic_lock(sc)) {
		val = iwm_read_prph(sc, reg) & mask;
		val |= bits;
		iwm_write_prph(sc, reg, val);
		iwm_nic_unlock(sc);
	}
}

void
iwm_set_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
	iwm_set_bits_mask_prph(sc, reg, bits, ~0);
}

void
iwm_clear_bits_prph(struct iwm_softc *sc, uint32_t reg, uint32_t bits)
{
	iwm_set_bits_mask_prph(sc, reg, 0, ~bits);
}

/*
 * High-level hardware frobbing routines
 */

void
iwm_enable_rfkill_int(struct iwm_softc *sc)
{
	sc->sc_intmask = IWM_CSR_INT_BIT_RF_KILL;
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

int
iwm_check_rfkill(struct iwm_softc *sc)
{
	uint32_t v;
	int rv;

	/*
	 * "documentation" is not really helpful here:
	 *  27:	HW_RF_KILL_SW
	 *	Indicates state of (platform's) hardware RF-Kill switch
	 *
	 * But apparently when it's off, it's on ...
	 */
	v = IWM_READ(sc, IWM_CSR_GP_CNTRL);
	rv = (v & IWM_CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW) == 0;
	if (rv) {
		sc->sc_flags |= IWM_FLAG_RFKILL;
	} else {
		sc->sc_flags &= ~IWM_FLAG_RFKILL;
	}

	return rv;
}


#define IWM_HW_READY_TIMEOUT 50
int
iwm_set_hw_ready(struct iwm_softc *sc)
{
	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY);

	return iwm_poll_bit(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_NIC_READY,
	    IWM_HW_READY_TIMEOUT);
}
#undef IWM_HW_READY_TIMEOUT

int
iwm_prepare_card_hw(struct iwm_softc *sc)
{
	int rv = 0;
	int t = 0;

	IWM_DPRINTF(sc, IWM_DEBUG_RESET, "->%s\n", __func__);
	if (iwm_set_hw_ready(sc))
		goto out;

	/* If HW is not ready, prepare the conditions to check again */
	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_PREPARE);

	do {
		if (iwm_set_hw_ready(sc))
			goto out;
		DELAY(200);
		t += 200;
	} while (t < 150000);

	rv = ETIMEDOUT;

 out:
	IWM_DPRINTF(sc, IWM_DEBUG_RESET, "<-%s\n", __func__);
	return rv;
}

void
iwm_apm_config(struct iwm_softc *sc)
{
	uint16_t reg;

	reg = pci_read_config(sc->sc_dev, PCIER_LINK_CTL, sizeof(reg));
	if (reg & PCIEM_LINK_CTL_ASPMC_L1)  {
		/* Um the Linux driver prints "Disabling L0S for this one ... */
		IWM_SETBITS(sc, IWM_CSR_GIO_REG,
		    IWM_CSR_GIO_REG_VAL_L0S_ENABLED);
	} else {
		/* ... and "Enabling" here */
		IWM_CLRBITS(sc, IWM_CSR_GIO_REG,
		    IWM_CSR_GIO_REG_VAL_L0S_ENABLED);
	}
}

/*
 * Start up NIC's basic functionality after it has been reset
 * (e.g. after platform boot, or shutdown via iwm_pcie_apm_stop())
 * NOTE:  This does not load uCode nor start the embedded processor
 */
int
iwm_apm_init(struct iwm_softc *sc)
{
	int error = 0;

	IWM_DPRINTF(sc, IWM_DEBUG_RESET, "iwm apm start\n");

	/* Disable L0S exit timer (platform NMI Work/Around) */
	IWM_SETBITS(sc, IWM_CSR_GIO_CHICKEN_BITS,
	    IWM_CSR_GIO_CHICKEN_BITS_REG_BIT_DIS_L0S_EXIT_TIMER);

	/*
	 * Disable L0s without affecting L1;
	 *  don't wait for ICH L0s (ICH bug W/A)
	 */
	IWM_SETBITS(sc, IWM_CSR_GIO_CHICKEN_BITS,
	    IWM_CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX);

	/* Set FH wait threshold to maximum (HW error during stress W/A) */
	IWM_SETBITS(sc, IWM_CSR_DBG_HPET_MEM_REG, IWM_CSR_DBG_HPET_MEM_REG_VAL);

	/*
	 * Enable HAP INTA (interrupt from management bus) to
	 * wake device's PCI Express link L1a -> L0s
	 */
	IWM_SETBITS(sc, IWM_CSR_HW_IF_CONFIG_REG,
	    IWM_CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);

	iwm_apm_config(sc);

#if 0 /* not for 7k */
	/* Configure analog phase-lock-loop before activating to D0A */
	if (trans->cfg->base_params->pll_cfg_val)
		IWM_SETBITS(trans, IWM_CSR_ANA_PLL_CFG,
		    trans->cfg->base_params->pll_cfg_val);
#endif

	/*
	 * Set "initialization complete" bit to move adapter from
	 * D0U* --> D0A* (powered-up active) state.
	 */
	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL, IWM_CSR_GP_CNTRL_REG_FLAG_INIT_DONE);

	/*
	 * Wait for clock stabilization; once stabilized, access to
	 * device-internal resources is supported, e.g. iwm_write_prph()
	 * and accesses to uCode SRAM.
	 */
	if (!iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY, 25000)) {
		device_printf(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");

		goto out;
	}

	if (sc->host_interrupt_operation_mode) {
		/*
		 * This is a bit of an abuse - This is needed for 7260 / 3160
		 * only check host_interrupt_operation_mode even if this is
		 * not related to host_interrupt_operation_mode.
		 *
		 * Enable the oscillator to count wake up time for L1 exit. This
		 * consumes slightly more power (100uA) - but allows to be sure
		 * that we wake up from L1 on time.
		 *
		 * This looks weird: read twice the same register, discard the
		 * value, set a bit, and yet again, read that same register
		 * just to discard the value. But that's the way the hardware
		 * seems to like it.
		 */
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_set_bits_prph(sc, IWM_OSC_CLK, IWM_OSC_CLK_FORCE_CONTROL);
		iwm_read_prph(sc, IWM_OSC_CLK);
		iwm_read_prph(sc, IWM_OSC_CLK);
	}

	/*
	 * Enable DMA clock and wait for it to stabilize.
	 *
	 * Write to "CLK_EN_REG"; "1" bits enable clocks, while "0" bits
	 * do not disable clocks.  This preserves any hardware bits already
	 * set by default in "CLK_CTRL_REG" after reset.
	 */
	iwm_write_prph(sc, IWM_APMG_CLK_EN_REG, IWM_APMG_CLK_VAL_DMA_CLK_RQT);
	//kpause("iwmapm", 0, mstohz(20), NULL);
	DELAY(20);

	/* Disable L1-Active */
	iwm_set_bits_prph(sc, IWM_APMG_PCIDEV_STT_REG,
	    IWM_APMG_PCIDEV_STT_VAL_L1_ACT_DIS);

	/* Clear the interrupt in APMG if the NIC is in RFKILL */
	iwm_write_prph(sc, IWM_APMG_RTC_INT_STT_REG,
	    IWM_APMG_RTC_INT_STT_RFKILL);

 out:
	if (error)
		device_printf(sc->sc_dev, "apm init error %d\n", error);
	return error;
}

/* iwlwifi/pcie/trans.c */
void
iwm_apm_stop(struct iwm_softc *sc)
{
	/* stop device's busmaster DMA activity */
	IWM_SETBITS(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_STOP_MASTER);

	if (!iwm_poll_bit(sc, IWM_CSR_RESET,
	    IWM_CSR_RESET_REG_FLAG_MASTER_DISABLED,
	    IWM_CSR_RESET_REG_FLAG_MASTER_DISABLED, 100))
		device_printf(sc->sc_dev, "timeout waiting for master\n");
	IWM_DPRINTF(sc, IWM_DEBUG_TRANS, "%s: iwm apm stop\n", __func__);
}

/* iwlwifi pcie/trans.c */
int
iwm_start_hw(struct iwm_softc *sc)
{
	int error;

	if ((error = iwm_prepare_card_hw(sc)) != 0)
		return error;

	/* Reset the entire device */
	IWM_WRITE(sc, IWM_CSR_RESET,
	    IWM_CSR_RESET_REG_FLAG_SW_RESET |
	    IWM_CSR_RESET_REG_FLAG_NEVO_RESET);
	DELAY(10);

	if ((error = iwm_apm_init(sc)) != 0)
		return error;

	iwm_enable_rfkill_int(sc);
	iwm_check_rfkill(sc);

	return 0;
}

/* iwlwifi pcie/trans.c (always main power) */
void
iwm_set_pwr(struct iwm_softc *sc)
{
	iwm_set_bits_mask_prph(sc, IWM_APMG_PS_CTRL_REG,
	    IWM_APMG_PS_CTRL_VAL_PWR_SRC_VMAIN, ~IWM_APMG_PS_CTRL_MSK_PWR_SRC);
}

/* iwlwifi pcie/rx.c */
int
iwm_pcie_rx_stop(struct iwm_softc *sc)
{

	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_CONFIG_REG, 0);
	return (iwm_poll_bit(sc, IWM_FH_MEM_RSSR_RX_STATUS_REG,
	    IWM_FH_RSSR_CHNL0_RX_STATUS_CHNL_IDLE,
	    IWM_FH_RSSR_CHNL0_RX_STATUS_CHNL_IDLE,
	    1000));
}
