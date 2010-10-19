! Check that we get errors when assembling DSP instructions.

! { dg-do assemble }
! { dg-options "-isa=SHcompact" }

! Regarding the opcode table, all insns are marked arch_sh_dsp_up; there are
! no insns marked arch_sh3_dsp_up.  We check a few marked arch_sh_dsp_up:
! two have operands only recognized with -dsp; the other has an opcode not
! recognized without -dsp.

	.text
start:
	ldc r3,mod		! { dg-error "invalid operands" }
	ldre @(16,pc)		! { dg-error "opcode not valid for this cpu variant" }
	lds r4,a0		! { dg-error "invalid operands" }
