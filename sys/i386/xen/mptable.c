/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/bus.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>

#include <xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/smp.h>
#include <xen/interface/vcpu.h>


static int	mptable_probe(void);
static int	mptable_probe_cpus(void);
static void	mptable_register(void *dummy);
static int	mptable_setup_local(void);
static int	mptable_setup_io(void);

static struct apic_enumerator mptable_enumerator = {
	"MPTable",
	mptable_probe,
	mptable_probe_cpus,
	mptable_setup_local,
	mptable_setup_io
};

static int
mptable_probe(void)
{

	return (-100);
}

static int
mptable_probe_cpus(void)
{
	int i, rc;

	for (i = 0; i < MAXCPU; i++) {
		rc = HYPERVISOR_vcpu_op(VCPUOP_is_up, i, NULL);
		if (rc >= 0)
			cpu_add(i, (i == 0));
	}

	return (0);
}

/*
 * Initialize the local APIC on the BSP.
 */
static int
mptable_setup_local(void)
{

	return (0);
}

static int
mptable_setup_io(void)
{

	return (0);
}

static void
mptable_register(void *dummy __unused)
{

	apic_register_enumerator(&mptable_enumerator);
}
SYSINIT(mptable_register, SI_SUB_TUNABLES - 1, SI_ORDER_FIRST, mptable_register,
    NULL);
