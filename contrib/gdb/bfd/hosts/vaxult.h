#include <machine/param.h>
#include <machine/vmparam.h>
#define	HOST_PAGE_SIZE		(NBPG*CLSIZE)
#define	HOST_MACHINE_ARCH	bfd_arch_vax

#define	HOST_TEXT_START_ADDR	USRTEXT
#define	HOST_STACK_END_ADDR	USRSTACK
#undef	HOST_BIG_ENDIAN_P
