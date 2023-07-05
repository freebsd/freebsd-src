/* SPDX-License-Identifier: BSD-2-Clause AND BSD-3-Clause */
/*	$NetBSD: qat.c,v 1.6 2020/06/14 23:23:12 riastradh Exp $	*/

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
 *   Copyright(c) 2007-2019 Intel Corporation. All rights reserved.
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
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(0, "$NetBSD: qat.c,v 1.6 2020/06/14 23:23:12 riastradh Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/md5.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include "cryptodev_if.h"

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "qatreg.h"
#include "qatvar.h"
#include "qat_aevar.h"

extern struct qat_hw qat_hw_c2xxx;

#define PCI_VENDOR_INTEL			0x8086
#define PCI_PRODUCT_INTEL_C2000_IQIA_PHYS	0x1f18

static const struct qat_product {
	uint16_t qatp_vendor;
	uint16_t qatp_product;
	const char *qatp_name;
	enum qat_chip_type qatp_chip;
	const struct qat_hw *qatp_hw;
} qat_products[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_C2000_IQIA_PHYS,
	  "Intel C2000 QuickAssist PF",
	  QAT_CHIP_C2XXX, &qat_hw_c2xxx },
	{ 0, 0, NULL, 0, NULL },
};

/* Hash Algorithm specific structure */

/* SHA1 - 20 bytes - Initialiser state can be found in FIPS stds 180-2 */
static const uint8_t sha1_initial_state[QAT_HASH_SHA1_STATE_SIZE] = {
	0x67, 0x45, 0x23, 0x01,
	0xef, 0xcd, 0xab, 0x89,
	0x98, 0xba, 0xdc, 0xfe,
	0x10, 0x32, 0x54, 0x76,
	0xc3, 0xd2, 0xe1, 0xf0
};

/* SHA 256 - 32 bytes - Initialiser state can be found in FIPS stds 180-2 */
static const uint8_t sha256_initial_state[QAT_HASH_SHA256_STATE_SIZE] = {
	0x6a, 0x09, 0xe6, 0x67,
	0xbb, 0x67, 0xae, 0x85,
	0x3c, 0x6e, 0xf3, 0x72,
	0xa5, 0x4f, 0xf5, 0x3a,
	0x51, 0x0e, 0x52, 0x7f,
	0x9b, 0x05, 0x68, 0x8c,
	0x1f, 0x83, 0xd9, 0xab,
	0x5b, 0xe0, 0xcd, 0x19
};

/* SHA 384 - 64 bytes - Initialiser state can be found in FIPS stds 180-2 */
static const uint8_t sha384_initial_state[QAT_HASH_SHA384_STATE_SIZE] = {
	0xcb, 0xbb, 0x9d, 0x5d, 0xc1, 0x05, 0x9e, 0xd8,
	0x62, 0x9a, 0x29, 0x2a, 0x36, 0x7c, 0xd5, 0x07,
	0x91, 0x59, 0x01, 0x5a, 0x30, 0x70, 0xdd, 0x17,
	0x15, 0x2f, 0xec, 0xd8, 0xf7, 0x0e, 0x59, 0x39,
	0x67, 0x33, 0x26, 0x67, 0xff, 0xc0, 0x0b, 0x31,
	0x8e, 0xb4, 0x4a, 0x87, 0x68, 0x58, 0x15, 0x11,
	0xdb, 0x0c, 0x2e, 0x0d, 0x64, 0xf9, 0x8f, 0xa7,
	0x47, 0xb5, 0x48, 0x1d, 0xbe, 0xfa, 0x4f, 0xa4
};

/* SHA 512 - 64 bytes - Initialiser state can be found in FIPS stds 180-2 */
static const uint8_t sha512_initial_state[QAT_HASH_SHA512_STATE_SIZE] = {
	0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08,
	0xbb, 0x67, 0xae, 0x85, 0x84, 0xca, 0xa7, 0x3b,
	0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94, 0xf8, 0x2b,
	0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1,
	0x51, 0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1,
	0x9b, 0x05, 0x68, 0x8c, 0x2b, 0x3e, 0x6c, 0x1f,
	0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd, 0x6b,
	0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79
};

static const struct qat_sym_hash_alg_info sha1_info = {
	.qshai_digest_len = QAT_HASH_SHA1_DIGEST_SIZE,
	.qshai_block_len = QAT_HASH_SHA1_BLOCK_SIZE,
	.qshai_state_size = QAT_HASH_SHA1_STATE_SIZE,
	.qshai_init_state = sha1_initial_state,
	.qshai_sah = &auth_hash_hmac_sha1,
	.qshai_state_offset = 0,
	.qshai_state_word = 4,
};

static const struct qat_sym_hash_alg_info sha256_info = {
	.qshai_digest_len = QAT_HASH_SHA256_DIGEST_SIZE,
	.qshai_block_len = QAT_HASH_SHA256_BLOCK_SIZE,
	.qshai_state_size = QAT_HASH_SHA256_STATE_SIZE,
	.qshai_init_state = sha256_initial_state,
	.qshai_sah = &auth_hash_hmac_sha2_256,
	.qshai_state_offset = offsetof(SHA256_CTX, state),
	.qshai_state_word = 4,
};

static const struct qat_sym_hash_alg_info sha384_info = {
	.qshai_digest_len = QAT_HASH_SHA384_DIGEST_SIZE,
	.qshai_block_len = QAT_HASH_SHA384_BLOCK_SIZE,
	.qshai_state_size = QAT_HASH_SHA384_STATE_SIZE,
	.qshai_init_state = sha384_initial_state,
	.qshai_sah = &auth_hash_hmac_sha2_384,
	.qshai_state_offset = offsetof(SHA384_CTX, state),
	.qshai_state_word = 8,
};

static const struct qat_sym_hash_alg_info sha512_info = {
	.qshai_digest_len = QAT_HASH_SHA512_DIGEST_SIZE,
	.qshai_block_len = QAT_HASH_SHA512_BLOCK_SIZE,
	.qshai_state_size = QAT_HASH_SHA512_STATE_SIZE,
	.qshai_init_state = sha512_initial_state,
	.qshai_sah = &auth_hash_hmac_sha2_512,
	.qshai_state_offset = offsetof(SHA512_CTX, state),
	.qshai_state_word = 8,
};

static const struct qat_sym_hash_alg_info aes_gcm_info = {
	.qshai_digest_len = QAT_HASH_AES_GCM_DIGEST_SIZE,
	.qshai_block_len = QAT_HASH_AES_GCM_BLOCK_SIZE,
	.qshai_state_size = QAT_HASH_AES_GCM_STATE_SIZE,
	.qshai_sah = &auth_hash_nist_gmac_aes_128,
};

/* Hash QAT specific structures */

static const struct qat_sym_hash_qat_info sha1_config = {
	.qshqi_algo_enc = HW_AUTH_ALGO_SHA1,
	.qshqi_auth_counter = QAT_HASH_SHA1_BLOCK_SIZE,
	.qshqi_state1_len = HW_SHA1_STATE1_SZ,
	.qshqi_state2_len = HW_SHA1_STATE2_SZ,
};

static const struct qat_sym_hash_qat_info sha256_config = {
	.qshqi_algo_enc = HW_AUTH_ALGO_SHA256,
	.qshqi_auth_counter = QAT_HASH_SHA256_BLOCK_SIZE,
	.qshqi_state1_len = HW_SHA256_STATE1_SZ,
	.qshqi_state2_len = HW_SHA256_STATE2_SZ
};

static const struct qat_sym_hash_qat_info sha384_config = {
	.qshqi_algo_enc = HW_AUTH_ALGO_SHA384,
	.qshqi_auth_counter = QAT_HASH_SHA384_BLOCK_SIZE,
	.qshqi_state1_len = HW_SHA384_STATE1_SZ,
	.qshqi_state2_len = HW_SHA384_STATE2_SZ
};

static const struct qat_sym_hash_qat_info sha512_config = {
	.qshqi_algo_enc = HW_AUTH_ALGO_SHA512,
	.qshqi_auth_counter = QAT_HASH_SHA512_BLOCK_SIZE,
	.qshqi_state1_len = HW_SHA512_STATE1_SZ,
	.qshqi_state2_len = HW_SHA512_STATE2_SZ
};

static const struct qat_sym_hash_qat_info aes_gcm_config = {
	.qshqi_algo_enc = HW_AUTH_ALGO_GALOIS_128,
	.qshqi_auth_counter = QAT_HASH_AES_GCM_BLOCK_SIZE,
	.qshqi_state1_len = HW_GALOIS_128_STATE1_SZ,
	.qshqi_state2_len =
	    HW_GALOIS_H_SZ + HW_GALOIS_LEN_A_SZ + HW_GALOIS_E_CTR0_SZ,
};

static const struct qat_sym_hash_def qat_sym_hash_defs[] = {
	[QAT_SYM_HASH_SHA1] = { &sha1_info, &sha1_config },
	[QAT_SYM_HASH_SHA256] = { &sha256_info, &sha256_config },
	[QAT_SYM_HASH_SHA384] = { &sha384_info, &sha384_config },
	[QAT_SYM_HASH_SHA512] = { &sha512_info, &sha512_config },
	[QAT_SYM_HASH_AES_GCM] = { &aes_gcm_info, &aes_gcm_config },
};

static const struct qat_product *qat_lookup(device_t);
static int	qat_probe(device_t);
static int	qat_attach(device_t);
static int	qat_init(device_t);
static int	qat_start(device_t);
static int	qat_detach(device_t);

static int	qat_newsession(device_t dev, crypto_session_t cses,
		    const struct crypto_session_params *csp);
static void	qat_freesession(device_t dev, crypto_session_t cses);

static int	qat_setup_msix_intr(struct qat_softc *);

static void	qat_etr_init(struct qat_softc *);
static void	qat_etr_deinit(struct qat_softc *);
static void	qat_etr_bank_init(struct qat_softc *, int);
static void	qat_etr_bank_deinit(struct qat_softc *sc, int);

static void	qat_etr_ap_bank_init(struct qat_softc *);
static void	qat_etr_ap_bank_set_ring_mask(uint32_t *, uint32_t, int);
static void	qat_etr_ap_bank_set_ring_dest(struct qat_softc *, uint32_t *,
		    uint32_t, int);
static void	qat_etr_ap_bank_setup_ring(struct qat_softc *,
		    struct qat_ring *);
static int	qat_etr_verify_ring_size(uint32_t, uint32_t);

static int	qat_etr_ring_intr(struct qat_softc *, struct qat_bank *,
		    struct qat_ring *);
static void	qat_etr_bank_intr(void *);

static void	qat_arb_update(struct qat_softc *, struct qat_bank *);

static struct qat_sym_cookie *qat_crypto_alloc_sym_cookie(
		    struct qat_crypto_bank *);
static void	qat_crypto_free_sym_cookie(struct qat_crypto_bank *,
		    struct qat_sym_cookie *);
static int	qat_crypto_setup_ring(struct qat_softc *,
		    struct qat_crypto_bank *);
static int	qat_crypto_bank_init(struct qat_softc *,
		    struct qat_crypto_bank *);
static int	qat_crypto_init(struct qat_softc *);
static void	qat_crypto_deinit(struct qat_softc *);
static int	qat_crypto_start(struct qat_softc *);
static void	qat_crypto_stop(struct qat_softc *);
static int	qat_crypto_sym_rxintr(struct qat_softc *, void *, void *);

static MALLOC_DEFINE(M_QAT, "qat", "Intel QAT driver");

static const struct qat_product *
qat_lookup(device_t dev)
{
	const struct qat_product *qatp;

	for (qatp = qat_products; qatp->qatp_name != NULL; qatp++) {
		if (pci_get_vendor(dev) == qatp->qatp_vendor &&
		    pci_get_device(dev) == qatp->qatp_product)
			return qatp;
	}
	return NULL;
}

static int
qat_probe(device_t dev)
{
	const struct qat_product *prod;

	prod = qat_lookup(dev);
	if (prod != NULL) {
		device_set_desc(dev, prod->qatp_name);
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

static int
qat_attach(device_t dev)
{
	struct qat_softc *sc = device_get_softc(dev);
	const struct qat_product *qatp;
	int bar, count, error, i;

	sc->sc_dev = dev;
	sc->sc_rev = pci_get_revid(dev);
	sc->sc_crypto.qcy_cid = -1;

	qatp = qat_lookup(dev);
	memcpy(&sc->sc_hw, qatp->qatp_hw, sizeof(struct qat_hw));

	/* Determine active accelerators and engines */
	sc->sc_accel_mask = sc->sc_hw.qhw_get_accel_mask(sc);
	sc->sc_ae_mask = sc->sc_hw.qhw_get_ae_mask(sc);

	sc->sc_accel_num = 0;
	for (i = 0; i < sc->sc_hw.qhw_num_accel; i++) {
		if (sc->sc_accel_mask & (1 << i))
			sc->sc_accel_num++;
	}
	sc->sc_ae_num = 0;
	for (i = 0; i < sc->sc_hw.qhw_num_engines; i++) {
		if (sc->sc_ae_mask & (1 << i))
			sc->sc_ae_num++;
	}

	if (!sc->sc_accel_mask || (sc->sc_ae_mask & 0x01) == 0) {
		device_printf(sc->sc_dev, "couldn't find acceleration");
		goto fail;
	}

	MPASS(sc->sc_accel_num <= MAX_NUM_ACCEL);
	MPASS(sc->sc_ae_num <= MAX_NUM_AE);

	/* Determine SKU and capabilities */
	sc->sc_sku = sc->sc_hw.qhw_get_sku(sc);
	sc->sc_accel_cap = sc->sc_hw.qhw_get_accel_cap(sc);
	sc->sc_fw_uof_name = sc->sc_hw.qhw_get_fw_uof_name(sc);

	i = 0;
	if (sc->sc_hw.qhw_sram_bar_id != NO_PCI_REG) {
		MPASS(sc->sc_hw.qhw_sram_bar_id == 0);
		uint32_t fusectl = pci_read_config(dev, FUSECTL_REG, 4);
		/* Skip SRAM BAR */
		i = (fusectl & FUSECTL_MASK) ? 1 : 0;
	}
	for (bar = 0; bar < PCIR_MAX_BAR_0; bar++) {
		uint32_t val = pci_read_config(dev, PCIR_BAR(bar), 4);
		if (val == 0 || !PCI_BAR_MEM(val))
			continue;

		sc->sc_rid[i] = PCIR_BAR(bar);
		sc->sc_res[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &sc->sc_rid[i], RF_ACTIVE);
		if (sc->sc_res[i] == NULL) {
			device_printf(dev, "couldn't map BAR %d\n", bar);
			goto fail;
		}

		sc->sc_csrt[i] = rman_get_bustag(sc->sc_res[i]);
		sc->sc_csrh[i] = rman_get_bushandle(sc->sc_res[i]);

		i++;
		if ((val & PCIM_BAR_MEM_TYPE) == PCIM_BAR_MEM_64)
			bar++;
	}

	pci_enable_busmaster(dev);

	count = sc->sc_hw.qhw_num_banks + 1;
	if (pci_msix_count(dev) < count) {
		device_printf(dev, "insufficient MSI-X vectors (%d vs. %d)\n",
		    pci_msix_count(dev), count);
		goto fail;
	}
	error = pci_alloc_msix(dev, &count);
	if (error != 0) {
		device_printf(dev, "failed to allocate MSI-X vectors\n");
		goto fail;
	}

	error = qat_init(dev);
	if (error == 0)
		return 0;

fail:
	qat_detach(dev);
	return ENXIO;
}

static int
qat_init(device_t dev)
{
	struct qat_softc *sc = device_get_softc(dev);
	int error;

	qat_etr_init(sc);

	if (sc->sc_hw.qhw_init_admin_comms != NULL &&
	    (error = sc->sc_hw.qhw_init_admin_comms(sc)) != 0) {
		device_printf(sc->sc_dev,
		    "Could not initialize admin comms: %d\n", error);
		return error;
	}

	if (sc->sc_hw.qhw_init_arb != NULL &&
	    (error = sc->sc_hw.qhw_init_arb(sc)) != 0) {
		device_printf(sc->sc_dev,
		    "Could not initialize hw arbiter: %d\n", error);
		return error;
	}

	error = qat_ae_init(sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not initialize Acceleration Engine: %d\n", error);
		return error;
	}

	error = qat_aefw_load(sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not load firmware: %d\n", error);
		return error;
	}

	error = qat_setup_msix_intr(sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not setup interrupts: %d\n", error);
		return error;
	}

	sc->sc_hw.qhw_enable_intr(sc);

	error = qat_crypto_init(sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not initialize service: %d\n", error);
		return error;
	}

	if (sc->sc_hw.qhw_enable_error_correction != NULL)
		sc->sc_hw.qhw_enable_error_correction(sc);

	if (sc->sc_hw.qhw_set_ssm_wdtimer != NULL &&
	    (error = sc->sc_hw.qhw_set_ssm_wdtimer(sc)) != 0) {
		device_printf(sc->sc_dev,
		    "Could not initialize watchdog timer: %d\n", error);
		return error;
	}

	error = qat_start(dev);
	if (error) {
		device_printf(sc->sc_dev,
		    "Could not start: %d\n", error);
		return error;
	}

	return 0;
}

static int
qat_start(device_t dev)
{
	struct qat_softc *sc = device_get_softc(dev);
	int error;

	error = qat_ae_start(sc);
	if (error)
		return error;

	if (sc->sc_hw.qhw_send_admin_init != NULL &&
	    (error = sc->sc_hw.qhw_send_admin_init(sc)) != 0) {
		return error;
	}

	error = qat_crypto_start(sc);
	if (error)
		return error;

	return 0;
}

static int
qat_detach(device_t dev)
{
	struct qat_softc *sc;
	int bar, i;

	sc = device_get_softc(dev);

	qat_crypto_stop(sc);
	qat_crypto_deinit(sc);
	qat_aefw_unload(sc);

	if (sc->sc_etr_banks != NULL) {
		for (i = 0; i < sc->sc_hw.qhw_num_banks; i++) {
			struct qat_bank *qb = &sc->sc_etr_banks[i];

			if (qb->qb_ih_cookie != NULL)
				(void)bus_teardown_intr(dev, qb->qb_ih,
				    qb->qb_ih_cookie);
			if (qb->qb_ih != NULL)
				(void)bus_release_resource(dev, SYS_RES_IRQ,
				    i + 1, qb->qb_ih);
		}
	}
	if (sc->sc_ih_cookie != NULL) {
		(void)bus_teardown_intr(dev, sc->sc_ih, sc->sc_ih_cookie);
		sc->sc_ih_cookie = NULL;
	}
	if (sc->sc_ih != NULL) {
		(void)bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_hw.qhw_num_banks + 1, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	pci_release_msi(dev);

	qat_etr_deinit(sc);

	for (bar = 0; bar < MAX_BARS; bar++) {
		if (sc->sc_res[bar] != NULL) {
			(void)bus_release_resource(dev, SYS_RES_MEMORY,
			    sc->sc_rid[bar], sc->sc_res[bar]);
			sc->sc_res[bar] = NULL;
		}
	}

	return 0;
}

void *
qat_alloc_mem(size_t size)
{
	return (malloc(size, M_QAT, M_WAITOK | M_ZERO));
}

void
qat_free_mem(void *ptr)
{
	free(ptr, M_QAT);
}

static void
qat_alloc_dmamem_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct qat_dmamem *qdm;

	if (error != 0)
		return;

	KASSERT(nseg == 1, ("%s: nsegs is %d", __func__, nseg));
	qdm = arg;
	qdm->qdm_dma_seg = segs[0];
}

int
qat_alloc_dmamem(struct qat_softc *sc, struct qat_dmamem *qdm,
    int nseg, bus_size_t size, bus_size_t alignment)
{
	int error;

	KASSERT(qdm->qdm_dma_vaddr == NULL,
	    ("%s: DMA memory descriptor in use", __func__));

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),
	    alignment, 0, 		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR, 		/* highaddr */
	    NULL, NULL, 		/* filter, filterarg */
	    size,			/* maxsize */
	    nseg,			/* nsegments */
	    size,			/* maxsegsize */
	    BUS_DMA_COHERENT,		/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &qdm->qdm_dma_tag);
	if (error != 0)
		return error;

	error = bus_dmamem_alloc(qdm->qdm_dma_tag, &qdm->qdm_dma_vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &qdm->qdm_dma_map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "couldn't allocate dmamem, error = %d\n", error);
		goto fail_0;
	}

	error = bus_dmamap_load(qdm->qdm_dma_tag, qdm->qdm_dma_map,
	    qdm->qdm_dma_vaddr, size, qat_alloc_dmamem_cb, qdm,
	    BUS_DMA_NOWAIT);
	if (error) {
		device_printf(sc->sc_dev,
		    "couldn't load dmamem map, error = %d\n", error);
		goto fail_1;
	}

	return 0;
fail_1:
	bus_dmamem_free(qdm->qdm_dma_tag, qdm->qdm_dma_vaddr, qdm->qdm_dma_map);
fail_0:
	bus_dma_tag_destroy(qdm->qdm_dma_tag);
	return error;
}

void
qat_free_dmamem(struct qat_softc *sc, struct qat_dmamem *qdm)
{
	if (qdm->qdm_dma_tag != NULL) {
		bus_dmamap_unload(qdm->qdm_dma_tag, qdm->qdm_dma_map);
		bus_dmamem_free(qdm->qdm_dma_tag, qdm->qdm_dma_vaddr,
		    qdm->qdm_dma_map);
		bus_dma_tag_destroy(qdm->qdm_dma_tag);
		explicit_bzero(qdm, sizeof(*qdm));
	}
}

static int
qat_setup_msix_intr(struct qat_softc *sc)
{
	device_t dev;
	int error, i, rid;

	dev = sc->sc_dev;

	for (i = 1; i <= sc->sc_hw.qhw_num_banks; i++) {
		struct qat_bank *qb = &sc->sc_etr_banks[i - 1];

		rid = i;
		qb->qb_ih = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_ACTIVE);
		if (qb->qb_ih == NULL) {
			device_printf(dev,
			    "failed to allocate bank intr resource\n");
			return ENXIO;
		}
		error = bus_setup_intr(dev, qb->qb_ih,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, qat_etr_bank_intr, qb,
		    &qb->qb_ih_cookie);
		if (error != 0) {
			device_printf(dev, "failed to set up bank intr\n");
			return error;
		}
		error = bus_bind_intr(dev, qb->qb_ih, (i - 1) % mp_ncpus);
		if (error != 0)
			device_printf(dev, "failed to bind intr %d\n", i);
	}

	rid = i;
	sc->sc_ih = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_ih == NULL)
		return ENXIO;
	error = bus_setup_intr(dev, sc->sc_ih, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, qat_ae_cluster_intr, sc, &sc->sc_ih_cookie);

	return error;
}

static void
qat_etr_init(struct qat_softc *sc)
{
	int i;

	sc->sc_etr_banks = qat_alloc_mem(
	    sizeof(struct qat_bank) * sc->sc_hw.qhw_num_banks);

	for (i = 0; i < sc->sc_hw.qhw_num_banks; i++)
		qat_etr_bank_init(sc, i);

	if (sc->sc_hw.qhw_num_ap_banks) {
		sc->sc_etr_ap_banks = qat_alloc_mem(
		    sizeof(struct qat_ap_bank) * sc->sc_hw.qhw_num_ap_banks);
		qat_etr_ap_bank_init(sc);
	}
}

static void
qat_etr_deinit(struct qat_softc *sc)
{
	int i;

	if (sc->sc_etr_banks != NULL) {
		for (i = 0; i < sc->sc_hw.qhw_num_banks; i++)
			qat_etr_bank_deinit(sc, i);
		qat_free_mem(sc->sc_etr_banks);
		sc->sc_etr_banks = NULL;
	}
	if (sc->sc_etr_ap_banks != NULL) {
		qat_free_mem(sc->sc_etr_ap_banks);
		sc->sc_etr_ap_banks = NULL;
	}
}

static void
qat_etr_bank_init(struct qat_softc *sc, int bank)
{
	struct qat_bank *qb = &sc->sc_etr_banks[bank];
	int i, tx_rx_gap = sc->sc_hw.qhw_tx_rx_gap;

	MPASS(bank < sc->sc_hw.qhw_num_banks);

	mtx_init(&qb->qb_bank_mtx, "qb bank", NULL, MTX_DEF);

	qb->qb_sc = sc;
	qb->qb_bank = bank;
	qb->qb_coalescing_time = COALESCING_TIME_INTERVAL_DEFAULT;

	/* Clean CSRs for all rings within the bank */
	for (i = 0; i < sc->sc_hw.qhw_num_rings_per_bank; i++) {
		struct qat_ring *qr = &qb->qb_et_rings[i];

		qat_etr_bank_ring_write_4(sc, bank, i,
		    ETR_RING_CONFIG, 0);
		qat_etr_bank_ring_base_write_8(sc, bank, i, 0);

		if (sc->sc_hw.qhw_tx_rings_mask & (1 << i)) {
			qr->qr_inflight = qat_alloc_mem(sizeof(uint32_t));
		} else if (sc->sc_hw.qhw_tx_rings_mask &
		    (1 << (i - tx_rx_gap))) {
			/* Share inflight counter with rx and tx */
			qr->qr_inflight =
			    qb->qb_et_rings[i - tx_rx_gap].qr_inflight;
		}
	}

	if (sc->sc_hw.qhw_init_etr_intr != NULL) {
		sc->sc_hw.qhw_init_etr_intr(sc, bank);
	} else {
		/* common code in qat 1.7 */
		qat_etr_bank_write_4(sc, bank, ETR_INT_REG,
		    ETR_INT_REG_CLEAR_MASK);
		for (i = 0; i < sc->sc_hw.qhw_num_rings_per_bank /
		    ETR_RINGS_PER_INT_SRCSEL; i++) {
			qat_etr_bank_write_4(sc, bank, ETR_INT_SRCSEL +
			    (i * ETR_INT_SRCSEL_NEXT_OFFSET),
			    ETR_INT_SRCSEL_MASK);
		}
	}
}

static void
qat_etr_bank_deinit(struct qat_softc *sc, int bank)
{
	struct qat_bank *qb;
	struct qat_ring *qr;
	int i;

	qb = &sc->sc_etr_banks[bank];
	for (i = 0; i < sc->sc_hw.qhw_num_rings_per_bank; i++) {
		if (sc->sc_hw.qhw_tx_rings_mask & (1 << i)) {
			qr = &qb->qb_et_rings[i];
			qat_free_mem(qr->qr_inflight);
		}
	}
}

static void
qat_etr_ap_bank_init(struct qat_softc *sc)
{
	int ap_bank;

	for (ap_bank = 0; ap_bank < sc->sc_hw.qhw_num_ap_banks; ap_bank++) {
		struct qat_ap_bank *qab = &sc->sc_etr_ap_banks[ap_bank];

		qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NF_MASK,
		    ETR_AP_NF_MASK_INIT);
		qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NF_DEST, 0);
		qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NE_MASK,
		    ETR_AP_NE_MASK_INIT);
		qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NE_DEST, 0);

		memset(qab, 0, sizeof(*qab));
	}
}

static void
qat_etr_ap_bank_set_ring_mask(uint32_t *ap_mask, uint32_t ring, int set_mask)
{
	if (set_mask)
		*ap_mask |= (1 << ETR_RING_NUMBER_IN_AP_BANK(ring));
	else
		*ap_mask &= ~(1 << ETR_RING_NUMBER_IN_AP_BANK(ring));
}

static void
qat_etr_ap_bank_set_ring_dest(struct qat_softc *sc, uint32_t *ap_dest,
    uint32_t ring, int set_dest)
{
	uint32_t ae_mask;
	uint8_t mailbox, ae, nae;
	uint8_t *dest = (uint8_t *)ap_dest;

	mailbox = ETR_RING_AP_MAILBOX_NUMBER(ring);

	nae = 0;
	ae_mask = sc->sc_ae_mask;
	for (ae = 0; ae < sc->sc_hw.qhw_num_engines; ae++) {
		if ((ae_mask & (1 << ae)) == 0)
			continue;

		if (set_dest) {
			dest[nae] = __SHIFTIN(ae, ETR_AP_DEST_AE) |
			    __SHIFTIN(mailbox, ETR_AP_DEST_MAILBOX) |
			    ETR_AP_DEST_ENABLE;
		} else {
			dest[nae] = 0;
		}
		nae++;
		if (nae == ETR_MAX_AE_PER_MAILBOX)
			break;
	}
}

static void
qat_etr_ap_bank_setup_ring(struct qat_softc *sc, struct qat_ring *qr)
{
	struct qat_ap_bank *qab;
	int ap_bank;

	if (sc->sc_hw.qhw_num_ap_banks == 0)
		return;

	ap_bank = ETR_RING_AP_BANK_NUMBER(qr->qr_ring);
	MPASS(ap_bank < sc->sc_hw.qhw_num_ap_banks);
	qab = &sc->sc_etr_ap_banks[ap_bank];

	if (qr->qr_cb == NULL) {
		qat_etr_ap_bank_set_ring_mask(&qab->qab_ne_mask, qr->qr_ring, 1);
		if (!qab->qab_ne_dest) {
			qat_etr_ap_bank_set_ring_dest(sc, &qab->qab_ne_dest,
			    qr->qr_ring, 1);
			qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NE_DEST,
			    qab->qab_ne_dest);
		}
	} else {
		qat_etr_ap_bank_set_ring_mask(&qab->qab_nf_mask, qr->qr_ring, 1);
		if (!qab->qab_nf_dest) {
			qat_etr_ap_bank_set_ring_dest(sc, &qab->qab_nf_dest,
			    qr->qr_ring, 1);
			qat_etr_ap_bank_write_4(sc, ap_bank, ETR_AP_NF_DEST,
			    qab->qab_nf_dest);
		}
	}
}

static int
qat_etr_verify_ring_size(uint32_t msg_size, uint32_t num_msgs)
{
	int i = QAT_MIN_RING_SIZE;

	for (; i <= QAT_MAX_RING_SIZE; i++)
		if ((msg_size * num_msgs) == QAT_SIZE_TO_RING_SIZE_IN_BYTES(i))
			return i;

	return QAT_DEFAULT_RING_SIZE;
}

int
qat_etr_setup_ring(struct qat_softc *sc, int bank, uint32_t ring,
    uint32_t num_msgs, uint32_t msg_size, qat_cb_t cb, void *cb_arg,
    const char *name, struct qat_ring **rqr)
{
	struct qat_bank *qb;
	struct qat_ring *qr = NULL;
	int error;
	uint32_t ring_size_bytes, ring_config;
	uint64_t ring_base;
	uint32_t wm_nf = ETR_RING_CONFIG_NEAR_WM_512;
	uint32_t wm_ne = ETR_RING_CONFIG_NEAR_WM_0;

	MPASS(bank < sc->sc_hw.qhw_num_banks);

	/* Allocate a ring from specified bank */
	qb = &sc->sc_etr_banks[bank];

	if (ring >= sc->sc_hw.qhw_num_rings_per_bank)
		return EINVAL;
	if (qb->qb_allocated_rings & (1 << ring))
		return ENOENT;
	qr = &qb->qb_et_rings[ring];
	qb->qb_allocated_rings |= 1 << ring;

	/* Initialize allocated ring */
	qr->qr_ring = ring;
	qr->qr_bank = bank;
	qr->qr_name = name;
	qr->qr_ring_id = qr->qr_bank * sc->sc_hw.qhw_num_rings_per_bank + ring;
	qr->qr_ring_mask = (1 << ring);
	qr->qr_cb = cb;
	qr->qr_cb_arg = cb_arg;

	/* Setup the shadow variables */
	qr->qr_head = 0;
	qr->qr_tail = 0;
	qr->qr_msg_size = QAT_BYTES_TO_MSG_SIZE(msg_size);
	qr->qr_ring_size = qat_etr_verify_ring_size(msg_size, num_msgs);

	/*
	 * To make sure that ring is alligned to ring size allocate
	 * at least 4k and then tell the user it is smaller.
	 */
	ring_size_bytes = QAT_SIZE_TO_RING_SIZE_IN_BYTES(qr->qr_ring_size);
	ring_size_bytes = QAT_RING_SIZE_BYTES_MIN(ring_size_bytes);
	error = qat_alloc_dmamem(sc, &qr->qr_dma, 1, ring_size_bytes,
	    ring_size_bytes);
	if (error)
		return error;

	qr->qr_ring_vaddr = qr->qr_dma.qdm_dma_vaddr;
	qr->qr_ring_paddr = qr->qr_dma.qdm_dma_seg.ds_addr;

	memset(qr->qr_ring_vaddr, QAT_RING_PATTERN,
	    qr->qr_dma.qdm_dma_seg.ds_len);

	bus_dmamap_sync(qr->qr_dma.qdm_dma_tag, qr->qr_dma.qdm_dma_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	if (cb == NULL) {
		ring_config = ETR_RING_CONFIG_BUILD(qr->qr_ring_size);
	} else {
		ring_config =
		    ETR_RING_CONFIG_BUILD_RESP(qr->qr_ring_size, wm_nf, wm_ne);
	}
	qat_etr_bank_ring_write_4(sc, bank, ring, ETR_RING_CONFIG, ring_config);

	ring_base = ETR_RING_BASE_BUILD(qr->qr_ring_paddr, qr->qr_ring_size);
	qat_etr_bank_ring_base_write_8(sc, bank, ring, ring_base);

	if (sc->sc_hw.qhw_init_arb != NULL)
		qat_arb_update(sc, qb);

	mtx_init(&qr->qr_ring_mtx, "qr ring", NULL, MTX_DEF);

	qat_etr_ap_bank_setup_ring(sc, qr);

	if (cb != NULL) {
		uint32_t intr_mask;

		qb->qb_intr_mask |= qr->qr_ring_mask;
		intr_mask = qb->qb_intr_mask;

		qat_etr_bank_write_4(sc, bank, ETR_INT_COL_EN, intr_mask);
		qat_etr_bank_write_4(sc, bank, ETR_INT_COL_CTL,
		    ETR_INT_COL_CTL_ENABLE | qb->qb_coalescing_time);
	}

	*rqr = qr;

	return 0;
}

static inline u_int
qat_modulo(u_int data, u_int shift)
{
	u_int div = data >> shift;
	u_int mult = div << shift;
	return data - mult;
}

int
qat_etr_put_msg(struct qat_softc *sc, struct qat_ring *qr, uint32_t *msg)
{
	uint32_t inflight;
	uint32_t *addr;

	mtx_lock(&qr->qr_ring_mtx);

	inflight = atomic_fetchadd_32(qr->qr_inflight, 1) + 1;
	if (inflight > QAT_MAX_INFLIGHTS(qr->qr_ring_size, qr->qr_msg_size)) {
		atomic_subtract_32(qr->qr_inflight, 1);
		qr->qr_need_wakeup = true;
		mtx_unlock(&qr->qr_ring_mtx);
		counter_u64_add(sc->sc_ring_full_restarts, 1);
		return ERESTART;
	}

	addr = (uint32_t *)((uintptr_t)qr->qr_ring_vaddr + qr->qr_tail);

	memcpy(addr, msg, QAT_MSG_SIZE_TO_BYTES(qr->qr_msg_size));

	bus_dmamap_sync(qr->qr_dma.qdm_dma_tag, qr->qr_dma.qdm_dma_map,
	    BUS_DMASYNC_PREWRITE);

	qr->qr_tail = qat_modulo(qr->qr_tail +
	    QAT_MSG_SIZE_TO_BYTES(qr->qr_msg_size),
	    QAT_RING_SIZE_MODULO(qr->qr_ring_size));

	qat_etr_bank_ring_write_4(sc, qr->qr_bank, qr->qr_ring,
	    ETR_RING_TAIL_OFFSET, qr->qr_tail);

	mtx_unlock(&qr->qr_ring_mtx);

	return 0;
}

static int
qat_etr_ring_intr(struct qat_softc *sc, struct qat_bank *qb,
    struct qat_ring *qr)
{
	uint32_t *msg, nmsg = 0;
	int handled = 0;
	bool blocked = false;

	mtx_lock(&qr->qr_ring_mtx);

	msg = (uint32_t *)((uintptr_t)qr->qr_ring_vaddr + qr->qr_head);

	bus_dmamap_sync(qr->qr_dma.qdm_dma_tag, qr->qr_dma.qdm_dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (atomic_load_32(msg) != ETR_RING_EMPTY_ENTRY_SIG) {
		atomic_subtract_32(qr->qr_inflight, 1);

		if (qr->qr_cb != NULL) {
			mtx_unlock(&qr->qr_ring_mtx);
			handled |= qr->qr_cb(sc, qr->qr_cb_arg, msg);
			mtx_lock(&qr->qr_ring_mtx);
		}

		atomic_store_32(msg, ETR_RING_EMPTY_ENTRY_SIG);

		qr->qr_head = qat_modulo(qr->qr_head +
		    QAT_MSG_SIZE_TO_BYTES(qr->qr_msg_size),
		    QAT_RING_SIZE_MODULO(qr->qr_ring_size));
		nmsg++;

		msg = (uint32_t *)((uintptr_t)qr->qr_ring_vaddr + qr->qr_head);
	}

	bus_dmamap_sync(qr->qr_dma.qdm_dma_tag, qr->qr_dma.qdm_dma_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	if (nmsg > 0) {
		qat_etr_bank_ring_write_4(sc, qr->qr_bank, qr->qr_ring,
		    ETR_RING_HEAD_OFFSET, qr->qr_head);
		if (qr->qr_need_wakeup) {
			blocked = true;
			qr->qr_need_wakeup = false;
		}
	}

	mtx_unlock(&qr->qr_ring_mtx);

	if (blocked)
		crypto_unblock(sc->sc_crypto.qcy_cid, CRYPTO_SYMQ);

	return handled;
}

static void
qat_etr_bank_intr(void *arg)
{
	struct qat_bank *qb = arg;
	struct qat_softc *sc = qb->qb_sc;
	uint32_t estat;
	int i;

	mtx_lock(&qb->qb_bank_mtx);

	qat_etr_bank_write_4(sc, qb->qb_bank, ETR_INT_COL_CTL, 0);

	/* Now handle all the responses */
	estat = ~qat_etr_bank_read_4(sc, qb->qb_bank, ETR_E_STAT);
	estat &= qb->qb_intr_mask;

	qat_etr_bank_write_4(sc, qb->qb_bank, ETR_INT_COL_CTL,
	    ETR_INT_COL_CTL_ENABLE | qb->qb_coalescing_time);

	mtx_unlock(&qb->qb_bank_mtx);

	while ((i = ffs(estat)) != 0) {
		struct qat_ring *qr = &qb->qb_et_rings[--i];
		estat &= ~(1 << i);
		(void)qat_etr_ring_intr(sc, qb, qr);
	}
}

void
qat_arb_update(struct qat_softc *sc, struct qat_bank *qb)
{

	qat_arb_ringsrvarben_write_4(sc, qb->qb_bank,
	    qb->qb_allocated_rings & 0xff);
}

static struct qat_sym_cookie *
qat_crypto_alloc_sym_cookie(struct qat_crypto_bank *qcb)
{
	struct qat_sym_cookie *qsc;

	mtx_lock(&qcb->qcb_bank_mtx);

	if (qcb->qcb_symck_free_count == 0) {
		mtx_unlock(&qcb->qcb_bank_mtx);
		return NULL;
	}

	qsc = qcb->qcb_symck_free[--qcb->qcb_symck_free_count];

	mtx_unlock(&qcb->qcb_bank_mtx);

	return qsc;
}

static void
qat_crypto_free_sym_cookie(struct qat_crypto_bank *qcb,
    struct qat_sym_cookie *qsc)
{
	explicit_bzero(qsc->qsc_iv_buf, EALG_MAX_BLOCK_LEN);
	explicit_bzero(qsc->qsc_auth_res, QAT_SYM_HASH_BUFFER_LEN);

	mtx_lock(&qcb->qcb_bank_mtx);
	qcb->qcb_symck_free[qcb->qcb_symck_free_count++] = qsc;
	mtx_unlock(&qcb->qcb_bank_mtx);
}

void
qat_memcpy_htobe64(void *dst, const void *src, size_t len)
{
	uint64_t *dst0 = dst;
	const uint64_t *src0 = src;
	size_t i;

	MPASS(len % sizeof(*dst0) == 0);

	for (i = 0; i < len / sizeof(*dst0); i++)
		*(dst0 + i) = htobe64(*(src0 + i));
}

void
qat_memcpy_htobe32(void *dst, const void *src, size_t len)
{
	uint32_t *dst0 = dst;
	const uint32_t *src0 = src;
	size_t i;

	MPASS(len % sizeof(*dst0) == 0);

	for (i = 0; i < len / sizeof(*dst0); i++)
		*(dst0 + i) = htobe32(*(src0 + i));
}

void
qat_memcpy_htobe(void *dst, const void *src, size_t len, uint32_t wordbyte)
{
	switch (wordbyte) {
	case 4:
		qat_memcpy_htobe32(dst, src, len);
		break;
	case 8:
		qat_memcpy_htobe64(dst, src, len);
		break;
	default:
		panic("invalid word size %u", wordbyte);
	}
}

void
qat_crypto_gmac_precompute(const struct qat_crypto_desc *desc,
    const uint8_t *key, int klen, const struct qat_sym_hash_def *hash_def,
    uint8_t *state)
{
	uint32_t ks[4 * (RIJNDAEL_MAXNR + 1)];
	char zeros[AES_BLOCK_LEN];
	int rounds;

	memset(zeros, 0, sizeof(zeros));
	rounds = rijndaelKeySetupEnc(ks, key, klen * NBBY);
	rijndaelEncrypt(ks, rounds, zeros, state);
	explicit_bzero(ks, sizeof(ks));
}

void
qat_crypto_hmac_precompute(const struct qat_crypto_desc *desc,
    const uint8_t *key, int klen, const struct qat_sym_hash_def *hash_def,
    uint8_t *state1, uint8_t *state2)
{
	union authctx ctx;
	const struct auth_hash *sah = hash_def->qshd_alg->qshai_sah;
	uint32_t state_offset = hash_def->qshd_alg->qshai_state_offset;
	uint32_t state_size = hash_def->qshd_alg->qshai_state_size;
	uint32_t state_word = hash_def->qshd_alg->qshai_state_word;

	hmac_init_ipad(sah, key, klen, &ctx);
	qat_memcpy_htobe(state1, (uint8_t *)&ctx + state_offset, state_size,
	    state_word);
	hmac_init_opad(sah, key, klen, &ctx);
	qat_memcpy_htobe(state2, (uint8_t *)&ctx + state_offset, state_size,
	    state_word);
	explicit_bzero(&ctx, sizeof(ctx));
}

static enum hw_cipher_algo
qat_aes_cipher_algo(int klen)
{
	switch (klen) {
	case HW_AES_128_KEY_SZ:
		return HW_CIPHER_ALGO_AES128;
	case HW_AES_192_KEY_SZ:
		return HW_CIPHER_ALGO_AES192;
	case HW_AES_256_KEY_SZ:
		return HW_CIPHER_ALGO_AES256;
	default:
		panic("invalid key length %d", klen);
	}
}

uint16_t
qat_crypto_load_cipher_session(const struct qat_crypto_desc *desc,
    const struct qat_session *qs)
{
	enum hw_cipher_algo algo;
	enum hw_cipher_dir dir;
	enum hw_cipher_convert key_convert;
	enum hw_cipher_mode mode;

	dir = desc->qcd_cipher_dir;
	key_convert = HW_CIPHER_NO_CONVERT;
	mode = qs->qs_cipher_mode;
	switch (mode) {
	case HW_CIPHER_CBC_MODE:
	case HW_CIPHER_XTS_MODE:
		algo = qs->qs_cipher_algo;

		/*
		 * AES decrypt key needs to be reversed.
		 * Instead of reversing the key at session registration,
		 * it is instead reversed on-the-fly by setting the KEY_CONVERT
		 * bit here.
		 */
		if (desc->qcd_cipher_dir == HW_CIPHER_DECRYPT)
			key_convert = HW_CIPHER_KEY_CONVERT;
		break;
	case HW_CIPHER_CTR_MODE:
		algo = qs->qs_cipher_algo;
		dir = HW_CIPHER_ENCRYPT;
		break;
	default:
		panic("unhandled cipher mode %d", mode);
		break;
	}

	return HW_CIPHER_CONFIG_BUILD(mode, algo, key_convert, dir);
}

uint16_t
qat_crypto_load_auth_session(const struct qat_crypto_desc *desc,
    const struct qat_session *qs, const struct qat_sym_hash_def **hash_def)
{
	enum qat_sym_hash_algorithm algo;

	switch (qs->qs_auth_algo) {
	case HW_AUTH_ALGO_SHA1:
		algo = QAT_SYM_HASH_SHA1;
		break;
	case HW_AUTH_ALGO_SHA256:
		algo = QAT_SYM_HASH_SHA256;
		break;
	case HW_AUTH_ALGO_SHA384:
		algo = QAT_SYM_HASH_SHA384;
		break;
	case HW_AUTH_ALGO_SHA512:
		algo = QAT_SYM_HASH_SHA512;
		break;
	case HW_AUTH_ALGO_GALOIS_128:
		algo = QAT_SYM_HASH_AES_GCM;
		break;
	default:
		panic("unhandled auth algorithm %d", qs->qs_auth_algo);
		break;
	}
	*hash_def = &qat_sym_hash_defs[algo];

	return HW_AUTH_CONFIG_BUILD(qs->qs_auth_mode,
	    (*hash_def)->qshd_qat->qshqi_algo_enc,
	    (*hash_def)->qshd_alg->qshai_digest_len);
}

struct qat_crypto_load_cb_arg {
	struct qat_session	*qs;
	struct qat_sym_cookie	*qsc;
	struct cryptop		*crp;
	int			error;
};

static int
qat_crypto_populate_buf_list(struct buffer_list_desc *buffers,
    bus_dma_segment_t *segs, int niseg, int noseg, int skip)
{
	struct flat_buffer_desc *flatbuf;
	bus_addr_t addr;
	bus_size_t len;
	int iseg, oseg;

	for (iseg = 0, oseg = noseg; iseg < niseg && oseg < QAT_MAXSEG;
	    iseg++) {
		addr = segs[iseg].ds_addr;
		len = segs[iseg].ds_len;

		if (skip > 0) {
			if (skip < len) {
				addr += skip;
				len -= skip;
				skip = 0;
			} else {
				skip -= len;
				continue;
			}
		}

		flatbuf = &buffers->flat_bufs[oseg++];
		flatbuf->data_len_in_bytes = (uint32_t)len;
		flatbuf->phy_buffer = (uint64_t)addr;
	}
	buffers->num_buffers = oseg;
	return iseg < niseg ? E2BIG : 0;
}

static void
qat_crypto_load_aadbuf_cb(void *_arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct qat_crypto_load_cb_arg *arg;
	struct qat_sym_cookie *qsc;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	qsc = arg->qsc;
	arg->error = qat_crypto_populate_buf_list(&qsc->qsc_buf_list, segs,
	    nseg, 0, 0);
}

static void
qat_crypto_load_buf_cb(void *_arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct cryptop *crp;
	struct qat_crypto_load_cb_arg *arg;
	struct qat_session *qs;
	struct qat_sym_cookie *qsc;
	int noseg, skip;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	crp = arg->crp;
	qs = arg->qs;
	qsc = arg->qsc;

	if (qs->qs_auth_algo == HW_AUTH_ALGO_GALOIS_128) {
		/* AAD was handled in qat_crypto_load(). */
		skip = crp->crp_payload_start;
		noseg = 0;
	} else if (crp->crp_aad == NULL && crp->crp_aad_length > 0) {
		skip = crp->crp_aad_start;
		noseg = 0;
	} else {
		skip = crp->crp_payload_start;
		noseg = crp->crp_aad == NULL ?
		    0 : qsc->qsc_buf_list.num_buffers;
	}
	arg->error = qat_crypto_populate_buf_list(&qsc->qsc_buf_list, segs,
	    nseg, noseg, skip);
}

static void
qat_crypto_load_obuf_cb(void *_arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct buffer_list_desc *ibufs, *obufs;
	struct flat_buffer_desc *ibuf, *obuf;
	struct cryptop *crp;
	struct qat_crypto_load_cb_arg *arg;
	struct qat_session *qs;
	struct qat_sym_cookie *qsc;
	int buflen, osegs, tocopy;

	arg = _arg;
	if (error != 0) {
		arg->error = error;
		return;
	}

	crp = arg->crp;
	qs = arg->qs;
	qsc = arg->qsc;

	/*
	 * The payload must start at the same offset in the output SG list as in
	 * the input SG list.  Copy over SG entries from the input corresponding
	 * to the AAD buffer.
	 */
	osegs = 0;
	if (qs->qs_auth_algo != HW_AUTH_ALGO_GALOIS_128 &&
	    crp->crp_aad_length > 0) {
		tocopy = crp->crp_aad == NULL ?
		    crp->crp_payload_start - crp->crp_aad_start :
		    crp->crp_aad_length;

		ibufs = &qsc->qsc_buf_list;
		obufs = &qsc->qsc_obuf_list;
		for (; osegs < ibufs->num_buffers && tocopy > 0; osegs++) {
			ibuf = &ibufs->flat_bufs[osegs];
			obuf = &obufs->flat_bufs[osegs];

			obuf->phy_buffer = ibuf->phy_buffer;
			buflen = imin(ibuf->data_len_in_bytes, tocopy);
			obuf->data_len_in_bytes = buflen;
			tocopy -= buflen;
		}
	}

	arg->error = qat_crypto_populate_buf_list(&qsc->qsc_obuf_list, segs,
	    nseg, osegs, crp->crp_payload_output_start);
}

static int
qat_crypto_load(struct qat_session *qs, struct qat_sym_cookie *qsc,
    struct qat_crypto_desc const *desc, struct cryptop *crp)
{
	struct qat_crypto_load_cb_arg arg;
	int error;

	crypto_read_iv(crp, qsc->qsc_iv_buf);

	arg.crp = crp;
	arg.qs = qs;
	arg.qsc = qsc;
	arg.error = 0;

	error = 0;
	if (qs->qs_auth_algo == HW_AUTH_ALGO_GALOIS_128 &&
	    crp->crp_aad_length > 0) {
		/*
		 * The firmware expects AAD to be in a contiguous buffer and
		 * padded to a multiple of 16 bytes.  To satisfy these
		 * constraints we bounce the AAD into a per-request buffer.
		 * There is a small limit on the AAD size so this is not too
		 * onerous.
		 */
		memset(qsc->qsc_gcm_aad, 0, QAT_GCM_AAD_SIZE_MAX);
		if (crp->crp_aad == NULL) {
			crypto_copydata(crp, crp->crp_aad_start,
			    crp->crp_aad_length, qsc->qsc_gcm_aad);
		} else {
			memcpy(qsc->qsc_gcm_aad, crp->crp_aad,
			    crp->crp_aad_length);
		}
	} else if (crp->crp_aad != NULL) {
		error = bus_dmamap_load(
		    qsc->qsc_dma[QAT_SYM_DMA_AADBUF].qsd_dma_tag,
		    qsc->qsc_dma[QAT_SYM_DMA_AADBUF].qsd_dmamap,
		    crp->crp_aad, crp->crp_aad_length,
		    qat_crypto_load_aadbuf_cb, &arg, BUS_DMA_NOWAIT);
		if (error == 0)
			error = arg.error;
	}
	if (error == 0) {
		error = bus_dmamap_load_crp_buffer(
		    qsc->qsc_dma[QAT_SYM_DMA_BUF].qsd_dma_tag,
		    qsc->qsc_dma[QAT_SYM_DMA_BUF].qsd_dmamap,
		    &crp->crp_buf, qat_crypto_load_buf_cb, &arg,
		    BUS_DMA_NOWAIT);
		if (error == 0)
			error = arg.error;
	}
	if (error == 0 && CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		error = bus_dmamap_load_crp_buffer(
		    qsc->qsc_dma[QAT_SYM_DMA_OBUF].qsd_dma_tag,
		    qsc->qsc_dma[QAT_SYM_DMA_OBUF].qsd_dmamap,
		    &crp->crp_obuf, qat_crypto_load_obuf_cb, &arg,
		    BUS_DMA_NOWAIT);
		if (error == 0)
			error = arg.error;
	}
	return error;
}

static inline struct qat_crypto_bank *
qat_crypto_select_bank(struct qat_crypto *qcy)
{
	u_int cpuid = PCPU_GET(cpuid);

	return &qcy->qcy_banks[cpuid % qcy->qcy_num_banks];
}

static int
qat_crypto_setup_ring(struct qat_softc *sc, struct qat_crypto_bank *qcb)
{
	char *name;
	int bank, curname, error, i, j;

	bank = qcb->qcb_bank;
	curname = 0;

	name = qcb->qcb_ring_names[curname++];
	snprintf(name, QAT_RING_NAME_SIZE, "bank%d sym_tx", bank);
	error = qat_etr_setup_ring(sc, qcb->qcb_bank,
	    sc->sc_hw.qhw_ring_sym_tx, QAT_NSYMREQ, sc->sc_hw.qhw_fw_req_size,
	    NULL, NULL, name, &qcb->qcb_sym_tx);
	if (error)
		return error;

	name = qcb->qcb_ring_names[curname++];
	snprintf(name, QAT_RING_NAME_SIZE, "bank%d sym_rx", bank);
	error = qat_etr_setup_ring(sc, qcb->qcb_bank,
	    sc->sc_hw.qhw_ring_sym_rx, QAT_NSYMREQ, sc->sc_hw.qhw_fw_resp_size,
	    qat_crypto_sym_rxintr, qcb, name, &qcb->qcb_sym_rx);
	if (error)
		return error;

	for (i = 0; i < QAT_NSYMCOOKIE; i++) {
		struct qat_dmamem *qdm = &qcb->qcb_symck_dmamems[i];
		struct qat_sym_cookie *qsc;

		error = qat_alloc_dmamem(sc, qdm, 1,
		    sizeof(struct qat_sym_cookie), QAT_OPTIMAL_ALIGN);
		if (error)
			return error;

		qsc = qdm->qdm_dma_vaddr;
		qsc->qsc_self_dmamap = qdm->qdm_dma_map;
		qsc->qsc_self_dma_tag = qdm->qdm_dma_tag;
		qsc->qsc_bulk_req_params_buf_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_bulk_cookie.qsbc_req_params_buf);
		qsc->qsc_buffer_list_desc_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_buf_list);
		qsc->qsc_obuffer_list_desc_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_obuf_list);
		qsc->qsc_obuffer_list_desc_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_obuf_list);
		qsc->qsc_iv_buf_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_iv_buf);
		qsc->qsc_auth_res_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_auth_res);
		qsc->qsc_gcm_aad_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_gcm_aad);
		qsc->qsc_content_desc_paddr =
		    qdm->qdm_dma_seg.ds_addr + offsetof(struct qat_sym_cookie,
		    qsc_content_desc);
		qcb->qcb_symck_free[i] = qsc;
		qcb->qcb_symck_free_count++;

		for (j = 0; j < QAT_SYM_DMA_COUNT; j++) {
			error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),
			    1, 0, 		/* alignment, boundary */
			    BUS_SPACE_MAXADDR,	/* lowaddr */
			    BUS_SPACE_MAXADDR, 	/* highaddr */
			    NULL, NULL, 	/* filter, filterarg */
			    QAT_MAXLEN,		/* maxsize */
			    QAT_MAXSEG,		/* nsegments */
			    QAT_MAXLEN,		/* maxsegsize */
			    BUS_DMA_COHERENT,	/* flags */
			    NULL, NULL,		/* lockfunc, lockarg */
			    &qsc->qsc_dma[j].qsd_dma_tag);
			if (error != 0)
				return error;
			error = bus_dmamap_create(qsc->qsc_dma[j].qsd_dma_tag,
			    BUS_DMA_COHERENT, &qsc->qsc_dma[j].qsd_dmamap);
			if (error != 0)
				return error;
		}
	}

	return 0;
}

static int
qat_crypto_bank_init(struct qat_softc *sc, struct qat_crypto_bank *qcb)
{
	mtx_init(&qcb->qcb_bank_mtx, "qcb bank", NULL, MTX_DEF);

	return qat_crypto_setup_ring(sc, qcb);
}

static void
qat_crypto_bank_deinit(struct qat_softc *sc, struct qat_crypto_bank *qcb)
{
	struct qat_dmamem *qdm;
	struct qat_sym_cookie *qsc;
	int i, j;

	for (i = 0; i < QAT_NSYMCOOKIE; i++) {
		qdm = &qcb->qcb_symck_dmamems[i];
		qsc = qcb->qcb_symck_free[i];
		for (j = 0; j < QAT_SYM_DMA_COUNT; j++) {
			bus_dmamap_destroy(qsc->qsc_dma[j].qsd_dma_tag,
			    qsc->qsc_dma[j].qsd_dmamap);
			bus_dma_tag_destroy(qsc->qsc_dma[j].qsd_dma_tag);
		}
		qat_free_dmamem(sc, qdm);
	}
	qat_free_dmamem(sc, &qcb->qcb_sym_tx->qr_dma);
	qat_free_dmamem(sc, &qcb->qcb_sym_rx->qr_dma);

	mtx_destroy(&qcb->qcb_bank_mtx);
}

static int
qat_crypto_init(struct qat_softc *sc)
{
	struct qat_crypto *qcy = &sc->sc_crypto;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;
	int bank, error, num_banks;

	qcy->qcy_sc = sc;

	if (sc->sc_hw.qhw_init_arb != NULL)
		num_banks = imin(mp_ncpus, sc->sc_hw.qhw_num_banks);
	else
		num_banks = sc->sc_ae_num;

	qcy->qcy_num_banks = num_banks;

	qcy->qcy_banks =
	    qat_alloc_mem(sizeof(struct qat_crypto_bank) * num_banks);

	for (bank = 0; bank < num_banks; bank++) {
		struct qat_crypto_bank *qcb = &qcy->qcy_banks[bank];
		qcb->qcb_bank = bank;
		error = qat_crypto_bank_init(sc, qcb);
		if (error)
			return error;
	}

	mtx_init(&qcy->qcy_crypto_mtx, "qcy crypto", NULL, MTX_DEF);

	ctx = device_get_sysctl_ctx(sc->sc_dev);
	oid = device_get_sysctl_tree(sc->sc_dev);
	children = SYSCTL_CHILDREN(oid);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "statistics");
	children = SYSCTL_CHILDREN(oid);

	sc->sc_gcm_aad_restarts = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "gcm_aad_restarts",
	    CTLFLAG_RD, &sc->sc_gcm_aad_restarts,
	    "GCM requests deferred due to AAD size change");
	sc->sc_gcm_aad_updates = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "gcm_aad_updates",
	    CTLFLAG_RD, &sc->sc_gcm_aad_updates,
	    "GCM requests that required session state update");
	sc->sc_ring_full_restarts = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "ring_full",
	    CTLFLAG_RD, &sc->sc_ring_full_restarts,
	    "Requests deferred due to in-flight max reached");
	sc->sc_sym_alloc_failures = counter_u64_alloc(M_WAITOK);
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "sym_alloc_failures",
	    CTLFLAG_RD, &sc->sc_sym_alloc_failures,
	    "Request allocation failures");

	return 0;
}

static void
qat_crypto_deinit(struct qat_softc *sc)
{
	struct qat_crypto *qcy = &sc->sc_crypto;
	struct qat_crypto_bank *qcb;
	int bank;

	counter_u64_free(sc->sc_sym_alloc_failures);
	counter_u64_free(sc->sc_ring_full_restarts);
	counter_u64_free(sc->sc_gcm_aad_updates);
	counter_u64_free(sc->sc_gcm_aad_restarts);

	if (qcy->qcy_banks != NULL) {
		for (bank = 0; bank < qcy->qcy_num_banks; bank++) {
			qcb = &qcy->qcy_banks[bank];
			qat_crypto_bank_deinit(sc, qcb);
		}
		qat_free_mem(qcy->qcy_banks);
		mtx_destroy(&qcy->qcy_crypto_mtx);
	}
}

static int
qat_crypto_start(struct qat_softc *sc)
{
	struct qat_crypto *qcy;

	qcy = &sc->sc_crypto;
	qcy->qcy_cid = crypto_get_driverid(sc->sc_dev,
	    sizeof(struct qat_session), CRYPTOCAP_F_HARDWARE);
	if (qcy->qcy_cid < 0) {
		device_printf(sc->sc_dev,
		    "could not get opencrypto driver id\n");
		return ENOENT;
	}

	return 0;
}

static void
qat_crypto_stop(struct qat_softc *sc)
{
	struct qat_crypto *qcy;

	qcy = &sc->sc_crypto;
	if (qcy->qcy_cid >= 0)
		(void)crypto_unregister_all(qcy->qcy_cid);
}

static void
qat_crypto_sym_dma_unload(struct qat_sym_cookie *qsc, enum qat_sym_dma i)
{
	bus_dmamap_sync(qsc->qsc_dma[i].qsd_dma_tag, qsc->qsc_dma[i].qsd_dmamap,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(qsc->qsc_dma[i].qsd_dma_tag,
	    qsc->qsc_dma[i].qsd_dmamap);
}

static int
qat_crypto_sym_rxintr(struct qat_softc *sc, void *arg, void *msg)
{
	char icv[QAT_SYM_HASH_BUFFER_LEN];
	struct qat_crypto_bank *qcb = arg;
	struct qat_crypto *qcy;
	struct qat_session *qs;
	struct qat_sym_cookie *qsc;
	struct qat_sym_bulk_cookie *qsbc;
	struct cryptop *crp;
	int error;
	uint16_t auth_sz;
	bool blocked;

	qsc = *(void **)((uintptr_t)msg + sc->sc_hw.qhw_crypto_opaque_offset);

	qsbc = &qsc->qsc_bulk_cookie;
	qcy = qsbc->qsbc_crypto;
	qs = qsbc->qsbc_session;
	crp = qsbc->qsbc_cb_tag;

	bus_dmamap_sync(qsc->qsc_self_dma_tag, qsc->qsc_self_dmamap,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (crp->crp_aad != NULL)
		qat_crypto_sym_dma_unload(qsc, QAT_SYM_DMA_AADBUF);
	qat_crypto_sym_dma_unload(qsc, QAT_SYM_DMA_BUF);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp))
		qat_crypto_sym_dma_unload(qsc, QAT_SYM_DMA_OBUF);

	error = 0;
	if ((auth_sz = qs->qs_auth_mlen) != 0) {
		if ((crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) != 0) {
			crypto_copydata(crp, crp->crp_digest_start,
			    auth_sz, icv);
			if (timingsafe_bcmp(icv, qsc->qsc_auth_res,
			    auth_sz) != 0) {
				error = EBADMSG;
			}
		} else {
			crypto_copyback(crp, crp->crp_digest_start,
			    auth_sz, qsc->qsc_auth_res);
		}
	}

	qat_crypto_free_sym_cookie(qcb, qsc);

	blocked = false;
	mtx_lock(&qs->qs_session_mtx);
	MPASS(qs->qs_status & QAT_SESSION_STATUS_ACTIVE);
	qs->qs_inflight--;
	if (__predict_false(qs->qs_need_wakeup && qs->qs_inflight == 0)) {
		blocked = true;
		qs->qs_need_wakeup = false;
	}
	mtx_unlock(&qs->qs_session_mtx);

	crp->crp_etype = error;
	crypto_done(crp);

	if (blocked)
		crypto_unblock(qcy->qcy_cid, CRYPTO_SYMQ);

	return 1;
}

static int
qat_probesession(device_t dev, const struct crypto_session_params *csp)
{
	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
		return EINVAL;

	if (csp->csp_cipher_alg == CRYPTO_AES_XTS &&
	    qat_lookup(dev)->qatp_chip == QAT_CHIP_C2XXX) {
		/*
		 * AES-XTS is not supported by the NanoQAT.
		 */
		return EINVAL;
	}

	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
		case CRYPTO_AES_ICM:
			if (csp->csp_ivlen != AES_BLOCK_LEN)
				return EINVAL;
			break;
		case CRYPTO_AES_XTS:
			if (csp->csp_ivlen != AES_XTS_IV_LEN)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_DIGEST:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512:
		case CRYPTO_SHA2_512_HMAC:
			break;
		case CRYPTO_AES_NIST_GMAC:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			break;
		default:
			return EINVAL;
		}
		break;
	case CSP_MODE_ETA:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
			switch (csp->csp_cipher_alg) {
			case CRYPTO_AES_CBC:
			case CRYPTO_AES_ICM:
				if (csp->csp_ivlen != AES_BLOCK_LEN)
					return EINVAL;
				break;
			case CRYPTO_AES_XTS:
				if (csp->csp_ivlen != AES_XTS_IV_LEN)
					return EINVAL;
				break;
			default:
				return EINVAL;
			}
			break;
		default:
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}

	return CRYPTODEV_PROBE_HARDWARE;
}

static int
qat_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct qat_crypto *qcy;
	struct qat_dmamem *qdm;
	struct qat_session *qs;
	struct qat_softc *sc;
	struct qat_crypto_desc *ddesc, *edesc;
	int error, slices;

	sc = device_get_softc(dev);
	qs = crypto_get_driver_session(cses);
	qcy = &sc->sc_crypto;

	qdm = &qs->qs_desc_mem;
	error = qat_alloc_dmamem(sc, qdm, QAT_MAXSEG,
	    sizeof(struct qat_crypto_desc) * 2, QAT_OPTIMAL_ALIGN);
	if (error != 0)
		return error;

	mtx_init(&qs->qs_session_mtx, "qs session", NULL, MTX_DEF);
	qs->qs_aad_length = -1;

	qs->qs_dec_desc = ddesc = qdm->qdm_dma_vaddr;
	qs->qs_enc_desc = edesc = ddesc + 1;

	ddesc->qcd_desc_paddr = qdm->qdm_dma_seg.ds_addr;
	ddesc->qcd_hash_state_paddr = ddesc->qcd_desc_paddr +
	    offsetof(struct qat_crypto_desc, qcd_hash_state_prefix_buf);
	edesc->qcd_desc_paddr = qdm->qdm_dma_seg.ds_addr +
	    sizeof(struct qat_crypto_desc);
	edesc->qcd_hash_state_paddr = edesc->qcd_desc_paddr +
	    offsetof(struct qat_crypto_desc, qcd_hash_state_prefix_buf);

	qs->qs_status = QAT_SESSION_STATUS_ACTIVE;
	qs->qs_inflight = 0;

	qs->qs_cipher_key = csp->csp_cipher_key;
	qs->qs_cipher_klen = csp->csp_cipher_klen;
	qs->qs_auth_key = csp->csp_auth_key;
	qs->qs_auth_klen = csp->csp_auth_klen;

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		qs->qs_cipher_algo = qat_aes_cipher_algo(csp->csp_cipher_klen);
		qs->qs_cipher_mode = HW_CIPHER_CBC_MODE;
		break;
	case CRYPTO_AES_ICM:
		qs->qs_cipher_algo = qat_aes_cipher_algo(csp->csp_cipher_klen);
		qs->qs_cipher_mode = HW_CIPHER_CTR_MODE;
		break;
	case CRYPTO_AES_XTS:
		qs->qs_cipher_algo =
		    qat_aes_cipher_algo(csp->csp_cipher_klen / 2);
		qs->qs_cipher_mode = HW_CIPHER_XTS_MODE;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		qs->qs_cipher_algo = qat_aes_cipher_algo(csp->csp_cipher_klen);
		qs->qs_cipher_mode = HW_CIPHER_CTR_MODE;
		qs->qs_auth_algo = HW_AUTH_ALGO_GALOIS_128;
		qs->qs_auth_mode = HW_AUTH_MODE1;
		break;
	case 0:
		break;
	default:
		panic("%s: unhandled cipher algorithm %d", __func__,
		    csp->csp_cipher_alg);
	}

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA1;
		qs->qs_auth_mode = HW_AUTH_MODE1;
		break;
	case CRYPTO_SHA1:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA1;
		qs->qs_auth_mode = HW_AUTH_MODE0;
		break;
	case CRYPTO_SHA2_256_HMAC:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA256;
		qs->qs_auth_mode = HW_AUTH_MODE1;
		break;
	case CRYPTO_SHA2_256:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA256;
		qs->qs_auth_mode = HW_AUTH_MODE0;
		break;
	case CRYPTO_SHA2_384_HMAC:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA384;
		qs->qs_auth_mode = HW_AUTH_MODE1;
		break;
	case CRYPTO_SHA2_384:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA384;
		qs->qs_auth_mode = HW_AUTH_MODE0;
		break;
	case CRYPTO_SHA2_512_HMAC:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA512;
		qs->qs_auth_mode = HW_AUTH_MODE1;
		break;
	case CRYPTO_SHA2_512:
		qs->qs_auth_algo = HW_AUTH_ALGO_SHA512;
		qs->qs_auth_mode = HW_AUTH_MODE0;
		break;
	case CRYPTO_AES_NIST_GMAC:
		qs->qs_cipher_algo = qat_aes_cipher_algo(csp->csp_auth_klen);
		qs->qs_cipher_mode = HW_CIPHER_CTR_MODE;
		qs->qs_auth_algo = HW_AUTH_ALGO_GALOIS_128;
		qs->qs_auth_mode = HW_AUTH_MODE1;

		qs->qs_cipher_key = qs->qs_auth_key;
		qs->qs_cipher_klen = qs->qs_auth_klen;
		break;
	case 0:
		break;
	default:
		panic("%s: unhandled auth algorithm %d", __func__,
		    csp->csp_auth_alg);
	}

	slices = 0;
	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
	case CSP_MODE_ETA:
		/* auth then decrypt */
		ddesc->qcd_slices[0] = FW_SLICE_AUTH;
		ddesc->qcd_slices[1] = FW_SLICE_CIPHER;
		ddesc->qcd_cipher_dir = HW_CIPHER_DECRYPT;
		ddesc->qcd_cmd_id = FW_LA_CMD_HASH_CIPHER;
		/* encrypt then auth */
		edesc->qcd_slices[0] = FW_SLICE_CIPHER;
		edesc->qcd_slices[1] = FW_SLICE_AUTH;
		edesc->qcd_cipher_dir = HW_CIPHER_ENCRYPT;
		edesc->qcd_cmd_id = FW_LA_CMD_CIPHER_HASH;
		slices = 2;
		break;
	case CSP_MODE_CIPHER:
		/* decrypt */
		ddesc->qcd_slices[0] = FW_SLICE_CIPHER;
		ddesc->qcd_cipher_dir = HW_CIPHER_DECRYPT;
		ddesc->qcd_cmd_id = FW_LA_CMD_CIPHER;
		/* encrypt */
		edesc->qcd_slices[0] = FW_SLICE_CIPHER;
		edesc->qcd_cipher_dir = HW_CIPHER_ENCRYPT;
		edesc->qcd_cmd_id = FW_LA_CMD_CIPHER;
		slices = 1;
		break;
	case CSP_MODE_DIGEST:
		if (qs->qs_auth_algo == HW_AUTH_ALGO_GALOIS_128) {
			/* auth then decrypt */
			ddesc->qcd_slices[0] = FW_SLICE_AUTH;
			ddesc->qcd_slices[1] = FW_SLICE_CIPHER;
			ddesc->qcd_cipher_dir = HW_CIPHER_DECRYPT;
			ddesc->qcd_cmd_id = FW_LA_CMD_HASH_CIPHER;
			/* encrypt then auth */
			edesc->qcd_slices[0] = FW_SLICE_CIPHER;
			edesc->qcd_slices[1] = FW_SLICE_AUTH;
			edesc->qcd_cipher_dir = HW_CIPHER_ENCRYPT;
			edesc->qcd_cmd_id = FW_LA_CMD_CIPHER_HASH;
			slices = 2;
		} else {
			ddesc->qcd_slices[0] = FW_SLICE_AUTH;
			ddesc->qcd_cmd_id = FW_LA_CMD_AUTH;
			edesc->qcd_slices[0] = FW_SLICE_AUTH;
			edesc->qcd_cmd_id = FW_LA_CMD_AUTH;
			slices = 1;
		}
		break;
	default:
		panic("%s: unhandled crypto algorithm %d, %d", __func__,
		    csp->csp_cipher_alg, csp->csp_auth_alg);
	}
	ddesc->qcd_slices[slices] = FW_SLICE_DRAM_WR;
	edesc->qcd_slices[slices] = FW_SLICE_DRAM_WR;

	qcy->qcy_sc->sc_hw.qhw_crypto_setup_desc(qcy, qs, ddesc);
	qcy->qcy_sc->sc_hw.qhw_crypto_setup_desc(qcy, qs, edesc);

	if (csp->csp_auth_mlen != 0)
		qs->qs_auth_mlen = csp->csp_auth_mlen;
	else
		qs->qs_auth_mlen = edesc->qcd_auth_sz;

	/* Compute the GMAC by specifying a null cipher payload. */
	if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC)
		ddesc->qcd_cmd_id = edesc->qcd_cmd_id = FW_LA_CMD_AUTH;

	return 0;
}

static void
qat_crypto_clear_desc(struct qat_crypto_desc *desc)
{
	explicit_bzero(desc->qcd_content_desc, sizeof(desc->qcd_content_desc));
	explicit_bzero(desc->qcd_hash_state_prefix_buf,
	    sizeof(desc->qcd_hash_state_prefix_buf));
	explicit_bzero(desc->qcd_req_cache, sizeof(desc->qcd_req_cache));
}

static void
qat_freesession(device_t dev, crypto_session_t cses)
{
	struct qat_session *qs;

	qs = crypto_get_driver_session(cses);
	KASSERT(qs->qs_inflight == 0,
	    ("%s: session %p has requests in flight", __func__, qs));

	qat_crypto_clear_desc(qs->qs_enc_desc);
	qat_crypto_clear_desc(qs->qs_dec_desc);
	qat_free_dmamem(device_get_softc(dev), &qs->qs_desc_mem);
	mtx_destroy(&qs->qs_session_mtx);
}

static int
qat_process(device_t dev, struct cryptop *crp, int hint)
{
	struct qat_crypto *qcy;
	struct qat_crypto_bank *qcb;
	struct qat_crypto_desc const *desc;
	struct qat_session *qs;
	struct qat_softc *sc;
	struct qat_sym_cookie *qsc;
	struct qat_sym_bulk_cookie *qsbc;
	int error;

	sc = device_get_softc(dev);
	qcy = &sc->sc_crypto;
	qs = crypto_get_driver_session(crp->crp_session);
	qsc = NULL;

	if (__predict_false(crypto_buffer_len(&crp->crp_buf) > QAT_MAXLEN)) {
		error = E2BIG;
		goto fail1;
	}

	mtx_lock(&qs->qs_session_mtx);
	if (qs->qs_auth_algo == HW_AUTH_ALGO_GALOIS_128) {
		if (crp->crp_aad_length > QAT_GCM_AAD_SIZE_MAX) {
			error = E2BIG;
			mtx_unlock(&qs->qs_session_mtx);
			goto fail1;
		}

		/*
		 * The firmware interface for GCM annoyingly requires the AAD
		 * size to be stored in the session's content descriptor, which
		 * is not really meant to be updated after session
		 * initialization.  For IPSec the AAD size is fixed so this is
		 * not much of a problem in practice, but we have to catch AAD
		 * size updates here so that the device code can safely update
		 * the session's recorded AAD size.
		 */
		if (__predict_false(crp->crp_aad_length != qs->qs_aad_length)) {
			if (qs->qs_inflight == 0) {
				if (qs->qs_aad_length != -1) {
					counter_u64_add(sc->sc_gcm_aad_updates,
					    1);
				}
				qs->qs_aad_length = crp->crp_aad_length;
			} else {
				qs->qs_need_wakeup = true;
				mtx_unlock(&qs->qs_session_mtx);
				counter_u64_add(sc->sc_gcm_aad_restarts, 1);
				error = ERESTART;
				goto fail1;
			}
		}
	}
	qs->qs_inflight++;
	mtx_unlock(&qs->qs_session_mtx);

	qcb = qat_crypto_select_bank(qcy);

	qsc = qat_crypto_alloc_sym_cookie(qcb);
	if (qsc == NULL) {
		counter_u64_add(sc->sc_sym_alloc_failures, 1);
		error = ENOBUFS;
		goto fail2;
	}

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		desc = qs->qs_enc_desc;
	else
		desc = qs->qs_dec_desc;

	error = qat_crypto_load(qs, qsc, desc, crp);
	if (error != 0)
		goto fail2;

	qsbc = &qsc->qsc_bulk_cookie;
	qsbc->qsbc_crypto = qcy;
	qsbc->qsbc_session = qs;
	qsbc->qsbc_cb_tag = crp;

	sc->sc_hw.qhw_crypto_setup_req_params(qcb, qs, desc, qsc, crp);

	if (crp->crp_aad != NULL) {
		bus_dmamap_sync(qsc->qsc_dma[QAT_SYM_DMA_AADBUF].qsd_dma_tag,
		    qsc->qsc_dma[QAT_SYM_DMA_AADBUF].qsd_dmamap,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	bus_dmamap_sync(qsc->qsc_dma[QAT_SYM_DMA_BUF].qsd_dma_tag,
	    qsc->qsc_dma[QAT_SYM_DMA_BUF].qsd_dmamap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		bus_dmamap_sync(qsc->qsc_dma[QAT_SYM_DMA_OBUF].qsd_dma_tag,
		    qsc->qsc_dma[QAT_SYM_DMA_OBUF].qsd_dmamap,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	bus_dmamap_sync(qsc->qsc_self_dma_tag, qsc->qsc_self_dmamap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	error = qat_etr_put_msg(sc, qcb->qcb_sym_tx,
	    (uint32_t *)qsbc->qsbc_msg);
	if (error)
		goto fail2;

	return 0;

fail2:
	if (qsc)
		qat_crypto_free_sym_cookie(qcb, qsc);
	mtx_lock(&qs->qs_session_mtx);
	qs->qs_inflight--;
	mtx_unlock(&qs->qs_session_mtx);
fail1:
	crp->crp_etype = error;
	crypto_done(crp);
	return 0;
}

static device_method_t qat_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		qat_probe),
	DEVMETHOD(device_attach,	qat_attach),
	DEVMETHOD(device_detach,	qat_detach),

	/* Cryptodev interface */
	DEVMETHOD(cryptodev_probesession, qat_probesession),
	DEVMETHOD(cryptodev_newsession,	qat_newsession),
	DEVMETHOD(cryptodev_freesession, qat_freesession),
	DEVMETHOD(cryptodev_process,	qat_process),

	DEVMETHOD_END
};

static driver_t qat_driver = {
	.name		= "qat_c2xxx",
	.methods	= qat_methods,
	.size		= sizeof(struct qat_softc),
};

DRIVER_MODULE(qat_c2xxx, pci, qat_driver, 0, 0);
MODULE_VERSION(qat_c2xxx, 1);
MODULE_DEPEND(qat_c2xxx, crypto, 1, 1, 1);
MODULE_DEPEND(qat_c2xxx, firmware, 1, 1, 1);
MODULE_DEPEND(qat_c2xxx, pci, 1, 1, 1);
