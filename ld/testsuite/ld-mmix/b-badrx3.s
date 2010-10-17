% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_FIXRX with invalid
% (!= 0, != 1), first byte of the operand word.
 .text
 .byte 0x98,5,0,24
 .byte 2,0,0,0
