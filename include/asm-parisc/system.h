#ifndef __PARISC_SYSTEM_H
#define __PARISC_SYSTEM_H

#include <linux/config.h>
#include <asm/psw.h>
#include <asm/system_irqsave.h>

#ifdef CONFIG_SMP
#include <asm/spinlock_t.h>
#endif

/* The program status word as bitfields.  */
struct pa_psw {
	unsigned int y:1;
	unsigned int z:1;
	unsigned int rv:2;
	unsigned int w:1;
	unsigned int e:1;
	unsigned int s:1;
	unsigned int t:1;

	unsigned int h:1;
	unsigned int l:1;
	unsigned int n:1;
	unsigned int x:1;
	unsigned int b:1;
	unsigned int c:1;
	unsigned int v:1;
	unsigned int m:1;

	unsigned int cb:8;

	unsigned int o:1;
	unsigned int g:1;
	unsigned int f:1;
	unsigned int r:1;
	unsigned int q:1;
	unsigned int p:1;
	unsigned int d:1;
	unsigned int i:1;
};

#ifdef __LP64__
#define pa_psw(task) ((struct pa_psw *) ((char *) (task) + TASK_PT_PSW + 4))
#else
#define pa_psw(task) ((struct pa_psw *) ((char *) (task) + TASK_PT_PSW))
#endif

struct task_struct;

extern struct task_struct *_switch_to(struct task_struct *, struct task_struct *);

#define prepare_to_switch()	do { } while(0)
#define switch_to(prev, next, last) do {			\
	(last) = _switch_to(prev, next);			\
} while(0)


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


#define mfctl(reg)	({		\
	unsigned long cr;		\
	__asm__ __volatile__(		\
		"mfctl " #reg ",%0" :	\
		 "=r" (cr)		\
	);				\
	cr;				\
})

#define mtctl(gr, cr) \
	__asm__ __volatile__("mtctl %0,%1" \
		: /* no outputs */ \
		: "r" (gr), "i" (cr))

/* these are here to de-mystefy the calling code, and to provide hooks */
/* which I needed for debugging EIEM problems -PB */
#define get_eiem() mfctl(15)
static inline void set_eiem(unsigned long val)
{
	mtctl(val, 15);
}

#define mfsp(reg)	({		\
	unsigned long cr;		\
	__asm__ __volatile__(		\
		"mfsp " #reg ",%0" :	\
		 "=r" (cr)		\
	);				\
	cr;				\
})

#define mtsp(gr, cr) \
	__asm__ __volatile__("mtsp %0,%1" \
		: /* no outputs */ \
		: "r" (gr), "i" (cr))


/*
** This is simply the barrier() macro from linux/kernel.h but when serial.c
** uses tqueue.h uses smp_mb() defined using barrier(), linux/kernel.h
** hasn't yet been included yet so it fails, thus repeating the macro here.
**
** PA-RISC architecture allows for weakly ordered memory accesses although
** none of the processors use it. There is a strong ordered bit that is
** set in the O-bit of the page directory entry. Operating systems that
** can not tolerate out of order accesses should set this bit when mapping
** pages. The O-bit of the PSW should also be set to 1 (I don't believe any
** of the processor implemented the PSW O-bit). The PCX-W ERS states that
** the TLB O-bit is not implemented so the page directory does not need to
** have the O-bit set when mapping pages (section 3.1). This section also
** states that the PSW Y, Z, G, and O bits are not implemented.
** So it looks like nothing needs to be done for parisc-linux (yet).
** (thanks to chada for the above comment -ggg)
**
** The __asm__ op below simple prevents gcc/ld from reordering
** instructions across the mb() "call".
*/
#define mb()		__asm__ __volatile__("":::"memory");	/* barrier() */
#define rmb()		mb()
#define wmb()		mb()
#define smp_mb()	mb()
#define smp_wmb()	mb()

#define set_mb(var, value) do { var = value; mb(); } while (0)

#endif
