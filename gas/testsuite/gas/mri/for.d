#objdump: -d --prefix-addresses
#name: MRI structured for
#as: -M

# Test MRI structured for pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> clrw %d1
0+002 <foo\+(0x|)2> movew #1,%d0
0+006 <foo\+(0x|)6> cmpiw #10,%d0
0+00a <foo\+(0x|)a> blts 0+016 <foo\+(0x|)16>
0+00c <foo\+(0x|)c> addw %d0,%d1
0+00e <foo\+(0x|)e> bvcs 0+012 <foo\+(0x|)12>
0+010 <foo\+(0x|)10> bras 0+016 <foo\+(0x|)16>
0+012 <foo\+(0x|)12> addqw #2,%d0
0+014 <foo\+(0x|)14> bras 0+006 <foo\+(0x|)6>
0+016 <foo\+(0x|)16> clrw %d1
0+018 <foo\+(0x|)18> movew #10,%d0
0+01c <foo\+(0x|)1c> cmpiw #1,%d0
0+020 <foo\+(0x|)20> bgts 0+030 <foo\+(0x|)30>
0+022 <foo\+(0x|)22> cmpiw #100,%d1
0+026 <foo\+(0x|)26> blts 0+02a <foo\+(0x|)2a>
0+028 <foo\+(0x|)28> bras 0+02c <foo\+(0x|)2c>
0+02a <foo\+(0x|)2a> addw %d0,%d1
0+02c <foo\+(0x|)2c> subqw #1,%d0
0+02e <foo\+(0x|)2e> bras 0+01c <foo\+(0x|)1c>
0+030 <foo\+(0x|)30> nop
0+032 <foo\+(0x|)32> nop
