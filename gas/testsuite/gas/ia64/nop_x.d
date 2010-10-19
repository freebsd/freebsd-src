# objdump: -d
# name: ia64 nop.x pseudo

.*: +file format .*

Disassembly of section \.text:

0+0 <_start>:
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+\[MLX][[:space:]]+nop.m 0x0
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+nop.x 0x0;;
[[:space:]]*[[:xdigit:]]*:[[:space:][:xdigit:]]+
