#source: dso-1.s
#source: dsov32-3.s
#as: --pic --no-underscore --march=v32 --em=criself
#ld: --shared -m crislinux -z nocombreloc
#readelf: -d -r
#warning: relocation R_CRIS_32_PCREL should not be used in a shared object; recompile with -fPIC

# Check that a TEXTREL reloc is correctly generated for PCREL
# relocations against global symbols.
# FIXME: Have a textrel-enabling(-disabling) linker option.
# (Warning always generated unless [other option] warnings are
# generally disabled.)  Split out the expected readelf output
# into a separate test using that option.

Dynamic section at offset 0x[0-9a-f][0-9a-f][0-9a-f] contains 10 entries:
  Tag[ 	]+Type[ 	]+Name/Value
 0x0+4 \(HASH\)[ 	]+0x94
 0x0+5 \(STRTAB\)[ 	]+0x[12][0-9a-f][0-9a-f]
 0x0+6 \(SYMTAB\)[ 	]+0x[0-9a-f][0-9a-f]
 0x0+a \(STRSZ\)[ 	]+38 \(bytes\)
 0x0+b \(SYMENT\)[ 	]+16 \(bytes\)
 0x0+7 \(RELA\)[ 	]+0x[12][0-9a-f][0-9a-f]
 0x0+8 \(RELASZ\)[ 	]+12 \(bytes\)
 0x0+9 \(RELAENT\)[ 	]+12 \(bytes\)
 0x0+16 \(TEXTREL\)[ 	]+0x0
 0x0+ \(NULL\)[ 	]+0x0

Relocation section '\.rela\.text' at offset 0x[12][0-9a-f][0-9a-f] contains 1 entries:
 Offset[ 	]+Info[ 	]+Type[ 	]+Sym\.Value  Sym\. Name \+ Addend
0+[12][0-9a-f][0-9a-f]  0+[0-9a-f]06 R_CRIS_32_PCREL[ 	]+0+[0-f]+[ 	]+dsofn \+ 6
