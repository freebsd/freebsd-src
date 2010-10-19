#source: expdyn1.s
#source: expdref1.s --pic
#source: comref1.s --pic
#as: --no-underscore --em=criself
#ld: -m crislinux --export-dynamic tmpdir/libdso-1.so
#objdump: -R

# Like expdyn2.d, but referencing COMMON symbols.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS \(none\)
