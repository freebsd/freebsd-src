#source: start1.s
#source: globsym1ref.s
#source: globsymw1.s
#target: cris-*-*elf* cris-*-*aout*
#as: --em=crisaout
#ld: -mcrisaout
#objdump: -p
# There should be no warning, since the symbol warned about is
# missing from the construct.
.*:     file format a\.out-cris
#pass
