/* Public domain. */

#ifndef _LINUXKPI_ASM_HYPERVISOR_H
#define _LINUXKPI_ASM_HYPERVISOR_H

#if defined(__i386__) || defined(__amd64__)

#define X86_HYPER_NATIVE	1
#define X86_HYPER_MS_HYPERV	2

static inline bool
hypervisor_is_type(int type)
{
	return (type == X86_HYPER_NATIVE);
}

#endif

#endif
