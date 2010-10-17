#objdump: -dr --prefix-addresses
#name: MIPS VR5500
#as: -march=vr5500

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <stuff> mul	a0,a1,a2
0+0004 <stuff\+0x4> mulu	a0,a1,a2
0+00008 <stuff\+0x8> mulhi	a0,a1,a2
0+0000c <stuff\+0xc> mulhiu	a0,a1,a2
0+00010 <stuff\+0x10> muls	a0,a1,a2
0+00014 <stuff\+0x14> mulsu	a0,a1,a2
0+00018 <stuff\+0x18> mulshi	a0,a1,a2
0+0001c <stuff\+0x1c> mulshiu	a0,a1,a2
0+00020 <stuff\+0x20> macc	a0,a1,a2
0+00024 <stuff\+0x24> maccu	a0,a1,a2
0+00028 <stuff\+0x28> macchi	a0,a1,a2
0+0002c <stuff\+0x2c> macchiu	a0,a1,a2
0+00030 <stuff\+0x30> msac	a0,a1,a2
0+00034 <stuff\+0x34> msacu	a0,a1,a2
0+00038 <stuff\+0x38> msachi	a0,a1,a2
0+0003c <stuff\+0x3c> msachiu	a0,a1,a2
0+00040 <stuff\+0x40> ror	a0,a1,0x19
0+00044 <stuff\+0x44> rorv	a0,a1,a2
0+00048 <stuff\+0x48> dror	a0,a1,0x19
0+0004c <stuff\+0x4c> dror32	a0,a1,0x19
0+00050 <stuff\+0x50> dror32	a0,a1,0x19
0+00054 <stuff\+0x54> drorv	a0,a1,a2
0+00058 <stuff\+0x58> prefx	0x4,a0\(a1\)
0+0005c <stuff\+0x5c> dbreak
0+00060 <stuff\+0x60> dret
0+00064 <stuff\+0x64> mfdr	v1,\$3
0+00068 <stuff\+0x68> mtdr	v1,\$3
0+0006c <stuff\+0x6c> mfpc	a0,1
0+00070 <stuff\+0x70> mfps	a0,1
0+00074 <stuff\+0x74> mtpc	a0,1
0+00078 <stuff\+0x78> mtps	a0,1
0+0007c <stuff\+0x7c> wait
0+00080 <stuff\+0x80> wait
0+00084 <stuff\+0x84> wait	0x56789
0+00088 <stuff\+0x88> ssnop
0+0008c <stuff\+0x8c> clo	v1,a0
0+00090 <stuff\+0x90> dclo	v1,a0
0+00094 <stuff\+0x94> clz	v1,a0
0+00098 <stuff\+0x98> dclz	v1,a0
0+0009c <stuff\+0x9c> luxc1	\$f0,a0\(v0\)
0+000a0 <stuff\+0xa0> suxc1	\$f2,a0\(v0\)
0+000a4 <stuff\+0xa4> tlbp
0+000a8 <stuff\+0xa8> tlbr
	\.\.\.
