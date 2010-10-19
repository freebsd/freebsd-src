#source: dso-1.s
#as: --pic --no-underscore --em=criself
#ld: --shared -m crislinux
#objdump: -T

# Just check that we actually got a DSO with the dsofn symbol.

.*:     file format elf32-cris

DYNAMIC SYMBOL TABLE:
#...
00000[12].[02468ace] g    DF .text	00000000 dsofn
#pass
