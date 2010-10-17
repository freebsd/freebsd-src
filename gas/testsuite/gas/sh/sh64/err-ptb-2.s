! Check that PTB to a assembly-time-resolvable SHcompact operand gets an
! error.  Mostly like err-ptb-1.s, except we also specify --no-expand.

! { dg-do assemble }
! { dg-options "--abi=32 --no-expand" }

	.text
	.mode SHmedia
start:
	ptb shmediasymbol1,tr1		! { dg-error "PTB operand is a SHmedia symbol" }
shmediasymbol3:
	ptb shcompactsymbol1,tr1
	pta shcompactsymbol2,tr3	! { dg-error "PTA operand is a SHcompact symbol" }
shmediasymbol1:
	ptb shmediasymbol2,tr2		! { dg-error "PTB operand is a SHmedia symbol" }

	.mode SHcompact
shcompact:
	nop
	nop
shcompactsymbol2:
	nop
	nop
shcompactsymbol1:
	nop
	nop

	.mode SHmedia
shmedia:
	nop
shmediasymbol2:
	nop
	ptb shmediasymbol3,tr3		! { dg-error "PTB operand is a SHmedia symbol" }
	nop
