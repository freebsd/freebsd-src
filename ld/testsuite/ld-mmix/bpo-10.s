# Overflow; specify 223*8 registers.
 .set i,0
 .rept 223*4*8
 LDA $11,_start+i*64
 .set i,i+1
 .endr
