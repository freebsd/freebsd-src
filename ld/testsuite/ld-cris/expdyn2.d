#source: expdyn1.s
#source: expdref1.s --pic
#as: --no-underscore --em=criself
#ld: -m crislinux --export-dynamic tmpdir/libdso-1.so
#objdump: -R

# Programs linked with --export-dynamic threw away .rela.got for exported
# symbols, but since got reference counter wasn't reset, there was a SEGV
# trying to generate the .rela.got relocations.  In this test, we have an
# object in the program that has pic-relocations to an exported symbol,
# but those relocations can be resolved at link-time.  We link to a DSO to
# get dynamic linking.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS \(none\)
