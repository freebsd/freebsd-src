/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Netflix, Inc
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

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <x86/cputypes.h>
#include <x86/specialreg.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_aes_gcm.h>
#include <crypto/openssl/ossl_cipher.h>

/*
 * See OPENSSL_ia32cap(3).
 *
 * [0] = cpu_feature but with a few custom bits
 * [1] = cpu_feature2 but with AMD XOP in bit 11
 * [2] = cpu_stdext_feature
 * [3] = cpu_stdext_feature2
 */
unsigned int OPENSSL_ia32cap_P[4];
#define AESNI_CAPABLE	(OPENSSL_ia32cap_P[1]&(1<<(57-32)))

ossl_cipher_setkey_t aesni_set_encrypt_key;
ossl_cipher_setkey_t aesni_set_decrypt_key;

#ifdef __amd64__
int ossl_vaes_vpclmulqdq_capable(void);
ossl_cipher_setkey_t ossl_aes_gcm_setkey_aesni;
ossl_cipher_setkey_t ossl_aes_gcm_setkey_avx512;
#endif

void
ossl_cpuid(struct ossl_softc *sc)
{
	uint64_t xcr0;
	u_int regs[4];
	u_int max_cores;

	/* Derived from OpenSSL_ia32_cpuid. */

	OPENSSL_ia32cap_P[0] = cpu_feature & ~(CPUID_B20 | CPUID_IA64);
	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		OPENSSL_ia32cap_P[0] |= CPUID_IA64;
		if ((cpu_id & 0xf00) != 0xf00)
			OPENSSL_ia32cap_P[0] |= CPUID_B20;
	}

	/* Only leave CPUID_HTT on if HTT is present. */
	if (cpu_vendor_id == CPU_VENDOR_AMD && cpu_exthigh >= 0x80000008) {
		max_cores = (cpu_procinfo2 & AMDID_CMP_CORES) + 1;
		if (cpu_feature & CPUID_HTT) {
			if ((cpu_procinfo & CPUID_HTT_CORES) >> 16 <= max_cores)
				OPENSSL_ia32cap_P[0] &= ~CPUID_HTT;
		}
	} else {
		if (cpu_high >= 4) {
			cpuid_count(4, 0, regs);
			max_cores = (regs[0] >> 26) & 0xfff;
		} else
			max_cores = -1;
	}
	if (max_cores == 0)
		OPENSSL_ia32cap_P[0] &= ~CPUID_HTT;
	else if ((cpu_procinfo & CPUID_HTT_CORES) >> 16 == 0)
		OPENSSL_ia32cap_P[0] &= ~CPUID_HTT;

	OPENSSL_ia32cap_P[1] = cpu_feature2 & ~AMDID2_XOP;
	if (cpu_vendor_id == CPU_VENDOR_AMD)
		OPENSSL_ia32cap_P[1] |= amd_feature2 & AMDID2_XOP;

	OPENSSL_ia32cap_P[2] = cpu_stdext_feature;
	if ((OPENSSL_ia32cap_P[1] & CPUID2_XSAVE) == 0)
		OPENSSL_ia32cap_P[2] &= ~(CPUID_STDEXT_AVX512F |
		    CPUID_STDEXT_AVX512DQ);

	/* Disable AVX512F on Skylake-X. */
	if ((cpu_id & 0x0fff0ff0) == 0x00050650)
		OPENSSL_ia32cap_P[2] &= ~(CPUID_STDEXT_AVX512F);

	if (cpu_feature2 & CPUID2_OSXSAVE)
		xcr0 = rxcr(0);
	else
		xcr0 = 0;

	if ((xcr0 & (XFEATURE_AVX512 | XFEATURE_AVX)) !=
	    (XFEATURE_AVX512 | XFEATURE_AVX))
		OPENSSL_ia32cap_P[2] &= ~(CPUID_STDEXT_AVX512VL |
		    CPUID_STDEXT_AVX512BW | CPUID_STDEXT_AVX512IFMA |
		    CPUID_STDEXT_AVX512F);
	if ((xcr0 & XFEATURE_AVX) != XFEATURE_AVX) {
		OPENSSL_ia32cap_P[1] &= ~(CPUID2_AVX | AMDID2_XOP | CPUID2_FMA);
		OPENSSL_ia32cap_P[2] &= ~CPUID_STDEXT_AVX2;
	}
	OPENSSL_ia32cap_P[3] = cpu_stdext_feature2;

	if (!AESNI_CAPABLE)
		return;

	sc->has_aes = true;
	ossl_cipher_aes_cbc.set_encrypt_key = aesni_set_encrypt_key;
	ossl_cipher_aes_cbc.set_decrypt_key = aesni_set_decrypt_key;

#ifdef __amd64__
	if (ossl_vaes_vpclmulqdq_capable()) {
		ossl_cipher_aes_gcm.set_encrypt_key =
		    ossl_aes_gcm_setkey_avx512;
		ossl_cipher_aes_gcm.set_decrypt_key =
		    ossl_aes_gcm_setkey_avx512;
		sc->has_aes_gcm = true;
	} else if ((cpu_feature2 &
	    (CPUID2_AVX | CPUID2_PCLMULQDQ | CPUID2_MOVBE)) ==
	    (CPUID2_AVX | CPUID2_PCLMULQDQ | CPUID2_MOVBE)) {
		ossl_cipher_aes_gcm.set_encrypt_key = ossl_aes_gcm_setkey_aesni;
		ossl_cipher_aes_gcm.set_decrypt_key = ossl_aes_gcm_setkey_aesni;
		sc->has_aes_gcm = true;
	} else {
		sc->has_aes_gcm = false;
	}
#else
	sc->has_aes_gcm = false;
#endif
}
