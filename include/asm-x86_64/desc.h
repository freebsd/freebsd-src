/* Written 2000 by Andi Kleen */ 
#ifndef __ARCH_DESC_H
#define __ARCH_DESC_H

#include <linux/threads.h>
#include <asm/ldt.h>

#ifndef __ASSEMBLY__

#define __TSS_INDEX(n)  ((n)*64)
#define __LDT_INDEX(n)  ((n)*64)

extern __u8 gdt_table[];
extern __u8 gdt_end[];

enum { 
	GATE_INTERRUPT = 0xE, 
	GATE_TRAP = 0xF, 	
	GATE_CALL = 0xC,
}; 	

// 16byte gate
struct gate_struct {          
	u16 offset_low;
	u16 segment; 
	unsigned ist : 3, zero0 : 5, type : 5, dpl : 2, p : 1;
	u16 offset_middle;
	u32 offset_high;
	u32 zero1; 
} __attribute__((packed));

// 8 byte segment descriptor
struct desc_struct { 
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 4, s : 1, dpl : 2, p : 1;
	unsigned limit : 4, avl : 1, l : 1, d : 1, g : 1, base2 : 8;
} __attribute__((packed)); 

// LDT or TSS descriptor in the GDT. 16 bytes.
struct ldttss_desc { 
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 5, dpl : 2, p : 1;
	unsigned limit1 : 4, zero0 : 3, g : 1, base2 : 8;
	u32 base3;
	u32 zero1; 
} __attribute__((packed)); 


struct per_cpu_gdt {
	struct ldttss_desc tss;
	struct ldttss_desc ldt; 
} ____cacheline_aligned; 

extern struct per_cpu_gdt gdt_cpu_table[]; 

#define PTR_LOW(x) ((unsigned long)(x) & 0xFFFF) 
#define PTR_MIDDLE(x) (((unsigned long)(x) >> 16) & 0xFFFF)
#define PTR_HIGH(x) ((unsigned long)(x) >> 32)

enum { 
	DESC_TSS = 0x9,
	DESC_LDT = 0x2,
}; 

struct desc_ptr {
	unsigned short size;
	unsigned long address;
} __attribute__((packed)) ;

#define __GDT_HEAD_SIZE 64
#define __CPU_DESC_INDEX(x,field) \
	((x) * sizeof(struct per_cpu_gdt) + offsetof(struct per_cpu_gdt, field) + __GDT_HEAD_SIZE)

#define load_TR(cpu) asm volatile("ltr %w0"::"r" (__CPU_DESC_INDEX(cpu, tss)));
#define __load_LDT(cpu) asm volatile("lldt %w0"::"r" (__CPU_DESC_INDEX(cpu, ldt)));
#define clear_LDT(n)  asm volatile("lldt %w0"::"r" (0))

extern struct gate_struct idt_table[]; 

#define copy_16byte(dst,src)  memcpy((dst), (src), 16)

static inline void _set_gate(void *adr, unsigned type, unsigned long func, unsigned dpl, unsigned ist)  
{
	struct gate_struct s; 	
	s.offset_low = PTR_LOW(func); 
	s.segment = __KERNEL_CS;
	s.ist = ist; 
	s.p = 1;
	s.dpl = dpl; 
	s.zero0 = 0;
	s.zero1 = 0; 
	s.type = type; 
	s.offset_middle = PTR_MIDDLE(func); 
	s.offset_high = PTR_HIGH(func); 
	copy_16byte(adr, &s);
} 

static inline void set_intr_gate(int nr, void *func) 
{ 
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 0, 0); 
} 

static inline void set_intr_gate_ist(int nr, void *func, unsigned ist) 
{ 
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 0, ist); 
} 

static inline void set_system_gate(int nr, void *func) 
{ 
	_set_gate(&idt_table[nr], GATE_INTERRUPT, (unsigned long) func, 3, 0); 
} 

static inline void set_tssldt_descriptor(struct ldttss_desc *dst, unsigned long ptr, unsigned type, 
					 unsigned size) 
{ 
	memset(dst,0,16); 
	dst->limit0 = size & 0xFFFF;
	dst->base0 = PTR_LOW(ptr); 
	dst->base1 = PTR_MIDDLE(ptr) & 0xFF; 
	dst->type = type;
	dst->p = 1; 
	dst->g = 1;
	dst->limit1 = (size >> 16) & 0xF;
	dst->base2 = (PTR_MIDDLE(ptr) >> 8) & 0xFF; 
	dst->base3 = PTR_HIGH(ptr); 
}

static inline void set_tss_desc(unsigned n, void *addr)
{ 
	set_tssldt_descriptor((void *)&gdt_table + __CPU_DESC_INDEX(n,tss), (unsigned long)addr, DESC_TSS, sizeof(struct tss_struct)); 
} 

static inline void set_ldt_desc(unsigned n, void *addr, int size)
{ 
	set_tssldt_descriptor((void *)&gdt_table + __CPU_DESC_INDEX(n,ldt), (unsigned long)addr, DESC_LDT, size); 
}	

/*
 * load one particular LDT into the current CPU
 */
static inline void load_LDT (struct mm_struct *mm)
{
	int cpu = smp_processor_id();
	void *segments = mm->context.segments;

	if (!segments) {
		clear_LDT(cpu);
		return;
	}
		
	set_ldt_desc(cpu, segments, LDT_ENTRIES);
	__load_LDT(cpu);
}

#endif /* !__ASSEMBLY__ */

#endif
