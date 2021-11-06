/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#include <machine/elf.h>
#include <machine/md_var.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/aarch64/arm_arch.h>

/*
 * Feature bits defined in arm_arch.h
 */
unsigned int OPENSSL_armcap_P;

void
ossl_cpuid(void)
{
	/* SHA features */
	if ((elf_hwcap & HWCAP_SHA1) != 0)
		OPENSSL_armcap_P |= ARMV8_SHA1;
	if ((elf_hwcap & HWCAP_SHA2) != 0)
		OPENSSL_armcap_P |= ARMV8_SHA256;
	if ((elf_hwcap & HWCAP_SHA512) != 0)
		OPENSSL_armcap_P |= ARMV8_SHA512;

	/* AES features */
	if ((elf_hwcap & HWCAP_AES) != 0)
		OPENSSL_armcap_P |= ARMV8_AES;
	if ((elf_hwcap & HWCAP_PMULL) != 0)
		OPENSSL_armcap_P |= ARMV8_PMULL;
}
