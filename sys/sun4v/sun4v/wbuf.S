#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/wbuf.S,v 1.6 2007/01/19 11:15:34 marius Exp $")

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pstate.h>

#include "assym.s"

ENTRY(fault_32bit_sn0)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
END(fault_32bit_sn0)

ENTRY(fault_32bit_sn1)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
END(fault_32bit_sn1)

ENTRY(fault_32bit_so0)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
END(fault_32bit_so0)

ENTRY(fault_32bit_so1)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
END(fault_32bit_so1)

ENTRY(fault_64bit_sn0)
	GET_PCB(%g4)
        stx	%sp, [%g4 + PCB_RWSP]
        add     %g4, PCB_RW, %g5
        SAVE_WINDOW(%g5)
	mov	1, %g5
	stx	%g5, [%g4 + PCB_NSAVED]

	set	trap, %g1
	sub	%g0, 1, %g4

	rdpr	%tstate, %g5
	and	%g5, TSTATE_CWP_MASK, %g5
	ba,pt	%xcc, tl0_skip_utrap
	  wrpr	%g0, %g5, %cwp	
END(fault_64bit_sn0)

ENTRY(fault_64bit_sn1)
	/* XXX need to use physical addresses here */
	GET_PCB_PHYS(%g5, %g6)
        wr      %g0, ASI_REAL, %asi
        stxa    %sp, [%g6 + PCB_RWSP]%asi
        add     %g6, PCB_RW, %g5
        SAVE_WINDOW_ASI(%g5)
        mov     1, %g5
        stxa    %g5, [%g6 + PCB_NSAVED]%asi
	saved
	set	tl0_trap, %g5
	wrpr	%g5, %tnpc
	done
END(fault_64bit_sn1)

ENTRY(fault_32bit_sk)
	MAGIC_TRAP_ON
	MAGIC_EXIT	
END(fault_32bit_sk)

ENTRY(fault_64bit_sk)
	GET_PCPU_PHYS_SCRATCH(%g5)
	wr	%g0, ASI_REAL, %asi
	stxa	%sp, [PCPU_REG + PC_KWBUF_SP]%asi
	add	PCPU_REG, PC_KWBUF, %g6
	SAVE_WINDOW_ASI(%g6)
	mov	1, %g6
	sta	%g6, [PCPU_REG + PC_KWBUF_FULL]%asi
	saved
	retry
END(fault_64bit_sk)

	
ENTRY(fault_64bit_so0)
	GET_PCB(%g6)
	ldx	[%g6 + PCB_NSAVED], %g2
	add	%g2, 1, %g3
	stx	%g3, [%g6 + PCB_NSAVED]

	sll	%g2, PTR_SHIFT, %g4
	add	%g6, PCB_RWSP, %g3
	stx	%sp, [%g3 + %g4]
	sll	%g2, RW_SHIFT, %g4
	add	%g4, %g6, %g4
	add	%g4, PCB_RW, %g3
	SAVE_WINDOW(%g3)
	saved
	retry
END(fault_64bit_so0)

ENTRY(fault_64bit_so1)
	GET_PCB_PHYS(%g5, %g6)
	wr	%g0, ASI_REAL, %asi
	ldxa	[%g6 + PCB_NSAVED]%asi, %g5
	add	%g5, 1, %g7
	stxa	%g7, [%g6 + PCB_NSAVED]%asi

	sll	%g5, PTR_SHIFT, %g7
	add	%g6, %g7, %g7
	stxa	%sp, [%g7 + PCB_RWSP]%asi

	sll	%g5, RW_SHIFT, %g7		! offset
	add	%g6, %g7, %g7			! pcb + offset
	add	%g7, PCB_RW, %g7		! offset into wbuf area

	SAVE_WINDOW_ASI(%g7)
	saved
	set	tl0_trap, %g5
	wrpr	%g5, %tnpc
	done
END(fault_64bit_so1)

ENTRY(fault_32bit_fn0)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
fault_fn0_common:
	set	trap, %g1
	sub	%g0, 1, %g4
	rdpr	%tstate, %g5
	and	%g5, TSTATE_CWP_MASK, %g5
	GET_PCPU_SCRATCH
	ba,pt	%xcc, tl0_skip_utrap
	  wrpr	%g0, %g5, %cwp	
END(fault_32bit_fn0)
	MAGIC_TRAP_ON	
	MAGIC_EXIT	
ENTRY(fault_32bit_fn1)
	MAGIC_TRAP_ON	
	MAGIC_EXIT
fault_fn1_common:
	rdpr	%tstate, %g1
	and	%g1, TSTATE_CWP_MASK, %g1
	wrpr	%g0, %g1, %cwp
	!
	! fake tl1 traps regs so that after pagefault runs, we
	! re-execute at user_rtt.
	!
	wrpr	%g0, 1, %tl
	set	TSTATE_KERNEL | TSTATE_IE, %g1
	wrpr	%g0, %g1, %tstate
	set	user_rtt, %g1
	wrpr	%g0, %g1, %tpc
	add	%g1, 4, %g1
	wrpr	%g0, %g1, %tnpc

	set	trap, %g1
	mov	1, %g5
	sllx	%g5, CTX_OTHER_SHIFT, %g5
	or	%g5, %g2, %g2	

	sub	%g0, 1, %g4
	rdpr	%wstate, %l1
	sllx	%l1, WSTATE_SHIFT, %l1
	wrpr	%l1, WSTATE_K64, %wstate
	mov	KCONTEXT, %g5
	mov	MMU_CID_P, %g6
	SET_MMU_CONTEXT(%g6, %g5)


	ba,pt	%xcc, tl0_ktrap 
	  membar	#Sync
END(fault_32bit_fn1)
	
ENTRY(fault_64bit_fn0)
	b	fault_fn0_common
	  nop
END(fault_64bit_fn0)

ENTRY(fault_64bit_fn1)
	wrpr	%g0, 1, %gl
	b	fault_fn1_common
	  nop
END(fault_64bit_fn1)

ENTRY(fault_rtt_fn1)
	b	fault_fn1_common
	  nop
END(fault_rtt_fn1)


ENTRY(fault_32bit_not)
ENTRY(fault_64bit_not)

	ba,pt	%xcc, ptl1_panic
	  mov	PTL1_BAD_WTRAP, %g1
	
END(fault_32bit_not)
END(fault_64bit_not)

ENTRY(ptl1_panic)
	GET_PCB_PHYS(%g5, %g6)
#ifdef TRAP_TRACING
	wr	%g0, ASI_REAL, %asi
	/* pcpu->pad[0] = %tpc */
	rdpr	%tpc, %g4
	stxa	%g4, [PCPU(PAD)]%asi

	/* pcpu->pad[1] = %l7 */
	stxa    %l7, [PCPU(PAD) + 8]%asi
	/* pcpu->pad[2] = pcb->pcb_kstack */
	ldxa	[%g6 + PCB_KSTACK]%asi, %g6
	stxa	%g6, [PCPU(PAD) + 16]%asi
	rdpr	%tt, %g6
	stxa	%g6, [PCPU(PAD) + 24]%asi
	wrpr	%g0, 1, %tl
	rdpr	%tt, %g6
	stxa	%g6, [PCPU(PAD) + 32]%asi
#endif


2:	nop
	ba,a,pt	%xcc, 2b
		
END(ptl1_panic)
