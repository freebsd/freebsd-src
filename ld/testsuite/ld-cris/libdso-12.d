#source: expdyn1.s
#source: dsov32-1.s
#source: dsov32-2.s
#as: --pic --no-underscore --march=v32 --em=criself
#ld: --shared -m crislinux -z nocombreloc
#objdump: -s -T

# Check for common DSO contents; load of GOT register, branch to
# function PLT, undefined symbol, GOT reloc.

.*:     file format elf32-cris

DYNAMIC SYMBOL TABLE:
#...
0+23e g    DF \.text	0+12 dsofn4
0+234 g    DF \.text	0+2 expfn
0+22fc g    DO \.data	0+4 expobj
#...
0+236 g    DF \.text	0+8 dsofn3
#...
0+      D  \*UND\*	0+ dsofn
#...
Contents of section \.rela\.got:
 01c0 f8220000 0a040000 00000000           .*
Contents of section \.rela\.plt:
 01cc f0220000 0b020000 00000000 f4220000  .*
 01dc 0b0a0000 00000000                    .*
Contents of section \.plt:
 01e4 84e20401 7e7a3f7a 04f26ffa bf09b005  .*
 01f4 00000000 00000000 00006f0d 0c000000  .*
 0204 6ffabf09 b0053f7e 00000000 bf0ed4ff  .*
 0214 ffffb005 6f0d1000 00006ffa bf09b005  .*
 0224 3f7e0c00 0000bf0e baffffff b005      .*
Contents of section \.text:
 0232 b005b005 bfbee2ff ffffb005 7f0da620  .*
 0242 00005f0d 1400bfbe b6ffffff b0050000  .*
Contents of section \.dynamic:
 2254 04000000 94000000 05000000 84010000  .*
 2264 06000000 d4000000 0a000000 3a000000  .*
 2274 0b000000 10000000 03000000 e4220000  .*
 2284 02000000 18000000 14000000 07000000  .*
 2294 17000000 cc010000 07000000 c0010000  .*
 22a4 08000000 0c000000 09000000 0c000000  .*
 22b4 00000000 00000000 00000000 00000000  .*
 22c4 00000000 00000000 00000000 00000000  .*
 22d4 00000000 00000000 00000000 00000000  .*
Contents of section \.got:
 22e4 54220000 00000000 00000000 0a020000  .*
 22f4 24020000 00000000                    .*
Contents of section \.data:
 22fc 00000000                             .*
