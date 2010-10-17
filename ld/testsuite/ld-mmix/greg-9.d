#source: gregget1.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-1.s
#source: a.s
#source: start.s
#as: -x
#ld: -m elf64mmix
#objdump: -dt
#error: Too many global registers: 224, max 223

# Allocating the maximum number of gregs *plus one* is an error; other end
# of the stick.
