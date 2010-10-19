#source: gregget1.s
#source: greg-1.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: a.s
#source: start.s
#as: -x
#ld: -m elf64mmix
#objdump: -dt

# Allocating the maximum number of gregs and referring to one at the
# *other* end still works.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+100 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+21 l       \*REG\*	0+ P
0+22 l       \*REG\*	0+ O
0+23 l       \*REG\*	0+ N
0+24 l       \*REG\*	0+ M
0+25 l       \*REG\*	0+ L
0+26 l       \*REG\*	0+ K
0+27 l       \*REG\*	0+ J
0+28 l       \*REG\*	0+ I
0+29 l       \*REG\*	0+ H
0+2a l       \*REG\*	0+ G
0+2b l       \*REG\*	0+ F
0+2c l       \*REG\*	0+ E
0+2d l       \*REG\*	0+ D
0+2e l       \*REG\*	0+ C
0+2f l       \*REG\*	0+ B
0+30 l       \*REG\*	0+ A
0+31 l       \*REG\*	0+ P
0+32 l       \*REG\*	0+ O
0+33 l       \*REG\*	0+ N
0+34 l       \*REG\*	0+ M
0+35 l       \*REG\*	0+ L
0+36 l       \*REG\*	0+ K
0+37 l       \*REG\*	0+ J
0+38 l       \*REG\*	0+ I
0+39 l       \*REG\*	0+ H
0+3a l       \*REG\*	0+ G
0+3b l       \*REG\*	0+ F
0+3c l       \*REG\*	0+ E
0+3d l       \*REG\*	0+ D
0+3e l       \*REG\*	0+ C
0+3f l       \*REG\*	0+ B
0+40 l       \*REG\*	0+ A
0+41 l       \*REG\*	0+ P
0+42 l       \*REG\*	0+ O
0+43 l       \*REG\*	0+ N
0+44 l       \*REG\*	0+ M
0+45 l       \*REG\*	0+ L
0+46 l       \*REG\*	0+ K
0+47 l       \*REG\*	0+ J
0+48 l       \*REG\*	0+ I
0+49 l       \*REG\*	0+ H
0+4a l       \*REG\*	0+ G
0+4b l       \*REG\*	0+ F
0+4c l       \*REG\*	0+ E
0+4d l       \*REG\*	0+ D
0+4e l       \*REG\*	0+ C
0+4f l       \*REG\*	0+ B
0+50 l       \*REG\*	0+ A
0+51 l       \*REG\*	0+ P
0+52 l       \*REG\*	0+ O
0+53 l       \*REG\*	0+ N
0+54 l       \*REG\*	0+ M
0+55 l       \*REG\*	0+ L
0+56 l       \*REG\*	0+ K
0+57 l       \*REG\*	0+ J
0+58 l       \*REG\*	0+ I
0+59 l       \*REG\*	0+ H
0+5a l       \*REG\*	0+ G
0+5b l       \*REG\*	0+ F
0+5c l       \*REG\*	0+ E
0+5d l       \*REG\*	0+ D
0+5e l       \*REG\*	0+ C
0+5f l       \*REG\*	0+ B
0+60 l       \*REG\*	0+ A
0+61 l       \*REG\*	0+ P
0+62 l       \*REG\*	0+ O
0+63 l       \*REG\*	0+ N
0+64 l       \*REG\*	0+ M
0+65 l       \*REG\*	0+ L
0+66 l       \*REG\*	0+ K
0+67 l       \*REG\*	0+ J
0+68 l       \*REG\*	0+ I
0+69 l       \*REG\*	0+ H
0+6a l       \*REG\*	0+ G
0+6b l       \*REG\*	0+ F
0+6c l       \*REG\*	0+ E
0+6d l       \*REG\*	0+ D
0+6e l       \*REG\*	0+ C
0+6f l       \*REG\*	0+ B
0+70 l       \*REG\*	0+ A
0+71 l       \*REG\*	0+ P
0+72 l       \*REG\*	0+ O
0+73 l       \*REG\*	0+ N
0+74 l       \*REG\*	0+ M
0+75 l       \*REG\*	0+ L
0+76 l       \*REG\*	0+ K
0+77 l       \*REG\*	0+ J
0+78 l       \*REG\*	0+ I
0+79 l       \*REG\*	0+ H
0+7a l       \*REG\*	0+ G
0+7b l       \*REG\*	0+ F
0+7c l       \*REG\*	0+ E
0+7d l       \*REG\*	0+ D
0+7e l       \*REG\*	0+ C
0+7f l       \*REG\*	0+ B
0+80 l       \*REG\*	0+ A
0+81 l       \*REG\*	0+ P
0+82 l       \*REG\*	0+ O
0+83 l       \*REG\*	0+ N
0+84 l       \*REG\*	0+ M
0+85 l       \*REG\*	0+ L
0+86 l       \*REG\*	0+ K
0+87 l       \*REG\*	0+ J
0+88 l       \*REG\*	0+ I
0+89 l       \*REG\*	0+ H
0+8a l       \*REG\*	0+ G
0+8b l       \*REG\*	0+ F
0+8c l       \*REG\*	0+ E
0+8d l       \*REG\*	0+ D
0+8e l       \*REG\*	0+ C
0+8f l       \*REG\*	0+ B
0+90 l       \*REG\*	0+ A
0+91 l       \*REG\*	0+ P
0+92 l       \*REG\*	0+ O
0+93 l       \*REG\*	0+ N
0+94 l       \*REG\*	0+ M
0+95 l       \*REG\*	0+ L
0+96 l       \*REG\*	0+ K
0+97 l       \*REG\*	0+ J
0+98 l       \*REG\*	0+ I
0+99 l       \*REG\*	0+ H
0+9a l       \*REG\*	0+ G
0+9b l       \*REG\*	0+ F
0+9c l       \*REG\*	0+ E
0+9d l       \*REG\*	0+ D
0+9e l       \*REG\*	0+ C
0+9f l       \*REG\*	0+ B
0+a0 l       \*REG\*	0+ A
0+a1 l       \*REG\*	0+ P
0+a2 l       \*REG\*	0+ O
0+a3 l       \*REG\*	0+ N
0+a4 l       \*REG\*	0+ M
0+a5 l       \*REG\*	0+ L
0+a6 l       \*REG\*	0+ K
0+a7 l       \*REG\*	0+ J
0+a8 l       \*REG\*	0+ I
0+a9 l       \*REG\*	0+ H
0+aa l       \*REG\*	0+ G
0+ab l       \*REG\*	0+ F
0+ac l       \*REG\*	0+ E
0+ad l       \*REG\*	0+ D
0+ae l       \*REG\*	0+ C
0+af l       \*REG\*	0+ B
0+b0 l       \*REG\*	0+ A
0+b1 l       \*REG\*	0+ P
0+b2 l       \*REG\*	0+ O
0+b3 l       \*REG\*	0+ N
0+b4 l       \*REG\*	0+ M
0+b5 l       \*REG\*	0+ L
0+b6 l       \*REG\*	0+ K
0+b7 l       \*REG\*	0+ J
0+b8 l       \*REG\*	0+ I
0+b9 l       \*REG\*	0+ H
0+ba l       \*REG\*	0+ G
0+bb l       \*REG\*	0+ F
0+bc l       \*REG\*	0+ E
0+bd l       \*REG\*	0+ D
0+be l       \*REG\*	0+ C
0+bf l       \*REG\*	0+ B
0+c0 l       \*REG\*	0+ A
0+c1 l       \*REG\*	0+ P
0+c2 l       \*REG\*	0+ O
0+c3 l       \*REG\*	0+ N
0+c4 l       \*REG\*	0+ M
0+c5 l       \*REG\*	0+ L
0+c6 l       \*REG\*	0+ K
0+c7 l       \*REG\*	0+ J
0+c8 l       \*REG\*	0+ I
0+c9 l       \*REG\*	0+ H
0+ca l       \*REG\*	0+ G
0+cb l       \*REG\*	0+ F
0+cc l       \*REG\*	0+ E
0+cd l       \*REG\*	0+ D
0+ce l       \*REG\*	0+ C
0+cf l       \*REG\*	0+ B
0+d0 l       \*REG\*	0+ A
0+d1 l       \*REG\*	0+ P
0+d2 l       \*REG\*	0+ O
0+d3 l       \*REG\*	0+ N
0+d4 l       \*REG\*	0+ M
0+d5 l       \*REG\*	0+ L
0+d6 l       \*REG\*	0+ K
0+d7 l       \*REG\*	0+ J
0+d8 l       \*REG\*	0+ I
0+d9 l       \*REG\*	0+ H
0+da l       \*REG\*	0+ G
0+db l       \*REG\*	0+ F
0+dc l       \*REG\*	0+ E
0+dd l       \*REG\*	0+ D
0+de l       \*REG\*	0+ C
0+df l       \*REG\*	0+ B
0+e0 l       \*REG\*	0+ A
0+e1 l       \*REG\*	0+ P
0+e2 l       \*REG\*	0+ O
0+e3 l       \*REG\*	0+ N
0+e4 l       \*REG\*	0+ M
0+e5 l       \*REG\*	0+ L
0+e6 l       \*REG\*	0+ K
0+e7 l       \*REG\*	0+ J
0+e8 l       \*REG\*	0+ I
0+e9 l       \*REG\*	0+ H
0+ea l       \*REG\*	0+ G
0+eb l       \*REG\*	0+ F
0+ec l       \*REG\*	0+ E
0+ed l       \*REG\*	0+ D
0+ee l       \*REG\*	0+ C
0+ef l       \*REG\*	0+ B
0+f0 l       \*REG\*	0+ A
0+f1 l       \*REG\*	0+ lsym
0+f2 l       \*REG\*	0+ lsym
0+f3 l       \*REG\*	0+ lsym
0+f4 l       \*REG\*	0+ lsym
0+f5 l       \*REG\*	0+ lsym
0+f6 l       \*REG\*	0+ lsym
0+f7 l       \*REG\*	0+ lsym
0+f8 l       \*REG\*	0+ lsym
0+f9 l       \*REG\*	0+ lsym
0+fa l       \*REG\*	0+ lsym
0+fb l       \*REG\*	0+ lsym
0+fc l       \*REG\*	0+ lsym
0+fd l       \*REG\*	0+ lsym
0+fe l       \*REG\*	0+ lsym
0+14 g       \.text	0+ _start
0+20 g       \*REG\*	0+ areg
2000000000000000 g       \*ABS\*	0+ __bss_start
2000000000000000 g       \*ABS\*	0+ _edata
2000000000000000 g       \*ABS\*	0+ _end
0+14 g       \.text	0+ _start\.
0+10 g       \.text	0+ a

Disassembly of section \.text:

0+ <a-0x10>:
   0:	e3200010 	setl \$32,0x10
   4:	e6200000 	incml \$32,0x0
   8:	e5200000 	incmh \$32,0x0
   c:	e4200000 	inch \$32,0x0

0+10 <a>:
  10:	e3fd0004 	setl \$253,0x4

0+14 <_start>:
  14:	e3fd0001 	setl \$253,0x1
