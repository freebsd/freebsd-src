.text
1:  add	    $3,$1,$2
    add	    %3,%1,%2
    addu    r3,r1,r2
    sub	    r4,r2,r3
    subu    r4,r2,r3
    mult    a1,a2,a3
    multu   t4,t2,t3
    div	    t7,t5,t6
    divu    s0,s1,s2
    and     s3,s4,s5
    or      s6,s7,zero
    xor     t7,t8,t9
    sll     k0,k1,zero
    sra     gp,sp,fp
    srl     t7,t5,ra

    seq     at,v0,v1
    sne     a0,ra,zero
    slt     t0,t1,t2
    sgt     $7,%5,r6
    sle     r7,$5,%6
    sge     r7,$5,%6

    seq     at,v0,v1
    sne     a0,ra,zero
    slt     t0,t1,t2
    sgt     $7,%5,r6
    sle     r7,$5,%6
    sge     r7,$5,%6

    mvts    $10,r5
    mvfs    r10,$5
