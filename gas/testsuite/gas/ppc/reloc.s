 .reloc x+8, R_PPC_ADDR32, y-4

 .data
x:
 .long 0,0,0,0

 .section .data.other,"aw",@progbits
y:
 .long 0,0,0,0

 .reloc 0, R_PPC_ADDR32, x
 .reloc y+4, R_PPC_ADDR32, x-4
 .reloc x+12, R_PPC_ADDR32, y
