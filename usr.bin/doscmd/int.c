/*
** No copyright?!
**
** $FreeBSD$
*/

/*
 * Notes:
 *   1) Second PIC is not implemented.
 *   2) Interrupt priority management is not implemented.
 *   3) What should be read from port 0x20?
 *
 * "within interrupt processing" means the following is true:
 *   1) Hardware interrupt <irql> is delivered by hardint().
 *   2) Next interrupt <irql> is not possible yet by either:
 *	a) V_IF;
 *	b) Interrupt mask;
 *	c) Current irql.
 *
 * Related functions:
 *   int isinhardint(int irql)
 *   void set_eoir(int irql, void (*eoir)(void *), void *arg);
 *
 */

#include "doscmd.h"

struct IRQ {
    int pending;
    int busy;
    int within;
    void (*eoir)(void *arg);
    void *arg;
};

static unsigned char IM;
static int Irql;
static struct IRQ Irqs[8];

#define int_allowed(n) ((IM & 1 << (n)) == 0 && Irql > (n))

void
set_eoir(int irql, void (*eoir)(void *), void *arg)
{
    Irqs [irql].eoir = eoir;
    Irqs [irql].arg = arg;
}

int
isinhardint(int irql)
{
    return Irqs[irql].within;
}

static void
set_vip(void)
{
    regcontext_t *REGS = saved_regcontext;
    int irql;
    
    if (R_EFLAGS & PSL_VIF) {
	R_EFLAGS &= ~PSL_VIP;
	return;
    }
    
    for (irql = 0; irql < 8; irql++)
	if (int_allowed(irql) && (Irqs[irql].within || Irqs[irql].pending)) {
	    R_EFLAGS |= PSL_VIP;
	    return;
	}
    
    R_EFLAGS &= ~PSL_VIP;
}

void
resume_interrupt(void)
{
    regcontext_t      *REGS = saved_regcontext;
    int irql;
    
    if (R_EFLAGS & PSL_VIF) {
	for (irql = 0; irql < 8; irql++)
	    if (Irqs[irql].within && int_allowed(irql)) {
		Irqs[irql].within = 0;
		if (Irqs[irql].eoir)
		    Irqs[irql].eoir(Irqs[irql].arg);
	    }
	
	for (irql = 0; irql < 8; irql++)
	    if (Irqs[irql].pending && int_allowed(irql)) {
		Irqs[irql].pending = 0;
		hardint(irql);
		break;
	    }
    }
    set_vip();
}

void
send_eoi(void)
{
    if (Irql >= 8)
	return;
    
    Irqs[Irql].busy = 0;
    
    while (++Irql < 8)
	if (Irqs [Irql].busy)
	    break;
    
    resume_interrupt();
}

/*
** Cause a hardware interrupt to happen immediately after
** we return to vm86 mode
*/
void
hardint(int irql)
{
    regcontext_t	*REGS = saved_regcontext;
    u_long vec = ivec[8 + irql];

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
    
    if (!int_allowed(irql)) {
	Irqs[irql].pending = 1;
	return;
    }
    
    if ((R_EFLAGS & PSL_VIF) == 0) {
	Irqs[irql].pending = 1;
	R_EFLAGS |= PSL_VIP;
	return;
    }
    
    debug(D_TRAPS | (8 + irql), "Int%02x [%04lx:%04lx]\n",
	  8 + irql, vec >> 16, vec & 0xffff);
    
    Irql = irql;
    Irqs[Irql].busy = 1;
    if (Irqs[Irql].eoir)
	Irqs[Irql].within = 1;
    
    PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
    PUSH(R_CS, REGS);
    PUSH(R_IP, REGS);
    R_EFLAGS &= ~PSL_VIF;		/* XXX disable interrupts */
    PUTVEC(R_CS, R_IP, vec);
}

void
unpend(int irql)
{
    if (!Irqs[irql].pending)
	return;
    Irqs[irql].pending = 0;
    set_vip();
}

static unsigned char
irqc_in(int port)
{
    return 0x60; /* What should be here? */
}
 
static void
irqc_out(int port, unsigned char val)
{
    if (val == 0x20)
	send_eoi();
}

static unsigned char
imr_in(int port)
{
    return IM;
}
 
static void
imr_out(int port, unsigned char val)
{
    IM = val;
    resume_interrupt();
}
 
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

    debug(D_TRAPS | intnum, "INT %02x [%04lx:%04lx]\n", 
	  intnum, vec >> 16, vec & 0xffff);

    PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
    PUSH(R_CS, REGS);
    PUSH(R_IP, REGS);
    R_EFLAGS &= ~PSL_VIF;		/* XXX disable interrupts? */
    PUTVEC(R_CS, R_IP, vec);
}

void
init_ints(void)
{
    int i;
    
    for (i = 0; i < 8; i++) {
	Irqs[i].busy = 0;
	Irqs[i].pending = 0;
	Irqs[i].within = 0;
    }
    
    IM = 0x00;
    Irql = 8;
    
    define_input_port_handler(0x20, irqc_in);
    define_output_port_handler(0x20, irqc_out);
    define_input_port_handler(0x21, imr_in);
    define_output_port_handler(0x21, imr_out);
}
