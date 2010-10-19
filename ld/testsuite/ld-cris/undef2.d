# source: start1.s
# source: stabs1.s
# target: cris-*-*elf* cris-*-*aout*
# as: --em=crisaout
# ld: -mcrisaout
# error: .o:/blah/foo.c:96: undefined reference to `globsym1'$
