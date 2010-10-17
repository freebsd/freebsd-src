#objdump: -dr --prefix-addresses -mmips:4010
#name: MIPS 4010
#as: -32 -march=4010


.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <stuff> flushi
0+0004 <stuff\+0x4> flushd
0+0008 <stuff\+0x8> flushid
0+000c <stuff\+0xc> madd	a0,a1
0+0010 <stuff\+0x10> maddu	a1,a2
0+0014 <stuff\+0x14> ffc	a2,a3
0+0018 <stuff\+0x18> ffs	a3,t0
0+001c <stuff\+0x1c> msub	t0,t1
0+0020 <stuff\+0x20> msubu	t1,t2
0+0024 <stuff\+0x24> selsl	t2,t3,t4
0+0028 <stuff\+0x28> selsr	t3,t4,t5
0+002c <stuff\+0x2c> waiti
0+0030 <stuff\+0x30> wb	16\(t6\)
0+0034 <stuff\+0x34> addciu	t6,t7,16
	...
