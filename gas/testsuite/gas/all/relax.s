 .data
 .byte 1, 2, 3
 .uleb128 L2 - L1
L1:
 .space 128 - 2
 .byte 4
 .p2align 1, 0xff
L2:
 .byte 5

 .p2align 2
 .byte 6, 7
 .uleb128 L4 - L3
L3:
 .space 128*128 - 2
 .byte 8
 .p2align 2, 0xff
L4:
 .byte 9
 .p2align 4, 9
