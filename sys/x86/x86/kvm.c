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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <x86/hypervisor.h>
#include <x86/kvm.h>

static int		kvm_identify(void);
static uint32_t		kvm_cpuid_identify(void);

const struct hypervisor_info kvm_hypervisor_info = {
	.hvi_name =		"KVM",
	.hvi_type =		VM_GUEST_KVM,
	.hvi_identify =		kvm_identify,
};

static uint32_t kvm_cpuid_base = -1;
static uint32_t kvm_cpuid_high = -1;

static uint32_t
kvm_cpuid_identify(void)
{

	if (kvm_cpuid_base == -1) {
		hypervisor_cpuid_base("KVMKVMKVM\0\0", 0, &kvm_cpuid_base,
		    &kvm_cpuid_high);
	}

	return (kvm_cpuid_base);
}

static int
kvm_identify(void)
{

	return (kvm_cpuid_identify() != 0);
}

int
kvm_paravirt_supported(void)
{

	return (kvm_cpuid_base != -1);
}

uint32_t
kvm_get_features(void)
{
	u_int regs[4];

	do_cpuid(kvm_cpuid_identify() | KVM_CPUID_FEATURES_LEAF, regs);

	return (regs[0]);
}
