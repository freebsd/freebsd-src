	;;
	;; -gstabs --no-gstabs-packing inserts nop's so gdb will have the
	;; correct line number information.
	;; -gstabs and -gstabs --gstabs-packing disable inserting nops.

	.text
	.global foo
foo:	
	ldi.l   r0,     #0x0000
	ldi.l   r1,     #0x1000

	ldi.s   r2,     #0x0002
	ldi.s   r3,     #0x0003

	ldi.l   r4,     #0x4000
	ldi.s   r5,     #0x0005

	jmp     r13
