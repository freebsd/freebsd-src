/*
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/cpu.h>

#include <x86/hypervisor.h>
#include <x86/kvm.h>
#include <x86/vmware.h>

char hv_vendor[16];
SYSCTL_STRING(_hw, OID_AUTO, hv_vendor, CTLFLAG_RD, hv_vendor, 0,
    "Hypervisor vendor");

extern const struct hypervisor_info kvm_hypervisor_info;
extern const struct hypervisor_info vmware_hypervisor_info;

static const struct hypervisor_info *hypervisor_infos[] = {
	&kvm_hypervisor_info,
	&vmware_hypervisor_info,
};

static const struct hypervisor_info *hv_info;

/*
 * [RFC] CPUID usage for interaction between Hypervisors and Linux.
 * http://lkml.org/lkml/2008/10/1/246
 */
int
hypervisor_cpuid_base(const char *signature, int leaves, uint32_t *base,
    uint32_t *high)
{
	uint32_t leaf, regs[4];

	for (leaf = 0x40000000; leaf < 0x40010000; leaf += 0x100) {
		do_cpuid(leaf, regs);
		if (!memcmp(signature, &regs[1], 12) &&
		    (leaves == 0 || (regs[0] - leaf >= leaves))) {
			*base = leaf;
			*high = regs[0];
			return (0);
		}
	}

	return (1);
}

void
hypervisor_cpuid_identify(void)
{
	const struct hypervisor_info *hvi;
	int i;

	for (i = 0; i < nitems(hypervisor_infos); i++) {
		hvi = hypervisor_infos[i];

		if (hvi->hvi_identify() != 0) {
			hv_info = hvi;
			vm_guest = hvi->hvi_type;
			strncpy(hv_vendor, hvi->hvi_name, sizeof(hv_vendor));
			return;
		}
	}

	vm_guest = VM_GUEST_VM;
}

void
hypervisor_print_info(void)
{

	if (*hv_vendor)
		printf("Hypervisor: Origin = \"%s\"\n", hv_vendor);
}
