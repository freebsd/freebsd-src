
#include <machine/asm.h>
#include <machine/cache_r4k.h>
#include <machine/cpuregs.h>
#include <machine/param.h>
#include <machine/pte.h>

#include "assym.s"
	


#define CPU_DISABLE_INTERRUPTS(reg, reg2, reg3) \
        mfc0    reg, MIPS_COP_0_STATUS; \
        nop; \
        move    reg3, reg; \
        li      reg2, ~MIPS_SR_INT_IE; \
        and     reg, reg2, reg; \
        mtc0    reg, MIPS_COP_0_STATUS; \
        COP0_SYNC



#define CPU_ENABLE_INTERRUPTS(reg, reg3) \
        mfc0    reg, MIPS_COP_0_STATUS; \
        nop; \
        or      reg, reg, reg3; \
        mtc0    reg, MIPS_COP_0_STATUS; \
        COP0_SYNC


#define PUSHR(reg) \
        addiu   sp,sp,-16        ; \
        sd      reg, 8(sp)      ; \
        nop                     ; 

#define POPR(reg) \
        ld      reg, 8(sp)      ; \
        addiu   sp,sp,16        ; \
        nop                     ; 



                
/*
 * octeon_ciu_get_interrupt_reg_addr
 *
 * Given  Int-X, En-X combination, return the CIU Interrupt Enable Register addr
 * a0 = ciu Int-X:  0/1
 * a1 = ciu EN-0:   0/1
 */
LEAF(octeon_ciu_get_interrupt_reg_addr)
        .set    noreorder
        .set    mips3

        beqz    a0, ciu_get_interrupt_reg_addr_Int_0
        nop

ciu_get_interrupt_reg_addr_Int_1:
        beqz    a1, ciu_get_interrupt_reg_addr_Int_1_En_0
        nop

ciu_get_interrupt_reg_addr_Int_1_En1:
        li      a0, OCTEON_CIU_ADDR_HI
        dsll32  a0, a0, 0
        nop
        ori      a0, OCTEON_CIU_EN1_INT1_LO
        j       ciu_get_interrupt_reg_addr_ret
        nop

ciu_get_interrupt_reg_addr_Int_1_En_0:
        li      a0, OCTEON_CIU_ADDR_HI
        dsll32  a0, a0, 0
        nop
        ori     a0, OCTEON_CIU_EN0_INT1_LO
        j       ciu_get_interrupt_reg_addr_ret
        nop

ciu_get_interrupt_reg_addr_Int_0:
        beqz    a1, ciu_get_interrupt_reg_addr_Int_0_En_0
        nop

ciu_get_interrupt_reg_addr_Int_0_En_1:
        li      a0, OCTEON_CIU_ADDR_HI
        dsll32  a0, a0, 0
        nop
        ori     a0, OCTEON_CIU_EN1_INT0_LO
        j       ciu_get_interrupt_reg_addr_ret
        nop

ciu_get_interrupt_reg_addr_Int_0_En_0:
        li      a0, OCTEON_CIU_ADDR_HI
        dsll32  a0, a0, 0
        nop
        ori     a0, OCTEON_CIU_EN0_INT0_LO
                
        
ciu_get_interrupt_reg_addr_ret: 
        j       ra
        nop
        
        .set	mips0
        .set    reorder
END(octeon_ciu_get_interrupt_reg_addr)


                
/*
 * octeon_ciu_mask_all_interrupts
 *
 * a0 = ciu Interrupt-X:  0/1
 * a1 = ciu Enable-X:   0/1
 */
LEAF(octeon_ciu_mask_all_interrupts)
	.set    noreorder
        .set    mips3

        PUSHR(ra)
        PUSHR(s0)
        
        move    t0, a0
        move    t1, a1
        li      a0, MIPS_SR_INT_IE
        CPU_DISABLE_INTERRUPTS(a2, a1, s0)
        move    a0, t0
        move    t1, a1
        jal     octeon_ciu_get_interrupt_reg_addr
        nop
        ld      a2, 0(a0)       # Dummy read
        nop
        move    a2, zero        # Clear all
        sd      a2, 0(a0)       # Write new Enable bits
        nop
        CPU_ENABLE_INTERRUPTS(a2, s0)

        POPR(s0)
        POPR(ra)
	j	ra			# Return
	nop				# (bd slot)

        .set	mips0
	.set	reorder
END(octeon_ciu_mask_all_interrupts)

