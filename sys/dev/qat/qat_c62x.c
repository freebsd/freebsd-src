/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_c62x.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

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
 *   Copyright(c) 2014 Intel Corporation.
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
#if 0
__KERNEL_RCSID(0, "$NetBSD: qat_c62x.c,v 1.1 2019/11/20 09:37:46 hikaru Exp $");
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "qatreg.h"
#include "qat_hw17reg.h"
#include "qat_c62xreg.h"
#include "qatvar.h"
#include "qat_hw17var.h"

static uint32_t
qat_c62x_get_accel_mask(struct qat_softc *sc)
{
	uint32_t fusectl, strap;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	strap = pci_read_config(sc->sc_dev, SOFTSTRAP_REG_C62X, 4);

	return (((~(fusectl | strap)) >> ACCEL_REG_OFFSET_C62X) &
	    ACCEL_MASK_C62X);
}

static uint32_t
qat_c62x_get_ae_mask(struct qat_softc *sc)
{
	uint32_t fusectl, me_strap, me_disable, ssms_disabled;

	fusectl = pci_read_config(sc->sc_dev, FUSECTL_REG, 4);
	me_strap = pci_read_config(sc->sc_dev, SOFTSTRAP_REG_C62X, 4);

	/* If SSMs are disabled, then disable the corresponding MEs */
	ssms_disabled = (~qat_c62x_get_accel_mask(sc)) & ACCEL_MASK_C62X;
	me_disable = 0x3;
	while (ssms_disabled) {
		if (ssms_disabled & 1)
			me_strap |= me_disable;
		ssms_disabled >>= 1;
		me_disable <<= 2;
	}

	return (~(fusectl | me_strap)) & AE_MASK_C62X;
}

static enum qat_sku
qat_c62x_get_sku(struct qat_softc *sc)
{
	switch (sc->sc_ae_num) {
	case 8:
		return QAT_SKU_2;
	case MAX_AE_C62X:
		return QAT_SKU_4;
	}

	return QAT_SKU_UNKNOWN;
}

static uint32_t
qat_c62x_get_accel_cap(struct qat_softc *sc)
{
	uint32_t cap, legfuse, strap;

	legfuse = pci_read_config(sc->sc_dev, LEGFUSE_REG, 4);
	strap = pci_read_config(sc->sc_dev, SOFTSTRAP_REG_C62X, 4);

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

	if ((strap | legfuse) & SOFTSTRAP_SS_POWERGATE_PKE_C62X)
		cap &= ~QAT_ACCEL_CAP_CRYPTO_ASYMMETRIC;
	if ((strap | legfuse) & SOFTSTRAP_SS_POWERGATE_CY_C62X)
		cap &= ~QAT_ACCEL_CAP_COMPRESSION;

	return cap;
}

static const char *
qat_c62x_get_fw_uof_name(struct qat_softc *sc)
{

	return AE_FW_UOF_NAME_C62X;
}

static void
qat_c62x_enable_intr(struct qat_softc *sc)
{

	/* Enable bundle and misc interrupts */
	qat_misc_write_4(sc, SMIAPF0_C62X, SMIA0_MASK_C62X);
	qat_misc_write_4(sc, SMIAPF1_C62X, SMIA1_MASK_C62X);
}

/* Worker thread to service arbiter mappings */
static uint32_t thrd_to_arb_map[] = {
	0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA,
	0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA
};

static void
qat_c62x_get_arb_mapping(struct qat_softc *sc, const uint32_t **arb_map_config)
{
	int i;

	for (i = 1; i < MAX_AE_C62X; i++) {
		if ((~sc->sc_ae_mask) & (1 << i))
			thrd_to_arb_map[i] = 0;
	}
	*arb_map_config = thrd_to_arb_map;
}

static void
qat_c62x_enable_error_interrupts(struct qat_softc *sc)
{
	qat_misc_write_4(sc, ERRMSK0, ERRMSK0_CERR_C62X); /* ME0-ME3 */
	qat_misc_write_4(sc, ERRMSK1, ERRMSK1_CERR_C62X); /* ME4-ME7 */
	qat_misc_write_4(sc, ERRMSK4, ERRMSK4_CERR_C62X); /* ME8-ME9 */
	qat_misc_write_4(sc, ERRMSK5, ERRMSK5_CERR_C62X); /* SSM2-SSM4 */

	/* Reset everything except VFtoPF1_16. */
	qat_misc_read_write_and_4(sc, ERRMSK3, VF2PF1_16_C62X);
	/* Disable Secure RAM correctable error interrupt */
	qat_misc_read_write_or_4(sc, ERRMSK3, ERRMSK3_CERR_C62X);

	/* RI CPP bus interface error detection and reporting. */
	qat_misc_write_4(sc, RICPPINTCTL_C62X, RICPP_EN_C62X);

	/* TI CPP bus interface error detection and reporting. */
	qat_misc_write_4(sc, TICPPINTCTL_C62X, TICPP_EN_C62X);

	/* Enable CFC Error interrupts and logging. */
	qat_misc_write_4(sc, CPP_CFC_ERR_CTRL_C62X, CPP_CFC_UE_C62X);

	/* Enable SecureRAM to fix and log Correctable errors */
	qat_misc_write_4(sc, SECRAMCERR_C62X, SECRAM_CERR_C62X);

	/* Enable SecureRAM Uncorrectable error interrupts and logging */
	qat_misc_write_4(sc, SECRAMUERR, SECRAM_UERR_C62X);

	/* Enable Push/Pull Misc Uncorrectable error interrupts and logging */
	qat_misc_write_4(sc, CPPMEMTGTERR, TGT_UERR_C62X);
}

static void
qat_c62x_disable_error_interrupts(struct qat_softc *sc)
{
	/* ME0-ME3 */
	qat_misc_write_4(sc, ERRMSK0, ERRMSK0_UERR_C62X | ERRMSK0_CERR_C62X);
	/* ME4-ME7 */
	qat_misc_write_4(sc, ERRMSK1, ERRMSK1_UERR_C62X | ERRMSK1_CERR_C62X);
	/* Secure RAM, CPP Push Pull, RI, TI, SSM0-SSM1, CFC */
	qat_misc_write_4(sc, ERRMSK3, ERRMSK3_UERR_C62X | ERRMSK3_CERR_C62X);
	/* ME8-ME9 */
	qat_misc_write_4(sc, ERRMSK4, ERRMSK4_UERR_C62X | ERRMSK4_CERR_C62X);
	/* SSM2-SSM4 */
	qat_misc_write_4(sc, ERRMSK5, ERRMSK5_UERR_C62X | ERRMSK5_CERR_C62X);
}

static void
qat_c62x_enable_error_correction(struct qat_softc *sc)
{
	u_int i, mask;

	/* Enable Accel Engine error detection & correction */
	for (i = 0, mask = sc->sc_ae_mask; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		qat_misc_read_write_or_4(sc, AE_CTX_ENABLES_C62X(i),
		    ENABLE_AE_ECC_ERR_C62X);
		qat_misc_read_write_or_4(sc, AE_MISC_CONTROL_C62X(i),
		    ENABLE_AE_ECC_PARITY_CORR_C62X);
	}

	/* Enable shared memory error detection & correction */
	for (i = 0, mask = sc->sc_accel_mask; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		qat_misc_read_write_or_4(sc, UERRSSMSH(i), ERRSSMSH_EN_C62X);
		qat_misc_read_write_or_4(sc, CERRSSMSH(i), ERRSSMSH_EN_C62X);
		qat_misc_read_write_or_4(sc, PPERR(i), PPERR_EN_C62X);
	}

	qat_c62x_enable_error_interrupts(sc);
}

const struct qat_hw qat_hw_c62x = {
	.qhw_sram_bar_id = BAR_SRAM_ID_C62X,
	.qhw_misc_bar_id = BAR_PMISC_ID_C62X,
	.qhw_etr_bar_id = BAR_ETR_ID_C62X,
	.qhw_cap_global_offset = CAP_GLOBAL_OFFSET_C62X,
	.qhw_ae_offset = AE_OFFSET_C62X,
	.qhw_ae_local_offset = AE_LOCAL_OFFSET_C62X,
	.qhw_etr_bundle_size = ETR_BUNDLE_SIZE_C62X,
	.qhw_num_banks = ETR_MAX_BANKS_C62X,
	.qhw_num_rings_per_bank = ETR_MAX_RINGS_PER_BANK,
	.qhw_num_accel = MAX_ACCEL_C62X,
	.qhw_num_engines = MAX_AE_C62X,
	.qhw_tx_rx_gap = ETR_TX_RX_GAP_C62X,
	.qhw_tx_rings_mask = ETR_TX_RINGS_MASK_C62X,
	.qhw_clock_per_sec = CLOCK_PER_SEC_C62X,
	.qhw_fw_auth = true,
	.qhw_fw_req_size = FW_REQ_DEFAULT_SZ_HW17,
	.qhw_fw_resp_size = FW_RESP_DEFAULT_SZ_HW17,
	.qhw_ring_asym_tx = 0,
	.qhw_ring_asym_rx = 8,
	.qhw_ring_sym_tx = 2,
	.qhw_ring_sym_rx = 10,
	.qhw_mof_fwname = AE_FW_MOF_NAME_C62X,
	.qhw_mmp_fwname = AE_FW_MMP_NAME_C62X,
	.qhw_prod_type = AE_FW_PROD_TYPE_C62X,
	.qhw_get_accel_mask = qat_c62x_get_accel_mask,
	.qhw_get_ae_mask = qat_c62x_get_ae_mask,
	.qhw_get_sku = qat_c62x_get_sku,
	.qhw_get_accel_cap = qat_c62x_get_accel_cap,
	.qhw_get_fw_uof_name = qat_c62x_get_fw_uof_name,
	.qhw_enable_intr = qat_c62x_enable_intr,
	.qhw_init_admin_comms = qat_adm_mailbox_init,
	.qhw_send_admin_init = qat_adm_mailbox_send_init,
	.qhw_init_arb = qat_arb_init,
	.qhw_get_arb_mapping = qat_c62x_get_arb_mapping,
	.qhw_enable_error_correction = qat_c62x_enable_error_correction,
	.qhw_disable_error_interrupts = qat_c62x_disable_error_interrupts,
	.qhw_set_ssm_wdtimer = qat_set_ssm_wdtimer,
	.qhw_check_slice_hang = qat_check_slice_hang,
	.qhw_crypto_setup_desc = qat_hw17_crypto_setup_desc,
	.qhw_crypto_setup_req_params = qat_hw17_crypto_setup_req_params,
	.qhw_crypto_opaque_offset = offsetof(struct fw_la_resp, opaque_data),
};
