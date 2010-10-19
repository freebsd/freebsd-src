#source: dso-2.s
#as: --pic --no-underscore --em=criself
#ld: --shared -m crislinux
#objdump: -T

# DSO with an undef symbol "dsofn".  See undef1.d.

.*:     file format elf32-cris
#...
0+      D  \*UND\*	0+ dsofn

