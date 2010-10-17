% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_QUOTE with invalid;
% non-zero, YZ field.
 .text
 .byte 0x98,0,0xff,0
