SEGMENT_SIZE = 0x10000
RVECTOR = 0x00010
.code16
 .globl _start
_start:
 jmp SEGMENT_SIZE-(0x1f00 +0xf0 +RVECTOR)
