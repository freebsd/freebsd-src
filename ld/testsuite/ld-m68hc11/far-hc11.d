#source: far-hc11.s
#as: -m68hc11
#ld: -m m68hc11elf
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*:     file format elf32-m68hc11

Disassembly of section .text:
0+8000 <tramp._far_foo> pshb
0+8001 <tramp._far_foo\+0x1> ldab	\#0
0+8003 <tramp._far_foo\+0x3> ldy	\#0+6c <_far_foo>
0+8007 <tramp._far_foo\+0x7> jmp	0+8056 <__far_trampoline>
0+800a <tramp._far_bar> pshb
0+800b <tramp._far_bar\+0x1> ldab	\#0
0+800d <tramp._far_bar\+0x3> ldy	\#0+64 <stack>
0+8011 <tramp._far_bar\+0x7> jmp	0+8056 <__far_trampoline>
0+8014 <_start> lds	\#0+64 <stack>
0+8017 <_start\+0x3> ldx	\#0+abcd <__data_image\+0x2b63>
0+801a <_start\+0x6> pshx
0+801b <_start\+0x7> ldd	\#0+1234 <__data_section_start\+0x134>
0+801e <_start\+0xa> ldx	\#0+5678 <__data_section_start\+0x4578>
0+8021 <_start\+0xd> jsr	0+800a <tramp._far_bar>
0+8024 <_start\+0x10> cpx	\#0+1234 <__data_section_start\+0x134>
0+8027 <_start\+0x13> bne	0+804e <fail>
0+8029 <_start\+0x15> cpd	\#0+5678 <__data_section_start\+0x4578>
0+802d <_start\+0x19> bne	0+804e <fail>
0+802f <_start\+0x1b> pulx
0+8030 <_start\+0x1c> cpx	\#0+abcd <__data_image\+0x2b63>
0+8033 <_start\+0x1f> bne	0+804e <fail>
0+8035 <_start\+0x21> ldd	\#0+8000 <tramp._far_foo>
0+8038 <_start\+0x24> xgdx
0+8039 <_start\+0x25> jsr	0,x
0+803b <_start\+0x27> ldd	\#0+800a <tramp._far_bar>
0+803e <_start\+0x2a> xgdy
0+8040 <_start\+0x2c> jsr	0,y
0+8043 <_start\+0x2f> ldaa	\#0
0+8045 <_start\+0x31> ldy	\#0+73 <_far_no_tramp>
0+8049 <_start\+0x35> bsr	0+8066 <__call_a16>
0+804b <_start\+0x37> clra
0+804c <_start\+0x38> clrb
0+804d <_start\+0x39> wai
0+804e <fail> ldd	\#0+1 <__bss_size\+0x1>
0+8051 <fail\+0x3> wai
0+8052 <fail\+0x4> bra	0+8014 <_start>
0+8054 <__return> ins
0+8055 <__return\+0x1> rts
0+8056 <__far_trampoline> psha
0+8057 <__far_trampoline\+0x1> psha
0+8058 <__far_trampoline\+0x2> pshx
0+8059 <__far_trampoline\+0x3> tsx
0+805a <__far_trampoline\+0x4> ldab	4,x
0+805c <__far_trampoline\+0x6> ldaa	2,x
0+805e <__far_trampoline\+0x8> staa	4,x
0+8060 <__far_trampoline\+0xa> pulx
0+8061 <__far_trampoline\+0xb> pula
0+8062 <__far_trampoline\+0xc> pula
0+8063 <__far_trampoline\+0xd> jmp	0,y
0+8066 <__call_a16> psha
0+8067 <__call_a16\+0x1> jmp	0,y
Disassembly of section .bank1:
0+64 <_far_bar> jsr	0+6b <local_bank1>
0+67 <_far_bar\+0x3> xgdx
0+68 <_far_bar\+0x4> jmp	0+8054 <__return>
0+6b <local_bank1> rts
Disassembly of section .bank2:
0+6c <_far_foo> jsr	0+72 <local_bank2>
0+6f <_far_foo\+0x3> jmp	0+8054 <__return>
0+72 <local_bank2> rts
Disassembly of section .bank3:
0+73 <_far_no_tramp> jsr	0+79 <local_bank3>
0+76 <_far_no_tramp\+0x3> jmp	0+8054 <__return>
0+79 <local_bank3> rts
