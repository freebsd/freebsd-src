% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s, and will provide a valid end of a mmo file but with no
% symbols (if that is actually valid).
 .text
 .byte 0x98,0x0b,0x00,0x00,0x98,0x0c,0x00,0x00
