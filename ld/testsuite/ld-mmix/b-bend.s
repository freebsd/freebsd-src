% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide an invalid LOP_END; one
% not at the end of a file.  It also does not in YZ specify a correct
% number of bytes between it and a preceding lop_stab.
 .text
 .byte 0x98,12,0,0
