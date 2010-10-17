#name: FRV uClinux PIC relocs to global symbols, failing shared linking
#source: fdpic2.s
#as: -mfdpic
#ld: -shared
#error: relocations between different segments are not supported
