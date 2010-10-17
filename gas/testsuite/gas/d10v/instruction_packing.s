	;; Test instruction packing

	.text	
	.global main
main:

MU_IU:	
        nop || nop

FM00_IU_MU:	
        sra r0,r1 || ld  r2,@r3
        sra r0,r1 || bra.s test_end

FM00_MU_IU:		
        ld  r2,@r3 || sra r0,r1
        bra.s test_end || sra r0,r1

FM00_IM_MU:	
        add r4,r5 || ld  r2,@r3
        add r4,r5 || bra.s test_end

FM00_IM_IU:	
        add r4,r5 || sra r0,r1
        add r4,r5 || mulx a0, r6, r7

FM00_MU_IM:	
        ld  r2,@r3 || add r4,r5
        bra.s test_end || add r4,r5

FM00_IU_IM:	
        sra r0,r1 || add r4,r5
        mulx a0, r6, r7 || add r4,r5

FM01_IU_MU:	
        sra r0,r1 -> ld  r2,@r3
        sra r0,r1 -> bra.s test_end

FM01_MU_IU:	
        ld  r2,@r3 -> sra r0,r1
        bra.s test_end -> sra r0,r1

FM01_IM_MU:	
        add r4,r5 -> ld  r2,@r3
        add r4,r5 -> bra.s test_end

FM01_IM_IU:	
        add r4,r5 -> sra r0,r1
        add r4,r5 -> mulx a0, r6, r7

FM01_MU_IM:	
        ld  r2,@r3 -> add r4,r5
        bra.s test_end -> add r4,r5

FM01_IU_IM:	
        sra r0,r1 -> add r4,r5
        mulx a0, r6, r7 -> add r4,r5

FM10_IU_MU:	
        sra r0,r1 <- ld  r2,@r3
        sra r0,r1 <- bra.s test_end

FM10_MU_IU:	
        ld  r2,@r3 <- sra r0,r1
        bra.s test_end <- sra r0,r1

FM10_IM_MU:	
        add r4,r5 <- ld  r2,@r3
        add r4,r5 <- bra.s test_end

FM10_IM_IU:	
        add r4,r5 <- sra r0,r1
        add r4,r5 <- mulx a0, r6, r7

FM10_MU_IM:	
        ld  r2,@r3 <- add r4,r5
        bra.s test_end <- add r4,r5

FM10_IU_IM:	
        sra r0,r1 <- add r4,r5
        mulx a0, r6, r7 <- add r4,r5
test_end:

	jmp r13	
