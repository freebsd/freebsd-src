# Use a symbolic register areg, presumably allocated by greg in another file.
# The "BZ" will be expanded, and the reloc for areg must be resolved
# before the other relocs for that insn.
 BZ areg,a
