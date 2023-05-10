/*- SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2022 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _HYPERV_REG_H_
#define _HYPERV_REG_H_

#include <sys/param.h>
#include <sys/systm.h>

/*
 * Hyper-V Synthetic MSRs
 */

#define MSR_HV_GUEST_OS_ID		0x40000000
#define MSR_HV_VP_INDEX			0x40000002
#define MSR_HV_SCONTROL			0x40000080
#define MSR_HV_SIEFP            0x40000082
#define MSR_HV_SIMP             0x40000083
#define MSR_HV_EOM			0x40000084
#define MSR_HV_SINT0			0x40000090
#define MSR_HV_SINT_AUTOEOI		0x00020000ULL
#define CPUID_LEAF_HV_IDENTITY		0x40000002
#define CPUID_LEAF_HV_FEATURES		0x40000003
#define CPUID_LEAF_HV_RECOMMENDS	0x40000004
#endif	/* !_HYPERV_REG_H_ */
