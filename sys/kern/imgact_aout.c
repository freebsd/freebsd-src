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
 *	$Id: imgact_aout.c,v 1.29 1996/08/31 16:52:22 bde Exp $
 */

#include "opt_rlimit.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

static int	exec_aout_imgact __P((struct image_params *imgp));

static int
exec_aout_imgact(imgp)
	struct image_params *imgp;
{
	const struct exec *a_out = (const struct exec *) imgp->image_header;
	struct vmspace *vmspace = imgp->proc->p_vmspace;
	vm_offset_t vmaddr;
	unsigned long virtual_offset;
	unsigned long file_offset;
	unsigned long bss_size;
	int error;

	/*
	 * Linux and *BSD binaries look very much alike,
	 * only the machine id is different:
	 * 0x64 for Linux, 0x86 for *BSD, 0x00 for BSDI.
	 * NetBSD is in network byte order.. ugh.
	 */
	if (((a_out->a_magic >> 16) & 0xff) != 0x86 &&
	    ((a_out->a_magic >> 16) & 0xff) != 0 &&
	    ((((int)ntohl(a_out->a_magic)) >> 16) & 0xff) != 0x86)
                return -1;

	/*
	 * Set file/virtual offset based on a.out variant.
	 *	We do two cases: host byte order and network byte order
	 *	(for NetBSD compatibility)
	 */
	switch ((int)(a_out->a_magic & 0xffff)) {
	case ZMAGIC:
		virtual_offset = 0;
		if (a_out->a_text) {
			file_offset = PAGE_SIZE;
		} else {
			/* Bill's "screwball mode" */
			file_offset = 0;
		}
		break;
	case QMAGIC:
		virtual_offset = PAGE_SIZE;
		file_offset = 0;
		break;
	default:
		/* NetBSD compatibility */
		switch ((int)(ntohl(a_out->a_magic) & 0xffff)) {
		case ZMAGIC:
		case QMAGIC:
			virtual_offset = PAGE_SIZE;
			file_offset = 0;
			break;
		default:
			return (-1);
		}
	}

	bss_size = roundup(a_out->a_bss, PAGE_SIZE);

	/*
	 * Check various fields in header for validity/bounds.
	 */
	if (/* entry point must lay with text region */
	    a_out->a_entry < virtual_offset ||
	    a_out->a_entry >= virtual_offset + a_out->a_text ||

	    /* text and data size must each be page rounded */
	    a_out->a_text & PAGE_MASK || a_out->a_data & PAGE_MASK)
		return (-1);

	/* text + data can't exceed file size */
	if (a_out->a_data + a_out->a_text > imgp->attr->va_size)
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
		imgp->proc->p_rlimit[RLIMIT_DATA].rlim_cur)
			return (ENOMEM);

	/* copy in arguments and/or environment from old process */
	error = exec_extract_strings(imgp);
	if (error)
		return (error);

	/*
	 * Destroy old process VM and create a new one (with a new stack)
	 */
	exec_new_vmspace(imgp);

	/*
	 * Map text/data read/execute
	 */
	vmaddr = virtual_offset;
	error =
	    vm_mmap(&vmspace->vm_map,			/* map */
		&vmaddr,				/* address */
		a_out->a_text + a_out->a_data,		/* size */
		VM_PROT_READ | VM_PROT_EXECUTE,		/* protection */
		VM_PROT_ALL,				/* max protection */
		MAP_PRIVATE | MAP_FIXED,		/* flags */
		(caddr_t)imgp->vp,			/* vnode */
		file_offset);				/* offset */
	if (error)
		return (error);

	/*
	 * allow writing of data
	 */
	vm_map_protect(&vmspace->vm_map,
		vmaddr + a_out->a_text,
		vmaddr + a_out->a_text + a_out->a_data,
		VM_PROT_ALL,
		FALSE);

	if (bss_size != 0) {
		/*
		 * Allocate demand-zeroed area for uninitialized data
		 * "bss" = 'block started by symbol' - named after the IBM 7090
		 *	instruction of the same name.
		 */
		vmaddr = virtual_offset + a_out->a_text + a_out->a_data;
		error = vm_map_find(&vmspace->vm_map, NULL, 0, &vmaddr, bss_size, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0);
		if (error)
			return (error);
	}

	/* Fill in process VM information */
	vmspace->vm_tsize = a_out->a_text >> PAGE_SHIFT;
	vmspace->vm_dsize = (a_out->a_data + bss_size) >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t) virtual_offset;
	vmspace->vm_daddr = (caddr_t) virtual_offset + a_out->a_text;

	/* Fill in image_params */
	imgp->interpreted = 0;
	imgp->entry_addr = a_out->a_entry;

	imgp->proc->p_sysent = &aout_sysvec;

	/* Indicate that this file should not be modified */
	imgp->vp->v_flag |= VTEXT;

	return (0);
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 * Since `const' objects end up in the text segment, TEXT_SET is the
 * correct directive to use.
 */
static const struct execsw aout_execsw = { exec_aout_imgact, "a.out" };
TEXT_SET(execsw_set, aout_execsw);
