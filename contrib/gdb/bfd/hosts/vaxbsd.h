#define	NO_CORE_COMMAND		/* No command name in core file */

#if 0
#undef	ALIGN			/* They use it, we use it too */
/* Does not exist on BSD 4.3, it uses machine/machparam.h.
   Whatever it is, it's included by <sys/param.h>, which trad-core.c,
   the only place that uses this (I think), already includes.  */
#include <machine/param.h>
#endif
#undef	ALIGN			/* They use it, we use it too */

/* Note that HOST_PAGE_SIZE -- the page size as far as executable files
   are concerned -- is not the same as NBPG, because of page clustering.  */
#define	HOST_PAGE_SIZE		1024
#define	HOST_MACHINE_ARCH	bfd_arch_vax

#define	HOST_TEXT_START_ADDR	0
#define	HOST_STACK_END_ADDR	(0x80000000 - (UPAGES * NBPG))
#undef	HOST_BIG_ENDIAN_P
