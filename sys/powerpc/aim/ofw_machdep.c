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

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <machine/powerpc.h>

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
mem_regions(struct mem_region **memp, struct mem_region **availp)
{
	int phandle /*, i, j, cnt*/;
	
	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1
	    || OF_getprop(phandle, "reg",
			  OFmem, sizeof OFmem[0] * OFMEM_REGIONS)
	       <= 0
	    || OF_getprop(phandle, "available",
			  OFavail, sizeof OFavail[0] * OFMEM_REGIONS)
	       <= 0)
		panic("no memory?");
	*memp = OFmem;
	*availp = OFavail;
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

	if (pmap_bootstrapped) {
		__asm __volatile("mfsr %0,0" : "=r"(srsave[0]));
		__asm __volatile("mfsr %0,1" : "=r"(srsave[1]));
		__asm __volatile("mfsr %0,2" : "=r"(srsave[2]));
		__asm __volatile("mfsr %0,3" : "=r"(srsave[3]));
		__asm __volatile("mfsr %0,4" : "=r"(srsave[4]));
		__asm __volatile("mfsr %0,5" : "=r"(srsave[5]));
		__asm __volatile("mfsr %0,6" : "=r"(srsave[6]));
		__asm __volatile("mfsr %0,7" : "=r"(srsave[7]));
		__asm __volatile("mfsr %0,8" : "=r"(srsave[8]));
		__asm __volatile("mfsr %0,9" : "=r"(srsave[9]));
		__asm __volatile("mfsr %0,10" : "=r"(srsave[10]));
		__asm __volatile("mfsr %0,11" : "=r"(srsave[11]));
		__asm __volatile("mfsr %0,12" : "=r"(srsave[12]));
		__asm __volatile("mfsr %0,13" : "=r"(srsave[13]));
		__asm __volatile("mfsr %0,14" : "=r"(srsave[14]));
		__asm __volatile("mfsr %0,15" : "=r"(srsave[15]));

		__asm __volatile("mtsr 0,%0" :: "r"(ofw_pmap.pm_sr[0]));
		__asm __volatile("mtsr 1,%0" :: "r"(ofw_pmap.pm_sr[1]));
		__asm __volatile("mtsr 2,%0" :: "r"(ofw_pmap.pm_sr[2]));
		__asm __volatile("mtsr 3,%0" :: "r"(ofw_pmap.pm_sr[3]));
		__asm __volatile("mtsr 4,%0" :: "r"(ofw_pmap.pm_sr[4]));
		__asm __volatile("mtsr 5,%0" :: "r"(ofw_pmap.pm_sr[5]));
		__asm __volatile("mtsr 6,%0" :: "r"(ofw_pmap.pm_sr[6]));
		__asm __volatile("mtsr 7,%0" :: "r"(ofw_pmap.pm_sr[7]));
		__asm __volatile("mtsr 8,%0" :: "r"(ofw_pmap.pm_sr[8]));
		__asm __volatile("mtsr 9,%0" :: "r"(ofw_pmap.pm_sr[9]));
		__asm __volatile("mtsr 10,%0" :: "r"(ofw_pmap.pm_sr[10]));
		__asm __volatile("mtsr 11,%0" :: "r"(ofw_pmap.pm_sr[11]));
		__asm __volatile("mtsr 12,%0" :: "r"(ofw_pmap.pm_sr[12]));
		__asm __volatile("mtsr 13,%0" :: "r"(ofw_pmap.pm_sr[13]));
		__asm __volatile("mtsr 14,%0" :: "r"(ofw_pmap.pm_sr[14]));
		__asm __volatile("mtsr 15,%0" :: "r"(ofw_pmap.pm_sr[15]));
	}

	ofmsr |= PSL_RI|PSL_EE;

	__asm __volatile(	"\t"
		"sync\n\t"
		"mfmsr  %0\n\t"
		"mtmsr  %1\n\t"
		"isync\n"
		: "=r" (oldmsr)
		: "r" (ofmsr)
	);

	result = ofwcall(args);

	__asm(	"\t"
		"mtmsr  %0\n\t"
		"isync\n"
		: : "r" (oldmsr)
	);

	if (pmap_bootstrapped) {
		__asm __volatile("mtsr 0,%0" :: "r"(srsave[0]));
		__asm __volatile("mtsr 1,%0" :: "r"(srsave[1]));
		__asm __volatile("mtsr 2,%0" :: "r"(srsave[2]));
		__asm __volatile("mtsr 3,%0" :: "r"(srsave[3]));
		__asm __volatile("mtsr 4,%0" :: "r"(srsave[4]));
		__asm __volatile("mtsr 5,%0" :: "r"(srsave[5]));
		__asm __volatile("mtsr 6,%0" :: "r"(srsave[6]));
		__asm __volatile("mtsr 7,%0" :: "r"(srsave[7]));
		__asm __volatile("mtsr 8,%0" :: "r"(srsave[8]));
		__asm __volatile("mtsr 9,%0" :: "r"(srsave[9]));
		__asm __volatile("mtsr 10,%0" :: "r"(srsave[10]));
		__asm __volatile("mtsr 11,%0" :: "r"(srsave[11]));
		__asm __volatile("mtsr 12,%0" :: "r"(srsave[12]));
		__asm __volatile("mtsr 13,%0" :: "r"(srsave[13]));
		__asm __volatile("mtsr 14,%0" :: "r"(srsave[14]));
		__asm __volatile("mtsr 15,%0" :: "r"(srsave[15]));
		__asm __volatile("sync");
	}


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
