/*
** No copyright?!
**
** $Id: int.c,v 1.4 1997/03/18 02:36:56 msmith Exp $
*/
#include "doscmd.h"

/*
** Cause a software interrupt to happen immediately after we
** return to vm86 mode
*/
void
softint(int intnum)
{
    regcontext_t	*REGS = saved_regcontext;
    u_long vec = ivec[intnum];

    /*
    ** if we're dead, or there's no vector or the saved registers are
    ** invalid
    */
    if (dead || !saved_valid || vec == 0)
	return;

    /* 
    ** if the vector points into the BIOS, or the handler at the other
    ** end is just an IRET, don't bother.
    */
    if ((vec >> 16) == 0xf000 || *(u_char *)VECPTR(vec) == 0xcf)
	return;

#if 0
    /*
     * software interrupts are always taken
     */
    if ((R_EFLAGS & PSL_VIF) == 0) {
        delay_interrupt(intnum, softint);
        return;
    }
#endif

    debug(D_TRAPS|intnum, "Int%x [%04x:%04x]\n", 
	  intnum, vec >> 16, vec & 0xffff);

    N_PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
    N_PUSH(R_CS, REGS);
    N_PUSH(R_IP, REGS);
#if 1
    R_EFLAGS &= ~PSL_VIF;		/* XXX disable interrupts? */
#else
    R_EFLAGS |= PSL_VIF;
#endif
    N_PUTVEC(R_CS, R_IP, vec);
}

/*
** Cause a hardware interrupt to happen immediately after
** we return to vm86 mode
*/
void
hardint(int intnum)
{
    regcontext_t	*REGS = saved_regcontext;
    u_long vec = ivec[intnum];

    /*
     * XXXXX
     * We should simulate the IRQ mask in the PIC.
     */
	
	/* 
	** if we're dead, or there's no vector, or the saved registers
	** are invalid
	*/
	if (dead || !saved_valid || vec == 0)
	    return;
	
	/* 
	** if the vector points into the BIOS, or the handler at the
	** other end is just an IRET, don't bother 
	*/
	if ((vec >> 16) == 0xf000 || *(u_char *)VECPTR(vec) == 0xcf)
	    return;

     if ((R_EFLAGS & PSL_VIF) == 0) {
         delay_interrupt(intnum, hardint);
         return;
     }
	
	debug(D_TRAPS|intnum, "Int%x [%04x:%04x]\n",
	      intnum, vec >> 16, vec & 0xffff);

        N_PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
	N_PUSH(R_CS, REGS);
	N_PUSH(R_IP, REGS);
#if 1
	R_EFLAGS &= ~PSL_VIF;		/* XXX disable interrupts */
#else
        R_EFLAGS |= PSL_VIF;
#endif
	N_PUTVEC(R_CS, R_IP, vec);
}

typedef void (*foo_t)(int);

void
resume_interrupt(void)
{
    int i;
    regcontext_t      *REGS = saved_regcontext;

    n_pending--;
    if (n_pending == 0)
        R_EFLAGS &= ~PSL_VIP;
    
    for (i = 0; i < 256; i++) {
        if (pending[i]) {
            ((foo_t)(pending[i]))(i);
            pending[i] = 0;
            break;    
        }
    }
}   
     
     
void 
delay_interrupt(int intnum, void (*func)(int))
{       
    regcontext_t      *REGS = saved_regcontext;

#if 0
printf("DELAY [%x/%d]\n", intnum, n_pending);
#endif
    if (pending[intnum] == 0) {
        pending[intnum] = (u_long)func;
        n_pending++;
    }
    R_EFLAGS |= PSL_VIP;
}   
