#objdump: -D
#source: warning-014.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	30 12 8e 2d 	ld	r1, @r2+	||	btsti	r1, 0x6
   4:	01 12 0e 2d 	add	r1, r2	||	btsti	r1, 0x6
