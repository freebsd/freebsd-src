/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/smp.h>

#include <machine/vmm.h>
#include "io/iommu.h"

static int
amdv_init(void)
{

	printf("amdv_init: not implemented\n");
	return (ENXIO);
}

static int
amdv_cleanup(void)
{

	printf("amdv_cleanup: not implemented\n");
	return (ENXIO);
}

static void
amdv_resume(void)
{
}

static void *
amdv_vminit(struct vm *vm, struct pmap *pmap)
{

	printf("amdv_vminit: not implemented\n");
	return (NULL);
}

static int
amdv_vmrun(void *arg, int vcpu, register_t rip, struct pmap *pmap)
{

	printf("amdv_vmrun: not implemented\n");
	return (ENXIO);
}

static void
amdv_vmcleanup(void *arg)
{

	printf("amdv_vmcleanup: not implemented\n");
	return;
}

static int
amdv_getreg(void *arg, int vcpu, int regnum, uint64_t *retval)
{
	
	printf("amdv_getreg: not implemented\n");
	return (EINVAL);
}

static int
amdv_setreg(void *arg, int vcpu, int regnum, uint64_t val)
{
	
	printf("amdv_setreg: not implemented\n");
	return (EINVAL);
}

static int
amdv_getdesc(void *vmi, int vcpu, int num, struct seg_desc *desc)
{

	printf("amdv_get_desc: not implemented\n");
	return (EINVAL);
}

static int
amdv_setdesc(void *vmi, int vcpu, int num, struct seg_desc *desc)
{

	printf("amdv_get_desc: not implemented\n");
	return (EINVAL);
}

static int
amdv_inject_event(void *vmi, int vcpu, int type, int vector,
		  uint32_t error_code, int error_code_valid)
{

	printf("amdv_inject_event: not implemented\n");
	return (EINVAL);
}

static int
amdv_getcap(void *arg, int vcpu, int type, int *retval)
{

	printf("amdv_getcap: not implemented\n");
	return (EINVAL);
}

static int
amdv_setcap(void *arg, int vcpu, int type, int val)
{

	printf("amdv_setcap: not implemented\n");
	return (EINVAL);
}

static struct vmspace *
amdv_vmspace_alloc(vm_offset_t min, vm_offset_t max)
{

	printf("amdv_vmspace_alloc: not implemented\n");
	return (NULL);
}

static void
amdv_vmspace_free(struct vmspace *vmspace)
{

	printf("amdv_vmspace_free: not implemented\n");
	return;
}

struct vmm_ops vmm_ops_amd = {
	amdv_init,
	amdv_cleanup,
	amdv_resume,
	amdv_vminit,
	amdv_vmrun,
	amdv_vmcleanup,
	amdv_getreg,
	amdv_setreg,
	amdv_getdesc,
	amdv_setdesc,
	amdv_inject_event,
	amdv_getcap,
	amdv_setcap,
	amdv_vmspace_alloc,
	amdv_vmspace_free,
};

static int
amd_iommu_init(void)
{

	printf("amd_iommu_init: not implemented\n");
	return (ENXIO);
}

static void
amd_iommu_cleanup(void)
{

	printf("amd_iommu_cleanup: not implemented\n");
}

static void
amd_iommu_enable(void)
{

	printf("amd_iommu_enable: not implemented\n");
}

static void
amd_iommu_disable(void)
{

	printf("amd_iommu_disable: not implemented\n");
}

static void *
amd_iommu_create_domain(vm_paddr_t maxaddr)
{

	printf("amd_iommu_create_domain: not implemented\n");
	return (NULL);
}

static void
amd_iommu_destroy_domain(void *domain)
{

	printf("amd_iommu_destroy_domain: not implemented\n");
}

static uint64_t
amd_iommu_create_mapping(void *domain, vm_paddr_t gpa, vm_paddr_t hpa,
			 uint64_t len)
{

	printf("amd_iommu_create_mapping: not implemented\n");
	return (0);
}

static uint64_t
amd_iommu_remove_mapping(void *domain, vm_paddr_t gpa, uint64_t len)
{

	printf("amd_iommu_remove_mapping: not implemented\n");
	return (0);
}

static void
amd_iommu_add_device(void *domain, int bus, int slot, int func)
{

	printf("amd_iommu_add_device: not implemented\n");
}

static void
amd_iommu_remove_device(void *domain, int bus, int slot, int func)
{

	printf("amd_iommu_remove_device: not implemented\n");
}

static void
amd_iommu_invalidate_tlb(void *domain)
{

	printf("amd_iommu_invalidate_tlb: not implemented\n");
}

struct iommu_ops iommu_ops_amd = {
	amd_iommu_init,
	amd_iommu_cleanup,
	amd_iommu_enable,
	amd_iommu_disable,
	amd_iommu_create_domain,
	amd_iommu_destroy_domain,
	amd_iommu_create_mapping,
	amd_iommu_remove_mapping,
	amd_iommu_add_device,
	amd_iommu_remove_device,
	amd_iommu_invalidate_tlb,
};
