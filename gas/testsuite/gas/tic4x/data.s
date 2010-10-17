         .text
BYTE:    .byte   10,-1,"abc",'a'
HWORD:   .hword  10,-1,"abc",'a'
INT:     .int    10,IEEE,-1,"abc",'a'
LONG:    .long   0FFFFABCDH,'A'+100h
WORD:    .word   3200,1+'A',-'A',0F410h,'A'
STRING:  .string "ABCD", 51h, 52h, 53h, 54h, "Houston", 36+12
ASCII:   .ascii  "This is a very long text","This is another"
ASCIZ:   .asciz  "This is a very long text","This is another"
BLOCK:   .block  4
SPACE:   .space  4
ALIGN:   ldi     r0,r0
         .align
         ldi     r0,r0
