/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Raptor Engineering, LLC
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

#include <sys/libkern.h>
#include <sys/malloc.h>

#include <machine/cpu.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/ossl_ppc.h>

unsigned int OPENSSL_ppccap_P = 0;

ossl_cipher_setkey_t aes_p8_set_encrypt_key;
ossl_cipher_setkey_t aes_p8_set_decrypt_key;

ossl_cipher_setkey_t vpaes_set_encrypt_key;
ossl_cipher_setkey_t vpaes_set_decrypt_key;

ossl_cipher_setkey_t ossl_aes_gcm_setkey;

void
ossl_cpuid(struct ossl_softc *sc)
{
	if (cpu_features2 & PPC_FEATURE2_HAS_VEC_CRYPTO) {
		OPENSSL_ppccap_P |= PPC_CRYPTO207;
	}

	if (cpu_features2 & PPC_FEATURE2_ARCH_3_00) {
		OPENSSL_ppccap_P |= PPC_MADD300;
	}

	if (cpu_features & PPC_FEATURE_64) {
		OPENSSL_ppccap_P |= PPC_MFTB;
	} else {
		OPENSSL_ppccap_P |= PPC_MFSPR268;
	}

	if (cpu_features & PPC_FEATURE_HAS_FPU) {
		OPENSSL_ppccap_P |= PPC_FPU;

		if (cpu_features & PPC_FEATURE_64) {
			OPENSSL_ppccap_P |= PPC_FPU64;
		}
	}

	if (cpu_features & PPC_FEATURE_HAS_ALTIVEC) {
		OPENSSL_ppccap_P |= PPC_ALTIVEC;
	}

	/* Pick P8 crypto if available, otherwise fall back to altivec */
	if (OPENSSL_ppccap_P & PPC_CRYPTO207) {
		ossl_cipher_aes_cbc.set_encrypt_key = aes_p8_set_encrypt_key;
		ossl_cipher_aes_cbc.set_decrypt_key = aes_p8_set_decrypt_key;
		sc->has_aes = true;

		ossl_cipher_aes_gcm.set_encrypt_key = ossl_aes_gcm_setkey;
		ossl_cipher_aes_gcm.set_decrypt_key = ossl_aes_gcm_setkey;
		sc->has_aes_gcm = true;
    } else if (OPENSSL_ppccap_P & PPC_ALTIVEC) {
		ossl_cipher_aes_cbc.set_encrypt_key = vpaes_set_encrypt_key;
		ossl_cipher_aes_cbc.set_decrypt_key = vpaes_set_decrypt_key;
		sc->has_aes = true;
	}
}

/*
 * The following trivial wrapper functions were copied from OpenSSL 1.1.1v's
 * crypto/ppccap.c.
 */

void sha256_block_p8(void *ctx, const void *inp, size_t len);
void sha256_block_ppc(void *ctx, const void *inp, size_t len);
void sha256_block_data_order(void *ctx, const void *inp, size_t len);
void sha256_block_data_order(void *ctx, const void *inp, size_t len)
{
	OPENSSL_ppccap_P & PPC_CRYPTO207 ? sha256_block_p8(ctx, inp, len) :
		sha256_block_ppc(ctx, inp, len);
}

void sha512_block_p8(void *ctx, const void *inp, size_t len);
void sha512_block_ppc(void *ctx, const void *inp, size_t len);
void sha512_block_data_order(void *ctx, const void *inp, size_t len);
void sha512_block_data_order(void *ctx, const void *inp, size_t len)
{
	OPENSSL_ppccap_P & PPC_CRYPTO207 ? sha512_block_p8(ctx, inp, len) :
		sha512_block_ppc(ctx, inp, len);
}

void ChaCha20_ctr32_int(unsigned char *out, const unsigned char *inp,
						size_t len, const unsigned int key[8],
						const unsigned int counter[4]);
void ChaCha20_ctr32_vmx(unsigned char *out, const unsigned char *inp,
						size_t len, const unsigned int key[8],
						const unsigned int counter[4]);
void ChaCha20_ctr32_vsx(unsigned char *out, const unsigned char *inp,
						size_t len, const unsigned int key[8],
						const unsigned int counter[4]);
void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp,
					size_t len, const unsigned int key[8],
					const unsigned int counter[4]);
void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp,
					size_t len, const unsigned int key[8],
					const unsigned int counter[4])
{
	OPENSSL_ppccap_P & PPC_CRYPTO207
		? ChaCha20_ctr32_vsx(out, inp, len, key, counter)
		: OPENSSL_ppccap_P & PPC_ALTIVEC
			? ChaCha20_ctr32_vmx(out, inp, len, key, counter)
			: ChaCha20_ctr32_int(out, inp, len, key, counter);
}

void poly1305_init_int(void *ctx, const unsigned char key[16]);
void poly1305_blocks(void *ctx, const unsigned char *inp, size_t len,
						 unsigned int padbit);
void poly1305_emit(void *ctx, unsigned char mac[16],
					   const unsigned int nonce[4]);
void poly1305_init_fpu(void *ctx, const unsigned char key[16]);
void poly1305_blocks_fpu(void *ctx, const unsigned char *inp, size_t len,
						 unsigned int padbit);
void poly1305_emit_fpu(void *ctx, unsigned char mac[16],
					   const unsigned int nonce[4]);
int poly1305_init(void *ctx, const unsigned char key[16], void *func[2]);
int poly1305_init(void *ctx, const unsigned char key[16], void *func[2])
{
	if (sizeof(size_t) == 4 && (OPENSSL_ppccap_P & PPC_FPU)) {
		poly1305_init_fpu(ctx, key);
		func[0] = (void*)(uintptr_t)poly1305_blocks_fpu;
		func[1] = (void*)(uintptr_t)poly1305_emit_fpu;
	} else {
		poly1305_init_int(ctx, key);
		func[0] = (void*)(uintptr_t)poly1305_blocks;
		func[1] = (void*)(uintptr_t)poly1305_emit;
	}
	return 1;
}
