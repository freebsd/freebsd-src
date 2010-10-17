; Test that pc-relative expressions give expected results for
; various expressions.
 .text
 .syntax no_register_prefix
 .space 10
x:
 .space 30
xx:
 move.d y-.,r10
 move.d x-.,r10
 move.d y-(.+6),r10
 move.d x-(.+6),r10
 move.d y-.-6,r10
 move.d x-.-6,r10
 move.d [pc+y-(.+12)],r3
 move.d [pc+x-(.+2)],r3
 move.d [pc+y-(y00-2)],r3
y00:
 move.d [pc+x-(y01-2)],r3
y01:
 move.d [pc+y-y02+2],r3
y02:
 move.d [pc+x-y03+2],r3
y03:
 .space 50
y:
 nop
 .space 1000
 move.d [pc+yy-(.+2)],r3
 move.d [pc+x-(.+2)],r3
 move.d [pc+yy-(yy00-2)],r3
yy00:
 move.d [pc+x-(yy01-2)],r3
yy01:
 move.d [pc+yy-yy02+2],r3
yy02:
 move.d [pc+x-yy03+2],r3
yy03:
 .space 1000
yy:
 nop
 .space 100000
 move.d [pc+z-(.+2)],r3
 move.d [pc+x-(.+2)],r3
 move.d [pc+z-(z00-2)],r3
z00:
 move.d [pc+x-(z01-2)],r3
z01:
 move.d [pc+z-z02+2],r3
z02:
 move.d [pc+x-z03+2],r3
z03:
 .space 100000
z:
 nop

