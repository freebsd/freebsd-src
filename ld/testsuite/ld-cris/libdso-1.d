#source: dso-1.s
#as: --pic --no-underscore
#ld: --shared -m crislinux
#objdump: -T

# Just check that we actually got a DSO with the dsofn symbol.

.*:     file format elf32-cris

DYNAMIC SYMBOL TABLE:
#...
000002.[02468ace] g    DF .text	00000000 dsofn
#pass
