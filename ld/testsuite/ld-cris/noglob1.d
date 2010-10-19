#ld:
#target: cris-*-*elf* cris-*-*aout*
#objdump: -p

# Check that we can link an object that doesn't have any global symbols;
# where elf_sym_hashes(bfd) is NULL.

.*:     file format .*-cris
#pass
