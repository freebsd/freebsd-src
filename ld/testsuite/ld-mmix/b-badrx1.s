% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_FIXRX with invalid
% (non-zero), Y field.
 .text
 .byte 0x98,5,1,0
 .4byte 0
