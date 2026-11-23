/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
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
 *
 */

#ifndef __CPUID_HH__
#define __CPUID_HH__

/*
 * CPUID leaf 1 eax decode, copied from machine/specialreg.h.  The values are
 * decoded from the log rather than the host CPU, so detection has to work on
 * any architecture pmc(8) is built for, not just x86.
 */
#define	IBS_CPUID_VENDOR_AMD	"AuthenticAMD"
#define	IBS_CPUID_MODEL		0x000000f0
#define	IBS_CPUID_FAMILY	0x00000f00
#define	IBS_CPUID_EXT_MODEL	0x000f0000
#define	IBS_CPUID_EXT_FAMILY	0x0ff00000
#define	IBS_CPUID_TO_MODEL(id) \
    ((((id) & IBS_CPUID_MODEL) >> 4) | (((id) & IBS_CPUID_EXT_MODEL) >> 12))
#define	IBS_CPUID_TO_FAMILY(id) \
    ((((id) & IBS_CPUID_FAMILY) >> 8) + (((id) & IBS_CPUID_EXT_FAMILY) >> 20))

#endif /* __CPUID_HH__ */
