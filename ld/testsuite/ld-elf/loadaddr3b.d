#source: loadaddr.s
#ld: -T loadaddr3.t -z max-page-size=0x200000
#objdump: -t
#target: *-*-linux*

#...
0+0000100 l    d  .text	0+0000000 .text
0+0000200 l    d  .data	0+0000000 .data
#...
0+0000110 g       \*ABS\*	0+0000000 data_load
#...
0+0000200 g       .data	0+0000000 data_start
#pass
