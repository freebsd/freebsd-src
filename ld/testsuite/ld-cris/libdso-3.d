#source: expdyn1.s
#source: dso-3.s
#as: --pic --no-underscore
#ld: --shared -m crislinux
#objdump: -R

# GOTOFF relocs against global symbols with non-default
# visibility got a linker error.  (A non-default visibility is
# to be treated as a local definition for the reloc.)  We also
# make sure we don't get unnecessary dynamic relocations.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS \(none\)
