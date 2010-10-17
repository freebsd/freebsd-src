; Test whether parallel insns get inappropriately moved during relaxation.

        .text
label1:
        bnc     label3
        nop
        addi    r3, #3 || addi  r2, #2
label2:
        .space 512
label3:
        nop
