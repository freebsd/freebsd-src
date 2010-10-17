; Relaxation is possible from @aa:32 to @aa:16 for following instructions
; ldc.w @@aa:32,ccr 
; stc.w ccr,@@aa:32
; ldc.w @aa:32,exr
; stc.w exr,@aa:32
	.h8300s
    	.globl	_start
;
; Relaxation of aa:32
;
    _start:
    	ldc  @s1:32,ccr
	ldc  @s2:32,ccr
	ldc  @s3:32,ccr
	ldc  @s4:32,ccr
	ldc  @s5:32,ccr
	ldc  @s6:32,ccr
	ldc  @s7:32,ccr
	ldc  @s8:32,ccr
	ldc  @s9:32,ccr
	ldc  @s10:32,ccr

	stc  ccr,@s1:32
	stc  ccr,@s2:32
	stc  ccr,@s3:32
	stc  ccr,@s4:32
	stc  ccr,@s5:32
	stc  ccr,@s6:32
	stc  ccr,@s7:32
	stc  ccr,@s8:32
	stc  ccr,@s9:32
	stc  ccr,@s10:32
	
    	ldc  @s1:32,exr
	ldc  @s2:32,exr
	ldc  @s3:32,exr
	ldc  @s4:32,exr
	ldc  @s5:32,exr
	ldc  @s6:32,exr
	ldc  @s7:32,exr
	ldc  @s8:32,exr
	ldc  @s9:32,exr
	ldc  @s10:32,exr

	stc  exr,@s1:32
	stc  exr,@s2:32
	stc  exr,@s3:32
	stc  exr,@s4:32
	stc  exr,@s5:32
	stc  exr,@s6:32
	stc  exr,@s7:32
	stc  exr,@s8:32
	stc  exr,@s9:32
	stc  exr,@s10:32
    	
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
