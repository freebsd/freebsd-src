; Check that register names, both upper and lower case work and that
; the spacing between the operands doesn't matter.

add R0,R1,R2
add r0,r1,r2
add R1,R2,r3
add R1, R3, r3
add R4,R5,R6
add R7,R8,R9
add R10,R11,R12
add R13,R14,R15
addui fp,sp,#1
addui ra,ira,#1

; Check that the range of legal operand values is accepted.

addui R0,R1,#0
addui R0,R1,#$FFFF



