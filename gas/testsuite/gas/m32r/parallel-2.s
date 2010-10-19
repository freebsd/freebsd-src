	.text
test:
        add r4,r5
        st r4,@(r6)
        addi r6,#4
        .debugsym .LM568
        bc.s test
