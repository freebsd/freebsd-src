.text
2:  addi    $3,$1,32767
    addui   r3,r1,-5
    subi    r4,r2,0x30
    subui   r4,r2,%hi(2b)
    andi    a1,a2,2b
    ori     t4,t2,'x'
    xori    t7,t5,%lo(2b)
1:  slli    s0,s1,1b
    srai    s3,s4,15
    srli    s6,s7,0xffff

    seqi    t7,t8,0x7fff
    snei    t7,t8,0x7fff
    slti    t7,t8,0x7fff
    sgti    k0,k1,0
    slei    gp,sp,-23767
    sgei    t7,t5,'0'

    sequi   t7,t8,0x7fff
    sneui   t7,t8,0x7fff
    sltui   t7,t8,0x7fff
    sgtui   k0,k1,0
    sleui   gp,sp,-3
    sgeui   t7,t5,'0'

    mov     at,-32765
    mov     t0,t1
    movu    at,%hi(1b)
    movu    t0,t1
