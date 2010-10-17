#source: reloc-2a.s -EB -mabi=32
#source: reloc-2b.s -EB -mabi=32
#ld: --oformat=srec -Treloc-2.ld
#objdump: -D -mmips:4000 --endian=big

.*:     file format .*

Disassembly of section .*:

.* <.*>:
#
# Relocations against tstarta
#
.*:	3c040020 	lui	a0,0x20
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040020 	lui	a0,0x20
.*:	24840000 	addiu	a0,a0,0
.*:	3c040021 	lui	a0,0x21
.*:	24848000 	addiu	a0,a0,-32768
.*:	3c040021 	lui	a0,0x21
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040021 	lui	a0,0x21
.*:	24840010 	addiu	a0,a0,16
#
# Relocations against t32a
#
.*:	3c040020 	lui	a0,0x20
.*:	24840010 	addiu	a0,a0,16
.*:	3c040020 	lui	a0,0x20
.*:	24840020 	addiu	a0,a0,32
.*:	3c040021 	lui	a0,0x21
.*:	24848020 	addiu	a0,a0,-32736
.*:	3c040021 	lui	a0,0x21
.*:	24840010 	addiu	a0,a0,16
.*:	3c040021 	lui	a0,0x21
.*:	24840030 	addiu	a0,a0,48
#
# Relocations against _start
#
.*:	3c040020 	lui	a0,0x20
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040020 	lui	a0,0x20
.*:	24840000 	addiu	a0,a0,0
.*:	3c040021 	lui	a0,0x21
.*:	24848000 	addiu	a0,a0,-32768
.*:	3c040021 	lui	a0,0x21
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040021 	lui	a0,0x21
.*:	24840010 	addiu	a0,a0,16
#
# Relocations against sdg
#
.*:	2484edd8 	addiu	a0,a0,-4648
.*:	2484eddc 	addiu	a0,a0,-4644
.*:	2484ede0 	addiu	a0,a0,-4640
#
# Relocations against sdla
#
.*:	2484edd8 	addiu	a0,a0,-4648
.*:	2484eddc 	addiu	a0,a0,-4644
.*:	2484ede0 	addiu	a0,a0,-4640
#
# Relocations against tstarta
#
.*:	0c081fff 	jal	0x207ffc
.*:	00000000 	nop
.*:	0c082000 	jal	0x208000
.*:	00000000 	nop
.*:	0c082001 	jal	0x208004
.*:	00000000 	nop
#
# Relocations against t32a
#
.*:	0c082007 	jal	0x20801c
.*:	00000000 	nop
.*:	0c082008 	jal	0x208020
.*:	00000000 	nop
.*:	0c082009 	jal	0x208024
.*:	00000000 	nop
#
# Relocations against _start
#
.*:	0c081fff 	jal	0x207ffc
.*:	00000000 	nop
.*:	0c082000 	jal	0x208000
.*:	00000000 	nop
.*:	0c082001 	jal	0x208004
.*:	00000000 	nop
	\.\.\.
#
# Relocations against tstartb
#
.*:	3c040021 	lui	a0,0x21
.*:	2484ffe0 	addiu	a0,a0,-32
.*:	3c040021 	lui	a0,0x21
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040021 	lui	a0,0x21
.*:	24847ff0 	addiu	a0,a0,32752
.*:	3c040022 	lui	a0,0x22
.*:	2484ffe0 	addiu	a0,a0,-32
.*:	3c040022 	lui	a0,0x22
.*:	24840000 	addiu	a0,a0,0
#
# Relocations against t32b
#
.*:	3c040021 	lui	a0,0x21
.*:	24840000 	addiu	a0,a0,0
.*:	3c040021 	lui	a0,0x21
.*:	24840010 	addiu	a0,a0,16
.*:	3c040022 	lui	a0,0x22
.*:	24848010 	addiu	a0,a0,-32752
.*:	3c040022 	lui	a0,0x22
.*:	24840000 	addiu	a0,a0,0
.*:	3c040022 	lui	a0,0x22
.*:	24840020 	addiu	a0,a0,32
#
# Relocations against _start
#
.*:	3c040020 	lui	a0,0x20
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040020 	lui	a0,0x20
.*:	24840000 	addiu	a0,a0,0
.*:	3c040021 	lui	a0,0x21
.*:	24848000 	addiu	a0,a0,-32768
.*:	3c040021 	lui	a0,0x21
.*:	2484fff0 	addiu	a0,a0,-16
.*:	3c040021 	lui	a0,0x21
.*:	24840010 	addiu	a0,a0,16
#
# Relocations against sdg
#
.*:	2484edd8 	addiu	a0,a0,-4648
.*:	2484eddc 	addiu	a0,a0,-4644
.*:	2484ede0 	addiu	a0,a0,-4640
#
# Relocations against sdl2
#
.*:	2484edf8 	addiu	a0,a0,-4616
.*:	2484edfc 	addiu	a0,a0,-4612
.*:	2484ee00 	addiu	a0,a0,-4608
#
# Relocations against tstartb
#
.*:	0c085ffb 	jal	0x217fec
.*:	00000000 	nop
.*:	0c085ffc 	jal	0x217ff0
.*:	00000000 	nop
.*:	0c085ffd 	jal	0x217ff4
.*:	00000000 	nop
#
# Relocations against t32b
#
.*:	0c086003 	jal	0x21800c
.*:	00000000 	nop
.*:	0c086004 	jal	0x218010
.*:	00000000 	nop
.*:	0c086005 	jal	0x218014
.*:	00000000 	nop
#
# Relocations against _start
#
.*:	0c081fff 	jal	0x207ffc
.*:	00000000 	nop
.*:	0c082000 	jal	0x208000
.*:	00000000 	nop
.*:	0c082001 	jal	0x208004
.*:	00000000 	nop
	\.\.\.
#pass
