/* SPDX-License-Identifier: BSD-2-Clause AND BSD-3-Clause */
/*
 * Copyright (c) 2020 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

/*
 *   Copyright(c) 2014 - 2020 Intel Corporation.
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "qatreg.h"
#include "qatvar.h"
#include "qat_hw17reg.h"
#include "qat_hw17var.h"
#include "qat_dh895xccreg.h"

static uint32_t
qat_dh895xcc_get_accel_mask(struct qat_softc *sc)
{
	uint32_t fusectl, strap;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	strap = pci_read_config(sc->sc_dev, SOFTSTRAP_REG_DH895XCC, 4);

	return (((~(fusectl | strap)) >> ACCEL_REG_OFFSET_DH895XCC) &
	    ACCEL_MASK_DH895XCC);
}

static uint32_t
qat_dh895xcc_get_ae_mask(struct qat_softc *sc)
{
	uint32_t fusectl, strap;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	strap = pci_read_config(sc->sc_dev, SOFTSTRAP_REG_DH895XCC, 4);

	return (~(fusectl | strap)) & AE_MASK_DH895XCC;
}

static enum qat_sku
qat_dh895xcc_get_sku(struct qat_softc *sc)
{
	uint32_t fusectl, sku;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	sku = (fusectl & FUSECTL_SKU_MASK_DH895XCC) >>
	    FUSECTL_SKU_SHIFT_DH895XCC;
	switch (sku) {
	case FUSECTL_SKU_1_DH895XCC:
		return QAT_SKU_1;
	case FUSECTL_SKU_2_DH895XCC:
		return QAT_SKU_2;
	case FUSECTL_SKU_3_DH895XCC:
		return QAT_SKU_3;
	case FUSECTL_SKU_4_DH895XCC:
		return QAT_SKU_4;
	default:
		return QAT_SKU_UNKNOWN;
	}
}

static uint32_t
qat_dh895xcc_get_accel_cap(struct qat_softc *sc)
{
	uint32_t cap, legfuse;

	legfuse = pci_read_config(sc->sc_dev, LEGFUSE_REG, 4);

	cap = QAT_ACCEL_CAP_CRYPTO_SYMMETRIC +
		QAT_ACCEL_CAP_CRYPTO_ASYMMETRIC +
		QAT_ACCEL_CAP_CIPHER +
		QAT_ACCEL_CAP_AUTHENTICATION +
		QAT_ACCEL_CAP_COMPRESSION +
		QAT_ACCEL_CAP_ZUC +
		QAT_ACCEL_CAP_SHA3;

	if (legfuse & LEGFUSE_ACCEL_MASK_CIPHER_SLICE) {
		cap &= ~QAT_ACCEL_CAP_CRYPTO_SYMMETRIC;
		cap &= ~QAT_ACCEL_CAP_CIPHER;
	}
	if (legfuse & LEGFUSE_ACCEL_MASK_AUTH_SLICE)
		cap &= ~QAT_ACCEL_CAP_AUTHENTICATION;
	if (legfuse & LEGFUSE_ACCEL_MASK_PKE_SLICE)
		cap &= ~QAT_ACCEL_CAP_CRYPTO_ASYMMETRIC;
	if (legfuse & LEGFUSE_ACCEL_MASK_COMPRESS_SLICE)
		cap &= ~QAT_ACCEL_CAP_COMPRESSION;
	if (legfuse & LEGFUSE_ACCEL_MASK_EIA3_SLICE)
		cap &= ~QAT_ACCEL_CAP_ZUC;

	return cap;
}

static const char *
qat_dh895xcc_get_fw_uof_name(struct qat_softc *sc)
{
	return AE_FW_UOF_NAME_DH895XCC;
}

static void
qat_dh895xcc_enable_intr(struct qat_softc *sc)
{
	/* Enable bundle and misc interrupts */
	qat_misc_write_4(sc, SMIAPF0_DH895XCC, SMIA0_MASK_DH895XCC);
	qat_misc_write_4(sc, SMIAPF1_DH895XCC, SMIA1_MASK_DH895XCC);
}

/* Worker thread to service arbiter mappings based on dev SKUs */
static uint32_t thrd_to_arb_map_sku4[] = {
	0x12222AAA, 0x11666666, 0x12222AAA, 0x11666666,
	0x12222AAA, 0x11222222, 0x12222AAA, 0x11222222,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static uint32_t thrd_to_arb_map_sku6[] = {
	0x12222AAA, 0x11666666, 0x12222AAA, 0x11666666,
	0x12222AAA, 0x11222222, 0x12222AAA, 0x11222222,
	0x12222AAA, 0x11222222, 0x12222AAA, 0x11222222,
};

static void
qat_dh895xcc_get_arb_mapping(struct qat_softc *sc,
    const uint32_t **arb_map_config)
{
	uint32_t *map, sku;
	int i;

	sku = qat_dh895xcc_get_sku(sc);
	switch (sku) {
	case QAT_SKU_1:
		map = thrd_to_arb_map_sku4;
		break;
	case QAT_SKU_2:
	case QAT_SKU_4:
		map = thrd_to_arb_map_sku6;
		break;
	default:
		*arb_map_config = NULL;
		return;
	}

	for (i = 1; i < MAX_AE_DH895XCC; i++) {
		if ((~sc->sc_ae_mask) & (1 << i))
			map[i] = 0;
	}
	*arb_map_config = map;
}

static void
qat_dh895xcc_enable_error_correction(struct qat_softc *sc)
{
	uint32_t mask;
	u_int i;

	/* Enable Accel Engine error detection & correction */
	for (i = 0, mask = sc->sc_ae_mask; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		qat_misc_read_write_or_4(sc, AE_CTX_ENABLES_DH895XCC(i),
		    ENABLE_AE_ECC_ERR_DH895XCC);
		qat_misc_read_write_or_4(sc, AE_MISC_CONTROL_DH895XCC(i),
		    ENABLE_AE_ECC_PARITY_CORR_DH895XCC);
	}

	/* Enable shared memory error detection & correction */
	for (i = 0, mask = sc->sc_accel_mask; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		qat_misc_read_write_or_4(sc, UERRSSMSH(i), ERRSSMSH_EN_DH895XCC);
		qat_misc_read_write_or_4(sc, CERRSSMSH(i), ERRSSMSH_EN_DH895XCC);
		qat_misc_read_write_or_4(sc, PPERR(i), PPERR_EN_DH895XCC);
	}
}

const struct qat_hw qat_hw_dh895xcc = {
	.qhw_sram_bar_id = BAR_SRAM_ID_DH895XCC,
	.qhw_misc_bar_id = BAR_PMISC_ID_DH895XCC,
	.qhw_etr_bar_id = BAR_ETR_ID_DH895XCC,
	.qhw_cap_global_offset = CAP_GLOBAL_OFFSET_DH895XCC,
	.qhw_ae_offset = AE_OFFSET_DH895XCC,
	.qhw_ae_local_offset = AE_LOCAL_OFFSET_DH895XCC,
	.qhw_etr_bundle_size = ETR_BUNDLE_SIZE_DH895XCC,
	.qhw_num_banks = ETR_MAX_BANKS_DH895XCC,
	.qhw_num_rings_per_bank = ETR_MAX_RINGS_PER_BANK,
	.qhw_num_accel = MAX_ACCEL_DH895XCC,
	.qhw_num_engines = MAX_AE_DH895XCC,
	.qhw_tx_rx_gap = ETR_TX_RX_GAP_DH895XCC,
	.qhw_tx_rings_mask = ETR_TX_RINGS_MASK_DH895XCC,
	.qhw_clock_per_sec = CLOCK_PER_SEC_DH895XCC,
	.qhw_fw_auth = false,
	.qhw_fw_req_size = FW_REQ_DEFAULT_SZ_HW17,
	.qhw_fw_resp_size = FW_RESP_DEFAULT_SZ_HW17,
	.qhw_ring_asym_tx = 0,
	.qhw_ring_asym_rx = 8,
	.qhw_ring_sym_tx = 2,
	.qhw_ring_sym_rx = 10,
	.qhw_mof_fwname = AE_FW_MOF_NAME_DH895XCC,
	.qhw_mmp_fwname = AE_FW_MMP_NAME_DH895XCC,
	.qhw_prod_type = AE_FW_PROD_TYPE_DH895XCC,
	.qhw_get_accel_mask = qat_dh895xcc_get_accel_mask,
	.qhw_get_ae_mask = qat_dh895xcc_get_ae_mask,
	.qhw_get_sku = qat_dh895xcc_get_sku,
	.qhw_get_accel_cap = qat_dh895xcc_get_accel_cap,
	.qhw_get_fw_uof_name = qat_dh895xcc_get_fw_uof_name,
	.qhw_enable_intr = qat_dh895xcc_enable_intr,
	.qhw_init_admin_comms = qat_adm_mailbox_init,
	.qhw_send_admin_init = qat_adm_mailbox_send_init,
	.qhw_init_arb = qat_arb_init,
	.qhw_get_arb_mapping = qat_dh895xcc_get_arb_mapping,
	.qhw_enable_error_correction = qat_dh895xcc_enable_error_correction,
	.qhw_check_slice_hang = qat_check_slice_hang,
	.qhw_crypto_setup_desc = qat_hw17_crypto_setup_desc,
	.qhw_crypto_setup_req_params = qat_hw17_crypto_setup_req_params,
	.qhw_crypto_opaque_offset = offsetof(struct fw_la_resp, opaque_data),
};
