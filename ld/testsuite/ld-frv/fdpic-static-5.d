#name: FRV uClinux PIC relocs to undefined symbols, static linking
#source: fdpic5.s
#objdump: -D
#ld: -static
#error: undefined reference
