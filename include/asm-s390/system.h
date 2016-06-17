/*
 *  include/asm-s390/system.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *
 *  Derived from "include/asm-i386/system.h"
 */

#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <linux/config.h>
#include <asm/types.h>
#ifdef __KERNEL__
#include <asm/lowcore.h>
#endif
#include <linux/kernel.h>

#define prepare_to_switch()	do { } while(0)
#define switch_to(prev,next,last) do {                                       \
        if (prev == next)                                                    \
                break;                                                       \
	save_fp_regs1(&prev->thread.fp_regs);                                \
	restore_fp_regs1(&next->thread.fp_regs);              		     \
	last = resume(prev,next);					     \
} while (0)

struct task_struct;

#define nop() __asm__ __volatile__ ("nop")

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

extern void __misaligned_u16(void);
extern void __misaligned_u32(void);

static inline unsigned long __xchg(unsigned long x, void * ptr, int size)
{
        switch (size) {
                case 1:
                        asm volatile (
                                "   lhi   1,3\n"
                                "   nr    1,%0\n"     /* isolate last 2 bits */
                                "   xr    %0,1\n"     /* align ptr */
                                "   bras  2,0f\n"
                                "   icm   1,8,3(%1)\n"   /* for ptr&3 == 0 */
                                "   stcm  0,8,3(%1)\n"
                                "   icm   1,4,3(%1)\n"   /* for ptr&3 == 1 */
                                "   stcm  0,4,3(%1)\n"
                                "   icm   1,2,3(%1)\n"   /* for ptr&3 == 2 */
                                "   stcm  0,2,3(%1)\n"
                                "   icm   1,1,3(%1)\n"   /* for ptr&3 == 3 */
                                "   stcm  0,1,3(%1)\n"
                                "0: sll   1,3\n"
                                "   la    2,0(1,2)\n" /* r2 points to an icm */
                                "   l     0,0(%0)\n"  /* get fullword */
                                "1: lr    1,0\n"      /* cs loop */
                                "   ex    0,0(2)\n"   /* insert x */
                                "   cs    0,1,0(%0)\n"
                                "   jl    1b\n"
                                "   ex    0,4(2)"     /* store *ptr to x */
                                : "+a&" (ptr) : "a" (&x)
                                : "memory", "cc", "0", "1", "2");
			break;
                case 2:
                        if(((__u32)ptr)&1)
				__misaligned_u16();
                        asm volatile (
                                "   lhi   1,2\n"
                                "   nr    1,%0\n"     /* isolate bit 2^1 */
                                "   xr    %0,1\n"     /* align ptr */
                                "   bras  2,0f\n"
                                "   icm   1,12,2(%1)\n"   /* for ptr&2 == 0 */
                                "   stcm  0,12,2(%1)\n"
                                "   icm   1,3,2(%1)\n"    /* for ptr&2 == 1 */
                                "   stcm  0,3,2(%1)\n"
                                "0: sll   1,2\n"
                                "   la    2,0(1,2)\n" /* r2 points to an icm */
                                "   l     0,0(%0)\n"  /* get fullword */
                                "1: lr    1,0\n"      /* cs loop */
                                "   ex    0,0(2)\n"   /* insert x */
                                "   cs    0,1,0(%0)\n"
                                "   jl    1b\n"
                                "   ex    0,4(2)"     /* store *ptr to x */
                                : "+a&" (ptr) : "a" (&x)
                                : "memory", "cc", "0", "1", "2");
                        break;
                case 4:
                        if(((__u32)ptr)&3)
				__misaligned_u32();
                        asm volatile (
                                "    l   0,0(%1)\n"
                                "0:  cs  0,%0,0(%1)\n"
                                "    jl  0b\n"
                                "    lr  %0,0\n"
                                : "+d&" (x) : "a" (ptr)
                                : "memory", "cc", "0" );
                        break;
        }
        return x;
}

/*
 * Force strict CPU ordering.
 * And yes, this is required on UP too when we're talking
 * to devices.
 *
 * This is very similar to the ppc eieio/sync instruction in that is
 * does a checkpoint syncronisation & makes sure that 
 * all memory ops have completed wrt other CPU's ( see 7-15 POP  DJB ).
 */

#define eieio()  __asm__ __volatile__ ("BCR 15,0") 
# define SYNC_OTHER_CORES(x)   eieio() 
#define mb()    eieio()
#define rmb()   eieio()
#define wmb()   eieio()
#define smp_mb()       mb()
#define smp_rmb()      rmb()
#define smp_wmb()      wmb()
#define smp_mb__before_clear_bit()     smp_mb()
#define smp_mb__after_clear_bit()      smp_mb()


#define set_mb(var, value)      do { var = value; mb(); } while (0)
#define set_wmb(var, value)     do { var = value; wmb(); } while (0)

/* interrupt control.. */
#define __sti() ({ \
        __u8 dummy; \
        __asm__ __volatile__ ( \
                "stosm 0(%0),0x03" : : "a" (&dummy) : "memory"); \
        })

#define __cli() ({ \
        __u32 flags; \
        __asm__ __volatile__ ( \
                "stnsm 0(%0),0xFC" : : "a" (&flags) : "memory"); \
        flags; \
        })

#define __save_flags(x) \
        __asm__ __volatile__("stosm 0(%0),0" : : "a" (&x) : "memory")

#define __restore_flags(x) \
        __asm__ __volatile__("ssm   0(%0)" : : "a" (&x) : "memory")

#define __load_psw(psw) \
	__asm__ __volatile__("lpsw 0(%0)" : : "a" (&psw) : "cc" );

#define __ctl_load(array, low, high) ({ \
	__asm__ __volatile__ ( \
		"   la    1,%0\n" \
		"   bras  2,0f\n" \
                "   lctl  0,0,0(1)\n" \
		"0: ex    %1,0(2)" \
		: : "m" (array), "a" (((low)<<4)+(high)) : "1", "2" ); \
	})

#define __ctl_store(array, low, high) ({ \
	__asm__ __volatile__ ( \
		"   la    1,%0\n" \
		"   bras  2,0f\n" \
		"   stctl 0,0,0(1)\n" \
		"0: ex    %1,0(2)" \
		: "=m" (array) : "a" (((low)<<4)+(high)): "1", "2" ); \
	})

#define __ctl_set_bit(cr, bit) ({ \
        __u8 dummy[16]; \
        __asm__ __volatile__ ( \
                "    la    1,%0\n"       /* align to 8 byte */ \
                "    ahi   1,7\n" \
                "    srl   1,3\n" \
                "    sll   1,3\n" \
                "    bras  2,0f\n"       /* skip indirect insns */ \
                "    stctl 0,0,0(1)\n" \
                "    lctl  0,0,0(1)\n" \
                "0:  ex    %1,0(2)\n"    /* execute stctl */ \
                "    l     0,0(1)\n" \
                "    or    0,%2\n"       /* set the bit */ \
                "    st    0,0(1)\n" \
                "1:  ex    %1,4(2)"      /* execute lctl */ \
                : "=m" (dummy) : "a" (cr*17), "a" (1<<(bit)) \
                : "cc", "0", "1", "2"); \
        })

#define __ctl_clear_bit(cr, bit) ({ \
        __u8 dummy[16]; \
        __asm__ __volatile__ ( \
                "    la    1,%0\n"       /* align to 8 byte */ \
                "    ahi   1,7\n" \
                "    srl   1,3\n" \
                "    sll   1,3\n" \
                "    bras  2,0f\n"       /* skip indirect insns */ \
                "    stctl 0,0,0(1)\n" \
                "    lctl  0,0,0(1)\n" \
                "0:  ex    %1,0(2)\n"    /* execute stctl */ \
                "    l     0,0(1)\n" \
                "    nr    0,%2\n"       /* set the bit */ \
                "    st    0,0(1)\n" \
                "1:  ex    %1,4(2)"      /* execute lctl */ \
                : "=m" (dummy) : "a" (cr*17), "a" (~(1<<(bit))) \
                : "cc", "0", "1", "2"); \
        })

#define __save_and_cli(x)	do { __save_flags(x); __cli(); } while(0);
#define __save_and_sti(x)	do { __save_flags(x); __sti(); } while(0);

/* For spinlocks etc */
#define local_irq_save(x)	((x) = __cli())
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
#define save_and_cli(x) do { save_flags(x); cli(); } while(0);
#define save_and_sti(x) do { save_flags(x); sti(); } while(0);

extern void smp_ctl_set_bit(int cr, int bit);
extern void smp_ctl_clear_bit(int cr, int bit);
#define ctl_set_bit(cr, bit) smp_ctl_set_bit(cr, bit)
#define ctl_clear_bit(cr, bit) smp_ctl_clear_bit(cr, bit)

#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_cli(x) __save_and_cli(x)
#define save_and_sti(x) __save_and_sti(x)

#define ctl_set_bit(cr, bit) __ctl_set_bit(cr, bit)
#define ctl_clear_bit(cr, bit) __ctl_clear_bit(cr, bit)


#endif

#ifdef __KERNEL__
extern struct task_struct *resume(void *, void *);

extern int save_fp_regs1(s390_fp_regs *fpregs);
extern void save_fp_regs(s390_fp_regs *fpregs);
extern int restore_fp_regs1(s390_fp_regs *fpregs);
extern void restore_fp_regs(s390_fp_regs *fpregs);

extern void (*_machine_restart)(char *command);
extern void (*_machine_halt)(void);
extern void (*_machine_power_off)(void);

#endif

#endif



