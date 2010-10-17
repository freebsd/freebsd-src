% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide an invalid lopcode.
 .text
 .byte 0x98,0xff,0,0
