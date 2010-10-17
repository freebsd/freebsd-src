#name: FRV uClinux PIC relocs to undefined symbols, static linking
#source: fdpic5.s
#objdump: -D
#as: -mfdpic
#ld: -static
#error: undefined reference
