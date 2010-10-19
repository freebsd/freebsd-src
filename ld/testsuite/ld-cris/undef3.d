#source: start1.s
#source: stabs1.s
#target: cris-*-*elf* cris-*-*aout*
#as: --em=criself
#ld: -mcriself
#error: .o:/blah/foo.c:96: undefined reference to `globsym1'$
