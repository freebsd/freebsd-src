/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019 Justin Hibbits
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

static int
__elfN(powerpc_copyout_auxargs)(struct image_params *imgp, uintptr_t base)
{
	Elf_Auxargs *args;
	Elf_Auxinfo *argarray, *pos;
	int error;

	/*
	 * XXX If we can't find image's OSREL, assume it uses the new auxv
	 * format.
	 *
	 * This is specially important for rtld, that is not tagged. Using
	 * direct exec mode with new (ELFv2) binaries that expect the new auxv
	 * format would result in crashes otherwise.
	 *
	 * Unfortunately, this may break direct exec'ing old binaries,
	 * but it seems better to correctly support new binaries by default,
	 * considering the transition to ELFv2 happened quite some time
	 * ago. If needed, a sysctl may be added to allow old auxv format to
	 * be used when OSREL is not found.
	 */
	if (imgp->proc->p_osrel >= P_OSREL_POWERPC_NEW_AUX_ARGS ||
	    imgp->proc->p_osrel == 0)
		return (__elfN(freebsd_copyout_auxargs)(imgp, base));

	args = (Elf_Auxargs *)imgp->auxargs;
	argarray = pos = malloc(AT_OLD_COUNT * sizeof(*pos), M_TEMP,
	    M_WAITOK | M_ZERO);

	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_OLD_EXECFD, args->execfd);
	AUXARGS_ENTRY(pos, AT_OLD_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_OLD_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_OLD_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_OLD_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_OLD_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_OLD_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_OLD_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_OLD_EHDRFLAGS, args->hdr_eflags);
	if (imgp->execpathp != 0)
		AUXARGS_ENTRY_PTR(pos, AT_OLD_EXECPATH, imgp->execpathp);
	AUXARGS_ENTRY(pos, AT_OLD_OSRELDATE,
	    imgp->proc->p_ucred->cr_prison->pr_osreldate);
	if (imgp->canary != 0) {
		AUXARGS_ENTRY_PTR(pos, AT_OLD_CANARY, imgp->canary);
		AUXARGS_ENTRY(pos, AT_OLD_CANARYLEN, imgp->canarylen);
	}
	AUXARGS_ENTRY(pos, AT_OLD_NCPUS, mp_ncpus);
	if (imgp->pagesizes != 0) {
		AUXARGS_ENTRY_PTR(pos, AT_OLD_PAGESIZES, imgp->pagesizes);
		AUXARGS_ENTRY(pos, AT_OLD_PAGESIZESLEN, imgp->pagesizeslen);
	}
	if (imgp->sysent->sv_timekeep_base != 0) {
		AUXARGS_ENTRY(pos, AT_OLD_TIMEKEEP,
		    imgp->sysent->sv_timekeep_base);
	}
	AUXARGS_ENTRY(pos, AT_OLD_STACKPROT, imgp->sysent->sv_shared_page_obj
	    != NULL && imgp->stack_prot != 0 ? imgp->stack_prot :
	    imgp->sysent->sv_stackprot);
	if (imgp->sysent->sv_hwcap != NULL)
		AUXARGS_ENTRY(pos, AT_OLD_HWCAP, *imgp->sysent->sv_hwcap);
	if (imgp->sysent->sv_hwcap2 != NULL)
		AUXARGS_ENTRY(pos, AT_OLD_HWCAP2, *imgp->sysent->sv_hwcap2);
	AUXARGS_ENTRY(pos, AT_OLD_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;
	KASSERT(pos - argarray <= AT_OLD_COUNT, ("Too many auxargs"));

	error = copyout(argarray, (void *)base, sizeof(*argarray) * AT_OLD_COUNT);
	free(argarray, M_TEMP);
	return (error);
}
