/*
** No copyright ?!
**
** $FreeBSD$
*/
#include "doscmd.h"

/*
** Hardware /0 interrupt
*/
void
int00(regcontext_t *REGS)
{
    debug(D_ALWAYS, "Divide by 0 in DOS program!\n");
    exit(1);
}

void
int01(regcontext_t *REGS)
{
    debug(D_ALWAYS, "INT 1 with no handler! (single-step/debug)\n");
}

void
int03(regcontext_t *REGS)
{
    debug(D_ALWAYS, "INT 3 with no handler! (breakpoint)\n");
}

void
int0d(regcontext_t *REGS)
{
    debug(D_ALWAYS, "IRQ5 with no handler!\n");
}

void
cpu_init(void)
{
    u_long vec;

    vec = insert_hardint_trampoline();
    ivec[0x00] = vec;
    register_callback(vec, int00, "int 00");

    vec = insert_softint_trampoline();
    ivec[0x01] = vec;
    register_callback(vec, int01, "int 01");

    vec = insert_softint_trampoline();
    ivec[0x03] = vec;
    register_callback(vec, int03, "int 03");

    vec = insert_hardint_trampoline();
    ivec[0x0d] = vec;
    register_callback(vec, int0d, "int 0d");

    vec = insert_null_trampoline();
    ivec[0x34] = vec;	/* floating point emulator */
    ivec[0x35] = vec;	/* floating point emulator */
    ivec[0x36] = vec;	/* floating point emulator */
    ivec[0x37] = vec;	/* floating point emulator */
    ivec[0x38] = vec;	/* floating point emulator */
    ivec[0x39] = vec;	/* floating point emulator */
    ivec[0x3a] = vec;	/* floating point emulator */
    ivec[0x3b] = vec;	/* floating point emulator */
    ivec[0x3c] = vec;	/* floating point emulator */
    ivec[0x3d] = vec;	/* floating point emulator */
    ivec[0x3e] = vec;	/* floating point emulator */
    ivec[0x3f] = vec;	/* floating point emulator */
}
