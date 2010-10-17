#objdump: -d --prefix-addresses
#name: link

# Test handling of link instruction.

.*: +file format .*

Disassembly of section .text:
0+000 <foo> linkw %fp,#0
0+004 <foo\+(0x|)4> linkw %fp,#-4
0+008 <foo\+(0x|)8> linkw %fp,#-32767
0+00c <foo\+(0x|)c> linkw %fp,#-32768
0+010 <foo\+(0x|)10> linkl %fp,#-32769
0+016 <foo\+(0x|)16> linkw %fp,#32767
0+01a <foo\+(0x|)1a> linkl %fp,#32768
0+020 <foo\+(0x|)20> linkl %fp,#32769
0+026 <foo\+(0x|)26> nop
