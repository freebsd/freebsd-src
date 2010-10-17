! Check that immediate operands with expressions with differences between
! local symbols work for other than 16-bit operands.

	.text
	.mode SHmedia
start:
	addi r50,.Lab500 - .Lab1,r40
	addi r50,-(.Lab500 - .Lab1),r40
	addi r50,(.Lab1000 - .Lab1)/2,r40
	addi r50,(.Lab4000 - .Lab1)/8,r40
	addi r50,-(.Lab1000 - .Lab1)/2,r40
	addi r50,-(.Lab4000 - .Lab1)/8,r40
	addi r50,.Lab500 - .Lab1 + 1,r40
	addi r50,.Lab500 - .Lab1 + 2,r40
	addi r50,-(.Lab500 - .Lab1 + 1),r40
	addi r50,-(.Lab500 - .Lab1 + 2),r40
	ld.uw r30,.Lab1000 - .Lab1,r40
	ld.uw r30,.Lab500 - .Lab1 - 2,r40
	ld.uw r30,.Lab500 - .Lab1 + 2,r40
	ld.uw r50,(.Lab2000 - .Lab1)/2,r20
	ld.uw r30,-(.Lab1000 - .Lab1),r40
	ld.uw r30,-(.Lab500 - .Lab1 - 2),r40
	ld.uw r30,-(.Lab500 - .Lab1 + 2),r40
	ld.uw r50,-(.Lab2000 - .Lab1)/2,r20
	ld.l r50,.Lab2000 - .Lab1,r20
	ld.l r50,.Lab2000 - .Lab1 + 4,r20
	ld.l r50,.Lab2000 - .Lab1 - 4,r20
	ld.l r50,(.Lab4000 - .Lab1)/2,r20
	ld.l r50,(.Lab4000 - .Lab1)/2 + 4,r20
	ld.l r50,(.Lab4000 - .Lab1)/2 - 4,r20
	ld.l r50,-(.Lab2000 - .Lab1),r20
	ld.l r50,-(.Lab2000 - .Lab1 + 4),r20
	ld.l r50,-(.Lab2000 - .Lab1 - 4),r20
	ld.l r50,-(.Lab4000 - .Lab1)/2,r20
	ld.l r50,-(.Lab4000 - .Lab1)/2 + 4,r20
	ld.l r50,-(.Lab4000 - .Lab1)/2 - 4,r20
	nop
	addi r50,.Lab500t - .Lab1t,r40
	addi r50,(.Lab1000t - .Lab1t)/2,r40
	addi r50,(.Lab4000t - .Lab1t)/8,r40
	addi r50,.Lab500t - .Lab1t + 1,r40
	addi r50,.Lab500t - .Lab1t + 2,r40
	ld.uw r30,.Lab1000t - .Lab1t,r40
	ld.uw r30,.Lab500t - .Lab1t - 2,r40
	ld.uw r30,.Lab500t - .Lab1t + 2,r40
	ld.uw r50,(.Lab2000t - .Lab1t)/2,r20
	ld.l r50,.Lab2000t - .Lab1t,r20
	ld.l r50,.Lab2000t - .Lab1t + 4,r20
	ld.l r50,.Lab2000t - .Lab1t - 4,r20
	addi r50,.Lab500t - .Lab1t,r40
	addi r50,-((.Lab1000t - .Lab1t)/2),r40
	addi r50,-((.Lab4000t - .Lab1t)/8),r40
	addi r50,-(.Lab500t - .Lab1t + 1),r40
	addi r50,-(.Lab500t - .Lab1t + 2),r40
	ld.uw r30,-(.Lab1000t - .Lab1t),r40
	ld.uw r30,-(.Lab500t - .Lab1t - 2),r40
	ld.uw r30,-(.Lab500t - .Lab1t + 2),r40
	ld.uw r50,-((.Lab2000t - .Lab1t)/2),r20
	ld.l r50,-(.Lab2000t - .Lab1t),r20
	ld.l r50,-(.Lab2000t - .Lab1t + 4),r20
	ld.l r50,-(.Lab2000t - .Lab1t - 4),r20
	nop
	.long 0
.Lab1t:
	.zero 500,0
.Lab500t:
	.zero 500,0
.Lab1000t:
	.zero 1000,0
.Lab2000t:
	.zero 2000,0
.Lab4000t:

	.data
	.long 0
.Lab1:
	.zero 500,0
.Lab500:
	.zero 500,0
.Lab1000:
	.zero 1000,0
.Lab2000:
	.zero 2000,0
.Lab4000:
	.long 0
