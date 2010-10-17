% The .text contents is supposed to be linked --oformat binary, and will
% correspond to the start of a mmo file with two instructions.  This file
% ends before the LOP_STAB.
 .text
 .byte 0x98,9,1,1,0x3b,0x7f,0x9c,0xe3,0x98,1,0,2,0,0,0,0
 .byte 0,0,0,0,0xe3,0xfd,0,1,0x98,1,0,2,0,0,0,0
 .byte 0,0,0,4,0xe3,0xfd,0,4
