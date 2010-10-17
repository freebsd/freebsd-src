#objdump: -d --prefix-addresses
#name: MRI structured repeat
#as: -M

# Test MRI structured repeat pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> bccs 0+000 <foo>
0+002 <foo\+(0x|)2> clrw %d1
0+004 <foo\+(0x|)4> addqw #1,%d1
0+006 <foo\+(0x|)6> cmpiw #10,%d1
0+00a <foo\+(0x|)a> blts 0+004 <foo\+(0x|)4>
0+00c <foo\+(0x|)c> nop
0+00e <foo\+(0x|)e> nop
