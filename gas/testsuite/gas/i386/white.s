# test handling of whitespace, and upper-case
.TeXt 
	ss 
	mov % al , ( % ebx ) 
        mOvl $ 123 , 4567 
 ADDr16	mov 123 ( % bx , % si , 1 ) , % bh 
	jmp * % eax 
foo:	jmpw % es : * ( % ebx )	
 
	mov ( 0x8 * 0Xa ) , % al 
	mov $ ( 8 * 4 ) , % al 
	mov $ foo , % bH 
	movb $ foo , % BH 
	
.CODE16	
	Mov $ foo , %eAx	
.Code32 
	mov $ foo , %ax	

	fxch   %st  (  1  ) 
	fxch   %           st(1)
