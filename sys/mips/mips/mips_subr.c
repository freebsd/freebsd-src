#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/cp0.h>

static void
mips_setwatchlo(u_int32_t watchlo)
{

	__asm __volatile ("mtc0 %0, $18, 0" : : "r" (watchlo));
}

static void
mips_setwatchhi(u_int32_t watchhi)
{

	__asm __volatile ("mtc0 %0, $19, 0" : : "r" (watchhi));
}


/*
 * mips_watchpoint -- set/clear a watchpoint
 */
void mips_watchpoint(void *addr, int access);//XXX kludge

void
mips_watchpoint(void *addr, int access)
{
	u_int32_t watchlo = 0;
	u_int32_t watchhi = 0;

	if (addr != NULL) {
		/*
		 * Set a new watchpoint.
		 * Parameter addr points to the address we'd like to monitor.
		 */
		watchhi = WATCHHI_GLOBAL_BIT;
		watchlo = (u_int32_t)addr & WATCHLO_PADDR0_MASK;

		access &= WATCHLO_STORE | WATCHLO_LOAD | WATCHLO_FETCH;

		watchlo |= access;
	}
	mips_setwatchlo(watchlo);
	mips_setwatchhi(watchhi);
}
