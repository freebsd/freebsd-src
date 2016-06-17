#ifndef __ASM_SH64_SYSTEM_H
#define __ASM_SH64_SYSTEM_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/system.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2003  Paul Mundt
 *
 */

#include <linux/config.h>
#include <asm/registers.h>

/*
 *	switch_to() should switch tasks to task nr n, first
 */

typedef struct {
	unsigned long seg;
} mm_segment_t;

#ifdef CONFIG_SMP
#error "no SMP SH64"
#else
#define prepare_to_switch()	do { } while(0)
#ifndef CS_SAVE_ALL
#define SAVE_CALLER_SAVED
#define RESTORE_CALLER_SAVED
#else
#define SAVE_CALLER_SAVED \
 __asm__ __volatile__("addi.l	r15, -256, r15\n\t"		\
		      "st.q	r15, 0, r0\n\t"			\
		      "st.q	r15, 8, r1\n\t"			\
		      "st.q	r15, 16, r2\n\t"		\
		      "st.q	r15, 24, r3\n\t"		\
		      "st.q	r15, 32, r4\n\t"		\
		      "st.q	r15, 40, r5\n\t"		\
		      "st.q	r15, 48, r6\n\t"		\
		      "st.q	r15, 56, r7\n\t"		\
		      "st.q	r15, 64, r8\n\t"		\
		      "st.q	r15, 72, r9\n\t"		\
		      "st.q	r15, 80, r17\n\t"		\
		      "st.q	r15, 88, r19\n\t"		\
		      "st.q	r15, 96, r20\n\t"		\
		      "st.q	r15, 104, r21\n\t"		\
		      "st.q	r15, 112, r22\n\t"		\
		      "st.q	r15, 120, r23\n\t"		\
		      "st.q	r15, 128, r36\n\t"		\
		      "st.q	r15, 136, r37\n\t"		\
		      "st.q	r15, 144, r38\n\t"		\
		      "st.q	r15, 152, r39\n\t"		\
		      "st.q	r15, 160, r40\n\t"		\
		      "st.q	r15, 168, r41\n\t"		\
		      "st.q	r15, 176, r42\n\t"		\
		      "st.q	r15, 184, r43\n\t"		\
		      "st.q	r15, 192, r60\n\t"		\
		      "st.q	r15, 200, r61\n\t"		\
		      "st.q	r15, 208, r62\n\t"		\
		      "gettr	" __t0 ", r0\n\t"			\
		      "st.q	r15, 216, r0\n\t"		\
		      "gettr	" __t1 ", r1\n\t"			\
		      "st.q	r15, 224, r1\n\t"		\
		      "gettr	" __t2 ", r2\n\t"			\
		      "st.q	r15, 232, r2\n\t"		\
		      "gettr	" __t3 ", r3\n\t"			\
		      "st.q	r15, 240, r3\n\t"		\
		      "gettr	" __t4 ", r4\n\t"			\
		      "st.q	r15, 248, r4\n\t");

/* Note. Do not restore r42 ! */
#define RESTORE_CALLER_SAVED \
 __asm__ __volatile__("ld.q	r15, 216, r0\n\t"		\
		      "ptabs	r0, " __t0 "\n\t"		\
		      "ld.q	r15, 224, r1\n\t"		\
		      "ptabs	r1, " __t1 "\n\t"		\
		      "ld.q	r15, 232, r2\n\t"		\
		      "ptabs	r2, " __t2 "\n\t"		\
		      "ld.q	r15, 240, r3\n\t"		\
		      "ptabs	r3, " __t3 "\n\t"		\
		      "ld.q	r15, 248, r4\n\t"		\
		      "ptabs	r4, " __t4 "\n\t"		\
		      "ld.q	r15, 0, r0\n\t"			\
		      "ld.q	r15, 8, r1\n\t"			\
		      "ld.q	r15, 16, r2\n\t"		\
		      "ld.q	r15, 24, r3\n\t"		\
		      "ld.q	r15, 32, r4\n\t"		\
		      "ld.q	r15, 40, r5\n\t"		\
		      "ld.q	r15, 48, r6\n\t"		\
		      "ld.q	r15, 56, r7\n\t"		\
		      "ld.q	r15, 64, r8\n\t"		\
		      "ld.q	r15, 72, r9\n\t"		\
		      "ld.q	r15, 80, r17\n\t"		\
		      "ld.q	r15, 88, r19\n\t"		\
		      "ld.q	r15, 96, r20\n\t"		\
		      "ld.q	r15, 104, r21\n\t"		\
		      "ld.q	r15, 112, r22\n\t"		\
		      "ld.q	r15, 120, r23\n\t"		\
		      "ld.q	r15, 128, r36\n\t"		\
		      "ld.q	r15, 136, r37\n\t"		\
		      "ld.q	r15, 144, r38\n\t"		\
		      "ld.q	r15, 152, r39\n\t"		\
		      "ld.q	r15, 160, r40\n\t"		\
		      "ld.q	r15, 168, r41\n\t"		\
		      "ld.q	r15, 184, r43\n\t"		\
		      "ld.q	r15, 192, r60\n\t"		\
		      "ld.q	r15, 200, r61\n\t"		\
		      "ld.q	r15, 208, r62\n\t"		\
		      "addi.l	r15, 256, r15\n\t");
#endif

#define switch_to(prev,next,last) do { \
 register unsigned long *      r36 __asm__ ("r36");		\
 register unsigned long *      r37 __asm__ ("r37");		\
 register unsigned long long   r38 __asm__ ("r38");		\
 register unsigned long long   r39 __asm__ ("r39");		\
 register unsigned long *      r40 __asm__ ("r40");		\
 register unsigned long *      r41 __asm__ ("r41");		\
 register struct task_struct * r42 __asm__ ("r42");		\
								\
/* printk("switch_to prev %08x next %08x last %08x\n", prev, next,last);  */ \
 if (last_task_used_math != next) {				\
  struct pt_regs* regs;						\
  regs = next->thread.kregs;                                    \
  regs->sr |= SR_FD;						\
 }								\
								\
 SAVE_CALLER_SAVED						\
								\
 r36 = &prev->thread.sp;					\
 r37 = &prev->thread.pc;					\
								\
 /* Note that we always are in kernel space */			\
 r38 = next->thread.sp | NEFF_MASK;				\
 r39 = next->thread.pc | NEFF_MASK;				\
								\
 r40 = (unsigned long *) prev;					\
 r41 = (unsigned long *) next;					\
								\
 __asm__ __volatile__("addi.l	r15, -304, r15\n\t"		\
		      "st.q	r15, 0, r10\n\t"		\
		      "st.q	r15, 8, r11\n\t"		\
		      "st.q	r15, 16, r12\n\t"		\
		      "st.q	r15, 24, r13\n\t"		\
		      "st.q	r15, 32, r14\n\t"		\
		      "st.q	r15, 40, r16\n\t"		\
		      "st.q	r15, 48, r18\n\t"		\
		      "st.q	r15, 56, r24\n\t"		\
		      "st.q	r15, 64, r25\n\t"		\
		      "st.q	r15, 72, r26\n\t"		\
		      "st.q	r15, 80, r27\n\t"		\
		      "st.q	r15, 88, r28\n\t"		\
		      "st.q	r15, 96, r29\n\t"		\
		      "st.q	r15, 104, r30\n\t"		\
		      "st.q	r15, 112, r31\n\t"		\
		      "st.q	r15, 120, r32\n\t"		\
		      "st.q	r15, 128, r33\n\t"		\
		      "st.q	r15, 136, r34\n\t"		\
		      "st.q	r15, 144, r35\n\t"		\
		      "st.q	r15, 152, r44\n\t"		\
		      "st.q	r15, 160, r45\n\t"		\
		      "st.q	r15, 168, r46\n\t"		\
		      "st.q	r15, 176, r47\n\t"		\
		      "st.q	r15, 184, r48\n\t"		\
		      "st.q	r15, 192, r49\n\t"		\
		      "st.q	r15, 200, r50\n\t"		\
		      "st.q	r15, 208, r51\n\t"		\
		      "st.q	r15, 216, r52\n\t"		\
		      "st.q	r15, 224, r53\n\t"		\
		      "st.q	r15, 232, r54\n\t"		\
		      "st.q	r15, 240, r55\n\t"		\
		      "st.q	r15, 248, r56\n\t"		\
		      "st.q	r15, 256, r57\n\t"		\
		      "st.q	r15, 264, r58\n\t"		\
		      "st.q	r15, 272, r59\n\t"		\
		      "gettr	" __t5 ", r55\n\t"		\
		      "st.q	r15, 280, r55\n\t"		\
		      "gettr	" __t6 ", r56\n\t"		\
		      "st.q	r15, 288, r56\n\t"		\
		      "gettr	" __t7 ", r57\n\t"		\
		      "st.q	r15, 296, r57\n\t"		\
		      "_loada	__switch_to, r18\n\t"  \
		      "ptabs	r18, " __t5 "\n\t"		\
		      "_pta	36, " __t6 "\n\t"		\
		      "gettr	" __t6 ", r18\n\t"		\
		      "st.l	%1, 0, r15\n\t"			\
		      "st.l	%2, 0, r18\n\t"			\
		      "or	%3, r63, r15\n\t"		\
		      "or	%4, r63, r18\n\t"		\
		      "or	%5, r63, r2\n\t"		\
		      "or	%6, r63, r3\n\t"		\
		      "blink	" __t5 ", r63\n\t"		\
                      "or       r2, r63, %0\n\t"                \
		      "ld.q	r15, 280, r55\n\t"		\
		      "ptabs	r55, " __t5 "\n\t"		\
		      "ld.q	r15, 288, r56\n\t"		\
		      "ptabs	r56, " __t6 "\n\t"		\
		      "ld.q	r15, 296, r57\n\t"		\
		      "ptabs	r57, " __t7 "\n\t"		\
		      "ld.q	r15, 0, r10\n\t"		\
		      "ld.q	r15, 8, r11\n\t"		\
		      "ld.q	r15, 16, r12\n\t"		\
		      "ld.q	r15, 24, r13\n\t"		\
		      "ld.q	r15, 32, r14\n\t"		\
		      "ld.q	r15, 40, r16\n\t"		\
		      "ld.q	r15, 48, r18\n\t"		\
		      "ld.q	r15, 56, r24\n\t"		\
		      "ld.q	r15, 64, r25\n\t"		\
		      "ld.q	r15, 72, r26\n\t"		\
		      "ld.q	r15, 80, r27\n\t"		\
		      "ld.q	r15, 88, r28\n\t"		\
		      "ld.q	r15, 96, r29\n\t"		\
		      "ld.q	r15, 104, r30\n\t"		\
		      "ld.q	r15, 112, r31\n\t"		\
		      "ld.q	r15, 120, r32\n\t"		\
		      "ld.q	r15, 128, r33\n\t"		\
		      "ld.q	r15, 136, r34\n\t"		\
		      "ld.q	r15, 144, r35\n\t"		\
		      "ld.q	r15, 152, r44\n\t"		\
		      "ld.q	r15, 160, r45\n\t"		\
		      "ld.q	r15, 168, r46\n\t"		\
		      "ld.q	r15, 176, r47\n\t"		\
		      "ld.q	r15, 184, r48\n\t"		\
		      "ld.q	r15, 192, r49\n\t"		\
		      "ld.q	r15, 200, r50\n\t"		\
		      "ld.q	r15, 208, r51\n\t"		\
		      "ld.q	r15, 216, r52\n\t"		\
		      "ld.q	r15, 224, r53\n\t"		\
		      "ld.q	r15, 232, r54\n\t"		\
		      "ld.q	r15, 240, r55\n\t"		\
		      "ld.q	r15, 248, r56\n\t"		\
		      "ld.q	r15, 256, r57\n\t"		\
		      "ld.q	r15, 264, r58\n\t"		\
		      "ld.q	r15, 272, r59\n\t"		\
		      "addi.l	r15, 304, r15\n\t"		\
		      : "=r" (r42)				\
		      : "r" (r36), "r" (r37), "r" (r38),	\
			"r" (r39), "r" (r40), "r" (r41));	\
 RESTORE_CALLER_SAVED						\
  last = r42;							\
} while (0)
#endif

#define nop() __asm__ __volatile__ ("nop")

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#define tas(ptr) (xchg((ptr), 1))

extern void __xchg_called_with_bad_pointer(void);

#define mb()	__asm__ __volatile__ ("synco": : :"memory")
#define rmb()	mb()
#define wmb()	__asm__ __volatile__ ("synco": : :"memory")
#define set_rmb(var, value) do { xchg(&var, value); } while (0)
#define set_mb(var, value) set_rmb(var, value)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

/* Interrupt Control */
#ifndef HARD_CLI
#define SR_MASK_L 0x000000f0L
#define SR_MASK_LL 0x00000000000000f0LL
#else
#define SR_MASK_L 0x10000000L
#define SR_MASK_LL 0x0000000010000000LL
#endif

extern __inline__ void __sti(void)
{
	/* cli/sti based on SR.BL */
	unsigned long long __dummy0, __dummy1=~SR_MASK_LL;

	__asm__ __volatile__("getcon	" __c0 ", %0\n\t"
			     "and	%0, %1, %0\n\t"
			     "putcon	%0, " __c0 "\n\t"
			     : "=&r" (__dummy0)
			     : "r" (__dummy1));
}

extern __inline__ void __cli(void)
{
	/* cli/sti based on SR.BL */
	unsigned long long __dummy0, __dummy1=SR_MASK_LL;
	__asm__ __volatile__("getcon	" __c0 ", %0\n\t"
			     "or	%0, %1, %0\n\t"
			     "putcon	%0, " __c0 "\n\t"
			     : "=&r" (__dummy0)
			     : "r" (__dummy1));
}

#define __save_flags(x) 						\
(__extension__ ({	unsigned long long __dummy=SR_MASK_LL;		\
	__asm__ __volatile__(						\
		"getcon	" __c0 ", %0\n\t"				\
		"and	%0, %1, %0"					\
		: "=&r" (x)						\
		: "r" (__dummy));}))

#define __save_and_cli(x)    							\
(__extension__ ({	unsigned long long __d2=SR_MASK_LL, __d1;	\
	__asm__ __volatile__(          	         			\
		"getcon	" __c0 ", %1\n\t" 				\
		"or	%1, r63, %0\n\t"				\
		"or	%1, %2, %1\n\t"					\
		"putcon	%1, " __c0 "\n\t"    				\
		"and	%0, %2, %0"    					\
		: "=&r" (x), "=&r" (__d1)				\
		: "r" (__d2));}));

#define __restore_flags(x) do { 					\
	if ( ((x) & SR_MASK_L) == 0 )		/* dropping to 0 ? */	\
		__sti();			/* yes...re-enable */	\
} while (0)

#define __save_and_sti(x)	do { __save_flags(x); __sti(); } while (0)

/* For spinlocks etc */
#define local_irq_save(x)	__save_and_cli(x)
#define local_irq_set(x)	__save_and_sti(x)
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
#define save_and_sti(x) do { save_flags(x); sti(x); } while (0)

#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define save_and_cli(x) __save_and_cli(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_sti(x) __save_and_sti(x)

#endif

extern __inline__ unsigned long xchg_u32(volatile int * m, unsigned long val)
{
	unsigned long flags, retval;

	save_and_cli(flags);
	retval = *m;
	*m = val;
	restore_flags(flags);
	return retval;
}

extern __inline__ unsigned long xchg_u8(volatile unsigned char * m, unsigned long val)
{
	unsigned long flags, retval;

	save_and_cli(flags);
	retval = *m;
	*m = val & 0xff;
	restore_flags(flags);
	return retval;
}

static __inline__ unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
	case 4:
		return xchg_u32(ptr, x);
		break;
	case 1:
		return xchg_u8(ptr, x);
		break;
	}
	__xchg_called_with_bad_pointer();
	return x;
}

/* XXX
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);


#define smp_mb()        barrier()
#define smp_rmb()       barrier()
#define smp_wmb()       barrier()

extern void print_seg(char *file,int line);

#define PLS() print_seg(__FILE__,__LINE__) 

#define PL() printk("@ <%s,%s:%d>\n",__FILE__,__FUNCTION__,__LINE__)

#endif /* __ASM_SH64_SYSTEM_H */
