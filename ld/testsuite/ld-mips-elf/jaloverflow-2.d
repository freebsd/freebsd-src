#name: JAL overflow 2
#source: jaloverflow-2.s
#as:
#ld: -Ttext=0x10000000 -e start
#objdump: -dr
#...
0*10000000:	0c000000.*
#pass
