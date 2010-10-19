#objdump: -str

# Check that some pseudos get output right.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ g     F \.text	0+ Main

Contents of section \.text:
 0000 00000020 00000020 00000020 00000020  .*
 0010 0000000a 00000000                    .*
