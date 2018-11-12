/*-
 * Copyright 1996-1998 John D. Polstra.
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <machine/elf.h>
#include <machine/md_var.h>

struct sysentvec elf64_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_mask	= 0,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_name	= "FreeBSD ELF64",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings	= exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_LP64,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
};
INIT_SYSENTVEC(elf64_sysvec, &elf64_freebsd_sysvec);

static Elf64_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_RISCV,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf64_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

SYSINIT(elf64, SI_SUB_EXEC, SI_ORDER_FIRST,
	(sysinit_cfunc_t) elf64_insert_brand_entry,
	&freebsd_brand_info);

static Elf64_Brandinfo freebsd_brand_oinfo = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_RISCV,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/usr/libexec/ld-elf.so.1",
	.sysvec		= &elf64_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf64_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

SYSINIT(oelf64, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf64_insert_brand_entry,
	&freebsd_brand_oinfo);

void
elf64_dump_thread(struct thread *td, void *dst, size_t *off)
{

}

/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{

	panic("elf_reloc_internal");
}

int
elf_reloc(linker_file_t lf, Elf_Addr relocbase, const void *data, int type,
    elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 0, lookup));
}

int
elf_reloc_local(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 1, lookup));
}

int
elf_cpu_load_file(linker_file_t lf __unused)
{

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
