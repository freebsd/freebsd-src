# bittest.s
#
# Bit operation instructions (BCLR, BNOT, BSET, BTST) should not be placed in IU.
# If the user specifically indicates they should be in the IU, GAS will
# generate warnings. The reason why this is not an error is that those instructions 
# will fail in IU only occasionally. Thus GAS should pack them in MU for
# safety, and it just needs to draw attention when a violation is given. 

	
	nop -> ldw R1, @(R2,R3)
        nop || ldw R6, @(R5,R4)
        
        nop -> BSET R1, R2, R3 
        nop <- BTST F1, R2, R3 
        nop || BCLR R1, R2, R3
        nop -> BNOT R1, R2, R3
        BNOT r1, r2, r3 -> nop
        
        bset r1, r2, r3 || moddec r4, 5

        bset r1, r2, r3
        moddec r4, 5

        bset r1, r2, r3
        joinll r4, r5, r6

        joinll r4, r5, r6
        bset r1, r2, r3
