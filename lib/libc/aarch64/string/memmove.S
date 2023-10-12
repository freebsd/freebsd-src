/*
 * memmove is implemented in memcpy.S. Having this file simplifies the build
 * as it stops MISRCS from picking up the C implementation and rtld expects
 * to copy the memmove object file into its object directory.
 */

#include <machine/asm.h>
#include <sys/elf_common.h>
GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
