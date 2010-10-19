#name: FRV uClinux PIC relocs to (mostly) global symbols with addends, failing shared linking
#source: fdpic8.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -shared
#error: (nonzero addend|may have caused)
