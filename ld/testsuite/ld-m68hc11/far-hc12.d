#source: far-hc12.s
#as: -m68hc12
#ld: -m m68hc12elf --script $srcdir/$subdir/far-hc12.ld
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*:     file format elf32\-m68hc12

Disassembly of section .text:
0+c000 <tramp\._far_foo> ldy	\#0+8000 <__bank_start>
0+c003 <tramp\._far_foo\+0x3> call	0+c049 <__far_trampoline> \{0+c049 <__far_trampoline>, 1\}
0+c007 <tramp\._far_bar> ldy	\#0+8000 <__bank_start>
0+c00a <tramp\._far_bar\+0x3> call	0+c049 <__far_trampoline> \{0+c049 <__far_trampoline>, 0\}
0+c00e <_start> lds	\#0+2063 <stack-0x1>
0+c011 <_start\+0x3> ldx	\#0+abcd <__bank_start\+0x2bcd>
0+c014 <_start\+0x6> pshx
0+c015 <_start\+0x7> ldd	\#0+1234 <stack\-0xe30>
0+c018 <_start\+0xa> ldx	\#0+5678 <__bank_size\+0x1678>
0+c01b <_start\+0xd> jsr	0+c007 <tramp._far_bar>
0+c01e <_start\+0x10> cpx	\#0+1234 <stack\-0xe30>
0+c021 <_start\+0x13> bne	0+c043 <fail>
0+c023 <_start\+0x15> cpd	\#0+5678 <__bank_size\+0x1678>
0+c026 <_start\+0x18> bne	0+c043 <fail>
0+c028 <_start\+0x1a> pulx
0+c029 <_start\+0x1b> cpx	\#0+abcd <__bank_start\+0x2bcd>
0+c02c <_start\+0x1e> bne	0+c043 <fail>
0+c02e <_start\+0x20> ldd	\#0+c000 <tramp._far_foo>
0+c031 <_start\+0x23> xgdx
0+c033 <_start\+0x25> jsr	0,X
0+c035 <_start\+0x27> ldd	\#0+c007 <tramp._far_bar>
0+c038 <_start\+0x2a> xgdy
0+c03a <_start\+0x2c> jsr	0,Y
0+c03c <_start\+0x2e> call	0+18000 <_far_no_tramp> \{0+8000 <__bank_start>, 2\}
0+c040 <_start\+0x32> clra
0+c041 <_start\+0x33> clrb
0+c042 <_start\+0x34> wai
0+c043 <fail> ldd	\#0+1 <stack\-0x2063>
0+c046 <fail\+0x3> wai
0+c047 <fail\+0x4> bra	0+c00e <_start>
0+c049 <__far_trampoline> movb	0,SP, 2,SP
0+c04d <__far_trampoline\+0x4> leas	2,SP
0+c04f <__far_trampoline\+0x6> jmp	0,Y
Disassembly of section .bank1:
0+10+ <_far_bar> jsr	0+10006 <local_bank1>
0+10003 <_far_bar\+0x3> xgdx
0+10005 <_far_bar\+0x5> rtc
0+10006 <local_bank1> rts
Disassembly of section .bank2:
0+14000 <_far_foo> jsr	0+14004 <local_bank2>
0+14003 <_far_foo\+0x3> rtc
0+14004 <local_bank2> rts
Disassembly of section .bank3:
0+18000 <_far_no_tramp> jsr	0+18004 <local_bank3>
0+18003 <_far_no_tramp\+0x3> rtc
0+18004 <local_bank3> rts
