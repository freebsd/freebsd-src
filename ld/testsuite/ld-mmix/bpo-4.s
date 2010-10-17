 .set i,0
 .rept 223
 LDA $11,_start+i*256
 .set i,i+1
 .endr
