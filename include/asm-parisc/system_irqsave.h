#ifndef __PARISC_SYSTEM_IRQSAVE_H
#define __PARISC_SYSTEM_IRQSAVE_H

/* interrupt control */
#define __save_flags(x)	__asm__ __volatile__("ssm 0, %0" : "=r" (x) : : "memory")
#define __restore_flags(x) __asm__ __volatile__("mtsm %0" : : "r" (x) : "memory")
#define __cli()	__asm__ __volatile__("rsm %0,%%r0\n" : : "i" (PSW_I) : "memory" )
#define __sti()	__asm__ __volatile__("ssm %0,%%r0\n" : : "i" (PSW_I) : "memory" )

#define __save_and_cli(x)  do { __save_flags(x); __cli(); } while(0);
#define __save_and_sti(x)  do { __save_flags(x); __sti(); } while(0);

/* For spinlocks etc */
#if 0
#define local_irq_save(x) \
	__asm__ __volatile__("rsm %1,%0" : "=r" (x) :"i" (PSW_I) : "memory" )
#define local_irq_set(x) \
#       "Warning local_irq_set(x) is not yet defined"
#else
#define local_irq_save(x)  __save_and_cli(x)
#define local_irq_set(x)   __save_and_sti(x)
#endif

#define local_irq_restore(x) __restore_flags(x)
#define local_irq_disable() __cli()
#define local_irq_enable()  __sti()

#endif /* __PARISC_SYSTEM_IRQSAVE_H */
