# Use a symbolic register b, presumably allocated by greg in another file.
# The "GETA" will be expanded, and the reloc for b must be resolved before
# the other relocs for that insn.
 GETA b,a
