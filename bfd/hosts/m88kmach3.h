#include <machine/vmparam.h>
#include <sys/param.h>

#undef UPAGES
#define UPAGES 3

#define	HOST_PAGE_SIZE		NBPG
#define	HOST_SEGMENT_SIZE	NBPG
#define	HOST_MACHINE_ARCH	bfd_arch_m88k
#define	HOST_TEXT_START_ADDR	USRTEXT
#define	HOST_STACK_END_ADDR	USRSTACK
