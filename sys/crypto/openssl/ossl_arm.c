/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Stormshield
 * Copyright (c) 2023 Semihalf
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <machine/elf.h>
#include <machine/md_var.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/arm_arch.h>

ossl_cipher_setkey_t AES_set_encrypt_key;
ossl_cipher_setkey_t AES_set_decrypt_key;

ossl_cipher_setkey_t ossl_aes_gcm_setkey;

unsigned int OPENSSL_armcap_P;

void
ossl_cpuid(struct ossl_softc *sc)
{
	if (elf_hwcap & HWCAP_NEON) {
		OPENSSL_armcap_P |= ARMV7_NEON;

		sc->has_aes = true;
		ossl_cipher_aes_cbc.set_encrypt_key = AES_set_encrypt_key;
		ossl_cipher_aes_cbc.set_decrypt_key = AES_set_decrypt_key;

		sc->has_aes_gcm = true;
		ossl_cipher_aes_gcm.set_encrypt_key = ossl_aes_gcm_setkey;
		ossl_cipher_aes_gcm.set_decrypt_key = ossl_aes_gcm_setkey;
	}
}
