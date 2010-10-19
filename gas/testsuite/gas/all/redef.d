#objdump: -s -j .data -j "\$DATA\$"
#name: .equ redefinitions

.*: .*

Contents of section (\.data|\$DATA\$):
 0000 00000000 0[04]00000[04] 0[08]00000[08] 0[0c]00000[0c][ 	]+................[ 	]*
#pass
