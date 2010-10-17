/* Sony News running NewsOS 3.2.  */

#include <machine/vmparam.h>

#define HOST_PAGE_SIZE NBPG
#define HOST_SEGMENT_SIZE NBPG
#define HOST_MACHINE_ARCH bfd_arch_m68k
#define HOST_TEXT_START_ADDR 0
#define HOST_STACK_END_ADDR (KERNBASE - (UPAGES * NBPG))
