/* Public domain. */

#ifndef _LINUXKPI_ASM_MEMTYPE_H_
#define _LINUXKPI_ASM_MEMTYPE_H_

#if defined(__amd64__) || defined(__i386__)

#include <asm/cpufeature.h>

static inline bool
pat_enabled(void)
{
	return (boot_cpu_has(X86_FEATURE_PAT));
}

#endif

#endif /* _LINUXKPI_ASM_MEMTYPE_H_ */
