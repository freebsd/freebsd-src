#include <machine/vmparam.h>
#include <sys/param.h>

/* This is an ugly way to hack around the incorrect
 * definition of UPAGES in i386/machparam.h.
 *
 * The definition should specify the size reserved
 * for "struct user" in core files in PAGES,
 * but instead it gives it in 512-byte core-clicks
 * for i386 and i860. UPAGES is used only in trad-core.c.
 */
#if UPAGES == 16
#undef  UPAGES
#define UPAGES 2
#endif

#if UPAGES != 2
FIXME!! UPAGES is neither 2 nor 16
#endif

#define	HOST_PAGE_SIZE		1
#define	HOST_SEGMENT_SIZE	NBPG
#define	HOST_MACHINE_ARCH	bfd_arch_i386
#define	HOST_TEXT_START_ADDR	USRTEXT
#define	HOST_STACK_END_ADDR	USRSTACK
