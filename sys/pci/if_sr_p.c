/*
 * Copyright (c) 1996 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include "sr.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <pci/pcivar.h>

#ifndef BUGGY
#define BUGGY		0
#endif

/*
 * The must match with the real functions in if_sr.c
 */
extern void *srattach_pci(int unit,
			  vm_offset_t plx_vaddr,
			  vm_offset_t sca_vaddr);
extern void srintr_hc(void *hc);

static const char *sr_pci_probe(pcici_t tag, pcidi_t type);
static void sr_pci_attach(pcici_t config_id, int unit);

static u_long src_count = NSR;

static struct pci_device sr_pci_driver =
{
	"src",
	sr_pci_probe,
	sr_pci_attach,
	&src_count,
	NULL
};

COMPAT_PCI_DRIVER (sr_pci, sr_pci_driver);

static const char *
sr_pci_probe(pcici_t tag, pcidi_t type)
{
	switch(type) {
	case 0x556812aa:
		return ("RISCom/N2pci");
		break;
	case 0x55684778:
	case 0x55684877:
		/*
		 * XXX This can probably be removed sometime.
		 */
		return ("RISCom/N2pci (old id)");
		break;
	default:
		break;
	}
	return (0);
}

static void
sr_pci_attach(pcici_t config_id, int unit)
{
	void *hc;
#if BUGGY > 0
	u_int *fecr;
#endif
	vm_offset_t plx_vaddr, plx_paddr, sca_vaddr, sca_paddr;

#if BUGGY > 0
	printf("srp: ID %x\n", pci_conf_read(config_id, 0));
	printf("srp: BADR0 %x\n", pci_conf_read(config_id, 0x10));
	printf("srp: BADR1 %x\n", pci_conf_read(config_id, 0x18));
#endif
	if(!pci_map_mem(config_id, 0x10, &plx_vaddr, &plx_paddr)) {
		printf("srp: map failed.\n");
		return;
	}
#if BUGGY > 0
	printf("srp: vaddr %x, paddr %x\n", plx_vaddr, plx_paddr);
#endif
	if(!pci_map_mem(config_id, 0x18, &sca_vaddr, &sca_paddr)) {
		printf("srp: map failed.\n");
		return;
	}
#if BUGGY > 0
	printf("srp: vaddr %x, paddr %x\n", sca_vaddr, sca_paddr);
	fecr = (u_int *)(sca_vaddr + 0x200);
	printf("srp: FECR  %x\n", *fecr);
#endif

	hc = srattach_pci(unit, plx_vaddr, sca_vaddr);
	if(!hc)
		return;

	if(!pci_map_int(config_id, srintr_hc, (void *)hc, &net_imask)) {
		free(hc, M_DEVBUF);
		return;
	}
}
