/*------------------------------------------------------------------
 * octeon_fau.c        Fetch & Add Block
 *
 *------------------------------------------------------------------
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <mips/octeon1/octeon_pcmap_regs.h>
#include "octeon_fau.h"

/*
 * oct_fau_init
 *
 * How do we initialize FAU unit. I don't even think we can reset it.
 */
void octeon_fau_init (void)
{
}


/*
 * oct_fau_enable
 *
 * Let the Fetch/Add unit roll
 */
void octeon_fau_enable (void)
{
}


/*
 * oct_fau_disable
 *
 * disable fau
 *
 * Don't know if we can even do that.
 */
void octeon_fau_disable (void)
{
}
