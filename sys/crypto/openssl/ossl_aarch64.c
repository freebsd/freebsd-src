/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Netflix, Inc
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Mitchell Horne <mhorne@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stdint.h>

#include <machine/armreg.h>
#include <machine/cpu.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/arm_arch.h>

/*
 * Feature bits defined in arm_arch.h
 */
unsigned int OPENSSL_armcap_P;
#define	AES_PMULL_CAPABLE					\
	((OPENSSL_armcap_P & (ARMV8_AES | ARMV8_PMULL)) ==	\
	    (ARMV8_AES | ARMV8_PMULL))

ossl_cipher_setkey_t aes_v8_set_encrypt_key;
ossl_cipher_setkey_t aes_v8_set_decrypt_key;
ossl_cipher_setkey_t ossl_aes_gcm_setkey;

ossl_cipher_setkey_t vpaes_set_encrypt_key;
ossl_cipher_setkey_t vpaes_set_decrypt_key;

void
ossl_cpuid(struct ossl_softc *sc)
{
	uint64_t isar0;

	/* Use sanitized kernel features, available before elf_hwcap is set. */
	get_kernel_reg(ID_AA64ISAR0_EL1, &isar0);

	/* SHA features */
	if (ID_AA64ISAR0_SHA1_VAL(isar0) >= ID_AA64ISAR0_SHA1_BASE)
		OPENSSL_armcap_P |= ARMV8_SHA1;
	if (ID_AA64ISAR0_SHA2_VAL(isar0) >= ID_AA64ISAR0_SHA2_BASE)
		OPENSSL_armcap_P |= ARMV8_SHA256;
	if (ID_AA64ISAR0_SHA2_VAL(isar0) >= ID_AA64ISAR0_SHA2_512)
		OPENSSL_armcap_P |= ARMV8_SHA512;

	/* AES features */
	if (ID_AA64ISAR0_AES_VAL(isar0) >= ID_AA64ISAR0_AES_BASE)
		OPENSSL_armcap_P |= ARMV8_AES;
	if (ID_AA64ISAR0_AES_VAL(isar0) >= ID_AA64ISAR0_AES_PMULL)
		OPENSSL_armcap_P |= ARMV8_PMULL;

	if ((OPENSSL_armcap_P & ARMV8_AES) == 0 &&
	    (OPENSSL_armcap_P & ARMV7_NEON) == 0) {
		sc->has_aes = false;
		return;
	}
	sc->has_aes = true;
	if (OPENSSL_armcap_P & ARMV8_AES) {
		ossl_cipher_aes_cbc.set_encrypt_key = aes_v8_set_encrypt_key;
		ossl_cipher_aes_cbc.set_decrypt_key = aes_v8_set_decrypt_key;
	} else {
		ossl_cipher_aes_cbc.set_encrypt_key = vpaes_set_encrypt_key;
		ossl_cipher_aes_cbc.set_decrypt_key = vpaes_set_decrypt_key;
	}

	if (AES_PMULL_CAPABLE) {
		ossl_cipher_aes_gcm.set_encrypt_key = ossl_aes_gcm_setkey;
		ossl_cipher_aes_gcm.set_decrypt_key = ossl_aes_gcm_setkey;
		sc->has_aes_gcm = true;
	}
}
