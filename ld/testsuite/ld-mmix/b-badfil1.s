% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide two LOP_FILEs, but
% specifying the same file number.
 .text
 .byte 0x98,06,42,2
 .ascii "foo.s"
 .byte 0,0,0
 .byte 0x98,06,42,2
 .ascii "bar.s"
 .byte 0,0,0
