#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#ifdef __KERNEL__

#ifdef CONFIG_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif

#define prepare_to_switch() do {} while(0)

#define __STR(x) #x
#define STR(x) __STR(x)

#define __PUSH(x) "pushq %%" __STR(x) "\n\t"
#define __POP(x)  "popq  %%" __STR(x) "\n\t"

struct save_context_frame { 
        unsigned long rbp; 
        unsigned long rbx;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long rcx;
	unsigned long rdx;
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long rdi;
	unsigned long rsi;
};

/* frame pointer must be last for get_wchan */
#define SAVE_CONTEXT \
	__PUSH(rsi) __PUSH(rdi) \
    __PUSH(r12) __PUSH(r13) __PUSH(r14) __PUSH(r15)  \
	__PUSH(rdx) __PUSH(rcx) __PUSH(r8) __PUSH(r9) __PUSH(r10) __PUSH(r11)  \
	__PUSH(rbx) __PUSH(rbp) 
#define RESTORE_CONTEXT \
	__POP(rbp) __POP(rbx) \
	__POP(r11) __POP(r10) __POP(r9) __POP(r8) __POP(rcx) __POP(rdx) \
	__POP(r15) __POP(r14) __POP(r13) __POP(r12) \
	__POP(rdi) __POP(rsi)

#define switch_to(prev,next,last) do { void *l; \
	asm volatile(SAVE_CONTEXT					\
		     "movq %%rsp,%0\n\t"	/* save RSP */		\
		     "movq %3,%%rsp\n\t"	/* restore RSP */	\
		     "leaq thread_return(%%rip),%%rax\n\t"		\
		     "movq %%rax,%1\n\t"	/* save RIP */		\
		     "pushq %4\n\t"		/* setup new RIP */	\
		     "jmp __switch_to\n\t"				\
		     ".globl thread_return\n"				\
		     "thread_return:\n\t"				\
		     RESTORE_CONTEXT					\
		     :"=m" (prev->thread.rsp),"=m" (prev->thread.rip), "=a" (l) \
		     :"m" (next->thread.rsp),"m" (next->thread.rip),	\
		      "S" (next), "D" (prev)				\
		     :"memory","cc");					\
	last = l; 							\
} while(0)
		     
extern void load_gs_index(unsigned); 

/*
 * Load a segment. Fall back on loading the zero
 * segment if something goes wrong..
 */
#define loadsegment(seg,value)	\
	asm volatile("\n"			\
		"1:\t"				\
		"movl %0,%%" #seg "\n"		\
		"2:\n"				\
		".section .fixup,\"ax\"\n"	\
		"3:\t"				\
		"movl %1,%%" #seg "\n\t" \
		"jmp 2b\n"			\
		".previous\n"			\
		".section __ex_table,\"a\"\n\t"	\
		".align 4\n\t"			\
		".quad 1b,3b\n"			\
		".previous"			\
		: :"r" ((int)(value)), "r" (0))

#define set_debug(value,register) \
                __asm__("movq %0,%%db" #register  \
		: /* no output */ \
		:"r" ((unsigned long) value))


/*
 * Clear and set 'TS' bit respectively
 */
#define clts() __asm__ __volatile__ ("clts")
#define read_cr0() ({ \
	unsigned long __dummy; \
	__asm__( \
		"movq %%cr0,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})
#define write_cr0(x) \
	__asm__("movq %0,%%cr0": :"r" (x));

#define read_cr4() ({ \
	unsigned long __dummy; \
	__asm__( \
		"movq %%cr4,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})
#define write_cr4(x) \
	__asm__("movq %0,%%cr4": :"r" (x));
#define stts() write_cr0(8 | read_cr0())

#define wbinvd() \
	__asm__ __volatile__ ("wbinvd": : :"memory");

#endif	/* __KERNEL__ */

#define nop() __asm__ __volatile__ ("nop")

#define xchg(ptr,v) ((__typeof__(*(ptr)))__xchg((unsigned long)(v),(ptr),sizeof(*(ptr))))

#define tas(ptr) (xchg((ptr),1))

#define __xg(x) ((volatile long *)(x))

extern inline void set_64bit(volatile unsigned long *ptr, unsigned long val)
{
	*ptr = val;
}

#define _set_64bit set_64bit

/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway
 * Note 2: xchg has side effect, so that attribute volatile is necessary,
 *	  but generally the primitive is invalid, *ptr is output argument. --ANK
 */
static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
		case 1:
			__asm__ __volatile__("xchgb %b0,%1"
				:"=q" (x)
				:"m" (*__xg(ptr)), "0" (x)
				:"memory");
			break;
		case 2:
			__asm__ __volatile__("xchgw %w0,%1"
				:"=r" (x)
				:"m" (*__xg(ptr)), "0" (x)
				:"memory");
			break;
		case 4:
			__asm__ __volatile__("xchgl %k0,%1"
				:"=r" (x)
				:"m" (*__xg(ptr)), "0" (x)
				:"memory");
			break;
		case 8:
			__asm__ __volatile__("xchgq %0,%1"
				:"=r" (x)
				:"m" (*__xg(ptr)), "0" (x)
				:"memory");
			break;
	}
	return x;
}

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */

#define __HAVE_ARCH_CMPXCHG 1

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	switch (size) {
	case 1:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 2:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 4:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %k1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 8:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgq %1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	}
	return old;
}

#define cmpxchg(ptr,o,n)\
	((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),\
					(unsigned long)(n),sizeof(*(ptr))))


#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#endif

    
/*
 * Force strict CPU ordering.
 * And yes, this is required on UP too when we're talking
 * to devices.
 *
 * For now, "wmb()" doesn't actually do anything, as all
 * Intel CPU's follow what Intel calls a *Processor Order*,
 * in which all writes are seen in the program order even
 * outside the CPU.
 *
 * I expect future Intel CPU's to have a weaker ordering,
 * but I'd also expect them to finally get their act together
 * and add some real memory barriers if so.
 */
#define mb() 	asm volatile("mfence":::"memory")
#define rmb()	asm volatile("lfence":::"memory")
#define wmb()	asm volatile("sfence":::"memory")
#define set_mb(var, value) do { xchg(&var, value); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

#define warn_if_not_ulong(x) do { unsigned long foo; (void) (&(x) == &foo); } while (0)

/* interrupt control.. */
#define __save_flags(x)		do { warn_if_not_ulong(x); __asm__ __volatile__("# save_flags \n\t pushfq ; popq %q0":"=g" (x): /* no input */ :"memory"); } while (0)
#define __restore_flags(x) 	__asm__ __volatile__("# restore_flags \n\t pushq %0 ; popfq": /* no output */ :"g" (x):"memory", "cc")
#define __cli() 		__asm__ __volatile__("cli": : :"memory")
#define __sti()			__asm__ __volatile__("sti": : :"memory")
/* used in the idle loop; sti takes one instruction cycle to complete */
#define safe_halt()		__asm__ __volatile__("sti; hlt": : :"memory")

#define __save_and_cli(x)      do { __save_flags(x); __cli(); } while(0)
#define __save_and_sti(x)      do { __save_flags(x); __sti(); } while(0)

/* For spinlocks etc */
#define local_irq_save(x) 	do { warn_if_not_ulong(x); __asm__ __volatile__("# local_irq_save \n\t pushfq ; popq %0 ; cli":"=g" (x): /* no input */ :"memory"); } while (0)
#define local_irq_set(x) 	do { warn_if_not_ulong(x); __asm__ __volatile__("# local_irq_set \n\t pushfq ; popq %0 ; sti":"=g" (x): /* no input */ :"memory"); } while (0)
#define local_irq_restore(x)	__asm__ __volatile__("# local_irq_restore \n\t pushq %0 ; popfq": /* no output */ :"g" (x):"memory")
#define local_irq_disable()	__cli()
#define local_irq_enable()	__sti()

#ifdef CONFIG_SMP

extern void __global_cli(void);
extern void __global_sti(void);
extern unsigned long __global_save_flags(void);
extern void __global_restore_flags(unsigned long);
#define cli() __global_cli()
#define sti() __global_sti()
#define save_flags(x) ((x)=__global_save_flags())
#define restore_flags(x) __global_restore_flags(x)
#define save_and_cli(x) do { save_flags(x); cli(); } while(0)
#define save_and_sti(x) do { save_flags(x); sti(); } while(0)

#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_cli(x) __save_and_cli(x)
#define save_and_sti(x) __save_and_sti(x)

#endif

/* Default simics "magic" breakpoint */
#define icebp() asm volatile("xchg %%bx,%%bx" ::: "ebx")

/*
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

#endif
