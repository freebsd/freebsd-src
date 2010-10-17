; Relaxation is possible for following bit manipulation instructions
; BAND, BCLR, BIAND, BILD, BIOR, BIST, BIXOR, BLD, BNOT, BOR, BSET, BST, BTST, BXOR
  	.h8300s
    	.globl	_start
    _start:
    	# s3-s6 aren't valid 16-bit addresses.
    	mov.b   #0x3,r0l
   	mov.b   #0x5,r2l
;
; Relaxation of aa:16
;	    	
   	bset    r0l,@s10:16
	bset    r2l,@s9:16
	btst    r2l,@s10:16
   	btst    r0l,@s9:16
   	
   	bset	#5,@s1:16
   	bset	#5,@s2:16
   	bset	#5,@s7:16
   	bset	#5,@s8:16
   	bset	#5,@s9:16
   	bset	#5,@s10:16   	   	
	   	
   	band	#5,@s1:16
	band	#5,@s2:16
	band	#5,@s7:16
   	band	#5,@s8:16
   	band	#5,@s9:16
   	band	#5,@s10:16
;
; Relaxation of aa:32
;
   	bset    r2l,@s10:32
   	bset    r0l,@s9:32
   	btst    r0l,@s10:32
   	btst    r2l,@s9:32
   	
   	bset	#6,@s1:32
   	bset	#6,@s2:32
   	bset	#6,@s3:32
   	bset	#6,@s4:32
   	bset	#6,@s5:32
   	bset	#6,@s6:32
   	bset	#6,@s7:32
   	bset	#6,@s8:32
   	bset	#6,@s9:32
   	bset	#6,@s10:32
	    
   	band	#6,@s1:32
   	band	#6,@s2:32
   	band	#6,@s3:32
   	band	#6,@s4:32
   	band	#6,@s5:32
   	band	#6,@s6:32
  	band	#6,@s7:32
   	band	#6,@s8:32
   	band	#6,@s9:32
   	band	#6,@s10:32
    
    	.equ	s1,0
    	.equ	s2,0x7fff
    	.equ	s3,0x8000
    	.equ	s4,0xff00
    	.equ	s5,0xffff00
    	.equ	s6,0xffff7fff
    	.equ	s7,0xffff8000
    	.equ	s8,0xfffffeff
    	.equ	s9,0xffffff00
    	.equ	s10,0xffffffff
    	
    	.end
    	
