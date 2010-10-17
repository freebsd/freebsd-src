#source: reloc-1a.s -mabi=64 -EB
#source: reloc-1b.s -mabi=64 -EB
#ld: -melf64btsmip -r
#readelf: --relocs

Relocation section '\.rela\.text' .*
.*
#
# Relocations against tstarta
#
.* R_MIPS_HI16 .* \.text \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32a
#
.* R_MIPS_HI16 .* \.text \+ ffff8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ ffff8020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 20
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 20
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 8030
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8030
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against _start
#
.* R_MIPS_HI16 .* _start \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against tstarta
#
.* R_MIPS_GOT16 .* \.text \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32a
#
.* R_MIPS_GOT16 .* \.text \+ ffff8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ ffff8020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ ffff8020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 20
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 20
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 8030
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8030
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against sdg
#
.* R_MIPS_GPREL16 .* sdg \+ fffffffc
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* sdg \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* sdg \+ 4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against sdla.  .sdata should be the first piece of gp-relative
# data, which the linker script should put _gp - 0x7ff0.
#
.* R_MIPS_GPREL16 .* \.sdata \+ ffff801c
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* \.sdata \+ ffff8020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* \.sdata \+ ffff8024
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against tstarta
#
.* R_MIPS_26 .* \.text \+ fffffffc
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32a
#
.* R_MIPS_26 .* \.text \+ 1c
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 20
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 24
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against _start
#
.* R_MIPS_26 .* _start \+ fffffffc
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* _start \+ 4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against tstartb
#
.* R_MIPS_HI16 .* \.text \+ 7fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ fff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ fff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 17fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 17fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32b
#
.* R_MIPS_HI16 .* \.text \+ 8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 10010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 10010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* \.text \+ 18020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against _start
#
.* R_MIPS_HI16 .* _start \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ ffff7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ ffff8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_HI16 .* _start \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* _start \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against tstartb
#
.* R_MIPS_GOT16 .* \.text \+ 7fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 7ff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ fff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ fff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 17fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 17fe0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32b
#
.* R_MIPS_GOT16 .* \.text \+ 8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 8010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 10010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 10010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18000
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GOT16 .* \.text \+ 18020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_LO16 .* \.text \+ 18020
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against sdg
#
.* R_MIPS_GPREL16 .* sdg \+ fffffffc
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* sdg \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* sdg \+ 4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against sdlb
#
.* R_MIPS_GPREL16 .* \.sdata \+ ffff803c
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* \.sdata \+ ffff8040
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_GPREL16 .* \.sdata \+ ffff8044
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against tstartb
#
.* R_MIPS_26 .* \.text \+ ffec
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ fff0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ fff4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against t32b
#
.* R_MIPS_26 .* \.text \+ 1000c
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 10010
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* \.text \+ 10014
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#
# Relocations against _start
#
.* R_MIPS_26 .* _start \+ fffffffc
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* _start \+ 0
.* R_MIPS_NONE *
.* R_MIPS_NONE *
.* R_MIPS_26 .* _start \+ 4
.* R_MIPS_NONE *
.* R_MIPS_NONE *
#pass
