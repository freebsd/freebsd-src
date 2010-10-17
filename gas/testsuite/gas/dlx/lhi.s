.text
2:  lhi     $3,32767
    lui     r3,%hi(2b)
    sethi   r4,%lo(2b)
    lui     r4, 2b - 5
    sethi   r4,('9' - '0') + ('3' - '0')
    mov	    r4,%hi(. - 2b)
    mov	    r4,%hi(.)
    ori	    r4,r4,%lo(. - 4)
    mov     r4,r3
