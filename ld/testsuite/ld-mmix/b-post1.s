% The .text contents is supposed to be linked --oformat binary, and will
% correspond to a LOP_POST for an initialization of $255 with 4.  A
% LOP_STAB, such as in b-goodmain.s should follow.
 .text
 .byte 0x98,0x0a,0,0xff,0,0,0,0,0,0,0,4
