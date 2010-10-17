#objdump: -r
#source: relocs2.s
#name: TIc80 simple relocs, static and global variables (relocs)

.*: +file format .*tic80.*

RELOCATION RECORDS FOR \[.text\]:
OFFSET   TYPE              VALUE 
00000004 32                .bss\+0xffffff1c
0000000c 32                _x_char
00000014 32                _x_char
0000001c 32                .bss\+0xffffff1c
00000024 32                .bss\+0xffffff1c
0000002c 32                _x_short
00000034 32                _x_short
0000003c 32                .bss\+0xffffff1c
00000044 32                .bss\+0xffffff1c
0000004c 32                .bss\+0xffffff1c
00000054 32                .bss\+0xffffff1c
0000005c 32                .bss\+0xffffff1c
00000064 32                .bss\+0xffffff1c
0000006c 32                _x_long
00000074 32                _x_long
0000007c 32                .bss\+0xffffff1c
00000084 32                .bss\+0xffffff1c
0000008c 32                _x_float
00000094 32                _x_float
0000009c 32                .bss\+0xffffff1c
000000a4 32                .bss\+0xffffff1c
000000ac 32                _x_double
000000b4 32                _x_double
000000bc 32                .bss\+0xffffff1c
000000c4 32                .bss\+0xffffff1c
000000cc 32                _x_char_p
000000d4 32                _x_char_p
000000dc 32                .bss\+0xffffff1c


