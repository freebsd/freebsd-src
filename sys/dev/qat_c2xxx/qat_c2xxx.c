/* SPDX-License-Identifier: BSD-2-Clause AND BSD-3-Clause */
/*	$NetBSD: qat_c2xxx.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2007-2013 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__KERNEL_RCSID(0, "$NetBSD: qat_c2xxx.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $");
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "qatreg.h"
#include "qat_hw15reg.h"
#include "qat_c2xxxreg.h"
#include "qatvar.h"
#include "qat_hw15var.h"

static uint32_t
qat_c2xxx_get_accel_mask(struct qat_softc *sc)
{
	uint32_t fusectl;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);

	return ((~fusectl) & ACCEL_MASK_C2XXX);
}

static uint32_t
qat_c2xxx_get_ae_mask(struct qat_softc *sc)
{
	uint32_t fusectl;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	if (fusectl & (
	    FUSECTL_C2XXX_PKE_DISABLE |
	    FUSECTL_C2XXX_ATH_DISABLE |
	    FUSECTL_C2XXX_CPH_DISABLE)) {
		return 0;
	} else {
		if ((~fusectl & AE_MASK_C2XXX) == 0x3) {
			/*
			 * With both AEs enabled we get spurious completions on
			 * ETR rings.  Work around that for now by simply
			 * disabling the second AE.
			 */
			device_printf(sc->sc_dev, "disabling second AE\n");
			fusectl |= 0x2;
		}
		return ((~fusectl) & AE_MASK_C2XXX);
	}
}

static enum qat_sku
qat_c2xxx_get_sku(struct qat_softc *sc)
{
	uint32_t fusectl;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);

	switch (sc->sc_ae_num) {
	case 1:
		if (fusectl & FUSECTL_C2XXX_LOW_SKU)
			return QAT_SKU_3;
		else if (fusectl & FUSECTL_C2XXX_MID_SKU)
			return QAT_SKU_2;
		break;
	case MAX_AE_C2XXX:
		return QAT_SKU_1;
	}

	return QAT_SKU_UNKNOWN;
}

static uint32_t
qat_c2xxx_get_accel_cap(struct qat_softc *sc)
{
	return QAT_ACCEL_CAP_CRYPTO_SYMMETRIC |
	    QAT_ACCEL_CAP_CRYPTO_ASYMMETRIC |
	    QAT_ACCEL_CAP_CIPHER |
	    QAT_ACCEL_CAP_AUTHENTICATION;
}

static const char *
qat_c2xxx_get_fw_uof_name(struct qat_softc *sc)
{
	if (sc->sc_rev < QAT_REVID_C2XXX_B0)
		return AE_FW_UOF_NAME_C2XXX_A0;

	/* QAT_REVID_C2XXX_B0 and QAT_REVID_C2XXX_C0 */
	return AE_FW_UOF_NAME_C2XXX_B0;
}

static void
qat_c2xxx_enable_intr(struct qat_softc *sc)
{

	qat_misc_write_4(sc, EP_SMIA_C2XXX, EP_SMIA_MASK_C2XXX);
}

static void
qat_c2xxx_init_etr_intr(struct qat_softc *sc, int bank)
{
	/*
	 * For now, all rings within the bank are setup such that the generation
	 * of flag interrupts will be triggered when ring leaves the empty
	 * state. Note that in order for the ring interrupt to generate an IRQ
	 * the interrupt must also be enabled for the ring.
	 */
	qat_etr_bank_write_4(sc, bank, ETR_INT_SRCSEL,
	    ETR_INT_SRCSEL_MASK_0_C2XXX);
	qat_etr_bank_write_4(sc, bank, ETR_INT_SRCSEL_2,
	    ETR_INT_SRCSEL_MASK_X_C2XXX);
}

const struct qat_hw qat_hw_c2xxx = {
	.qhw_sram_bar_id = BAR_SRAM_ID_C2XXX,
	.qhw_misc_bar_id = BAR_PMISC_ID_C2XXX,
	.qhw_etr_bar_id = BAR_ETR_ID_C2XXX,
	.qhw_cap_global_offset = CAP_GLOBAL_OFFSET_C2XXX,
	.qhw_ae_offset = AE_OFFSET_C2XXX,
	.qhw_ae_local_offset = AE_LOCAL_OFFSET_C2XXX,
	.qhw_etr_bundle_size = ETR_BUNDLE_SIZE_C2XXX,
	.qhw_num_banks = ETR_MAX_BANKS_C2XXX,
	.qhw_num_ap_banks = ETR_MAX_AP_BANKS_C2XXX,
	.qhw_num_rings_per_bank = ETR_MAX_RINGS_PER_BANK,
	.qhw_num_accel = MAX_ACCEL_C2XXX,
	.qhw_num_engines = MAX_AE_C2XXX,
	.qhw_tx_rx_gap = ETR_TX_RX_GAP_C2XXX,
	.qhw_tx_rings_mask = ETR_TX_RINGS_MASK_C2XXX,
	.qhw_msix_ae_vec_gap = MSIX_AE_VEC_GAP_C2XXX,
	.qhw_fw_auth = false,
	.qhw_fw_req_size = FW_REQ_DEFAULT_SZ_HW15,
	.qhw_fw_resp_size = FW_REQ_DEFAULT_SZ_HW15,
	.qhw_ring_asym_tx = 2,
	.qhw_ring_asym_rx = 3,
	.qhw_ring_sym_tx = 4,
	.qhw_ring_sym_rx = 5,
	.qhw_mof_fwname = AE_FW_MOF_NAME_C2XXX,
	.qhw_mmp_fwname = AE_FW_MMP_NAME_C2XXX,
	.qhw_prod_type = AE_FW_PROD_TYPE_C2XXX,
	.qhw_get_accel_mask = qat_c2xxx_get_accel_mask,
	.qhw_get_ae_mask = qat_c2xxx_get_ae_mask,
	.qhw_get_sku = qat_c2xxx_get_sku,
	.qhw_get_accel_cap = qat_c2xxx_get_accel_cap,
	.qhw_get_fw_uof_name = qat_c2xxx_get_fw_uof_name,
	.qhw_enable_intr = qat_c2xxx_enable_intr,
	.qhw_init_etr_intr = qat_c2xxx_init_etr_intr,
	.qhw_init_admin_comms = qat_adm_ring_init,
	.qhw_send_admin_init = qat_adm_ring_send_init,
	.qhw_crypto_setup_desc = qat_hw15_crypto_setup_desc,
	.qhw_crypto_setup_req_params = qat_hw15_crypto_setup_req_params,
	.qhw_crypto_opaque_offset =
	    offsetof(struct fw_la_resp, comn_resp.opaque_data),
};
