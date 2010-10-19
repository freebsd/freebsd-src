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
0+252 g    DF \.text	0+12 dsofn4
0+248 g    DF \.text	0+2 expfn
0+2310 g    DO \.data	0+4 expobj
#...
0+24a g    DF \.text	0+8 dsofn3
#...
0+      D  \*UND\*	0+ dsofn
#...
Contents of section \.rela\.got:
 01d4 0c230000 0a050000 00000000           .*
Contents of section \.rela\.plt:
 01e0 04230000 0b030000 00000000 08230000  .*
 01f0 0b0b0000 00000000                    .*
Contents of section \.plt:
 01f8 84e20401 7e7a3f7a 04f26ffa bf09b005  .*
 0208 00000000 00000000 00006f0d 0c000000  .*
 0218 6ffabf09 b0053f7e 00000000 bf0ed4ff  .*
 0228 ffffb005 6f0d1000 00006ffa bf09b005  .*
 0238 3f7e0c00 0000bf0e baffffff b005      .*
Contents of section \.text:
 0246 b005b005 bfbee2ff ffffb005 7f0da620  .*
 0256 00005f0d 1400bfbe b6ffffff b0050000  .*
Contents of section \.dynamic:
 2268 04000000 94000000 05000000 98010000  .*
 2278 06000000 d8000000 0a000000 3a000000  .*
 2288 0b000000 10000000 03000000 f8220000  .*
 2298 02000000 18000000 14000000 07000000  .*
 22a8 17000000 e0010000 07000000 d4010000  .*
 22b8 08000000 0c000000 09000000 0c000000  .*
 22c8 00000000 00000000 00000000 00000000  .*
 22d8 00000000 00000000 00000000 00000000  .*
 22e8 00000000 00000000 00000000 00000000  .*
Contents of section \.got:
 22f8 68220000 00000000 00000000 1e020000  .*
 2308 38020000 00000000                    .*
Contents of section \.data:
 2310 00000000                             .*
