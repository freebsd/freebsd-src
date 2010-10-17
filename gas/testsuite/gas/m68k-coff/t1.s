# 1 "libgcc1.S"
# 42 "libxyz1.S"
# 259 "libgcc1.S"
	.text
	.proc
|#PROC# 04
	.globl	 __mulsi3      
 __mulsi3      :
|#PROLOGUE# 0
	link	  %a6       ,#0
	addl	#-LF14,  %sp       
	moveml	#LS14,  %sp       @
|#PROLOGUE# 1
	movew	  %a6       @(0x8),   %d0       	 
	muluw	  %a6       @(0xe),   %d0       	 
	movew	  %a6       @(0xa),   %d1       	 
	muluw	  %a6       @(0xc),   %d1       	 
	addw	  %d1       ,   %d0       
	lsll	#8,   %d0       
	lsll	#8,   %d0       
	movew	  %a6       @(0xa),   %d1       	 
	muluw	  %a6       @(0xe),   %d1       	 
	addl	  %d1       ,   %d0       
	jra	LE14
LE14:
|#PROLOGUE# 2
	moveml	  %sp       @, #LS14
	unlk	  %a6       
|#PROLOGUE# 3
	rts
	LF14 = 4
	LS14 = 0x0002		 
	LFF14 = 0
# 354 "libgcc1.S"
	LSS14 = 0x0
	LV14 = 0
