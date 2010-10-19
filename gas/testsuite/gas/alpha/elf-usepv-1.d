#objdump: --syms
#name: alpha elf-usepv-1

.*:     file format elf64-alpha.*

SYMBOL TABLE:
0*0000000 l    d  \.text	0*0000000 (|\.text)
0*0000000 l    d  \.data	0*0000000 (|\.data)
0*0000000 l    d  \.bss	0*0000000 (|\.bss)
0*0000000 l       \.text	0*0000000 0x80 foo
0*0000004 l       \.text	0*0000000 0x88 bar
