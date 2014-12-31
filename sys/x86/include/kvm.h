/*-
 * Copyright (c) 2014 Bryan Venteicher <bryanv@FreeBSD.org>
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
 * $FreeBSD$
 */

#ifndef _X86_KVM_H_
#define _X86_KVM_H_

#define KVM_CPUID_FEATURES_LEAF		0x40000001

#define KVM_FEATURE_CLOCKSOURCE		0x00000001
#define KVM_FEATURE_CLOCKSOURCE2	0x00000008

/* Deprecated: for the CLOCKSOURCE feature. */
#define KVM_MSR_WALL_CLOCK		0x11
#define KVM_MSR_SYSTEM_TIME		0x12

#define KVM_MSR_WALL_CLOCK_NEW		0x4b564d00
#define KVM_MSR_SYSTEM_TIME_NEW		0x4b564d01

int		kvm_paravirt_supported(void);
uint32_t	kvm_get_features(void);

void		kvm_clock_cpu_stop(int restart);
uint64_t	kvm_clock_tsc_freq(void);

#endif /* !_X86_KVM_H_ */
