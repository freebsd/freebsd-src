/*
 * Copyright (c) 1993, David Greenman
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
 *	This product includes software developed by David Greenman
 * 4. The name of the developer may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: imgact_aout.c,v 1.1 1993/12/20 16:16:45 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "resourcevar.h"
#include "exec.h"
#include "mman.h"
#include "imgact.h"
#include "kernel.h"

#include "vm/vm.h"

int
exec_aout_imgact(iparams)
	struct image_params *iparams;
{
	struct exec *a_out = (struct exec *) iparams->image_header;
	struct vmspace *vmspace = iparams->proc->p_vmspace;
	unsigned long vmaddr, virtual_offset, file_offset;
	unsigned long bss_size;
	int error, len;

	/*
	 * Set file/virtual offset based on a.out variant.
	 *	We do two cases: host byte order and network byte order
	 *	(for NetBSD compatibility)
	 */
	switch ((int)(a_out->a_magic & 0xffff)) {
	case ZMAGIC:
		virtual_offset = 0;
		if (a_out->a_text) {
			file_offset = NBPG;
		} else {
			/* Bill's "screwball mode" */
			file_offset = 0;
		}
		break;
	case QMAGIC:
		virtual_offset = NBPG;
		file_offset = 0;
		break;
	default:
		/* NetBSD compatibility */
		switch ((int)(ntohl(a_out->a_magic) & 0xffff)) {
		case ZMAGIC:
		case QMAGIC:
			virtual_offset = NBPG;
			file_offset = 0;
			break;
		default:
			return (-1);
		}
	}

	bss_size = roundup(a_out->a_bss, NBPG);

	/*
	 * Check various fields in header for validity/bounds.
	 */
	if (/* entry point must lay with text region */
	    a_out->a_entry < virtual_offset ||
	    a_out->a_entry >= virtual_offset + a_out->a_text ||

	    /* text and data size must each be page rounded */
	    a_out->a_text % NBPG ||
	    a_out->a_data % NBPG)
		return (-1);

	/* text + data can't exceed file size */
	if (a_out->a_data + a_out->a_text > iparams->attr->va_size)
		return (EFAULT);

	/*
	 * text/data/bss must not exceed limits
	 */
	if (/* text can't exceed maximum text size */
	    a_out->a_text > MAXTSIZ ||

	    /* data + bss can't exceed maximum data size */
	    a_out->a_data + bss_size > MAXDSIZ ||

	    /* data + bss can't exceed rlimit */
	    a_out->a_data + bss_size >
		iparams->proc->p_rlimit[RLIMIT_DATA].rlim_cur)
			return (ENOMEM);

	/* copy in arguments and/or environment from old process */
	error = exec_extract_strings(iparams);
	if (error)
		return (error);

	/*
	 * Destroy old process VM and create a new one (with a new stack)
	 */
	exec_new_vmspace(iparams);

	/*
	 * Map text read/execute
	 */
	vmaddr = virtual_offset;
	error =
	    vm_mmap(&vmspace->vm_map,			/* map */
		&vmaddr,				/* address */
		a_out->a_text,				/* size */
		VM_PROT_READ | VM_PROT_EXECUTE,		/* protection */
		VM_PROT_READ | VM_PROT_EXECUTE,		/* max protection */
		MAP_FILE | MAP_PRIVATE | MAP_FIXED,	/* flags */
		iparams->vnodep,			/* vnode */
		file_offset);				/* offset */
	if (error)
		return (error);

	/*
	 * Map data read/write (if text is 0, assume text is in data area
	 *	[Bill's screwball mode])
	 */
	vmaddr = virtual_offset + a_out->a_text;
	error =
	    vm_mmap(&vmspace->vm_map,
		&vmaddr,
		a_out->a_data,
		VM_PROT_READ | VM_PROT_WRITE | (a_out->a_text ? 0 : VM_PROT_EXECUTE),
		VM_PROT_READ | VM_PROT_WRITE | (a_out->a_text ? 0 : VM_PROT_EXECUTE),
		MAP_FILE | MAP_PRIVATE | MAP_FIXED,
		iparams->vnodep,
		file_offset + a_out->a_text);
	if (error)
		return (error);

	/*
	 * Allocate demand-zeroed area for uninitialized data
	 * "bss" = 'block started by symbol' - named after the IBM 7090
	 *	instruction of the same name.
	 */
	vmaddr = virtual_offset + a_out->a_text + a_out->a_data;
	error = vm_allocate(&vmspace->vm_map, &vmaddr, bss_size, FALSE);
	if (error)
		return (error);

	/* Fill in process VM information */
	vmspace->vm_tsize = a_out->a_text >> PAGE_SHIFT;
	vmspace->vm_dsize = (a_out->a_data + bss_size) >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t) virtual_offset;
	vmspace->vm_daddr = (caddr_t) virtual_offset + a_out->a_text;

	/* Fill in image_params */
	iparams->interpreted = 0;
	iparams->entry_addr = a_out->a_entry;
	
	return (0);
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 * Since `const' objects end up in the text segment, TEXT_SET is the
 * correct directive to use.
 */
static const struct execsw aout_execsw = { exec_aout_imgact };
TEXT_SET(execsw_set, aout_execsw);

