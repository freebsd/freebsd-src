% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide an invalid LOP_STAB, one
% with non-zero y and/or z.
 .text
 .byte 0x98,11,1,2
