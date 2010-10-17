#source: adj-brset.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+8000 <_start> brclr	140,x \#\$c8 0+804a <L8>
0+8004 <L1> addd	\*0+4 <_toto>
0+8006 <L1\+0x2> brclr	20,x \#\$03 0+8004 <L1>
0+800a <L1\+0x6> brclr	90,x \#\$63 0+801a <L3>
0+800e <L2> addd	\*0+4 <_toto>
0+8010 <L2\+0x2> brclr	19,y \#\$04 0+800e <L2>
0+8015 <L2\+0x7> brclr	91,y \#\$62 0+8024 <L4>
0+801a <L3> addd	\*0+4 <_toto>
0+801c <L3\+0x2> brset	18,x \#\$05 0+801a <L3>
0+8020 <L3\+0x6> brset	92,x \#\$61 0+8030 <L5>
0+8024 <L4> addd	\*0+4 <_toto>
0+8026 <L4\+0x2> brset	17,y \#\$06 0+8024 <L4>
0+802b <L4\+0x7> brset	93,y \#\$60 0+8030 <L5>
0+8030 <L5> addd	\*0+4 <_toto>
0+8032 <L5\+0x2> brset	\*0+32 <_table> \#\$07 0+8030 <L5>
0+8036 <L5\+0x6> brset	\*0+3c <_table\+0xa> \#\$5f 0+8044 <L7>
0+803a <L6> addd	\*0+4 <_toto>
0+803c <L6\+0x2> brclr	\*0+33 <_table\+0x1> \#\$08 0+803a <L6>
0+8040 <L6\+0x6> brset	\*0+3d <_table\+0xb> \#\$5e 0+804a <L8>
0+8044 <L7> addd	\*0+4 <_toto>
0+8046 <L7\+0x2> brclr	\*0+33 <_table\+0x1> \#\$08 0+803a <L6>
0+804a <L8> brclr	140,x \#\$c8 0+8000 <_start>
0+804e <L8\+0x4> rts
