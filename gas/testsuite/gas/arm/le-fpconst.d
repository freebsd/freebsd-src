#objdump: -s --section=.text
#as: -EL
#name: arm little-endian fpconst

.*: +file format .*arm.*

Contents of section .text:
 0000 cdcc8c3f 00000000 9999f13f 9a999999 .*
