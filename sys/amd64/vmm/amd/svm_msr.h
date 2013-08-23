/*-
 * Copyright (c) 2013 Anish Gupta (akgupt3@gmail.com)
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _SVM_MSR_H_
#define _SVM_MSR_H_

/*
 * SVM CPUID function, address 0x8000_000A edx bit decoding.
 */
#define AMD_CPUID_SVM_NP		BIT(0)  /* Nested paging or RVI */
#define AMD_CPUID_SVM_LBR		BIT(1)  /* Last branch virtualization */
#define AMD_CPUID_SVM_SVML		BIT(2)  /* SVM lock */
#define AMD_CPUID_SVM_NRIP_SAVE		BIT(3)  /* Next RIP is saved */
#define AMD_CPUID_SVM_TSC_RATE		BIT(4)  /* TSC rate control. */
#define AMD_CPUID_SVM_VMCB_CLEAN	BIT(5)  /* VMCB state caching */
#define AMD_CPUID_SVM_ASID_FLUSH	BIT(6)  /* Flush by ASID */
#define AMD_CPUID_SVM_DECODE_ASSIST	BIT(7)  /* Decode assist */
#define AMD_CPUID_SVM_PAUSE_INC		BIT(10) /* Pause intercept filter. */
#define AMD_CPUID_SVM_PAUSE_FTH		BIT(12) /* Pause filter threshold */

#endif /* _SVM_MSR_H_ */
