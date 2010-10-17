# Use a symbolic register areg, presumably allocated by greg in another file.
# The "PUSHJ" will be expanded, and the reloc for areg must be resolved
# before the other relocs for that insn.
 PUSHJ areg,a
