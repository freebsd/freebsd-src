#source: provide-1.s
#ld: -T provide-1.t
#objdump: -s -j .data

.*:     file format .*

Contents of section .data:
 [0-9a-f]* (1020)?0000(2010)? (2020)?0000(2020)? 00000000 .*
#pass
