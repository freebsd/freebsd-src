#objdump: -d --prefix-addresses
#name: MRI structured while
#as: -M

# Test MRI structure while pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> bccs 0+004 <foo\+(0x|)4>
0+002 <foo\+(0x|)2> bras 0+000 <foo>
0+004 <foo\+(0x|)4> clrw %d1
0+006 <foo\+(0x|)6> cmpiw #10,%d1
0+00a <foo\+(0x|)a> bgts 0+010 <foo\+(0x|)10>
0+00c <foo\+(0x|)c> addqw #1,%d1
0+00e <foo\+(0x|)e> bras 0+006 <foo\+(0x|)6>
0+010 <foo\+(0x|)10> nop
0+012 <foo\+(0x|)12> nop
