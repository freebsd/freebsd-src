/*
 * include/asm-i386/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef __ASM_I386_PROCESSOR_H
#define __ASM_I386_PROCESSOR_H

#include <asm/vm86.h>
#include <asm/math_emu.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/sigcontext.h>
#include <asm/cpufeature.h>
#include <linux/cache.h>
#include <linux/config.h>
#include <linux/threads.h>

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; __asm__("movl $1f,%0\n1:":"=g" (pc)); pc; })

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 *  Members of this structure are referenced in head.S, so think twice
 *  before touching them. [mj]
 */

struct cpuinfo_x86 {
	__u8	x86;		/* CPU family */
	__u8	x86_vendor;	/* CPU vendor */
	__u8	x86_model;
	__u8	x86_mask;
	char	wp_works_ok;	/* It doesn't on 386's */
	char	hlt_works_ok;	/* Problems on some 486Dx4's and old 386's */
	char	hard_math;
	char	rfu;
       	int	cpuid_level;	/* Maximum supported CPUID level, -1=no CPUID */
	__u32	x86_capability[NCAPINTS];
	char	x86_vendor_id[16];
	char	x86_model_id[64];
	int 	x86_cache_size;  /* in KB - valid for CPUS which support this
				    call  */
	int	fdiv_bug;
	int	f00f_bug;
	int	coma_bug;
	unsigned long loops_per_jiffy;
	unsigned long *pgd_quick;
	unsigned long *pmd_quick;
	unsigned long *pte_quick;
	unsigned long pgtable_cache_sz;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NSC 8
#define X86_VENDOR_SIS 9
#define X86_VENDOR_UNKNOWN 0xff

/*
 * capabilities of CPUs
 */

extern struct cpuinfo_x86 boot_cpu_data;
extern struct tss_struct init_tss[NR_CPUS];

#ifdef CONFIG_SMP
extern struct cpuinfo_x86 cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]
#else
#define cpu_data (&boot_cpu_data)
#define current_cpu_data boot_cpu_data
#endif

extern char ignore_irq13;

extern void identify_cpu(struct cpuinfo_x86 *);
extern void print_cpu_info(struct cpuinfo_x86 *);
extern void dodgy_tsc(void);

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */
#define X86_EFLAGS_PF	0x00000004 /* Parity Flag */
#define X86_EFLAGS_AF	0x00000010 /* Auxillary carry Flag */
#define X86_EFLAGS_ZF	0x00000040 /* Zero Flag */
#define X86_EFLAGS_SF	0x00000080 /* Sign Flag */
#define X86_EFLAGS_TF	0x00000100 /* Trap Flag */
#define X86_EFLAGS_IF	0x00000200 /* Interrupt Flag */
#define X86_EFLAGS_DF	0x00000400 /* Direction Flag */
#define X86_EFLAGS_OF	0x00000800 /* Overflow Flag */
#define X86_EFLAGS_IOPL	0x00003000 /* IOPL mask */
#define X86_EFLAGS_NT	0x00004000 /* Nested Task */
#define X86_EFLAGS_RF	0x00010000 /* Resume Flag */
#define X86_EFLAGS_VM	0x00020000 /* Virtual Mode */
#define X86_EFLAGS_AC	0x00040000 /* Alignment Check */
#define X86_EFLAGS_VIF	0x00080000 /* Virtual Interrupt Flag */
#define X86_EFLAGS_VIP	0x00100000 /* Virtual Interrupt Pending */
#define X86_EFLAGS_ID	0x00200000 /* CPUID detection flag */

/*
 * Generic CPUID function
 */
static inline void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx)
{
	__asm__("cpuid"
		: "=a" (*eax),
		  "=b" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (op));
}

/*
 * CPUID functions returning a single datum
 */
static inline unsigned int cpuid_eax(unsigned int op)
{
	unsigned int eax;

	__asm__("cpuid"
		: "=a" (eax)
		: "0" (op)
		: "bx", "cx", "dx");
	return eax;
}
static inline unsigned int cpuid_ebx(unsigned int op)
{
	unsigned int eax, ebx;

	__asm__("cpuid"
		: "=a" (eax), "=b" (ebx)
		: "0" (op)
		: "cx", "dx" );
	return ebx;
}
static inline unsigned int cpuid_ecx(unsigned int op)
{
	unsigned int eax, ecx;

	__asm__("cpuid"
		: "=a" (eax), "=c" (ecx)
		: "0" (op)
		: "bx", "dx" );
	return ecx;
}
static inline unsigned int cpuid_edx(unsigned int op)
{
	unsigned int eax, edx;

	__asm__("cpuid"
		: "=a" (eax), "=d" (edx)
		: "0" (op)
		: "bx", "cx");
	return edx;
}

/*
 * Intel CPU features in CR4
 */
#define X86_CR4_VME		0x0001	/* enable vm86 extensions */
#define X86_CR4_PVI		0x0002	/* virtual interrupts flag enable */
#define X86_CR4_TSD		0x0004	/* disable time stamp at ipl 3 */
#define X86_CR4_DE		0x0008	/* enable debugging extensions */
#define X86_CR4_PSE		0x0010	/* enable page size extensions */
#define X86_CR4_PAE		0x0020	/* enable physical address extensions */
#define X86_CR4_MCE		0x0040	/* Machine check enable */
#define X86_CR4_PGE		0x0080	/* enable global pages */
#define X86_CR4_PCE		0x0100	/* enable performance counters at ipl 3 */
#define X86_CR4_OSFXSR		0x0200	/* enable fast FPU save and restore */
#define X86_CR4_OSXMMEXCPT	0x0400	/* enable unmasked SSE exceptions */

#define load_cr3(pgdir) \
	asm volatile("movl %0,%%cr3": :"r" (__pa(pgdir)));

/*
 * Save the cr4 feature set we're using (ie
 * Pentium 4MB enable and PPro Global page
 * enable), so that any CPU's that boot up
 * after us can get the correct flags.
 */
extern unsigned long mmu_cr4_features;

static inline void set_in_cr4 (unsigned long mask)
{
	mmu_cr4_features |= mask;
	__asm__("movl %%cr4,%%eax\n\t"
		"orl %0,%%eax\n\t"
		"movl %%eax,%%cr4\n"
		: : "irg" (mask)
		:"ax");
}

static inline void clear_in_cr4 (unsigned long mask)
{
	mmu_cr4_features &= ~mask;
	__asm__("movl %%cr4,%%eax\n\t"
		"andl %0,%%eax\n\t"
		"movl %%eax,%%cr4\n"
		: : "irg" (~mask)
		:"ax");
}

/*
 *      Cyrix CPU configuration register indexes
 */
#define CX86_CCR0 0xc0
#define CX86_CCR1 0xc1
#define CX86_CCR2 0xc2
#define CX86_CCR3 0xc3
#define CX86_CCR4 0xe8
#define CX86_CCR5 0xe9
#define CX86_CCR6 0xea
#define CX86_CCR7 0xeb
#define CX86_DIR0 0xfe
#define CX86_DIR1 0xff
#define CX86_ARR_BASE 0xc4
#define CX86_RCR_BASE 0xdc

/*
 *      Cyrix CPU indexed register access macros
 */

#define getCx86(reg) ({ outb((reg), 0x22); inb(0x23); })

#define setCx86(reg, data) do { \
	outb((reg), 0x22); \
	outb((data), 0x23); \
} while (0)

/*
 * Bus types (default is ISA, but people can check others with these..)
 */
#ifdef CONFIG_EISA
extern int EISA_bus;
#else
#define EISA_bus (0)
#endif
extern int MCA_bus;

/* from system description table in BIOS.  Mostly for MCA use, but
others may find it useful. */
extern unsigned int machine_id;
extern unsigned int machine_submodel_id;
extern unsigned int BIOS_revision;
extern unsigned int mca_pentium_flag;

/*
 * User space process size: 3GB (default).
 */
#define TASK_SIZE	(PAGE_OFFSET)

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32
#define IO_BITMAP_BYTES (IO_BITMAP_SIZE * 4)
#define IO_BITMAP_OFFSET offsetof(struct tss_struct,io_bitmap)
#define INVALID_IO_BITMAP_OFFSET 0x8000

struct i387_fsave_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
	long	status;		/* software status information */
};

struct i387_fxsave_struct {
	unsigned short	cwd;
	unsigned short	swd;
	unsigned short	twd;
	unsigned short	fop;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	mxcsr;
	long	reserved;
	long	st_space[32];	/* 8*16 bytes for each FP-reg = 128 bytes */
	long	xmm_space[32];	/* 8*16 bytes for each XMM-reg = 128 bytes */
	long	padding[56];
} __attribute__ ((aligned (16)));

struct i387_soft_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
	unsigned char	ftop, changed, lookahead, no_update, rm, alimit;
	struct info	*info;
	unsigned long	entry_eip;
};

union i387_union {
	struct i387_fsave_struct	fsave;
	struct i387_fxsave_struct	fxsave;
	struct i387_soft_struct soft;
};

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct tss_struct {
	unsigned short	back_link,__blh;
	unsigned long	esp0;
	unsigned short	ss0,__ss0h;
	unsigned long	esp1;
	unsigned short	ss1,__ss1h;
	unsigned long	esp2;
	unsigned short	ss2,__ss2h;
	unsigned long	__cr3;
	unsigned long	eip;
	unsigned long	eflags;
	unsigned long	eax,ecx,edx,ebx;
	unsigned long	esp;
	unsigned long	ebp;
	unsigned long	esi;
	unsigned long	edi;
	unsigned short	es, __esh;
	unsigned short	cs, __csh;
	unsigned short	ss, __ssh;
	unsigned short	ds, __dsh;
	unsigned short	fs, __fsh;
	unsigned short	gs, __gsh;
	unsigned short	ldt, __ldth;
	unsigned short	trace, bitmap;
	unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
	/*
	 * pads the TSS to be cacheline-aligned (size is 0x100)
	 */
	unsigned long __cacheline_filler[5];
};

struct thread_struct {
	unsigned long	esp0;
	unsigned long	eip;
	unsigned long	esp;
	unsigned long	fs;
	unsigned long	gs;
/* Hardware debugging registers */
	unsigned long	debugreg[8];  /* %%db0-7 debug registers */
/* fault info */
	unsigned long	cr2, trap_no, error_code;
/* floating point info */
	union i387_union	i387;
/* virtual 86 mode info */
	struct vm86_struct	* vm86_info;
	unsigned long		screen_bitmap;
	unsigned long		v86flags, v86mask, saved_esp0;
/* IO permissions */
	int		ioperm;
	unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
};

#define INIT_THREAD  {						\
	0,							\
	0, 0, 0, 0, 						\
	{ [0 ... 7] = 0 },	/* debugging registers */	\
	0, 0, 0,						\
	{ { 0, }, },		/* 387 state */			\
	0,0,0,0,0,						\
	0,{~0,}			/* io permissions */		\
}

#define INIT_TSS  {						\
	0,0, /* back_link, __blh */				\
	sizeof(init_stack) + (long) &init_stack, /* esp0 */	\
	__KERNEL_DS, 0, /* ss0 */				\
	0,0,0,0,0,0, /* stack1, stack2 */			\
	0, /* cr3 */						\
	0,0, /* eip,eflags */					\
	0,0,0,0, /* eax,ecx,edx,ebx */				\
	0,0,0,0, /* esp,ebp,esi,edi */				\
	0,0,0,0,0,0, /* es,cs,ss */				\
	0,0,0,0,0,0, /* ds,fs,gs */				\
	__LDT(0),0, /* ldt */					\
	0, INVALID_IO_BITMAP_OFFSET, /* tace, bitmap */		\
	{~0, } /* ioperm */					\
}

#define start_thread(regs, new_eip, new_esp) do {		\
	__asm__("movl %0,%%fs ; movl %0,%%gs": :"r" (0));	\
	set_fs(USER_DS);					\
	regs->xds = __USER_DS;					\
	regs->xes = __USER_DS;					\
	regs->xss = __USER_DS;					\
	regs->xcs = __USER_CS;					\
	regs->eip = new_eip;					\
	regs->esp = new_esp;					\
} while (0)

/* Forward declaration, a strange C thing */
struct task_struct;
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);
/*
 * create a kernel thread without removing it from tasklists
 */
extern int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/* Copy and release all segment info associated with a VM
 * Unusable due to lack of error handling, use {init_new,destroy}_context
 * instead.
 */
static inline void copy_segments(struct task_struct *p, struct mm_struct * mm) { }
static inline void release_segments(struct mm_struct * mm) { }

/*
 * Return saved PC of a blocked thread.
 */
static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	return ((unsigned long *)t->esp)[3];
}

unsigned long get_wchan(struct task_struct *p);
#define KSTK_EIP(tsk)	(((unsigned long *)(4096+(unsigned long)(tsk)))[1019])
#define KSTK_ESP(tsk)	(((unsigned long *)(4096+(unsigned long)(tsk)))[1022])

#define THREAD_SIZE (2*PAGE_SIZE)
#define alloc_task_struct() ((struct task_struct *) __get_free_pages(GFP_KERNEL,1))
#define free_task_struct(p) free_pages((unsigned long) (p), 1)
#define get_task_struct(tsk)      atomic_inc(&virt_to_page(tsk)->count)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

struct microcode_header {
	unsigned int hdrver;
	unsigned int rev;
	unsigned int date;
	unsigned int sig;
	unsigned int cksum;
	unsigned int ldrver;
	unsigned int pf;
	unsigned int datasize;
	unsigned int totalsize;
	unsigned int reserved[3];
};

struct microcode {
	struct microcode_header hdr;
	unsigned int bits[0];
};

typedef struct microcode microcode_t;
typedef struct microcode_header microcode_header_t;

/* microcode format is extended from prescott processors */
struct extended_signature {
	unsigned int sig;
	unsigned int pf;
	unsigned int cksum;
};

struct extended_sigtable {
	unsigned int count;
	unsigned int cksum;
	unsigned int reserved[3];
	struct extended_signature sigs[0];
};
/* '6' because it used to be for P6 only (but now covers Pentium 4 as well) */
#define MICROCODE_IOCFREE	_IO('6',0)

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static inline void rep_nop(void)
{
	__asm__ __volatile__("rep;nop" ::: "memory");
}

#define cpu_relax()	rep_nop()

/* Prefetch instructions for Pentium III and AMD Athlon */
#if defined(CONFIG_MPENTIUMIII) || defined (CONFIG_MPENTIUM4)

#define ARCH_HAS_PREFETCH
extern inline void prefetch(const void *x)
{
	__asm__ __volatile__ ("prefetchnta (%0)" : : "r"(x));
}

#elif CONFIG_X86_USE_3DNOW

#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

extern inline void prefetch(const void *x)
{
	 __asm__ __volatile__ ("prefetch (%0)" : : "r"(x));
}

extern inline void prefetchw(const void *x)
{
	 __asm__ __volatile__ ("prefetchw (%0)" : : "r"(x));
}
#define spin_lock_prefetch(x)	prefetchw(x)

#endif

#endif /* __ASM_I386_PROCESSOR_H */
