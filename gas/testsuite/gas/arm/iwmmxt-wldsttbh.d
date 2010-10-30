#objdump: -dr --prefix-addresses --show-raw-insn -miwmmxt
#name: Intel(r) Wireless MMX(tm) technology instructions version 1
#as: -mcpu=xscale+iwmmxt -EL

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <iwmmxt> ecb11000[ 	]+wldrb[ 	]+wr1, \[r1\]
0+004 <[^>]*> ecf11000[ 	]+wldrh[ 	]+wr1, \[r1\]
0+008 <[^>]*> eca11000[ 	]+wstrb[ 	]+wr1, \[r1\]
0+00c <[^>]*> ece11000[ 	]+wstrh[ 	]+wr1, \[r1\]
