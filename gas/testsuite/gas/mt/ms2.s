
code:	
	loop R1, label
	loopi #16,label
	dfbc #7,#7,#-1,#-1,#1,#1,#63
	dwfb #7,#7,#-1,#-1,#1,#63
	fbwfb #7,#7,#-1,#-1,#1,#1,#63
	dfbr #7,#7,R0,#7,#7,#7,#1,#63
	nop
label:	 
	fbcbincs  #0,#0,#0,#0,#0,#0,#0,#0,#0,#0
