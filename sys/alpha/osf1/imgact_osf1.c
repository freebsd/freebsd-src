/*
 * Copyright (c) 1998-1999 Andrew Gallatin
 * All rights reserved.
 *
 * Based heavily on imgact_linux.c which is 
 * Copyright (c) 1994-1996 Søren Schmidt.  
 * Which in turn is based heavily on /sys/kern/imgact_aout.c which is:
 * Copyright (c) 1993, David Greenman
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/malloc.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/resourcevar.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <alpha/osf1/exec_ecoff.h>
extern struct sysentvec osf1_sysvec;

#ifdef DEBUG
#define	DPRINTF(a)	printf a;
#else
#define	DPRINTF(a)
#endif

static int
exec_osf1_imgact(struct image_params *imgp)
{
	int error;
	int path_not_saved;
	size_t bytes;
	const struct ecoff_exechdr *execp;
	const struct ecoff_aouthdr *eap;
        struct vmspace *vmspace;
	vm_offset_t  baddr;
	vm_offset_t  bsize;
	vm_offset_t  bss_start;
        vm_offset_t  daddr;
	vm_offset_t  dend; 
        vm_offset_t  dsize;
	vm_offset_t  raw_dend;
        vm_offset_t  taddr;
	vm_offset_t  tend; 
        vm_offset_t  tsize;
	struct nameidata *ndp;
	Osf_Auxargs *osf_auxargs;

	GIANT_REQUIRED;

	execp = (const struct ecoff_exechdr*)imgp->image_header;
	eap = &execp->a;
	ndp = NULL;

/* check to make sure we have an alpha ecoff executable */
	if (ECOFF_BADMAG(execp))
		return -1;

/* verfify it an OSF/1 exectutable */
	if (eap->magic != ECOFF_ZMAGIC) {
		printf("unknown ecoff magic %x\n", eap->magic);
		return ENOEXEC;
	}
	osf_auxargs = malloc(sizeof(Osf_Auxargs), M_TEMP, M_WAITOK | M_ZERO);
	imgp->auxargs = osf_auxargs;
	osf_auxargs->executable = osf_auxargs->exec_path;
	path_not_saved = copyinstr(imgp->fname, osf_auxargs->executable,
	    PATH_MAX, &bytes);
	if (execp->f.f_flags & DYNAMIC_FLAG) {
		if (path_not_saved) {
			uprintf("path to dynamic exectutable not found\n");
			free(imgp->auxargs, M_TEMP);
			return(path_not_saved);
		}
		/* 
		 *  Unmap the executable & attempt to slide in
		 *  /sbin/loader in its place.
		 */
		if (imgp->firstpage)
			exec_unmap_first_page(imgp);

		/* 
		 * Replicate what execve does, and map the first
		 * page of the loader.
		 */
		ndp = (struct nameidata *)malloc(sizeof(struct nameidata),
		    M_TEMP, M_WAITOK);
		NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME, UIO_SYSSPACE,
		    "/compat/osf1/sbin/loader",
		    FIRST_THREAD_IN_PROC(imgp->proc));
		error = namei(ndp);
		if (error) {
			uprintf("imgact_osf1: can't read /compat/osf1/sbin/loader\n");
			free(imgp->auxargs, M_TEMP);
			return(error);
		} 
		if (imgp->vp) {
			vrele(imgp->vp);
		/* leaking in the nameizone ??? XXX */
		}
		imgp->vp = ndp->ni_vp;
		error = exec_map_first_page(imgp);
		VOP_UNLOCK(imgp->vp, 0, FIRST_THREAD_IN_PROC(imgp->proc));
		osf_auxargs->loader = "/compat/osf1/sbin/loader";
	}

	execp = (const struct ecoff_exechdr*)imgp->image_header;
	eap = &execp->a;
	taddr = ECOFF_SEGMENT_ALIGN(execp, eap->text_start);
	tend  = round_page(eap->text_start + eap->tsize);
	tsize = tend - taddr;

	daddr = ECOFF_SEGMENT_ALIGN(execp, eap->data_start);
	dend  = round_page(eap->data_start + eap->dsize);
	dsize = dend - daddr;

	bss_start = ECOFF_SEGMENT_ALIGN(execp, eap->bss_start);
	bsize = eap->bsize;

	imgp->entry_addr = eap->entry;
	/* copy in arguments and/or environment from old process */

	error = exec_extract_strings(imgp);
	if (error)
		goto bail;

	/*
	 * Destroy old process VM and create a new one (with a new stack).
	 */
	exec_new_vmspace(imgp, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS, USRSTACK);

	/*
	 * The vm space can now be changed.
	 */
	vmspace = imgp->proc->p_vmspace;

	imgp->interpreted = 0;
	imgp->proc->p_sysent = &osf1_sysvec;

	if ((eap->tsize != 0 || eap->dsize != 0) &&
	    imgp->vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (imgp->vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	imgp->vp->v_flag |= VTEXT;

	/* set up text segment */
	if ((error = vm_mmap(&vmspace->vm_map, &taddr, tsize,
	    VM_PROT_READ|VM_PROT_EXECUTE, VM_PROT_ALL, MAP_FIXED|MAP_COPY,
	    (caddr_t)imgp->vp, ECOFF_TXTOFF(execp)))) {
		DPRINTF(("%s(%d): error = %d\n", __FILE__, __LINE__, error));
		return error;
	}
	/* .. data .. */
	if ((error = vm_mmap(&vmspace->vm_map, &daddr, dsize,
	    VM_PROT_READ|VM_PROT_EXECUTE|VM_PROT_WRITE, VM_PROT_ALL,
	    MAP_FIXED|MAP_COPY, (caddr_t)imgp->vp, ECOFF_DATOFF(execp)))) {
		DPRINTF(("%s(%d): error = %d\n", __FILE__, __LINE__, error));
		goto bail;
	}	
	/* .. bss .. */
	if (round_page(bsize)) {
		baddr = bss_start;
		if ((error = vm_map_find(&vmspace->vm_map, NULL,
		    (vm_offset_t) 0, &baddr, round_page(bsize), FALSE,
		    VM_PROT_ALL, VM_PROT_ALL, FALSE))) {
			DPRINTF(("%s(%d): error = %d\n", __FILE__, __LINE__,
			    error));
			goto bail;

		}
	}
	

	raw_dend = (eap->data_start + eap->dsize);
	if (dend > raw_dend) {
		caddr_t zeros;
		zeros = malloc(dend-raw_dend,M_TEMP,M_WAITOK|M_ZERO);
		if ((error = copyout(zeros, (caddr_t)raw_dend,
		    dend-raw_dend))) {
			uprintf("Can't zero start of bss, error %d\n",error);
			free(zeros,M_TEMP);
			goto bail;
		}
		free(zeros,M_TEMP);
			
	}
	vmspace->vm_tsize = btoc(round_page(tsize));
	vmspace->vm_dsize = btoc((round_page(dsize) + round_page(bsize)));
	vmspace->vm_taddr = (caddr_t)taddr;
	vmspace->vm_daddr = (caddr_t)daddr;

	return(0);

 bail:
	free(imgp->auxargs, M_TEMP);
	if (ndp) {
		VOP_CLOSE(ndp->ni_vp, FREAD, imgp->proc->p_ucred,
		    FIRST_THREAD_IN_PROC(imgp->proc));
		vrele(ndp->ni_vp);
	}
	return(error);
}
/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
struct execsw osf1_execsw = { exec_osf1_imgact, "OSF/1 ECOFF" };
EXEC_SET(osf1_ecoff, osf1_execsw);
