/*
 * Copyright (c) 2014 Roger Pau Monné <roger.pau@citrix.com>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/smp.h>

#include <x86/apicreg.h>
#include <x86/apicvar.h>

#include <xen/xen-os.h>
#include <xen/features.h>
#include <xen/gnttab.h>
#include <xen/hypervisor.h>
#include <xen/hvm.h>
#include <xen/xen_intr.h>

#include <xen/interface/arch-x86/cpuid.h>
#include <xen/interface/vcpu.h>

/*--------------------------- Forward Declarations ---------------------------*/
static driver_filter_t xen_smp_rendezvous_action;
#ifdef __amd64__
static driver_filter_t xen_invlop;
#else
static driver_filter_t xen_invltlb;
static driver_filter_t xen_invlpg;
static driver_filter_t xen_invlrng;
static driver_filter_t xen_invlcache;
#endif
static driver_filter_t xen_ipi_bitmap_handler;
static driver_filter_t xen_cpustop_handler;
static driver_filter_t xen_cpususpend_handler;
static driver_filter_t xen_ipi_swi_handler;

/*---------------------------------- Macros ----------------------------------*/
#define	IPI_TO_IDX(ipi) ((ipi) - APIC_IPI_INTS)

/*--------------------------------- Xen IPIs ---------------------------------*/
struct xen_ipi_handler
{
	driver_filter_t	*filter;
	const char	*description;
};

static struct xen_ipi_handler xen_ipis[] = 
{
	[IPI_TO_IDX(IPI_RENDEZVOUS)]	= { xen_smp_rendezvous_action,	"r"   },
#ifdef __amd64__
	[IPI_TO_IDX(IPI_INVLOP)]	= { xen_invlop,			"itlb"},
#else
	[IPI_TO_IDX(IPI_INVLTLB)]	= { xen_invltlb,		"itlb"},
	[IPI_TO_IDX(IPI_INVLPG)]	= { xen_invlpg,			"ipg" },
	[IPI_TO_IDX(IPI_INVLRNG)]	= { xen_invlrng,		"irg" },
	[IPI_TO_IDX(IPI_INVLCACHE)]	= { xen_invlcache,		"ic"  },
#endif
	[IPI_TO_IDX(IPI_BITMAP_VECTOR)] = { xen_ipi_bitmap_handler,	"b"   },
	[IPI_TO_IDX(IPI_STOP)]		= { xen_cpustop_handler,	"st"  },
	[IPI_TO_IDX(IPI_SUSPEND)]	= { xen_cpususpend_handler,	"sp"  },
	[IPI_TO_IDX(IPI_SWI)]		= { xen_ipi_swi_handler,	"sw"  },
};

/*
 * Save previous (native) handler as a fallback. Xen < 4.7 doesn't support
 * VCPUOP_send_nmi for HVM guests, and thus we need a fallback in that case:
 *
 * https://lists.freebsd.org/archives/freebsd-xen/2022-January/000032.html
 */
void (*native_ipi_vectored)(u_int, int);

/*------------------------------- Per-CPU Data -------------------------------*/
DPCPU_DEFINE(xen_intr_handle_t, ipi_handle[nitems(xen_ipis)]);

/*------------------------------- Xen PV APIC --------------------------------*/

#define PCPU_ID_GET(id, field) (pcpu_find(id)->pc_##field)
static int
send_nmi(int dest)
{
	unsigned int cpu;
	int rc = 0;

	/*
	 * NMIs are not routed over event channels, and instead delivered as on
	 * native using the exception vector (#2). Triggering them can be done
	 * using the local APIC, or an hypercall as a shortcut like it's done
	 * below.
	 */
	switch(dest) {
	case APIC_IPI_DEST_SELF:
		rc = HYPERVISOR_vcpu_op(VCPUOP_send_nmi, PCPU_GET(vcpu_id), NULL);
		break;
	case APIC_IPI_DEST_ALL:
		CPU_FOREACH(cpu) {
			rc = HYPERVISOR_vcpu_op(VCPUOP_send_nmi,
			    PCPU_ID_GET(cpu, vcpu_id), NULL);
			if (rc != 0)
				break;
		}
		break;
	case APIC_IPI_DEST_OTHERS:
		CPU_FOREACH(cpu) {
			if (cpu != PCPU_GET(cpuid)) {
				rc = HYPERVISOR_vcpu_op(VCPUOP_send_nmi,
				    PCPU_ID_GET(cpu, vcpu_id), NULL);
				if (rc != 0)
					break;
			}
		}
		break;
	default:
		rc = HYPERVISOR_vcpu_op(VCPUOP_send_nmi,
		    PCPU_ID_GET(apic_cpuid(dest), vcpu_id), NULL);
		break;
	}

	return rc;
}
#undef PCPU_ID_GET

static void
xen_pv_lapic_ipi_vectored(u_int vector, int dest)
{
	xen_intr_handle_t *ipi_handle;
	int ipi_idx, to_cpu, self;
	static bool pvnmi = true;

	if (vector >= IPI_NMI_FIRST) {
		if (pvnmi) {
			int rc = send_nmi(dest);

			if (rc != 0) {
				printf(
    "Sending NMI using hypercall failed (%d) switching to APIC\n", rc);
				pvnmi = false;
				native_ipi_vectored(vector, dest);
			}
		} else
			native_ipi_vectored(vector, dest);

		return;
	}

	ipi_idx = IPI_TO_IDX(vector);
	if (ipi_idx >= nitems(xen_ipis))
		panic("IPI out of range");

	switch(dest) {
	case APIC_IPI_DEST_SELF:
		ipi_handle = DPCPU_GET(ipi_handle);
		xen_intr_signal(ipi_handle[ipi_idx]);
		break;
	case APIC_IPI_DEST_ALL:
		CPU_FOREACH(to_cpu) {
			ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
			xen_intr_signal(ipi_handle[ipi_idx]);
		}
		break;
	case APIC_IPI_DEST_OTHERS:
		self = PCPU_GET(cpuid);
		CPU_FOREACH(to_cpu) {
			if (to_cpu != self) {
				ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
				xen_intr_signal(ipi_handle[ipi_idx]);
			}
		}
		break;
	default:
		to_cpu = apic_cpuid(dest);
		ipi_handle = DPCPU_ID_GET(to_cpu, ipi_handle);
		xen_intr_signal(ipi_handle[ipi_idx]);
		break;
	}
}

/*---------------------------- XEN PV IPI Handlers ---------------------------*/
/*
 * These are C clones of the ASM functions found in apic_vector.
 */
static int
xen_ipi_bitmap_handler(void *arg)
{
	struct trapframe *frame;

	frame = arg;
	ipi_bitmap_handler(*frame);
	return (FILTER_HANDLED);
}

static int
xen_smp_rendezvous_action(void *arg)
{
#ifdef COUNT_IPIS
	(*ipi_rendezvous_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	smp_rendezvous_action();
	return (FILTER_HANDLED);
}

#ifdef __amd64__
static int
xen_invlop(void *arg)
{

	invlop_handler();
	return (FILTER_HANDLED);
}

#else /* __i386__ */

static int
xen_invltlb(void *arg)
{

	invltlb_handler();
	return (FILTER_HANDLED);
}

static int
xen_invlpg(void *arg)
{

	invlpg_handler();
	return (FILTER_HANDLED);
}

static int
xen_invlrng(void *arg)
{

	invlrng_handler();
	return (FILTER_HANDLED);
}

static int
xen_invlcache(void *arg)
{

	invlcache_handler();
	return (FILTER_HANDLED);
}
#endif /* __amd64__ */

static int
xen_cpustop_handler(void *arg)
{

	cpustop_handler();
	return (FILTER_HANDLED);
}

static int
xen_cpususpend_handler(void *arg)
{

	cpususpend_handler();
	return (FILTER_HANDLED);
}

static int
xen_ipi_swi_handler(void *arg)
{
	struct trapframe *frame = arg;

	ipi_swi_handler(*frame);
	return (FILTER_HANDLED);
}

/*----------------------------- XEN PV IPI setup -----------------------------*/
/*
 * Those functions are provided outside of the Xen PV APIC implementation
 * so PVHVM guests can also use PV IPIs without having an actual Xen PV APIC,
 * because on PVHVM there's an emulated LAPIC provided by Xen.
 */
static void
xen_cpu_ipi_init(int cpu)
{
	xen_intr_handle_t *ipi_handle;
	const struct xen_ipi_handler *ipi;
	int idx, rc;

	ipi_handle = DPCPU_ID_GET(cpu, ipi_handle);

	for (ipi = xen_ipis, idx = 0; idx < nitems(xen_ipis); ipi++, idx++) {
		if (ipi->filter == NULL) {
			ipi_handle[idx] = NULL;
			continue;
		}

		rc = xen_intr_alloc_and_bind_ipi(cpu, ipi->filter,
		    INTR_TYPE_TTY, &ipi_handle[idx]);
		if (rc != 0)
			panic("Unable to allocate a XEN IPI port");
		xen_intr_describe(ipi_handle[idx], "%s", ipi->description);
	}
}

static void
xen_setup_cpus(void)
{
	uint32_t regs[4];
	int i;

	if (!xen_vector_callback_enabled)
		return;

	/*
	 * Check whether the APIC virtualization is hardware assisted, as
	 * that's faster than using event channels because it avoids the VM
	 * exit.
	 */
	KASSERT(xen_cpuid_base != 0, ("Invalid base Xen CPUID leaf"));
	cpuid_count(xen_cpuid_base + 4, 0, regs);
	if ((x2apic_mode && (regs[0] & XEN_HVM_CPUID_X2APIC_VIRT)) ||
	    (!x2apic_mode && (regs[0] & XEN_HVM_CPUID_APIC_ACCESS_VIRT)))
		return;

	CPU_FOREACH(i)
		xen_cpu_ipi_init(i);

	/* Set the xen pv ipi ops to replace the native ones */
	ipi_vectored = xen_pv_lapic_ipi_vectored;
	native_ipi_vectored = ipi_vectored;
}

/* Switch to using PV IPIs as soon as the vcpu_id is set. */
SYSINIT(xen_setup_cpus, SI_SUB_SMP, SI_ORDER_SECOND, xen_setup_cpus, NULL);
