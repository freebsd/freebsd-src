; Test the addi insn.
 .text
 .syntax no_register_prefix
start:
 addi r0.b,r1
 addi r0.w,r1
 addi r0.d,r1
 addi r0.b,r0
 addi r0.w,r0
 addi r0.d,r0
 addi r5.b,r7
 addi r9.w,r0
 addi r11.d,r13
 addi r4.b,r4
 addi r4.w,r4
 addi r4.d,r4
end:
