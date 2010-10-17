;
; There are actually two regressions tested here:
; - That a .byte does not overwrite beyond the "byte", if given
;   a non-immediate-constant value (related to "False broken words").
; - That "quick-operands" (in this case, just the unsigned 6-bit
;   one is tested) can take "difference-expressions".
;
 .text
 .syntax no_register_prefix
start:
 .dword 0xf0+b-a-0xc5
 .byte 0xf0+b-a-0xc7
 .byte 0xab
 move.b 0xf0+b-a-0xca,r8
 move.w 0xf0+b-a-0xcb,r8
 subq 0xf0+b-a-0xcf,r3
 .ascii "Hello, world\n\0"
 .space 260,0
a:
 .dword 0
b:
 .dword 1
