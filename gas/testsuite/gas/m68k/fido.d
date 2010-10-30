#objdump: -d --prefix-addresses
#name: fido

# Test parsing of the operands of the fido-specific instructions.

.*: +file format .*

Disassembly of section .text:
0+000 <foo> sleep
0+002 <foo\+(0x|)2> trapx #0
0+004 <foo\+(0x|)4> trapx #1
0+006 <foo\+(0x|)6> trapx #2
0+008 <foo\+(0x|)8> trapx #3
0+00a <foo\+(0x|)a> trapx #4
0+00c <foo\+(0x|)c> trapx #5
0+00e <foo\+(0x|)e> trapx #6
0+010 <foo\+(0x|)10> trapx #7
0+012 <foo\+(0x|)12> trapx #8
0+014 <foo\+(0x|)14> trapx #9
0+016 <foo\+(0x|)16> trapx #10
0+018 <foo\+(0x|)18> trapx #11
0+01a <foo\+(0x|)1a> trapx #12
0+01c <foo\+(0x|)1c> trapx #13
0+01e <foo\+(0x|)1e> trapx #14
0+020 <foo\+(0x|)20> trapx #15
0+022 <foo\+(0x|)22> movec %cac,%d0
0+026 <foo\+(0x|)26> movec %cac,%a0
0+02a <foo\+(0x|)2a> movec %mbb,%d1
0+02e <foo\+(0x|)2e> movec %mbb,%a1
0+032 <foo\+(0x|)32> movec %d2,%cac
0+036 <foo\+(0x|)36> movec %a2,%cac
0+03a <foo\+(0x|)3a> movec %d3,%mbb
0+03e <foo\+(0x|)3e> movec %a3,%mbb
0+042 <foo\+(0x|)42> movec %cac,%d4
0+046 <foo\+(0x|)46> movec %cac,%a4
0+04a <foo\+(0x|)4a> movec %mbb,%d5
0+04e <foo\+(0x|)4e> movec %mbb,%a5
0+052 <foo\+(0x|)52> movec %d6,%cac
0+056 <foo\+(0x|)56> movec %fp,%cac
0+05a <foo\+(0x|)5a> movec %d7,%mbb
0+05e <foo\+(0x|)5e> movec %sp,%mbb
