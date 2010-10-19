#objdump: -dr
#as: --underscore --march=v32 --em=criself
#source: v32-err-8.s

# Check that USP gets the right number for V32.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <\.text>:
   0:	3ae6                	move r10,usp
   2:	3fee b0ab 0f00      	move 0xfabb0,usp
   8:	75ea                	move usp,\[r5\]
   a:	3cea                	move \[r12\],usp
