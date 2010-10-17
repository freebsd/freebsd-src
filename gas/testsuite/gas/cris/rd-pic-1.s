; Check that PIC operands get their right relocation type.
; First some expected uses, similar to what GCC will emit.
	.text
	.syntax no_register_prefix
start:
	sub.d .:GOTOFF,r1
	move.d [r3+extsym:GOT],r10
	add.d extsym2:GOTOFF,r9
	move.d extsym5:PLT,r8
	move.d extsym9:PLTG,r8
	move.d [r3+extsym:GOTPLT],r10
	move.d [r13+extsym13:GOT16],r10
	move.w extsym14:GOTPLT16,r10

; Other for GAS valid operands (some with questionable PIC semantics).
	sub.d [r3+extsym3:GOT],r4,r10
	sub.d extsym4:GOTOFF+42,r9
	sub.d extsym4:GOTOFF-96,r3
	add.d [r10+extsym3:GOT+56],r7,r8
	move.d [r5+extsym6:GOT+10],r1
	add.d [r10+extsym3:GOT-560],r4,r8
	move.d [r5+extsym6:GOT-110],r12
	move.d [r9=r5+extsym6:GOT-220],r12
	move.d [r7=r3+extsym10:GOTOFF-330],r13
	move.d extsym7:PLT+4,r5
	move.d extsym7:PLT-40,r9
	move.d extsym11:PLTG+16,r5
	move.d extsym12:PLTG-60,r9
	sub.d [r12+extsym3:GOT16-156],r9,r8
	move.d [r11+extsym14:GOTPLT16-256],r9
	add.d [r10+extsym3:GOTPLT+56],r7,r8
