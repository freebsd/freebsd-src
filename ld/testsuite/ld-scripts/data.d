#source: data.s
#ld: -T data.t
#objdump: -s -j .text

.*:     file format .*

Contents of section .text:
 [0-9a-f]* (04)?000000(04)? (0020)?0000(2000)? .*
#pass
