#define	NO_CORE_COMMAND

#undef	ALIGN			/* They use it, we use it too */
#include <machine/param.h>
#undef	ALIGN			/* They use it, we use it too */

#define	HOST_PAGE_SIZE		NBPG
#define	HOST_MACHINE_ARCH	bfd_arch_tahoe

#define	HOST_TEXT_START_ADDR	0
#define	HOST_STACK_END_ADDR	(KERNBASE - (UPAGES * NBPG))
#define	HOST_BIG_ENDIAN_P
