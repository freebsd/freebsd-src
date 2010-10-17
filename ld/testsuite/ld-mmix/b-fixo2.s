% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_FIXO storing the
% current address at address 0x2068098510aa5560.
 .text
 .byte 0x98,3,0x20,2
 .8byte 0x68098510aa5560
