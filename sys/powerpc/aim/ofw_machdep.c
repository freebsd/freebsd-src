/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: ofw_machdep.c,v 1.5 2000/05/23 13:25:43 tsubai Exp $
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <net/ethernet.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <machine/powerpc.h>
#include <machine/ofw_machdep.h>
#include <powerpc/ofw/ofw_pci.h>

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];

extern long	ofmsr;
extern struct	pmap ofw_pmap;
extern int	pmap_bootstrapped;
static int	(*ofwcall)(void *);

/*
 * This is called during powerpc_init, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
mem_regions(struct mem_region **memp, int *memsz,
		struct mem_region **availp, int *availsz)
{
	int phandle;
	int asz, msz; 
	
	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1
	    || (msz = OF_getprop(phandle, "reg",
			  OFmem, sizeof OFmem[0] * OFMEM_REGIONS))
	       <= 0
	    || (asz = OF_getprop(phandle, "available",
			  OFavail, sizeof OFavail[0] * OFMEM_REGIONS))
	       <= 0)
		panic("no memory?");
	*memp = OFmem;
	*memsz = msz / sizeof(struct mem_region);
	*availp = OFavail;
	*availsz = asz / sizeof(struct mem_region);
}

void
set_openfirm_callback(int (*openfirm)(void *))
{

	ofwcall = openfirm;
}

int
openfirmware(void *args)
{
	long	oldmsr;
	int	result;
	u_int	srsave[16];
	u_int   i;

	__asm __volatile(	"\t"
		"sync\n\t"
		"mfmsr  %0\n\t"
		"mtmsr  %1\n\t"
		"isync\n"
		: "=r" (oldmsr)
		: "r" (ofmsr)
	);

	if (pmap_bootstrapped) {
		/*
		 * Swap the kernel's address space with OpenFirmware's
		 */
		for (i = 0; i < 16; i++) {
			srsave[i] = mfsrin(i << ADDR_SR_SHFT);
			mtsrin(i << ADDR_SR_SHFT, ofw_pmap.pm_sr[i]);
		}

		/*
		 * Clear battable[] translations
		 */
		__asm __volatile("mtdbatu 2, %0\n"
				 "mtdbatu 3, %0" : : "r" (0));
		isync();
	}
	
	result = ofwcall(args);

	if (pmap_bootstrapped) {
		/*
		 * Restore the kernel's addr space. The isync() doesn;t
		 * work outside the loop unless mtsrin() is open-coded
		 * in an asm statement :(
		 */
		for (i = 0; i < 16; i++) {
			mtsrin(i << ADDR_SR_SHFT, srsave[i]);
			isync();
		}
	}

	__asm(	"\t"
		"mtmsr  %0\n\t"
		"isync\n"
		: : "r" (oldmsr)
	);

	return (result);
}

void
ppc_exit()
{

	OF_exit();
}

void
ppc_boot(str)
	char *str;
{

	OF_boot(str);
}

void
OF_getetheraddr(device_t dev, u_char *addr)
{
	phandle_t	node;

	node = ofw_pci_find_node(dev);
	OF_getprop(node, "local-mac-address", addr, ETHER_ADDR_LEN);
}
