; Return-type insns.  Keep a nop after them, in case a sane
; warning is added to the assembler.
 .text
start:
 ret
 nop
 reti
 nop
 retb
 nop
end:
