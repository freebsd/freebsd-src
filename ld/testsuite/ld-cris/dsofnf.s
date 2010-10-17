 .text
 .global f
 .type f,@function
f:
 move.d [$r0+dsofn:GOT],$r1
0:
 .size f,0b-f
