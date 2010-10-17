#objdump: -fdr --prefix-addresses --show-raw-insn
#name: SH4a FP constructs

.*:     file format elf.*sh.*
architecture: sh4a, flags 0x00000010:
HAS_SYMS
start address 0x00000000

Disassembly of section \.text:
0x00000000 f7 fd       	fpchg	
0x00000002 f1 7d       	fsrra	fr1
0x00000004 f9 7d       	fsrra	fr9
0x00000006 f6 7d       	fsrra	fr6
0x00000008 f2 fd       	fsca	fpul,dr2
0x0000000a fc fd       	fsca	fpul,dr12
