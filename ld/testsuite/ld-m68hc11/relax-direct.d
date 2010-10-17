#source: relax-direct.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32-m68hc11

Disassembly of section .text:
0+8000 <_start> lds	\*0+28 <stack>
0+8002 <_start\+0x2> ldd	\*0+ <__bss_size>
0+8004 <_start\+0x4> beq	0+800f <F1>
0+8006 <_start\+0x6> bne	0+800b <_start\+0xb>
0+8008 <_start\+0x8> jmp	0+8138 <F2>
0+800b <_start\+0xb> std	\*0+ <__bss_size>
0+800d <_start\+0xd> jsr	\*0+ <__bss_size>
0+800f <F1> addd	\*0+4 <_toto>
0+8011 <F1\+0x2> bne	0+8000 <_start>
0+8013 <F1\+0x4> addd	\*0+cc <_table\+0x9a>
0+8015 <F1\+0x6> addd	0+114 <_stack_top\+0x1a>
0+8018 <F1\+0x9> adca	\*0+34 <_table\+0x2>
0+801a <F1\+0xb> adcb	\*0+35 <_table\+0x3>
0+801c <F1\+0xd> adda	\*0+36 <_table\+0x4>
0+801e <F1\+0xf> addb	\*0+37 <_table\+0x5>
0+8020 <F1\+0x11> addd	\*0+38 <_table\+0x6>
0+8022 <F1\+0x13> anda	\*0+39 <_table\+0x7>
0+8024 <F1\+0x15> andb	\*0+3a <_table\+0x8>
0+8026 <F1\+0x17> cmpa	\*0+3b <_table\+0x9>
0+8028 <F1\+0x19> cmpb	\*0+3c <_table\+0xa>
0+802a <F1\+0x1b> cpd	\*0+3d <_table\+0xb>
0+802d <F1\+0x1e> cpx	\*0+3e <_table\+0xc>
0+802f <F1\+0x20> cpy	\*0+3f <_table\+0xd>
0+8032 <F1\+0x23> eora	\*0+40 <_table\+0xe>
0+8034 <F1\+0x25> eorb	\*0+41 <_table\+0xf>
0+8036 <F1\+0x27> jsr	\*0+42 <_table\+0x10>
0+8038 <F1\+0x29> ldaa	\*0+43 <_table\+0x11>
0+803a <F1\+0x2b> ldab	\*0+44 <_table\+0x12>
0+803c <F1\+0x2d> ldd	\*0+45 <_table\+0x13>
0+803e <F1\+0x2f> lds	\*0+46 <_table\+0x14>
0+8040 <F1\+0x31> ldx	\*0+47 <_table\+0x15>
0+8042 <F1\+0x33> ldy	\*0+48 <_table\+0x16>
0+8045 <F1\+0x36> oraa	\*0+49 <_table\+0x17>
0+8047 <F1\+0x38> orab	\*0+4a <_table\+0x18>
0+8049 <F1\+0x3a> sbcb	\*0+4b <_table\+0x19>
0+804b <F1\+0x3c> sbca	\*0+4c <_table\+0x1a>
0+804d <F1\+0x3e> staa	\*0+4d <_table\+0x1b>
0+804f <F1\+0x40> stab	\*0+4e <_table\+0x1c>
0+8051 <F1\+0x42> std	\*0+4f <_table\+0x1d>
0+8053 <F1\+0x44> sts	\*0+50 <_table\+0x1e>
0+8055 <F1\+0x46> stx	\*0+51 <_table\+0x1f>
0+8057 <F1\+0x48> sty	\*0+52 <_table\+0x20>
0+805a <F1\+0x4b> suba	\*0+53 <_table\+0x21>
0+805c <F1\+0x4d> subb	\*0+54 <_table\+0x22>
0+805e <F1\+0x4f> subd	\*0+55 <_table\+0x23>
0+8060 <F1\+0x51> bne	0+8000 <_start>
0+8062 <F1\+0x53> bra	0+800f <F1>
0+8064 <F1\+0x55> rts
0+8065 <no_relax> addd	0+136 <_stack_top\+0x3c>
0+8068 <no_relax\+0x3> std	0+122 <_stack_top\+0x28>
0+806b <no_relax\+0x6> tst	0+5 <_toto\+0x1>
0+806e <no_relax\+0x9> bne	0+8065 <no_relax>
	...
0+8138 <F2> jmp	0+8000 <_start>
