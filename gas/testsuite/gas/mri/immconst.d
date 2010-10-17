#objdump: -d --prefix-addresses
#name: MRI immediate constants
#as: -M
#source: constants.s

# Test MRI immediate constants

.*:     file format .*

Disassembly of section .text:
0+000 <foo> moveq #10,%d0
0+002 <foo\+(0x|)2> moveq #10,%d0
0+004 <foo\+(0x|)4> moveq #10,%d0
0+006 <foo\+(0x|)6> moveq #10,%d0
0+008 <foo\+(0x|)8> moveq #10,%d0
0+00a <foo\+(0x|)a> moveq #10,%d0
0+00c <foo\+(0x|)c> moveq #10,%d0
0+00e <foo\+(0x|)e> moveq #10,%d0
0+010 <foo\+(0x|)10> moveq #10,%d0
0+012 <foo\+(0x|)12> moveq #97,%d0
0+014 <foo\+(0x|)14> moveq #97,%d0
0+016 <foo\+(0x|)16> nop
