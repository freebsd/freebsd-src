#source: emit-relocs1.s
#ld: -Ttext 0x10000 --defsym target=0xc000 -e0 --emit-relocs
#objdump: -dr
#...
 +10000:	e1a00000 	nop	.*
 +10004:	e1a00000 	nop	.*
 +10008:	e1a00000 	nop	.*
 +1000c:	e1a00000 	nop	.*
 +10010:	eaffeffa 	b	c000 <target>
	+10010: R_ARM_PC24	target\+0xf+8
 +10014:	eaffeffd 	b	c010 <target\+0x10>
	+10014: R_ARM_PC24	target\+0x8
