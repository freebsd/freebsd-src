/*-
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
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
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/vm86.h>
#include <machine/pc/bios.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

struct ACPIrsdp *
acpi_find_rsdp(void)
{
    u_long		sigaddr;
    struct ACPIrsdp	*rsdp;
    u_int8_t		ck, *cv;
    int			i, year;
    char		*dp;

    /*
     * Search for the RSD PTR signature.
     */
    if ((sigaddr = bios_sigsearch(0, "RSD PTR ", 8, 16, 0)) == 0)
	return(NULL);

    /* get a virtual pointer to the structure */
    rsdp = (struct ACPIrsdp *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
    for (cv = (u_int8_t *)rsdp, ck = 0, i = 0; i < sizeof(struct ACPIrsdp); i++) {
	ck += cv[i];
    }
	
    /*
     * Fail if the checksum doesn't match.
     */
    if (ck != 0) {
	printf("ACPI: Bad ACPI BIOS data checksum\n");
	return(NULL);
    }
    
    /*
     * Fail if the BIOS is too old to be trustworthy.
     * XXX we should check the ACPI BIOS blacklist/goodlist here.
     */
    dp = (char *)(uintptr_t)BIOS_PADDRTOVADDR(0xffff5);
    year = ((*(dp + 6) - '0') * 10) + (*(dp + 7) - '0');
    if (year < 70)
	year += 100;
    if (year < 99) {
	printf("ACPI: BIOS too old (%.8s < 01/01/99)\n", dp);
	return(NULL);
    }

    return(rsdp);
}

/*
 * Find and map all memory regions that are regarded as belonging to ACPI
 * and let the MI code know about them.  Scan the ACPI memory map as managed
 * by the MI code and map it into kernel space.
 */
void
acpi_mapmem(void)
{
    struct vm86frame	vmf;
    struct vm86context	vmc;
    struct bios_smap	*smap;
    vm_offset_t		va;
    int			i;

    bzero(&vmf, sizeof(struct vm86frame));

    acpi_init_addr_range();

    /*
     * Scan memory map with INT 15:E820
     */
    vmc.npages = 0;
    smap = (void *)vm86_addpage(&vmc, 1, KERNBASE + (1 << PAGE_SHIFT));
    vm86_getptr(&vmc, (vm_offset_t)smap, &vmf.vmf_es, &vmf.vmf_di);

    vmf.vmf_ebx = 0;
    do {
	vmf.vmf_eax = 0xE820;
	vmf.vmf_edx = SMAP_SIG;
	vmf.vmf_ecx = sizeof(struct bios_smap);
	i = vm86_datacall(0x15, &vmf, &vmc);
	if (i || vmf.vmf_eax != SMAP_SIG)
	    break;

	/* ACPI-owned memory? */
	if (smap->type == 0x03 || smap->type == 0x04) {
	    acpi_register_addr_range(smap->base, smap->length, smap->type);
	}
    } while (vmf.vmf_ebx != 0);

    /*
     * Map the physical ranges that have been registered into the kernel.
     */
    for (i = 0; i < acpi_addr.entries; i++) {
	va = (vm_offset_t)pmap_mapdev(acpi_addr.t[i].pa_base, acpi_addr.t[i].size);
	acpi_addr.t[i].va_base = va;
    }
}
