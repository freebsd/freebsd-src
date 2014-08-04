/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 2013 Roger Pau Monné <roger.pau@citrix.com>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/pcpu.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/intr_machdep.h>
#include <x86/apicvar.h>

#include <machine/cpu.h>
#include <machine/smp.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>

#include <xen/interface/vcpu.h>

static int xenpv_probe(void);
static int xenpv_probe_cpus(void);
static int xenpv_setup_local(void);
static int xenpv_setup_io(void);

static struct apic_enumerator xenpv_enumerator = {
	"Xen PV",
	xenpv_probe,
	xenpv_probe_cpus,
	xenpv_setup_local,
	xenpv_setup_io
};

/*
 * This enumerator will only be registered on PVH
 */
static int
xenpv_probe(void)
{
	return (0);
}

/*
 * Test each possible vCPU in order to find the number of vCPUs
 */
static int
xenpv_probe_cpus(void)
{
#ifdef SMP
	int i, ret;

	for (i = 0; i < MAXCPU; i++) {
		ret = HYPERVISOR_vcpu_op(VCPUOP_is_up, i, NULL);
		if (ret >= 0)
			lapic_create((i * 2), (i == 0));
	}
#endif
	return (0);
}

/*
 * Initialize the vCPU id of the BSP
 */
static int
xenpv_setup_local(void)
{
	PCPU_SET(vcpu_id, 0);
	lapic_init(0);
	return (0);
}

/*
 * On PVH guests there's no IO APIC
 */
static int
xenpv_setup_io(void)
{
	return (0);
}

static void
xenpv_register(void *dummy __unused)
{
	if (xen_pv_domain()) {
		apic_register_enumerator(&xenpv_enumerator);
	}
}
SYSINIT(xenpv_register, SI_SUB_TUNABLES - 1, SI_ORDER_FIRST, xenpv_register, NULL);

/*
 * Setup per-CPU vCPU IDs
 */
static void
xenpv_set_ids(void *dummy)
{
	struct pcpu *pc;
	int i;

	CPU_FOREACH(i) {
		pc = pcpu_find(i);
		pc->pc_vcpu_id = i;
	}
}
SYSINIT(xenpv_set_ids, SI_SUB_CPU, SI_ORDER_MIDDLE, xenpv_set_ids, NULL);
