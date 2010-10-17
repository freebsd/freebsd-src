.text
    lb      %3,%hi(L)
    lb      %3,%hi(L - 7f + ((4f - 5f)<<4))(r2)
2:  lb      %3,%hi(1f)
1:  lbu     %3,%hi((4f - 5f) + 8 - ((5f - 4f)<<4))(r2)
7:
L:
    lh      r3,%hi(5f)[r5]
5:  lhu     v1,('8' - '0')(t7)
    lw	    r1,32767($2)
    lw	    r1,2b
    .word   0x1000
    .long   0x2000
4:
    .asciz  "this is a test"
    .align  4
    ldstbu  %3,%hi((4b - 5b) + 8 - ((5b - 4b)<<4))(r2)
    ldsthu  %3,%hi((4b - 5b) + 8 - ((5b - 4b)<<4))(r2)
    ldstw   %3,%hi((4b - 5b) + 8 - ((5b - 4b)<<4))(r2)
