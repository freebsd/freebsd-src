#objdump: -d --prefix-addresses
#name: MRI structured if
#as: -M

# Test MRI structured if pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> cmpw %d1,%d0
0+002 <foo\+(0x|)2> bges 0+014 <foo\+(0x|)14>
0+004 <foo\+(0x|)4> cmpw %d2,%d0
0+006 <foo\+(0x|)6> bges 0+014 <foo\+(0x|)14>
0+008 <foo\+(0x|)8> cmpw %d1,%d2
0+00a <foo\+(0x|)a> bges 0+010 <foo\+(0x|)10>
0+00c <foo\+(0x|)c> movew %d1,%d3
0+00e <foo\+(0x|)e> bras 0+012 <foo\+(0x|)12>
0+010 <foo\+(0x|)10> movew %d2,%d3
0+012 <foo\+(0x|)12> bras 0+01e <foo\+(0x|)1e>
0+014 <foo\+(0x|)14> cmpw %d0,%d1
0+016 <foo\+(0x|)16> blts 0+01c <foo\+(0x|)1c>
0+018 <foo\+(0x|)18> cmpw %d0,%d2
0+01a <foo\+(0x|)1a> bges 0+01e <foo\+(0x|)1e>
0+01c <foo\+(0x|)1c> movew %d0,%d3
0+01e <foo\+(0x|)1e> nop
