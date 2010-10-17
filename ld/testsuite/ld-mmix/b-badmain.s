% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s, and will provide the end of a mmo file with a value of
% :Main that does not correspond to the address in the initialization of
% $255 - the start address.
 .text
 .byte 0x98,0x0b,0,0,0x20,0x3a,0x30,0x4d,0x20,0x61,0x20,0x69
 .byte 1,0x6e,0,0x81,1,0x61,4,0x82,0x98,0x0c,0,4
