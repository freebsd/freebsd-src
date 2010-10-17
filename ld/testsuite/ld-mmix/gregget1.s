# Use a symbolic register areg, presumably allocated by greg in another file.
# The "GETA" will be expanded, and the reloc for areg must be resolved
# before the other relocs for that insn.
 GETA areg,a
