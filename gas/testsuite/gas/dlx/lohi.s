    .text
    .align  2
    nop
.L1:
    lhi     r1,%hi(.L1 + 200000)
    ori     r1,r0,%lo(.L1 + 200000)
    lhi     r1,%hi(.L1 + 200000000)
    ori     r1,r0,%lo(.L1 + 200000000)
    .end
