GAS LISTING .*


   1                                	# test handling of whitespace, and upper-case
   2                                	.TeXt 
   3 0000 36                         		ss 
   4 0001 8803                       		mov % al , \( % ebx \) 
   5 0003 C705D711 00007B00 0000     	        mOvl \$ 123 , 4567 
   6 000d 678A787B                   	 ADDr16	mov 123 \( % bx , % si , 1 \) , % bh 
   7 0011 FFE0                       		jmp \* % eax 
   8 0013 6626FF23                   	foo:	jmpw % es : \* \( % ebx \)	
   9                                	 
  10 0017 A0500000 00                		mov \( 0x8 \* 0Xa \) , % al 
  11 001c B020                       		mov \$ \( 8 \* 4 \) , % al 
  12 001e B713                       		mov \$ foo , % bH 
  13 0020 B713                       		movb \$ foo , % BH 
  14                                		
  15                                	.CODE16	
  16 0022 66B81300 0000              		Mov \$ foo , %eAx	
  17                                	.Code32 
  18 0028 66B81300                   		mov \$ foo , %ax	
  19                                	
  20 002c D9C9                       		fxch   %st  \(  1  \) 
  21 002e D9C9                       		fxch   %           st\(1\)
