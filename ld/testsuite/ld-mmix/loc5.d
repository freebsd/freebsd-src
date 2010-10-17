#source: loc1.s
#source: start.s
#source: loc2.s
#ld: -m elf64mmix
#objdump: -str
#error: multiple definition of `__\.MMIX\.start\.\.text'
