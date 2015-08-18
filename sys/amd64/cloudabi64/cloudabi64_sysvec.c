/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
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
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysent.h>
#include <sys/systm.h>

#include <vm/pmap.h>
#include <vm/vm.h>

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/vmparam.h>

#include <compat/cloudabi/cloudabi_util.h>

#include <compat/cloudabi64/cloudabi64_syscall.h>
#include <compat/cloudabi64/cloudabi64_syscalldefs.h>
#include <compat/cloudabi64/cloudabi64_util.h>

extern const char *cloudabi64_syscallnames[];
extern struct sysent cloudabi64_sysent[];

static register_t *
cloudabi64_copyout_strings(struct image_params *imgp)
{
	uintptr_t begin;
	size_t len;

	/* Copy out program arguments. */
	len = imgp->args->begin_envv - imgp->args->begin_argv;
	begin = rounddown2(USRSTACK - len, sizeof(register_t));
	copyout(imgp->args->begin_argv, (void *)begin, len);
	return ((register_t *)begin);
}

static int
cloudabi64_fixup(register_t **stack_base, struct image_params *imgp)
{
	char canarybuf[64];
	Elf64_Auxargs *args;
	struct thread *td;
	void *argdata, *canary;
	size_t argdatalen;
	int error;

	/*
	 * CloudABI executables do not store the FreeBSD OS release
	 * number in their header. Set the OS release number to the
	 * latest version of FreeBSD, so that system calls behave as if
	 * called natively.
	 */
	td = curthread;
	td->td_proc->p_osrel = __FreeBSD_version;

	/* Store canary for stack smashing protection. */
	argdata = *stack_base;
	arc4rand(canarybuf, sizeof(canarybuf), 0);
	*stack_base -= howmany(sizeof(canarybuf), sizeof(register_t));
	canary = *stack_base;
	error = copyout(canarybuf, canary, sizeof(canarybuf));
	if (error != 0)
		return (error);

	/*
	 * Compute length of program arguments. As the argument data is
	 * binary safe, we had to add a trailing null byte in
	 * exec_copyin_data_fds(). Undo this by reducing the length.
	 */
	args = (Elf64_Auxargs *)imgp->auxargs;
	argdatalen = imgp->args->begin_envv - imgp->args->begin_argv;
	if (argdatalen > 0)
		--argdatalen;

	/* Write out an auxiliary vector. */
	cloudabi64_auxv_t auxv[] = {
#define	VAL(type, val)	{ .a_type = (type), .a_val = (val) }
#define	PTR(type, ptr)	{ .a_type = (type), .a_ptr = (uintptr_t)(ptr) }
		PTR(CLOUDABI_AT_ARGDATA, argdata),
		VAL(CLOUDABI_AT_ARGDATALEN, argdatalen),
		PTR(CLOUDABI_AT_CANARY, canary),
		VAL(CLOUDABI_AT_CANARYLEN, sizeof(canarybuf)),
		VAL(CLOUDABI_AT_NCPUS, mp_ncpus),
		VAL(CLOUDABI_AT_PAGESZ, args->pagesz),
		PTR(CLOUDABI_AT_PHDR, args->phdr),
		VAL(CLOUDABI_AT_PHNUM, args->phnum),
		VAL(CLOUDABI_AT_TID, td->td_tid),
#undef VAL
#undef PTR
		{ .a_type = CLOUDABI_AT_NULL },
	};
	*stack_base -= howmany(sizeof(auxv), sizeof(register_t));
	return (copyout(auxv, *stack_base, sizeof(auxv)));
}

static int
cloudabi64_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct trapframe *frame = td->td_frame;

	/* Obtain system call number. */
	sa->code = frame->tf_rax;
	if (sa->code >= CLOUDABI64_SYS_MAXSYSCALL)
		return (ENOSYS);
	sa->callp = &cloudabi64_sysent[sa->code];

	/* Fetch system call arguments. */
	sa->args[0] = frame->tf_rdi;
	sa->args[1] = frame->tf_rsi;
	sa->args[2] = frame->tf_rdx;
	sa->args[3] = frame->tf_rcx; /* Actually %r10. */
	sa->args[4] = frame->tf_r8;
	sa->args[5] = frame->tf_r9;

	/* Default system call return values. */
	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_rdx;
	return (0);
}

static void
cloudabi64_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame = td->td_frame;

	switch (error) {
	case 0:
		/* System call succeeded. */
		frame->tf_rax = td->td_retval[0];
		frame->tf_rdx = td->td_retval[1];
		frame->tf_rflags &= ~PSL_C;
		break;
	case ERESTART:
		/* Restart system call. */
		frame->tf_rip -= frame->tf_err;
		frame->tf_r10 = frame->tf_rcx;
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
		break;
	case EJUSTRETURN:
		break;
	default:
		/* System call returned an error. */
		frame->tf_rax = cloudabi_convert_errno(error);
		frame->tf_rflags |= PSL_C;
		break;
	}
}

static void
cloudabi64_schedtail(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	/* Initial register values for processes returning from fork. */
	frame->tf_rax = CLOUDABI_PROCESS_CHILD;
	frame->tf_rdx = td->td_tid;
}

void
cloudabi64_thread_setregs(struct thread *td,
    const cloudabi64_threadattr_t *attr)
{
	struct trapframe *frame;
	stack_t stack;

	/* Perform standard register initialization. */
	stack.ss_sp = (void *)attr->stack;
	stack.ss_size = attr->stack_size;
	cpu_set_upcall_kse(td, (void *)attr->entry_point, NULL, &stack);

	/*
	 * Pass in the thread ID of the new thread and the argument
	 * pointer provided by the parent thread in as arguments to the
	 * entry point.
	 */
	frame = td->td_frame;
	frame->tf_rdi = td->td_tid;
	frame->tf_rsi = attr->argument;
}

static struct sysentvec cloudabi64_elf_sysvec = {
	.sv_size		= CLOUDABI64_SYS_MAXSYSCALL,
	.sv_table		= cloudabi64_sysent,
	.sv_fixup		= cloudabi64_fixup,
	.sv_name		= "CloudABI ELF64",
	.sv_coredump		= elf64_coredump,
	.sv_pagesize		= PAGE_SIZE,
	.sv_minuser		= VM_MIN_ADDRESS,
	.sv_maxuser		= VM_MAXUSER_ADDRESS,
	.sv_usrstack		= USRSTACK,
	.sv_stackprot		= VM_PROT_READ | VM_PROT_WRITE,
	.sv_copyout_strings	= cloudabi64_copyout_strings,
	.sv_flags		= SV_ABI_CLOUDABI | SV_CAPSICUM,
	.sv_set_syscall_retval	= cloudabi64_set_syscall_retval,
	.sv_fetch_syscall_args	= cloudabi64_fetch_syscall_args,
	.sv_syscallnames	= cloudabi64_syscallnames,
	.sv_schedtail		= cloudabi64_schedtail,
};

INIT_SYSENTVEC(elf_sysvec, &cloudabi64_elf_sysvec);

static Elf64_Brandinfo cloudabi64_brand = {
	.brand		= ELFOSABI_CLOUDABI,
	.machine	= EM_X86_64,
	.sysvec		= &cloudabi64_elf_sysvec,
	.compat_3_brand	= "CloudABI",
};

static int
cloudabi64_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		if (elf64_insert_brand_entry(&cloudabi64_brand) < 0) {
			printf("Failed to add CloudABI ELF brand handler\n");
			return (EINVAL);
		}
		return (0);
	case MOD_UNLOAD:
		if (elf64_brand_inuse(&cloudabi64_brand))
			return (EBUSY);
		if (elf64_remove_brand_entry(&cloudabi64_brand) < 0) {
			printf("Failed to remove CloudABI ELF brand handler\n");
			return (EINVAL);
		}
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t cloudabi64_module = {
	"cloudabi64",
	cloudabi64_modevent,
	NULL
};

DECLARE_MODULE_TIED(cloudabi64, cloudabi64_module, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(cloudabi64, cloudabi, 1, 1, 1);
