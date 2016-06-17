#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/segment.h>
#include <linux/bitops.h> /* for LOCK_PREFIX */

#ifdef __KERNEL__

struct task_struct;	/* one of the stranger aspects of C forward declarations.. */
extern void FASTCALL(__switch_to(struct task_struct *prev, struct task_struct *next));

#define prepare_to_switch()	do { } while(0)
#define switch_to(prev,next,last) do {					\
	asm volatile("pushl %%esi\n\t"					\
		     "pushl %%edi\n\t"					\
		     "pushl %%ebp\n\t"					\
		     "movl %%esp,%0\n\t"	/* save ESP */		\
		     "movl %3,%%esp\n\t"	/* restore ESP */	\
		     "movl $1f,%1\n\t"		/* save EIP */		\
		     "pushl %4\n\t"		/* restore EIP */	\
		     "jmp __switch_to\n"				\
		     "1:\t"						\
		     "popl %%ebp\n\t"					\
		     "popl %%edi\n\t"					\
		     "popl %%esi\n\t"					\
		     :"=m" (prev->thread.esp),"=m" (prev->thread.eip),	\
		      "=b" (last)					\
		     :"m" (next->thread.esp),"m" (next->thread.eip),	\
		      "a" (prev), "d" (next),				\
		      "b" (prev));					\
} while (0)

#define _set_base(addr,base) do { unsigned long __pr; \
__asm__ __volatile__ ("movw %%dx,%1\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%2\n\t" \
	"movb %%dh,%3" \
	:"=&d" (__pr) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
         "0" (base) \
        ); } while(0)

#define _set_limit(addr,limit) do { unsigned long __lr; \
__asm__ __volatile__ ("movw %%dx,%1\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %2,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%2" \
	:"=&d" (__lr) \
	:"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "0" (limit) \
        ); } while(0)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , (base) )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , ((limit)-1)>>12 )

static inline unsigned long _get_base(char * addr)
{
	unsigned long __base;
	__asm__("movb %3,%%dh\n\t"
		"movb %2,%%dl\n\t"
		"shll $16,%%edx\n\t"
		"movw %1,%%dx"
		:"=&d" (__base)
		:"m" (*((addr)+2)),
		 "m" (*((addr)+4)),
		 "m" (*((addr)+7)));
	return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

/*
 * Load a segment. Fall back on loading the zero
 * segment if something goes wrong..
 */
#define loadsegment(seg,value)			\
	asm volatile("\n"			\
		"1:\t"				\
		"movl %0,%%" #seg "\n"		\
		"2:\n"				\
		".section .fixup,\"ax\"\n"	\
		"3:\t"				\
		"pushl $0\n\t"			\
		"popl %%" #seg "\n\t"		\
		"jmp 2b\n"			\
		".previous\n"			\
		".section __ex_table,\"a\"\n\t"	\
		".align 4\n\t"			\
		".long 1b,3b\n"			\
		".previous"			\
		: :"m" (*(unsigned int *)&(value)))

/*
 * Clear and set 'TS' bit respectively
 */
#define clts() __asm__ __volatile__ ("clts")
#define read_cr0() ({ \
	unsigned int __dummy; \
	__asm__( \
		"movl %%cr0,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})
#define write_cr0(x) \
	__asm__("movl %0,%%cr0": :"r" (x));

#define read_cr4() ({ \
	unsigned int __dummy; \
	__asm__( \
		"movl %%cr4,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})
#define write_cr4(x) \
	__asm__("movl %0,%%cr4": :"r" (x));
#define stts() write_cr0(8 | read_cr0())

#endif	/* __KERNEL__ */

#define wbinvd() \
	__asm__ __volatile__ ("wbinvd": : :"memory");

static inline unsigned long get_limit(unsigned long segment)
{
	unsigned long __limit;
	__asm__("lsll %1,%0"
		:"=r" (__limit):"r" (segment));
	return __limit+1;
}

#define nop() __asm__ __volatile__ ("nop")

#define xchg(ptr,v) ((__typeof__(*(ptr)))__xchg((unsigned long)(v),(ptr),sizeof(*(ptr))))

#define tas(ptr) (xchg((ptr),1))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((struct __xchg_dummy *)(x))


/*
 * The semantics of XCHGCMP8B are a bit strange, this is why
 * there is a loop and the loading of %%eax and %%edx has to
 * be inside. This inlines well in most cases, the cached
 * cost is around ~38 cycles. (in the future we might want
 * to do an SIMD/3DNOW!/MMX/FPU 64-bit store here, but that
 * might have an implicit FPU-save as a cost, so it's not
 * clear which path to go.)
 *
 * chmxchg8b must be used with the lock prefix here to allow
 * the instruction to be executed atomically, see page 3-102
 * of the instruction set reference 24319102.pdf. We need
 * the reader side to see the coherent 64bit value.
 */
static inline void __set_64bit (unsigned long long * ptr,
		unsigned int low, unsigned int high)
{
	__asm__ __volatile__ (
		"\n1:\t"
		"movl (%0), %%eax\n\t"
		"movl 4(%0), %%edx\n\t"
		"lock cmpxchg8b (%0)\n\t"
		"jnz 1b"
		: /* no outputs */
		:	"D"(ptr),
			"b"(low),
			"c"(high)
		:	"ax","dx","memory");
}

static inline void __set_64bit_constant (unsigned long long *ptr,
						 unsigned long long value)
{
	__set_64bit(ptr,(unsigned int)(value), (unsigned int)((value)>>32ULL));
}
#define ll_low(x)	*(((unsigned int*)&(x))+0)
#define ll_high(x)	*(((unsigned int*)&(x))+1)

static inline void __set_64bit_var (unsigned long long *ptr,
			 unsigned long long value)
{
	__set_64bit(ptr,ll_low(value), ll_high(value));
}

#define set_64bit(ptr,value) \
(__builtin_constant_p(value) ? \
 __set_64bit_constant(ptr, value) : \
 __set_64bit_var(ptr, value) )

#define _set_64bit(ptr,value) \
(__builtin_constant_p(value) ? \
 __set_64bit(ptr, (unsigned int)(value), (unsigned int)((value)>>32ULL) ) : \
 __set_64bit(ptr, ll_low(value), ll_high(value)) )

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
			__asm__ __volatile__("xchgl %0,%1"
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

#ifdef CONFIG_X86_CMPXCHG
#define __HAVE_ARCH_CMPXCHG 1
#endif

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
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
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
 *
 * Some non intel clones support out of order store. wmb() ceases to be a
 * nop for these.
 */
 
#define mb() 	__asm__ __volatile__ ("lock; addl $0,0(%%esp)": : :"memory")
#define rmb()	mb()

#ifdef CONFIG_X86_OOSTORE
#define wmb() 	__asm__ __volatile__ ("lock; addl $0,0(%%esp)": : :"memory")
#else
#define wmb()	__asm__ __volatile__ ("": : :"memory")
#endif

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define set_mb(var, value) do { xchg(&var, value); } while (0)
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define set_mb(var, value) do { var = value; barrier(); } while (0)
#endif

#define set_wmb(var, value) do { var = value; wmb(); } while (0)

/* interrupt control.. */
#define __save_flags(x)		__asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */)
#define __restore_flags(x) 	__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (x):"memory", "cc")
#define __cli() 		__asm__ __volatile__("cli": : :"memory")
#define __sti()			__asm__ __volatile__("sti": : :"memory")
/* used in the idle loop; sti takes one instruction cycle to complete */
#define safe_halt()		__asm__ __volatile__("sti; hlt": : :"memory")

#define __save_and_cli(x)	do { __save_flags(x); __cli(); } while(0);
#define __save_and_sti(x)	do { __save_flags(x); __sti(); } while(0);

/* For spinlocks etc */
#if 0
#define local_irq_save(x)	__asm__ __volatile__("pushfl ; popl %0 ; cli":"=g" (x): /* no input */ :"memory")
#define local_irq_set(x)	__asm__ __volatile__("pushfl ; popl %0 ; sti":"=g" (x): /* no input */ :"memory")
#else
#define local_irq_save(x)	__save_and_cli(x)
#define local_irq_set(x)	__save_and_sti(x)
#endif

#define local_irq_restore(x)	__restore_flags(x)
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
#define save_and_cli(x) do { save_flags(x); cli(); } while(0);
#define save_and_sti(x) do { save_flags(x); sti(); } while(0);

#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_cli(x) __save_and_cli(x)
#define save_and_sti(x) __save_and_sti(x)

#endif

/*
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

extern unsigned long dmi_broken;
extern int is_sony_vaio_laptop;

#define BROKEN_ACPI_Sx		0x0001
#define BROKEN_INIT_AFTER_S1	0x0002
#define BROKEN_PNP_BIOS		0x0004

#endif
