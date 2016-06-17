/* 
 * Written 2000,2002 by Andi Kleen. 
 * 
 * Losely based on the sparc64 and IA64 32bit emulation loaders.
 * This tricks binfmt_elf.c into loading 32bit binaries using lots 
 * of ugly preprocessor tricks. Talk about very very poor man's inheritance.
 */ 
#include <linux/types.h>
#include <linux/config.h> 
#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/personality.h>
#include <asm/segment.h> 
#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/user32.h>
#include <asm/sigcontext32.h>
#include <asm/fpu32.h>
#include <asm/i387.h>

struct file;
struct elf_phdr; 

#define IA32_EMULATOR 1

#define ELF_NAME "elf/i386"

#define IA32_STACK_TOP IA32_PAGE_OFFSET
#define ELF_ET_DYN_BASE		(IA32_PAGE_OFFSET/3 + 0x1000000)

#undef ELF_ARCH
#define ELF_ARCH EM_386

#undef ELF_CLASS
#define ELF_CLASS ELFCLASS32

#define ELF_DATA	ELFDATA2LSB

#define USE_ELF_CORE_DUMP 1

/* Overwrite elfcore.h */ 
#define _LINUX_ELFCORE_H 1
typedef unsigned int elf_greg_t;

#define ELF_NGREG (sizeof (struct user_regs_struct32) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

struct elf_siginfo
{
	int	si_signo;			/* signal number */
	int	si_code;			/* extra code */
	int	si_errno;			/* errno */
};

struct timeval32
{
    int tv_sec, tv_usec;
};

struct elf_prstatus
{
	struct elf_siginfo pr_info;	/* Info associated with signal */
	short	pr_cursig;		/* Current signal */
	unsigned int pr_sigpend;	/* Set of pending signals */
	unsigned int pr_sighold;	/* Set of held signals */
	pid_t	pr_pid;
	pid_t	pr_ppid;
	pid_t	pr_pgrp;
	pid_t	pr_sid;
	struct timeval32 pr_utime;	/* User time */
	struct timeval32 pr_stime;	/* System time */
	struct timeval32 pr_cutime;	/* Cumulative user time */
	struct timeval32 pr_cstime;	/* Cumulative system time */
	elf_gregset_t pr_reg;	/* GP registers */
	int pr_fpvalid;		/* True if math co-processor being used.  */
};

#define ELF_PRARGSZ	(80)	/* Number of chars for args */

struct elf_prpsinfo
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
	unsigned int pr_flag;	/* flags */
	__u16	pr_uid;
	__u16	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

#define __STR(x) #x
#define STR(x) __STR(x)

#define _GET_SEG(x) \
	({ __u32 seg; asm("movl %%" STR(x) ",%0" : "=r"(seg)); seg; })

/* Assumes current==process to be dumped */
#define ELF_CORE_COPY_REGS(pr_reg, regs)       		\
	pr_reg[0] = regs->rbx;				\
	pr_reg[1] = regs->rcx;				\
	pr_reg[2] = regs->rdx;				\
	pr_reg[3] = regs->rsi;				\
	pr_reg[4] = regs->rdi;				\
	pr_reg[5] = regs->rbp;				\
	pr_reg[6] = regs->rax;				\
	pr_reg[7] = _GET_SEG(ds);   			\
	pr_reg[8] = _GET_SEG(es);			\
	pr_reg[9] = _GET_SEG(fs);			\
	pr_reg[10] = _GET_SEG(gs);			\
	pr_reg[11] = regs->orig_rax;			\
	pr_reg[12] = regs->rip;				\
	pr_reg[13] = regs->cs;				\
	pr_reg[14] = regs->eflags;			\
	pr_reg[15] = regs->rsp;				\
	pr_reg[16] = regs->ss;

#define user user32

#define dump_fpu dump_fpu_ia32

#define __ASM_X86_64_ELF_H 1
#include <asm/ia32.h>
#include <linux/elf.h>

typedef struct user_i387_ia32_struct elf_fpregset_t;
typedef struct user32_fxsr_struct elf_fpxregset_t;

#undef elf_check_arch
#define elf_check_arch(x) \
	((x)->e_machine == EM_386)

#define ELF_EXEC_PAGESIZE PAGE_SIZE
#define ELF_HWCAP (boot_cpu_data.x86_capability[0])
#define ELF_PLATFORM  ("i686")
#define SET_PERSONALITY(ex, ibcs2)			\
do {							\
	set_personality((ibcs2)?PER_SVR4:current->personality);	\
} while (0)

/* Override some function names */
#define elf_format			elf32_format

#define init_elf_binfmt			init_elf32_binfmt
#define exit_elf_binfmt			exit_elf32_binfmt

#define load_elf_binary load_elf32_binary

#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
# define CONFIG_BINFMT_ELF		CONFIG_BINFMT_ELF32
#endif

#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
# define CONFIG_BINFMT_ELF_MODULE	CONFIG_BINFMT_ELF32_MODULE
#endif

#define ELF_PLAT_INIT(r, load_addr)	elf32_init(r)
#define setup_arg_pages(bprm)		ia32_setup_arg_pages(bprm)

extern void load_gs_index(unsigned);

#undef start_thread
#define start_thread(regs,new_rip,new_rsp) do { \
	asm volatile("movl %0,%%fs": :"r" (0)); \
	load_gs_index(0);	\
	asm volatile("movl %0,%%es; movl %0,%%ds": :"r" (__USER32_DS)); \
	(regs)->rip = (new_rip); \
	(regs)->rsp = (new_rsp); \
	(regs)->eflags = 0x200; \
	(regs)->cs = __USER32_CS; \
	(regs)->ss = __USER32_DS; \
	set_fs(USER_DS); \
} while(0) 


#define elf_map elf32_map

MODULE_DESCRIPTION("Binary format loader for compatibility with IA32 ELF binaries."); 
MODULE_AUTHOR("Eric Youngdale, Andi Kleen");

#undef MODULE_DESCRIPTION
#undef MODULE_AUTHOR

#define elf_addr_t __u32
#define elf_caddr_t __u32

static void elf32_init(struct pt_regs *);
int ia32_setup_arg_pages(struct linux_binprm *bprm);

#include "../../../fs/binfmt_elf.c" 

static void elf32_init(struct pt_regs *regs)
{
	struct task_struct *me = current; 
	regs->rdi = 0;
	regs->rsi = 0;
	regs->rdx = 0;
	regs->rcx = 0;
	regs->rax = 0;
	regs->rbx = 0; 
	regs->rbp = 0; 
	regs->r8 = regs->r9 = regs->r10 = regs->r11 = regs->r12 = regs->r13 =
		regs->r14 = regs->r15 = 0;	
	me->thread.fs = 0; 
	me->thread.gs = 0;
	me->thread.fsindex = 0; 
	me->thread.gsindex = 0;
	me->thread.ds = __USER_DS; 
	me->thread.es = __USER_DS;
	me->thread.flags |= THREAD_IA32;
}

extern void put_dirty_page(struct task_struct * tsk, struct page *page, unsigned long address);
 

int ia32_setup_arg_pages(struct linux_binprm *bprm)
{
	unsigned long stack_base;
	struct vm_area_struct *mpnt;
	int i;

	stack_base = IA32_STACK_TOP - MAX_ARG_PAGES*PAGE_SIZE;

	bprm->p += stack_base;
	if (bprm->loader)
		bprm->loader += stack_base;
	bprm->exec += stack_base;

	mpnt = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!mpnt) 
		return -ENOMEM; 
	
	down_write(&current->mm->mmap_sem);
	{
		mpnt->vm_mm = current->mm;
		mpnt->vm_start = PAGE_MASK & (unsigned long) bprm->p;
		mpnt->vm_end = IA32_STACK_TOP;
		mpnt->vm_flags = vm_stack_flags32; 
		mpnt->vm_page_prot = (mpnt->vm_flags & VM_EXEC) ? 
			PAGE_COPY_EXEC : PAGE_COPY;
		mpnt->vm_ops = NULL;
		mpnt->vm_pgoff = 0;
		mpnt->vm_file = NULL;
		mpnt->vm_private_data = (void *) 0;
		insert_vm_struct(current->mm, mpnt);
		current->mm->total_vm = (mpnt->vm_end - mpnt->vm_start) >> PAGE_SHIFT;
	} 

	for (i = 0 ; i < MAX_ARG_PAGES ; i++) {
		struct page *page = bprm->page[i];
		if (page) {
			bprm->page[i] = NULL;
			current->mm->rss++;
			put_dirty_page(current,page,stack_base);
		}
		stack_base += PAGE_SIZE;
	}
	up_write(&current->mm->mmap_sem);
	
	return 0;
}
static unsigned long
elf32_map (struct file *filep, unsigned long addr, struct elf_phdr *eppnt, int prot, int type)
{
	unsigned long map_addr;
	struct task_struct *me = current; 

	if (prot & PROT_READ) 
		prot |= PROT_EXEC; 

	down_write(&me->mm->mmap_sem);
	map_addr = do_mmap(filep, ELF_PAGESTART(addr),
			   eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr), prot, 
			   type|MAP_32BIT,
			   eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr));
	up_write(&me->mm->mmap_sem);
	return(map_addr);
}

int dump_fpu_ia32(struct pt_regs *regs, elf_fpregset_t *fp)
{
	struct _fpstate_ia32 *fpu = (void*)fp; 
	struct task_struct *tsk = current;
	mm_segment_t oldfs = get_fs();
	int ret;

	if (!tsk->used_math) 
		return 0;
	if (!(tsk->thread.flags & THREAD_IA32))
		BUG(); 
	unlazy_fpu(tsk);
	set_fs(KERNEL_DS); 
	ret = save_i387_ia32(current, fpu, regs, 1);
	/* Correct for i386 bug. It puts the fop into the upper 16bits of 
	   the tag word (like FXSAVE), not into the fcs*/ 
	fpu->cssel |= fpu->tag & 0xffff0000; 
	set_fs(oldfs); 
	return ret; 
}
