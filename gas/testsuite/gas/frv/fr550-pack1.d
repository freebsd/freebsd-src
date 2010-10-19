#as: -mcpu=fr550
#objdump: -dr

.*:     file format elf32-frv(|fdpic)

Disassembly of section \.text:

00000000 <.*>:
.*:	09 b0 00 00 	cfmovs\.p fr0,fr4,cc0,0x0
.*:	0b b0 00 01 	cfmovs\.p fr1,fr5,cc0,0x0
.*:	0d b0 00 02 	cfmovs\.p fr2,fr6,cc0,0x0
.*:	8f b0 00 03 	cfmovs fr3,fr7,cc0,0x0
