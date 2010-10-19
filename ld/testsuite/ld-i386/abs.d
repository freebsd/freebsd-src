#name: Absolute non-overflowing relocs
#source: abs.s
#source: zero.s
#ld:
#objdump: -rs

.*:     file format .*

Contents of section \.text:
[ 	][0-9a-f]+ c800fff0 c8000110 c9c3.*
