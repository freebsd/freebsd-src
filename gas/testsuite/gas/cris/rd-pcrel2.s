; Test border-cases for relaxation of pc-relative expressions.

 .macro relaxcode
 .endm

 .text
 .syntax no_register_prefix
; Region of relaxation is after insn, same segment 
z:
 move.d [pc-(x1-x+128-4)],r8 ; 4
 move.d [pc-(x2-x+129-8)],r8 ; 6
 move.d [pc+x3-x+127-14],r8 ; 4
 move.d [pc+x4-x+128-18],r8 ; 6
 move.d [pc-(x5-x+32768-26)],r8 ; 6
 move.d [pc-(x6-x+32769-32)],r8 ; 8
 move.d [pc+x7-x+32767-40],r8 ; 6
 move.d [pc+x8-x+32768-46],r8 ; 8

 .p2align 1

; Region of relaxation is around insn, same segment 
x:
 move.d [pc-(x1-x+128-4)],r8 ; 4
x1:
 move.d [pc-(x2-x+129-8)],r8 ; 6
x2:
 move.d [pc+x3-x+127-14],r8 ; 4
x3:
 move.d [pc+x4-x+128-18],r8 ; 6
x4:
 move.d [pc-(x5-x+32768-26)],r8 ; 6
x5:
 move.d [pc-(x6-x+32769-32)],r8 ; 8
x6:
 move.d [pc+x7-x+32767-40],r8 ; 6
x7:
 move.d [pc+x8-x+32768-46],r8 ; 8
x8:

; Region of relaxation is before insn, same segment.
 move.d [pc-(x1-x+128-4)],r8 ; 4
 move.d [pc-(x2-x+129-8)],r8 ; 6
 move.d [pc+x3-x+127-14],r8 ; 4
 move.d [pc+x4-x+128-18],r8 ; 6
 move.d [pc-(x5-x+32768-26)],r8 ; 6
 move.d [pc-(x6-x+32769-32)],r8 ; 8
 move.d [pc+x7-x+32767-40],r8 ; 6
 move.d [pc+x8-x+32768-46],r8 ; 8

; Region of relaxation is in other segment.
 .section .text.other
y:
 move.d [pc-(x1-x+128-4)],r8 ; 4
 move.d [pc-(x2-x+129-8)],r8 ; 6
 move.d [pc+x3-x+127-14],r8 ; 4
 move.d [pc+x4-x+128-18],r8 ; 6
 move.d [pc-(x5-x+32768-26)],r8 ; 6
 move.d [pc-(x6-x+32769-32)],r8 ; 8
 move.d [pc+x7-x+32767-40],r8 ; 6
 move.d [pc+x8-x+32768-46],r8 ; 8
