/*
** No copyright!
**
** NetBIOS etc. hooks.
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "doscmd.h"

static void
int2a(regcontext_t *REGS)
{
    unknown_int2(0x2a, R_AH, REGS);
}

static void
int5c(regcontext_t *REGS)
{
    unknown_int2(0x5c, R_AH, REGS);
}

void
net_init(void)
{
    u_long vec;

    vec = insert_softint_trampoline();
    ivec[0x2a] = vec;
    register_callback(vec, int2a, "int 2a");

    vec = insert_softint_trampoline();
    ivec[0x5c] = vec;
    register_callback(vec, int5c, "int 5c");
}
