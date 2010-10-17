# 1 "libgcc1.S"
;; libgcc1 routines for the Hitachi H8/300 cpu.
;; Contributed by Steve Chamberlain.
;; sac@cygnus.com
	.section .text
	.align 2
	.global ___cmpsi2
___cmpsi2:
	cmp.w	r2 ,r0 
	bne	.L2
	cmp.w	r3 ,r1 
	bne	.L2
	mov.w	#1,r0 
	rts
.L2:
	cmp.w	r0 ,r2 
	bgt	.L4
	bne	.L3
	cmp.w	r1 ,r3 
	bls	.L3
.L4:
	sub.w	r0 ,r0 
	rts
.L3:
	mov.w	#2,r0 
.L5:
	rts
	.end
