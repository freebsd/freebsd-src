% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_FIXRX with invalid
% (!= 16, != 24), Z field.
 .text
 .byte 0x98,5,0,8
 .4byte 0
