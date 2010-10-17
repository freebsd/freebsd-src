#source: reloc-1a.s
#source: reloc-1b.s
#ld: -r
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

.* <.*>:
#
# Relocations against tstarta
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_HI16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text

.* <t32a>:
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
#
# Relocations against t32a
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24848020 	addiu	a0,a0,-32736
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24840020 	addiu	a0,a0,32
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24848030 	addiu	a0,a0,-32720
			.*: R_MIPS_LO16	\.text
#
# Relocations against _start
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_HI16	_start
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	_start
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	_start
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	_start
#
# Relocations against tstarta
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_GOT16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
#
# Relocations against t32a
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24848020 	addiu	a0,a0,-32736
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24840020 	addiu	a0,a0,32
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24848030 	addiu	a0,a0,-32720
			.*: R_MIPS_LO16	\.text
#
# Relocations against sdg
#
.*:	2484fffc 	addiu	a0,a0,-4
			.*: R_MIPS_GPREL16	sdg
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_GPREL16	sdg
.*:	24840004 	addiu	a0,a0,4
			.*: R_MIPS_GPREL16	sdg
#
# Relocations against sdla
#
.*:	2484801c 	addiu	a0,a0,-32740
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
.*:	24848020 	addiu	a0,a0,-32736
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
.*:	24848024 	addiu	a0,a0,-32732
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
#
# Relocations against tstarta
#
.*:	0fffffff 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c000000 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c000001 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
#
# Relocations against t32a
#
.*:	0c000007 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c000008 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c000009 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
#
# Relocations against _start
#
.*:	0fffffff 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
.*:	0c000000 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
.*:	0c000001 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
	\.\.\.

.* <tstartb>:
#
# Relocations against tstartb
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24847fe0 	addiu	a0,a0,32736
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	2484fff0 	addiu	a0,a0,-16
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24847fe0 	addiu	a0,a0,32736
			.*: R_MIPS_LO16	\.text

.* <t32b>:
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_HI16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
#
# Relocations against t32b
#
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	\.text
.*:	24840010 	addiu	a0,a0,16
			.*: R_MIPS_LO16	\.text
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_HI16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_HI16	\.text
.*:	24848020 	addiu	a0,a0,-32736
			.*: R_MIPS_LO16	\.text
#
# Relocations against _start
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_HI16	_start
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_LO16	_start
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	_start
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	_start
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_HI16	_start
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	_start
#
# Relocations against tstartb
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24847fe0 	addiu	a0,a0,32736
			.*: R_MIPS_LO16	\.text
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GOT16	\.text
.*:	24847ff0 	addiu	a0,a0,32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	2484fff0 	addiu	a0,a0,-16
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24847fe0 	addiu	a0,a0,32736
			.*: R_MIPS_LO16	\.text
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_GOT16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
#
# Relocations against t32b
#
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24848010 	addiu	a0,a0,-32752
			.*: R_MIPS_LO16	\.text
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GOT16	\.text
.*:	24840010 	addiu	a0,a0,16
			.*: R_MIPS_LO16	\.text
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_GOT16	\.text
.*:	24848000 	addiu	a0,a0,-32768
			.*: R_MIPS_LO16	\.text
.*:	3c040002 	lui	a0,0x2
			.*: R_MIPS_GOT16	\.text
.*:	24848020 	addiu	a0,a0,-32736
			.*: R_MIPS_LO16	\.text
#
# Relocations against sdg
#
.*:	2484fffc 	addiu	a0,a0,-4
			.*: R_MIPS_GPREL16	sdg
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_GPREL16	sdg
.*:	24840004 	addiu	a0,a0,4
			.*: R_MIPS_GPREL16	sdg
#
# Relocations against sdlb
#
.*:	2484803c 	addiu	a0,a0,-32708
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
.*:	24848040 	addiu	a0,a0,-32704
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
.*:	24848044 	addiu	a0,a0,-32700
			.*: R_MIPS_GPREL16	\.sdata\+0x7ff0
#
# Relocations against tstartb
#
.*:	0c003ffb 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c003ffc 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c003ffd 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
#
# Relocations against t32b
#
.*:	0c004003 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c004004 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
.*:	0c004005 	jal	.*
			.*: R_MIPS_26	\.text
.*:	00000000 	nop
#
# Relocations against _start
#
.*:	0fffffff 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
.*:	0c000000 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
.*:	0c000001 	jal	.*
			.*: R_MIPS_26	_start
.*:	00000000 	nop
	\.\.\.
