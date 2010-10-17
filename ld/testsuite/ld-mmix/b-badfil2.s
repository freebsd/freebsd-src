% The .text contents is supposed to be linked --oformat binary with
% b-twoinsn.s and b-goodmain.s, and will provide a LOP_FILE for file
% number 42, without specifying the file name, which an earlier LOP_FILE
% for the same file number was supposed to have filled in
 .text
 .byte 0x98,06,42,0
