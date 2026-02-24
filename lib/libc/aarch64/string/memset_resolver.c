/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Arm Ltd
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>

#include <machine/armreg.h>
#include <machine/ifunc.h>

#include <elf.h>

void *__memset_aarch64(void *, int, size_t);
void *__memset_aarch64_zva64(void *, int, size_t);
void *__memset_aarch64_mops(void *, int, size_t);

DEFINE_UIFUNC(, void *, memset, (void *, int, size_t))
{
	uint64_t dczid;

	if (ifunc_arg->_hwcap2 & HWCAP2_MOPS)
		return (__memset_aarch64_mops);

	/*
	 * Check for the DC ZVA instruction, and it will
	 * zero 64 bytes (4 * 4byte words).
	 */
	dczid = READ_SPECIALREG(dczid_el0);
	if ((dczid & DCZID_DZP) == 0 && DCZID_BS_SIZE(dczid) == 4)
		return (__memset_aarch64_zva64);

	return (__memset_aarch64);
}

