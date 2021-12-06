/* Public domain. */

#ifndef _ASM_CPUFEATURE_H
#define _ASM_CPUFEATURE_H

#if defined(__amd64__) || defined(__i386__)

#include <sys/types.h>
#include <machine/md_var.h>

#define	X86_FEATURE_CLFLUSH	1
#define	X86_FEATURE_XMM4_1	2
#define	X86_FEATURE_PAT		3
#define	X86_FEATURE_HYPERVISOR	4

static inline bool
static_cpu_has(uint16_t f)
{
	switch (f) {
	case X86_FEATURE_CLFLUSH:
		return ((cpu_feature & CPUID_CLFSH) != 0);
	case X86_FEATURE_XMM4_1:
		return ((cpu_feature2 & CPUID2_SSE41) != 0);
	case X86_FEATURE_PAT:
		return ((cpu_feature & CPUID_PAT) != 0);
	case X86_FEATURE_HYPERVISOR:
		return ((cpu_feature2 & CPUID2_HV) != 0);
	default:
		return (false);
	}
}

#define	boot_cpu_has(x)	static_cpu_has(x)

#endif

#endif
