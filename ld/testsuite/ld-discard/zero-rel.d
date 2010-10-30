#source: zero-rel.s
#ld: -T discard.ld
#objdump: -s -j .debug_info

.*:     file format .*elf.*

Contents of section .debug_info:
 0000 0+( 0+)? +(\.+) .*
