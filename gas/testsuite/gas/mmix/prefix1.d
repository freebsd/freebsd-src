# objdump: -rt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.text	0+ a
0+4 l       \.text	0+ c
0+24 l       \.text	0+ d
0+8 l       \.text	0+ prea
0+c l       \.text	0+ pre:c
0+10 l       \.text	0+ prefixa
0+14 l       \.text	0+ pre:fix:c
0+18 l       \.text	0+ aprefixa
0+1c l       \.text	0+ aprefix:c
0+20 l       \.text	0+ a0
0+         \*UND\*	0+ b
0+         \*UND\*	0+ preb
0+         \*UND\*	0+ pre:d
0+         \*UND\*	0+ prefixb
0+         \*UND\*	0+ pre:fix:d
0+         \*UND\*	0+ aprefixb
0+         \*UND\*	0+ aprefix:d

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_32         b
0+4 R_MMIX_32         \.text\+0x0+24
0+8 R_MMIX_32         preb
0+c R_MMIX_32         pre:d
0+10 R_MMIX_32         prefixb
0+14 R_MMIX_32         pre:fix:d
0+18 R_MMIX_32         aprefixb
0+1c R_MMIX_32         aprefix:d
0+20 R_MMIX_32         \.text
0+24 R_MMIX_32         \.text\+0x0+4
