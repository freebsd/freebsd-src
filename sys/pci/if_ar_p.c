/*
 * Copyright (c) 1999 John Hay.
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

#include "ar.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <pci/pcivar.h>

/*
 * The must match with the real functions in if_ar.c
 */
extern void *arattach_pci(int unit, vm_offset_t mem_vaddr);
extern void arintr_hc(void *hc);

static const char *ar_pci_probe(pcici_t tag, pcidi_t type);
static void ar_pci_attach(pcici_t config_id, int unit);

static u_long arc_count = NAR;

static struct pci_device ar_pci_driver =
{
	"ar",
	ar_pci_probe,
	ar_pci_attach,
	&arc_count,
	NULL
};

COMPAT_PCI_DRIVER (ar_pci, ar_pci_driver);

static const char *
ar_pci_probe(pcici_t tag, pcidi_t type)
{
	switch(type) {
	case 0x5012114f:
		return ("Digi SYNC/570i-PCI 2 port");
		break;
	case 0x5010114f:
		printf("Digi SYNC/570i-PCI 2 port (mapped below 1M)\n");
		printf("Please change the jumper to select linear mode.\n");
		break;
	case 0x5013114f:
		return ("Digi SYNC/570i-PCI 4 port");
		break;
	case 0x5011114f:
		printf("Digi SYNC/570i-PCI 4 port (mapped below 1M)\n");
		printf("Please change the jumper to select linear mode.\n");
		break;
	default:
		break;
	}
	return (0);
}

static void
ar_pci_attach(pcici_t config_id, int unit)
{
	u_char *inten;
	void *hc;
	vm_offset_t mem_vaddr, mem_paddr;
	vm_offset_t plx_vaddr, plx_paddr;

	if(!pci_map_mem(config_id, 0x10, &plx_vaddr, &plx_paddr)) {
		printf("arp: map failed.\n");
		return;
	}

	if(!pci_map_mem(config_id, 0x18, &mem_vaddr, &mem_paddr)) {
		printf("arp: map failed.\n");
		return;
	}

	hc = arattach_pci(unit, mem_vaddr);
	if(!hc)
		return;

	/* Magic to enable the card to generate interrupts. */
	inten = (u_char *)plx_vaddr;
	inten[0x69] = 0x09;

	if(!pci_map_int(config_id, arintr_hc, (void *)hc, &net_imask)) {
		free(hc, M_DEVBUF);
		return;
	}
}
