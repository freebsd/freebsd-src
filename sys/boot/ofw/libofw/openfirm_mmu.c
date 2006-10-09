/*
 * Copyright (c) 2006 Kip Macy
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
 * ARE DISCLAIMED. IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/stdarg.h>
#include <stand.h>

#include "openfirm.h"

static ihandle_t
OF_mmu_ihandle(void)
{
	static ihandle_t immu;

	if (immu != (ihandle_t)0)
		return (immu);

	if (OF_getproplen(OF_chosennode(), "mmu") != sizeof (ihandle_t))
		return (immu = (ihandle_t)-1);

	(void) OF_getprop(OF_chosennode(), "mmu", (caddr_t)(&immu), sizeof immu);
	return (immu);
}


int
OF_translate_virt(vm_offset_t va, int *valid, vm_paddr_t *physaddr, int *mode)
{
	int rv;
	static struct {
		cell_t	 name;
		cell_t	 nargs;
		cell_t	 nreturns;
		cell_t   method;
		cell_t	 immu;
		cell_t	 va;
		cell_t   result;
		cell_t   valid;
		cell_t   mode;
		cell_t   phys_hi;
		cell_t   phys_lo;
	} args = {
		(cell_t)"call-method",
		3,
		5,
		(cell_t)"translate",
	};

	args.immu = (cell_t) OF_mmu_ihandle();
	args.result = 0;
	args.valid = 0;
	args.mode = 0;
	rv = openfirmware(&args);
	if (rv == -1)
		return (-1);
	if (args.result != 0)
		return (-1);
	
	*valid = args.valid;
	*mode = args.mode;
	*physaddr = (vm_paddr_t)(args.phys_hi << 32 | args.phys_lo);

	return (0);
}

vm_paddr_t 
OF_vtophys(vm_offset_t va)
{
	int mode, valid, error;
	vm_paddr_t physaddr;

	error = OF_translate_virt(va, &valid, &physaddr, &mode);
	
	if (error == 0 && valid == -1)
		return physaddr;
	
	return (0);
}		

int
OF_map_phys(int mode, size_t size, vm_offset_t va, uint64_t pa)
{
	int rv;

	static struct {
		cell_t	 name;
		cell_t	 nargs;
		cell_t	 nreturns;
		cell_t   method;
		cell_t	 immu;
		cell_t	 mode;
		cell_t   size;
		cell_t   va;
		cell_t   pa_hi;
		cell_t   pa_lo;
		cell_t   result;
	} args = {
		(cell_t)"call-method",
		7,
		1,
		(cell_t)"map",
	};
	args.immu = (cell_t)OF_mmu_ihandle();
	args.mode = (cell_t)mode;
	args.size = (cell_t)size;
	args.va   = (cell_t)va;
	args.pa_hi = (cell_t)(pa >> 32);
	args.pa_lo = (cell_t)((uint32_t)pa);

	rv = openfirmware(&args);
	if (rv == -1)
		return (-1);
	if (args.result != 0)
		return (-1);

	return (0);
}
