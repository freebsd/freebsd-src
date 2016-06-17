#ifndef X86_64_MSR_H
#define X86_64_MSR_H 1

#ifndef __ASSEMBLY__
/*
 * Access to machine-specific registers (available on 586 and better only)
 * Note: the rd* operations modify the parameters directly (without using
 * pointer indirection), this allows gcc to optimize better
 */

#define rdmsr(msr,val1,val2) \
       __asm__ __volatile__("rdmsr" \
			    : "=a" (val1), "=d" (val2) \
			    : "c" (msr))


#define rdmsrl(msr,val) do { unsigned long a__,b__; \
       __asm__ __volatile__("rdmsr" \
			    : "=a" (a__), "=d" (b__) \
			    : "c" (msr)); \
       val = a__ | (b__<<32); \
} while(0); 

#define wrmsr(msr,val1,val2) \
     __asm__ __volatile__("wrmsr" \
			  : /* no outputs */ \
			  : "c" (msr), "a" (val1), "d" (val2))

#define wrmsrl(msr,val) wrmsr(msr,(__u32)((__u64)(val)),((__u64)(val))>>32) 

/* wrmsrl with exception handling */
#define checking_wrmsrl(msr,val) ({ int ret__;					\
	asm volatile("2: wrmsr ; xorl %0,%0\n"					\
		     "1:\n\t"							\
		     ".section .fixup,\"ax\"\n\t"				\
		     "3:  movl %4,%0 ; jmp 1b\n\t"				\
		     ".previous\n\t"						\
 		     ".section __ex_table,\"a\"\n"				\
		     "   .align 8\n\t"						\
		     "   .quad 	2b,3b\n\t"					\
		     ".previous"						\
		     : "=a" (ret__)						\
		     : "c" (msr), "0" ((__u32)val), "d" ((val)>>32), "i" (-EFAULT));\
	ret__; })

#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscl(low) \
     __asm__ __volatile__ ("rdtsc" : "=a" (low) : : "edx")

#define rdtscll(val) do { \
     unsigned int a,d; \
     asm volatile("rdtsc" : "=a" (a), "=d" (d)); \
     (val) = ((unsigned long)a) | (((unsigned long)d)<<32); \
} while(0)

#define rdpmc(counter,low,high) \
     __asm__ __volatile__("rdpmc" \
			  : "=a" (low), "=d" (high) \
			  : "c" (counter))

#define write_tsc(val1,val2) wrmsr(0x10, val1, val2)

#define rdpmc(counter,low,high) \
     __asm__ __volatile__("rdpmc" \
			  : "=a" (low), "=d" (high) \
			  : "c" (counter))

#endif

/* AMD/K8 specific MSRs */ 
#define MSR_EFER 0xc0000080		/* extended feature register */
#define MSR_STAR 0xc0000081		/* legacy mode SYSCALL target */
#define MSR_LSTAR 0xc0000082 		/* long mode SYSCALL target */
#define MSR_CSTAR 0xc0000083		/* compatibility mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084	/* EFLAGS mask for syscall */
#define MSR_FS_BASE 0xc0000100		/* 64bit GS base */
#define MSR_GS_BASE 0xc0000101		/* 64bit FS base */
#define MSR_KERNEL_GS_BASE  0xc0000102	/* SwapGS GS shadow (or USER_GS from kernel) */ 
/* EFER bits: */ 
#define _EFER_SCE 0  /* SYSCALL/SYSRET */
#define _EFER_LME 8  /* Long mode enable */
#define _EFER_LMA 10 /* Long mode active (read-only) */
#define _EFER_NX 11  /* No execute enable */

#define EFER_SCE (1<<_EFER_SCE)
#define EFER_LME (1<<EFER_LME)
#define EFER_LMA (1<<EFER_LMA)
#define EFER_NX (1<<_EFER_NX)

/* Intel MSRs. Some also available on other CPUs */
#define MSR_IA32_PLATFORM_ID	0x17

#define MSR_IA32_PERFCTR0      0xc1
#define MSR_IA32_PERFCTR1      0xc2

#define MSR_MTRRcap		0x0fe
#define MSR_IA32_BBL_CR_CTL        0x119

#define MSR_IA32_MCG_CAP       0x179
#define MSR_IA32_MCG_STATUS        0x17a
#define MSR_IA32_MCG_CTL       0x17b

#define MSR_IA32_EVNTSEL0      0x186
#define MSR_IA32_EVNTSEL1      0x187

#define MSR_IA32_DEBUGCTLMSR       0x1d9
#define MSR_IA32_LASTBRANCHFROMIP  0x1db
#define MSR_IA32_LASTBRANCHTOIP        0x1dc
#define MSR_IA32_LASTINTFROMIP     0x1dd
#define MSR_IA32_LASTINTTOIP       0x1de

#define MSR_MTRRfix64K_00000	0x250
#define MSR_MTRRfix16K_80000	0x258
#define MSR_MTRRfix16K_A0000	0x259
#define MSR_MTRRfix4K_C0000	0x268
#define MSR_MTRRfix4K_C8000	0x269
#define MSR_MTRRfix4K_D0000	0x26a
#define MSR_MTRRfix4K_D8000	0x26b
#define MSR_MTRRfix4K_E0000	0x26c
#define MSR_MTRRfix4K_E8000	0x26d
#define MSR_MTRRfix4K_F0000	0x26e
#define MSR_MTRRfix4K_F8000	0x26f
#define MSR_MTRRdefType		0x2ff

#define MSR_IA32_MC0_CTL       0x400
#define MSR_IA32_MC0_STATUS        0x401
#define MSR_IA32_MC0_ADDR      0x402
#define MSR_IA32_MC0_MISC      0x403

#define MSR_P6_PERFCTR0			0xc1
#define MSR_P6_PERFCTR1			0xc2
#define MSR_P6_EVNTSEL0			0x186
#define MSR_P6_EVNTSEL1			0x187

/* K7/K8 MSRs. Not complete. See the architecture manual for a more complete list. */
#define MSR_K7_EVNTSEL0            0xC0010000
#define MSR_K7_PERFCTR0            0xC0010004
#define MSR_K7_EVNTSEL1            0xC0010001
#define MSR_K7_PERFCTR1            0xC0010005
#define MSR_K7_EVNTSEL2            0xC0010002
#define MSR_K7_PERFCTR2            0xC0010006
#define MSR_K7_EVNTSEL3            0xC0010003
#define MSR_K7_PERFCTR3            0xC0010007
#define MSR_K8_TOP_MEM1		   0xC001001A
#define MSR_K8_TOP_MEM2		   0xC001001D
#define MSR_K8_SYSCFG		   0xC0000010	

/* K6 MSRs */
#define MSR_K6_EFER			0xC0000080
#define MSR_K6_STAR			0xC0000081
#define MSR_K6_WHCR			0xC0000082
#define MSR_K6_UWCCR			0xC0000085
#define MSR_K6_PSOR			0xC0000087
#define MSR_K6_PFIR			0xC0000088

/* Centaur-Hauls/IDT defined MSRs. */
#define MSR_IDT_FCR1			0x107
#define MSR_IDT_FCR2			0x108
#define MSR_IDT_FCR3			0x109
#define MSR_IDT_FCR4			0x10a

#define MSR_IDT_MCR0			0x110
#define MSR_IDT_MCR1			0x111
#define MSR_IDT_MCR2			0x112
#define MSR_IDT_MCR3			0x113
#define MSR_IDT_MCR4			0x114
#define MSR_IDT_MCR5			0x115
#define MSR_IDT_MCR6			0x116
#define MSR_IDT_MCR7			0x117
#define MSR_IDT_MCR_CTRL		0x120

/* VIA Cyrix defined MSRs*/
#define MSR_VIA_FCR			0x1107
#define MSR_VIA_LONGHAUL		0x110a
#define MSR_VIA_RNG			0x110b
#define MSR_VIA_BCR2			0x1147

/* Intel defined MSRs. */
#define MSR_IA32_P5_MC_ADDR		0
#define MSR_IA32_P5_MC_TYPE		1
#define MSR_IA32_PLATFORM_ID		0x17
#define MSR_IA32_EBL_CR_POWERON		0x2a

#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_BASE          (0xfffff<<12)

#endif
