#source: relax-group.s
#as: -m68hc11
#ld: --relax
#objdump: -d --prefix-addresses -r
#target: m6811-*-* m6812-*-*

.*: +file format elf32-m68hc11

Disassembly of section .text:
0+8000 <_start> bset	\*0+ <__bss_size> #\$04
0+8003 <L1x> bset	\*0+ <__bss_size> #\$04
0+8006 <L1y> bset	\*0+3 <__bss_size\+0x3> #\$04
0+8009 <L1y\+0x3> bset	\*0+4 <table4> #\$08
0+800c <L2x> bset	\*0+3 <__bss_size\+0x3> #\$04
0+800f <L2x\+0x3> bset	\*0+4 <table4> #\$08
0+8012 <L2y> bset	\*0+6 <table4\+0x2> #\$04
0+8015 <L2y\+0x3> bset	\*0+7 <table4\+0x3> #\$08
0+8018 <L2y\+0x6> bset	\*0+8 <table8> #\$0c
0+801b <L2y\+0x9> bset	\*0+9 <table8\+0x1> #\$0c
0+801e <L2y\+0xc> bset	\*0+a <table8\+0x2> #\$0c
0+8021 <L2y\+0xf> bset	\*0+b <table8\+0x3> #\$0c
0+8024 <L3x> bset	\*0+6 <table4\+0x2> #\$04
0+8027 <L3x\+0x3> bset	\*0+7 <table4\+0x3> #\$08
0+802a <L3x\+0x6> bset	\*0+8 <table8> #\$0c
0+802d <L3x\+0x9> bset	\*0+9 <table8\+0x1> #\$0c
0+8030 <L3x\+0xc> bset	\*0+a <table8\+0x2> #\$0c
0+8033 <L3x\+0xf> bset	\*0+b <table8\+0x3> #\$0c
0+8036 <L3y> bra	0+8000 <_start>
0+8038 <L3y\+0x2> ldx	#0+fe <end_table\+0xe8>
0+803b <L3y\+0x5> bset	\*0+fe <end_table\+0xe8> #\$04
0+803e <L3y\+0x8> bset	\*0+ff <end_table\+0xe9> #\$08
0+8041 <L3y\+0xb> bset	2,x #\$0c
0+8044 <L3y\+0xe> bset	3,x #\$0c
0+8047 <L3y\+0x11> bset	4,x #\$0c
0+804a <L3y\+0x14> bset	5,x #\$0c
0+804d <L4x> ldy	#0+fe <end_table\+0xe8>
0+8051 <L4x\+0x4> bset	\*0+fe <end_table\+0xe8> #\$04
0+8054 <L4x\+0x7> bset	\*0+ff <end_table\+0xe9> #\$08
0+8057 <L4x\+0xa> bset	2,y #\$0c
0+805b <L4x\+0xe> bset	3,y #\$0c
0+805f <L4x\+0x12> bset	4,y #\$0c
0+8063 <L4x\+0x16> bset	5,y #\$0c
0+8067 <L4y> bclr	\*0+a <table8\+0x2> #\$04
0+806a <L4y\+0x3> bclr	\*0+b <table8\+0x3> #\$08
0+806d <L5x> bclr	\*0+1a <end_table\+0x4> #\$04
0+8070 <L5x\+0x3> bclr	\*0+1b <end_table\+0x5> #\$08
0+8073 <L5y> brset	\*0+8 <table8> #\$04 0+8073 <L5y>
0+8077 <L6x> brset	\*0+8 <table8> #\$04 0+8077 <L6x>
0+807b <L7x> brset	\*0+8 <table8> #\$04 0+8094 <brend>
0+807f <L8x> brset	\*0+8 <table8> #\$04 0+8094 <brend>
0+8083 <L8y> brclr	\*0+8 <table8> #\$04 0+8083 <L8y>
0+8087 <L9x> brclr	\*0+8 <table8> #\$04 0+8087 <L9x>
0+808b <L9y> brclr	\*0+8 <table8> #\$04 0+8094 <brend>
0+808f <L10x> brclr	\*0+8 <table8> #\$04 0+8094 <brend>
0+8093 <L10y> nop
0+8094 <brend> bset	0,x #\$04
0+8097 <w2> ldx	#0+ <__bss_size>
0+809a <w3> ldy	#0+8 <table8>
0+809e <w4> rts
0+809f <w5> ldx	#0+ <__bss_size>
0+80a2 <w5\+0x3> bset	0,x #\$05
0+80a5 <w5\+0x6> jmp	0+8000 <_start>
0+80a8 <w5\+0x9> rts
