#source: expdyn1.s
#source: expdref1.s --pic
#source: euwref1.s --pic
#as: --no-underscore --em=criself
#ld: -m crislinux --export-dynamic tmpdir/libdso-1.so
#objdump: -s -j .got

# Like expdyn5.d, weakly referencing symbols.

.*:     file format elf32-cris
Contents of section \.got:
 822a0 38220800 00000000 00000000 00000000  .*
 822b0 07020800 b8220800                    .*
