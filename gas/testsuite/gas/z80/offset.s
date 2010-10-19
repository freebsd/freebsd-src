;;;  various instructions involving offsets

	.section .text
	.org 0
10:
	jr 1f
	inc (ix+5)
	dec (iy-1)
	ld a,(ix-128)
	ld (iy+127),a
	djnz 10b
	jr z,2f
	jr c,3f
2:
	jr nz,3f
	jr nc,2b
3:
	ld (ix+34),9
	ld (iy-34),-9
	rr (ix+55)
	rl (iy-55)
	.balign 0x80
1:	
