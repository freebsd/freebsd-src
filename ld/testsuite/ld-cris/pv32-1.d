#source: pv32.s
#as: --march=v32 --no-underscore --em=criself
#ld: -e here -m crislinux tmpdir/libdso-12.so
#objdump: -s -T

# Trivial test of linking a program to a v32 DSO.

.*:     file format elf32-cris

DYNAMIC SYMBOL TABLE:
0+8021e      DF \*UND\*	0+2 expfn
0+82324 g    DO \.bss	0+4 expobj
0+82324 g    D  \*ABS\*	0+ __bss_start
0+80238      DF \*UND\*	0+8 dsofn3
0+82324 g    D  \*ABS\*	0+ _edata
0+82340 g    D  \*ABS\*	0+ _end
0+80264 g    DF \.text	0+8 dsofn

Contents of section \.interp:
 800d4 2f6c6962 2f6c642e 736f2e31 00        .*
#...
Contents of section \.rela\.dyn:
 801e0 24230800 09020000 00000000           .*
Contents of section \.rela\.plt:
 801ec 1c230800 0b010000 00000000 20230800  .*
 801fc 0b040000 00000000                    .*
Contents of section \.plt:
 80204 84e26ffe 14230800 7e7a3f7a 04f26ffa  .*
 80214 bf09b005 00000000 00006ffe 1c230800  .*
 80224 6ffabf09 b0053f7e 00000000 bf0ed4ff  .*
 80234 ffffb005 6ffe2023 08006ffa bf09b005  .*
 80244 3f7e0c00 0000bf0e baffffff b005      .*
Contents of section \.text:
 80252 b005bfbe caffffff b005bfbe dcffffff  .*
 80262 b0056fae 24230800 b0050000           .*
Contents of section \.dynamic:
#...
Contents of section \.got:
 82310 70220800 00000000 00000000 2a020800  .*
 82320 44020800                             .*
