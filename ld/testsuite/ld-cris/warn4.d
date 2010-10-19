#source: start1.s
#source: globsym1ref.s
#source: globsymw2.s
#target: cris-*-*elf* cris-*-*aout*
#as: --em=criself
#ld: -mcriself
#warning: warning: isatty is not implemented and will always fail$
#objdump: -p
#xfail: *-*-*
# The test is xfailed because ELF stabs doesn't handle the stabs
# warning construct.
.*:     file format elf32.*-cris
#pass
