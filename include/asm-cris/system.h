#ifndef __ASM_CRIS_SYSTEM_H
#define __ASM_CRIS_SYSTEM_H

#include <linux/config.h>

#include <asm/segment.h>

/* the switch_to macro calls resume, an asm function in entry.S which does the actual
 * task switching.
 */

extern struct task_struct *resume(struct task_struct *prev, struct task_struct *next, int);
#define prepare_to_switch()     do { } while(0)
#define switch_to(prev,next,last) last = resume(prev,next, \
					 (int)&((struct task_struct *)0)->thread)

/* read the CPU PC register */

extern inline unsigned long rdpc(void)
{ 
	unsigned long pc;
	__asm__ volatile ("move.d $pc,%0" : "=rm" (pc));
	return pc;
}

/* read the CPU version register */

extern inline unsigned long rdvr(void) { 
	unsigned char vr;
	__asm__ volatile ("move $vr,%0" : "=rm" (vr));
	return vr;
}

/* read/write the user-mode stackpointer */

extern inline unsigned long rdusp(void) {
	unsigned long usp;
	__asm__ __volatile__("move $usp,%0" : "=rm" (usp));
	return usp;
}

#define wrusp(usp) \
	__asm__ __volatile__("move %0,$usp" : /* no outputs */ : "rm" (usp))

/* read the current stackpointer */

extern inline unsigned long rdsp(void) {
	unsigned long sp;
	__asm__ __volatile__("move.d $sp,%0" : "=rm" (sp));
	return sp;
}

extern inline unsigned long _get_base(char * addr)
{
  return 0;
}

#define nop() __asm__ __volatile__ ("nop");

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((struct __xchg_dummy *)(x))

#ifdef CONFIG_ETRAX_DEBUG_INTERRUPT
#if 0
/* use these and an oscilloscope to see the fraction of time we're running with IRQ's disabled */
/* it assumes the LED's are on port 0x90000000 of course. */
#define sti() __asm__ __volatile__ ( "ei\n\tpush $r0\n\tmoveq 0,$r0\n\tmove.d $r0,[0x90000000]\n\tpop $r0" );
#define cli() __asm__ __volatile__ ( "di\n\tpush $r0\n\tmove.d 0x40000,$r0\n\tmove.d $r0,[0x90000000]\n\tpop $r0");
#define save_flags(x) __asm__ __volatile__ ("move $ccr,%0" : "=rm" (x) : : "memory");
#define restore_flags(x) __asm__ __volatile__ ("move %0,$ccr\n\tbtstq 5,%0\n\tbpl 1f\n\tnop\n\tpush $r0\n\tmoveq 0,$r0\n\tmove.d $r0,[0x90000000]\n\tpop $r0\n1:\n" : : "r" (x) : "memory");
#else

/* Log when interrupts are turned on and off and who did it. */
#define CCR_EI_MASK (1 << 5)
/* in debug.c */
extern int log_int_pos;
extern int log_int_size;
extern int log_int_enable;
extern int log_int_trig0_pos;
extern int log_int_trig1_pos;
extern void log_int(unsigned long pc, unsigned long prev_ccr, unsigned long next_ccr);

/* If you only want to log changes - change to 1 to a 0 below */
#define LOG_INT(pc, curr_ccr, next_ccr) do { \
  if (1 || (curr_ccr ^ next_ccr) & CCR_EI_MASK) \
          log_int((pc), curr_ccr, next_ccr); \
  }while(0)

#define __save_flags(x) __asm__ __volatile__ ("move $ccr,%0" : "=rm" (x) : : "memory");

extern inline void __cli(void)
{
  unsigned long pc = rdpc();  
  unsigned long curr_ccr; __save_flags(curr_ccr); 
  LOG_INT(pc, curr_ccr, 0); 
  __asm__ __volatile__ ( "di" : : :"memory");
}


extern inline void __sti(void)
{
  unsigned long pc = rdpc();  
  unsigned long curr_ccr; __save_flags(curr_ccr); 
  LOG_INT(pc, curr_ccr, CCR_EI_MASK); 
  __asm__ __volatile__ ( "ei" : : :"memory");
}

extern inline void __restore_flags(unsigned long x)
{
  unsigned long pc = rdpc();
  unsigned long curr_ccr; __save_flags(curr_ccr);
  LOG_INT(pc, curr_ccr, x);   
  __asm__ __volatile__ ("move %0,$ccr" : : "rm" (x) : "memory");
}

/* For spinlocks etc */
#define local_irq_save(x) do { __save_flags(x); __cli(); }while (0)
#define local_irq_restore(x) restore_flags(x)

#define local_irq_disable()  cli()
#define local_irq_enable()   sti()

#endif

#else
#define __cli() __asm__ __volatile__ ( "di" : : :"memory");
#define __sti() __asm__ __volatile__ ( "ei" : : :"memory");
#define __save_flags(x) __asm__ __volatile__ ("move $ccr,%0" : "=rm" (x) : : "memory");
#define __restore_flags(x) __asm__ __volatile__ ("move %0,$ccr" : : "rm" (x) : "memory");

/* For spinlocks etc */
#define local_irq_save(x) __asm__ __volatile__ ("move $ccr,%0\n\tdi" : "=rm" (x) : : "memory"); 
#define local_irq_set(x) __asm__ __volatile__ ("move $ccr,%0\n\tei" : "=rm" (x) : : "memory");
#define local_irq_restore(x) restore_flags(x)

#define local_irq_disable()  cli()
#define local_irq_enable()   sti()

#endif

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_cli(x) do { save_flags(x); cli(); } while(0)
#define save_and_sti(x) do { save_flags(x); sti(); } while(0)

extern inline unsigned long __xchg(unsigned long x, void * ptr, int size)
{
  /* since Etrax doesn't have any atomic xchg instructions, we need to disable
     irq's (if enabled) and do it with move.d's */
#if 0
  unsigned int flags;
  save_flags(flags); /* save flags, including irq enable bit */
  cli();             /* shut off irq's */
  switch (size) {
  case 1:
    __asm__ __volatile__ (
       "move.b %0,r0\n\t"
       "move.b %1,%0\n\t"
       "move.b r0,%1\n\t"
       : "=r" (x)
       : "m" (*__xg(ptr)), "r" (x)
       : "memory","r0");    
    break;
  case 2:
    __asm__ __volatile__ (
       "move.w %0,r0\n\t"
       "move.w %1,%0\n\t"
       "move.w r0,%1\n\t"
       : "=r" (x)
       : "m" (*__xg(ptr)), "r" (x)
       : "memory","r0");
    break;
  case 4:
    __asm__ __volatile__ (
       "move.d %0,r0\n\t"
       "move.d %1,%0\n\t"
       "move.d r0,%1\n\t"
       : "=r" (x)
       : "m" (*__xg(ptr)), "r" (x)
       : "memory","r0");
    break;
  }
  restore_flags(flags); /* restore irq enable bit */
  return x;
#else
  unsigned long flags,temp;
  save_flags(flags); /* save flags, including irq enable bit */
  cli();             /* shut off irq's */
  switch (size) {
  case 1:
    *((unsigned char *)&temp) = x;
    x = *(unsigned char *)ptr;
    *(unsigned char *)ptr = *((unsigned char *)&temp);
    break;
  case 2:
    *((unsigned short *)&temp) = x;
    x = *(unsigned short *)ptr;
    *(unsigned short *)ptr = *((unsigned short *)&temp);
    break;
  case 4:
    temp = x;
    x = *(unsigned long *)ptr;
    *(unsigned long *)ptr = temp;
    break;
  }
  restore_flags(flags); /* restore irq enable bit */
  return x;
#endif
}

#define mb() __asm__ __volatile__ ("" : : : "memory")
#define rmb() mb()
#define wmb() mb()
#define set_mb(var, value)  do { var = value; mb(); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

#ifdef CONFIG_SMP
#define smp_mb()        mb()
#define smp_rmb()       rmb()
#define smp_wmb()       wmb()
#else
#define smp_mb()        barrier()
#define smp_rmb()       barrier()
#define smp_wmb()       barrier()
#endif

#define iret()

/*
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

#endif
