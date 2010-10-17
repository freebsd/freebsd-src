.text
L1:
1:  beqz    r4, L4
    nop
    bnez    r5, 1b
L2:
    mov	    r4, L5
    j	    L5
    nop
    jal	    L4
    nop
    break   L4
    nop
    trap    1b
    nop
    jr	    s1
    nop
    jalr    s1
L4:
    lw	    r2, 8+((L5 - L4)<<4)(r2)
    rfe
L5:
    lw	    r2, L1
    call    1b
    nop
    return
    nop
    ret
    nop
    retr    at
    nop
