	;; ops with immediate args

	.text
	.global foo
foo:	
        ldi     r0,0x7fff
        ldi     r0,0xffff
        addi    r0,0xf
        bclri   r0,0xf
        bnoti   r0,0xf
        bseti   r0,0xf
        btsti   r0,0xf
        rac     r0,a0,3
	rac	r0,a0,-2
        rachi   r0,a0,3
        rachi   r0,a0,-2
        slli    r0,0xf
        slli    a1,0xf
        srai    r0,0xf
        srai    a0,0x10
        srai    a0,0xf	
        srli    r0,0xf
        srli    a0,0x10
        srli    a0,0xf
        subi    r0,0x10
        subi    r0,0xf
        trap    0xf
	tst0i	r0,0x7fff
	tst0i	r0,0xffff
	tst1i	r0,0x7fff
	tst1i	r0,0xffff

