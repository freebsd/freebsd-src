#source: dso-1.s
#source: dsov32-1.s
#as: --pic --no-underscore --march=v32 --em=criself
#ld: --shared -m crislinux
#objdump: -s -T

.*:     file format elf32-cris

DYNAMIC SYMBOL TABLE:
#...
0+1a0 g    DF \.text	0+8 dsofn3
#...
0+19c g    DF \.text	0+ dsofn
#...
Contents of section \.rela\.plt:
 015c 2c220000 0b060000 00000000           .*
Contents of section \.plt:
 0168 84e20401 7e7a3f7a 04f26ffa bf09b005  .*
 0178 00000000 00000000 00006f0d 0c000000  .*
 0188 6ffabf09 b0053f7e 00000000 bf0ed4ff  .*
 0198 ffffb005                             .*
Contents of section \.text:
 019c b0050000 bfbee2ff ffffb005           .*
Contents of section \.dynamic:
#...
Contents of section \.got:
 2220 a8210000 00000000 00000000 8e010000  .*
