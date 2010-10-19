#source: gotrel2.s
#as: --pic --no-underscore --em=criself
#ld: -m crislinux tmpdir/libdso-1.so
#objdump: -R

# A dynamic reloc for an undefined weak reference in a program got a
# confused symbol reference count mismatch with a bfd assertion.  Linking
# with a DSO was needed as a catalyst to get to the faulty code; nothing
# in the DSO was needed.  We just check that we don't get the bfd
# assertion.  Note that no actual dynamic reloc is created for the
# unresolved weak.  Perhaps it should; the symbol could be defined in a
# preloaded object or a new version of the DSO.  FIXME: Revisit and adjust
# test-result.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS \(none\)
