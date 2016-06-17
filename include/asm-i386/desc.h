#ifndef __ARCH_DESC_H
#define __ARCH_DESC_H

#include <asm/ldt.h>

/*
 * The layout of the GDT under Linux:
 *
 *   0 - null
 *   1 - not used
 *   2 - kernel code segment
 *   3 - kernel data segment
 *   4 - user code segment                  <-- new cacheline 
 *   5 - user data segment
 *   6 - not used
 *   7 - not used
 *   8 - APM BIOS support                   <-- new cacheline 
 *   9 - APM BIOS support
 *  10 - APM BIOS support
 *  11 - APM BIOS support
 *
 * The TSS+LDT descriptors are spread out a bit so that every CPU
 * has an exclusive cacheline for the per-CPU TSS and LDT:
 *
 *  12 - CPU#0 TSS                          <-- new cacheline 
 *  13 - CPU#0 LDT
 *  14 - not used 
 *  15 - not used 
 *  16 - CPU#1 TSS                          <-- new cacheline 
 *  17 - CPU#1 LDT
 *  18 - not used 
 *  19 - not used 
 *  ... NR_CPUS per-CPU TSS+LDT's if on SMP
 *
 * Entry into gdt where to find first TSS.
 */
#define __FIRST_TSS_ENTRY 12
#define __FIRST_LDT_ENTRY (__FIRST_TSS_ENTRY+1)

#define __TSS(n) (((n)<<2) + __FIRST_TSS_ENTRY)
#define __LDT(n) (((n)<<2) + __FIRST_LDT_ENTRY)

#ifndef __ASSEMBLY__
struct desc_struct {
	unsigned long a,b;
};

extern struct desc_struct gdt_table[];
extern struct desc_struct *idt, *gdt;

struct Xgt_desc_struct {
	unsigned short size;
	unsigned long address __attribute__((packed));
};

#define idt_descr (*(struct Xgt_desc_struct *)((char *)&idt - 2))
#define gdt_descr (*(struct Xgt_desc_struct *)((char *)&gdt - 2))

#define load_TR(n) __asm__ __volatile__("ltr %%ax"::"a" (__TSS(n)<<3))

#define __load_LDT(n) __asm__ __volatile__("lldt %%ax"::"a" (__LDT(n)<<3))

/*
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt[];
extern void set_intr_gate(unsigned int irq, void * addr);
extern void set_ldt_desc(unsigned int n, void *addr, unsigned int size);
extern void set_tss_desc(unsigned int n, void *addr);

static inline void clear_LDT(void)
{
	int cpu = smp_processor_id();
	set_ldt_desc(cpu, &default_ldt[0], 5);
	__load_LDT(cpu);
}

/*
 * load one particular LDT into the current CPU
 */
static inline void load_LDT (mm_context_t *pc)
{
	int cpu = smp_processor_id();
	void *segments = pc->ldt;
	int count = pc->size;

	if (!count) {
		segments = &default_ldt[0];
		count = 5;
	}
		
	set_ldt_desc(cpu, segments, count);
	__load_LDT(cpu);
}

#endif /* !__ASSEMBLY__ */

#endif
