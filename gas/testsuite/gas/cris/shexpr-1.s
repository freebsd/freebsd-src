; This expression had a bit moved.
 .text
 .syntax no_register_prefix
start:
 move.d ((0x17<<23)+((0xfede4194/8192)<<4)+8),r6
 nop
