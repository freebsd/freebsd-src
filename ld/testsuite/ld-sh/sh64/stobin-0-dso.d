#source: stolib.s
#as: --abi=32 --isa=SHmedia
#ld: -shared -mshelf32
#objdump: -drj.text
#target: sh64-*-elf

.*: +file format elf32-sh64.*

#pass
