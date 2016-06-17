#ifndef __ASM_MSR_H
#define __ASM_MSR_H

/*
 * Access to machine-specific registers (available on 586 and better only)
 * Note: the rd* operations modify the parameters directly (without using
 * pointer indirection), this allows gcc to optimize better
 */

#define rdmsr(msr,val1,val2) \
     __asm__ __volatile__("rdmsr" \
			  : "=a" (val1), "=d" (val2) \
			  : "c" (msr))

#define wrmsr(msr,val1,val2) \
     __asm__ __volatile__("wrmsr" \
			  : /* no outputs */ \
			  : "c" (msr), "a" (val1), "d" (val2))

#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscl(low) \
     __asm__ __volatile__("rdtsc" : "=a" (low) : : "edx")

#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))

#define write_tsc(val1,val2) wrmsr(0x10, val1, val2)

#define rdpmc(counter,low,high) \
     __asm__ __volatile__("rdpmc" \
			  : "=a" (low), "=d" (high) \
			  : "c" (counter))

/* symbolic names for some interesting MSRs */
/* Intel defined MSRs. */
#define MSR_IA32_P5_MC_ADDR		0
#define MSR_IA32_P5_MC_TYPE		1
#define MSR_IA32_PLATFORM_ID		0x17
#define MSR_IA32_EBL_CR_POWERON		0x2a

#define MSR_IA32_APICBASE		0x1b
#define MSR_IA32_APICBASE_BSP		(1<<8)
#define MSR_IA32_APICBASE_ENABLE	(1<<11)
#define MSR_IA32_APICBASE_BASE		(0xfffff<<12)

#define MSR_IA32_UCODE_WRITE		0x79
#define MSR_IA32_UCODE_REV		0x8b

#define MSR_IA32_BBL_CR_CTL		0x119

#define MSR_IA32_MCG_CAP		0x179
#define MSR_IA32_MCG_STATUS		0x17a
#define MSR_IA32_MCG_CTL		0x17b

#define MSR_IA32_THERM_CONTROL		0x19a
#define MSR_IA32_THERM_INTERRUPT	0x19b
#define MSR_IA32_THERM_STATUS		0x19c
#define MSR_IA32_MISC_ENABLE		0x1a0

#define MSR_IA32_DEBUGCTLMSR		0x1d9
#define MSR_IA32_LASTBRANCHFROMIP	0x1db
#define MSR_IA32_LASTBRANCHTOIP		0x1dc
#define MSR_IA32_LASTINTFROMIP		0x1dd
#define MSR_IA32_LASTINTTOIP		0x1de

#define MSR_IA32_MC0_CTL		0x400
#define MSR_IA32_MC0_STATUS		0x401
#define MSR_IA32_MC0_ADDR		0x402
#define MSR_IA32_MC0_MISC		0x403

#define MSR_P6_PERFCTR0			0xc1
#define MSR_P6_PERFCTR1			0xc2
#define MSR_P6_EVNTSEL0			0x186
#define MSR_P6_EVNTSEL1			0x187

#define MSR_IA32_PERF_STATUS		0x198
#define MSR_IA32_PERF_CTL		0x199

/* AMD Defined MSRs */
#define MSR_K6_EFER			0xC0000080
#define MSR_K6_STAR			0xC0000081
#define MSR_K6_WHCR			0xC0000082
#define MSR_K6_UWCCR			0xC0000085
#define MSR_K6_EPMR			0xC0000086
#define MSR_K6_PSOR			0xC0000087
#define MSR_K6_PFIR			0xC0000088

#define MSR_K7_EVNTSEL0			0xC0010000
#define MSR_K7_PERFCTR0			0xC0010004
#define MSR_K7_HWCR			0xC0010015
#define MSR_K7_CLK_CTL			0xC001001b
#define MSR_K7_FID_VID_CTL		0xC0010041
#define MSR_K7_VID_STATUS		0xC0010042

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

/* Transmeta defined MSRs */
#define MSR_TMTA_LONGRUN_CTRL		0x80868010
#define MSR_TMTA_LONGRUN_FLAGS		0x80868011
#define MSR_TMTA_LRTI_READOUT		0x80868018
#define MSR_TMTA_LRTI_VOLT_MHZ		0x8086801a

#endif /* __ASM_MSR_H */
