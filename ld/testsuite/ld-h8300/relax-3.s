	.h8300s
	.globl	_start
_start:
	# s3-s6 aren't valid 16-bit addresses.
	mov.b	@s1:16,r0l
	mov.b	@s2:16,r0l
	mov.b	@s7:16,r0l
	mov.b	@s8:16,r0l
	mov.b	@s9:16,r0l
	mov.b	@s10:16,r0l

	mov.b	@s1:32,r0l
	mov.b	@s2:32,r0l
	mov.b	@s3:32,r0l
	mov.b	@s4:32,r0l
	mov.b	@s5:32,r0l
	mov.b	@s6:32,r0l
	mov.b	@s7:32,r0l
	mov.b	@s8:32,r0l
	mov.b	@s9:32,r0l
	mov.b	@s10:32,r0l

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
