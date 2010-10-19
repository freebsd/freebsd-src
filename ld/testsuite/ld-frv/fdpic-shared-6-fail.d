#name: FRV uClinux PIC relocs to undefined symbols, shared linking
#source: fdpic6.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -shared
#error: different segment
