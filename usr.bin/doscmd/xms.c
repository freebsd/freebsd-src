/*
** No copyright?!
**
** $Id: xms.c,v 1.3 1996/09/22 15:43:01 miff Exp $
*/
#include "doscmd.h"

u_long xms_vector;

int
int2f_43(regcontext_t *REGS)
{               

    switch (R_AL) {
    case 0x00:			/* installation check */
	R_AL = 0x80;
	break;

    case 0x10:			/* get handler address */
	N_PUTVEC(R_ES, R_BX, xms_vector);
	break;

    default:
	return (0);
    }
    return (1);
}

/*
** XXX DANGER WILL ROBINSON!
*/
static void
xms_entry(regcontext_t *REGS)
{
    switch (R_AH) {
    case 0x00:			/* get version number */
	R_AX = 0x0300;		/* 3.0 */
	R_BX = 0x0001;		/* internal revision 0.1 */
	R_DX = 0x0001;		/* HMA exists */
	break;

    default:
	debug(D_ALWAYS, "XMS %02x\n", R_AH);
	R_AX = 0;
	break;
    }
}

static u_char xms_trampoline[] = {
    0xeb,	/* JMP 5 */
    0x03,
    0x90,	/* NOP */
    0x90,	/* NOP */
    0x90,	/* NOP */
    0xf4,	/* HLT */
    0xcb,	/* RETF */
};

void
xms_init(void)
{
    xms_vector = insert_generic_trampoline(
	sizeof(xms_trampoline), xms_trampoline);
    register_callback(xms_vector + 5, xms_entry, "xms");
}
