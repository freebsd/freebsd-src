/*-
 * Copyright (c) 2009-2012,2016-2017 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
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
 */

/**
 * Implements low-level interactions with Hyper-V/Azure
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/cpuset.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#if defined(__aarch64__)
#include <dev/hyperv/vmbus/aarch64/hyperv_machdep.h>
#include <dev/hyperv/vmbus/aarch64/hyperv_reg.h>
#else
#include <dev/hyperv/vmbus/x86/hyperv_machdep.h>
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#endif
#include <dev/hyperv/vmbus/vmbus_var.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>
#include <dev/hyperv/vmbus/hyperv_var.h>

#define HYPERV_FREEBSD_BUILD		0ULL
#define HYPERV_FREEBSD_VERSION		((uint64_t)__FreeBSD_version)
#define HYPERV_FREEBSD_OSID		0ULL

#define MSR_HV_GUESTID_BUILD_FREEBSD	\
	(HYPERV_FREEBSD_BUILD & MSR_HV_GUESTID_BUILD_MASK)
#define MSR_HV_GUESTID_VERSION_FREEBSD	\
	((HYPERV_FREEBSD_VERSION << MSR_HV_GUESTID_VERSION_SHIFT) & \
	 MSR_HV_GUESTID_VERSION_MASK)
#define MSR_HV_GUESTID_OSID_FREEBSD	\
	((HYPERV_FREEBSD_OSID << MSR_HV_GUESTID_OSID_SHIFT) & \
	 MSR_HV_GUESTID_OSID_MASK)

#define MSR_HV_GUESTID_FREEBSD		\
	(MSR_HV_GUESTID_BUILD_FREEBSD |	\
	 MSR_HV_GUESTID_VERSION_FREEBSD | \
	 MSR_HV_GUESTID_OSID_FREEBSD |	\
	 MSR_HV_GUESTID_OSTYPE_FREEBSD)


static bool			hyperv_identify(void);
static void			hypercall_memfree(void);

static struct hypercall_ctx	hypercall_context;

uint64_t
hypercall_post_message(bus_addr_t msg_paddr)
{
	return hypercall_md(hypercall_context.hc_addr,
	    HYPERCALL_POST_MESSAGE, msg_paddr, 0);
}

uint64_t
hypercall_signal_event(bus_addr_t monprm_paddr)
{
	return hypercall_md(hypercall_context.hc_addr,
	    HYPERCALL_SIGNAL_EVENT, monprm_paddr, 0);
}

static inline int hv_result(uint64_t status)
{
	return status & HV_HYPERCALL_RESULT_MASK;
}

static inline bool hv_result_success(uint64_t status)
{
	return hv_result(status) == HV_STATUS_SUCCESS;
}

static inline unsigned int hv_repcomp(uint64_t status)
{
	/* Bits [43:32] of status have 'Reps completed' data. */
	return ((status & HV_HYPERCALL_REP_COMP_MASK) >>
	    HV_HYPERCALL_REP_COMP_OFFSET);
}

/*
 * Rep hypercalls. Callers of this functions are supposed to ensure that
 * rep_count and varhead_size comply with Hyper-V hypercall definition.
 */
uint64_t
hv_do_rep_hypercall(uint16_t code, uint16_t rep_count, uint16_t varhead_size,
    uint64_t input, uint64_t output)
{
	uint64_t control = code;
	uint64_t status;
	uint16_t rep_comp;

	control |= (uint64_t)varhead_size << HV_HYPERCALL_VARHEAD_OFFSET;
	control |= (uint64_t)rep_count << HV_HYPERCALL_REP_COMP_OFFSET;

	do {
		status = hypercall_do_md(control, input, output);
		if (!hv_result_success(status))
			return status;

		rep_comp = hv_repcomp(status);

		control &= ~HV_HYPERCALL_REP_START_MASK;
		control |= (uint64_t)rep_comp << HV_HYPERCALL_REP_START_OFFSET;

	} while (rep_comp < rep_count);
	if (hv_result_success(status))
		return HV_STATUS_SUCCESS;

	return status;
}

uint64_t
hypercall_do_md(uint64_t input_val, uint64_t input_addr, uint64_t out_addr)
{
	uint64_t phys_inaddr, phys_outaddr;
	phys_inaddr = input_addr ? vtophys(input_addr) : 0;
	phys_outaddr = out_addr ? vtophys(out_addr) : 0;
	return hypercall_md(hypercall_context.hc_addr,
	    input_val, phys_inaddr, phys_outaddr);
}

int
hyperv_guid2str(const struct hyperv_guid *guid, char *buf, size_t sz)
{
	const uint8_t *d = guid->hv_guid;

	return snprintf(buf, sz, "%02x%02x%02x%02x-"
	    "%02x%02x-%02x%02x-%02x%02x-"
	    "%02x%02x%02x%02x%02x%02x",
	    d[3], d[2], d[1], d[0],
	    d[5], d[4], d[7], d[6], d[8], d[9],
	    d[10], d[11], d[12], d[13], d[14], d[15]);
}

static bool
hyperv_identify(void)
{
	return(hyperv_identify_features());
}
static void
hyperv_init(void *dummy __unused)
{
	if (!hyperv_identify()) {
		/* Not Hyper-V; reset guest id to the generic one. */
		if (vm_guest == VM_GUEST_HV)
			vm_guest = VM_GUEST_VM;
		return;
	}

	/* Set guest id */
	WRMSR(MSR_HV_GUEST_OS_ID, MSR_HV_GUESTID_FREEBSD);
	hyperv_init_tc();	
}
SYSINIT(hyperv_initialize, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, hyperv_init,
    NULL);

static void
hypercall_memfree(void)
{
	kmem_free(hypercall_context.hc_addr, PAGE_SIZE);
	hypercall_context.hc_addr = NULL;
}

static void
hypercall_create(void *arg __unused)
{

	int ret;
	if (vm_guest != VM_GUEST_HV)
		return;

	/*
	 * NOTE:
	 * - busdma(9), i.e. hyperv_dmamem APIs, can _not_ be used due to
	 *   the NX bit.
	 * - Assume kmem_malloc() returns properly aligned memory.
	 */
	hypercall_context.hc_addr = kmem_malloc(PAGE_SIZE, M_EXEC | M_WAITOK);
	hypercall_context.hc_paddr = vtophys(hypercall_context.hc_addr);
	ret = hypercall_page_setup(hypercall_context.hc_paddr);
	if (ret) {
		hypercall_memfree();
		return;
	}
	if (bootverbose)
		printf("hyperv: Hypercall created\n");
}
SYSINIT(hypercall_ctor, SI_SUB_DRIVERS, SI_ORDER_FIRST, hypercall_create, NULL);

static void
hypercall_destroy(void *arg __unused)
{

	if (hypercall_context.hc_addr == NULL)
		return;
	hypercall_disable();
	hypercall_memfree();
	if (bootverbose)
		printf("hyperv: Hypercall destroyed\n");
}
SYSUNINIT(hypercall_dtor, SI_SUB_DRIVERS, SI_ORDER_FIRST, hypercall_destroy,
    NULL);
