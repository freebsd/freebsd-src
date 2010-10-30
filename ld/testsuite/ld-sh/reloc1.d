#source: reloc1.s
#as: -big
#ld: -shared -EB --defsym foo=0x9000
#objdump: -sj.data
#target: sh*-*-elf sh-*-vxworks

.*:     file format elf32-sh.*

Contents of section \.data:
 .* 9123 .*
