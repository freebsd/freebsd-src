;;; Test 68HC11 linker relaxation (group relax)
;;; 
	.sect .text
	.globl _start
_start:
;;;
;;; The following group of instructions are adjusted.
;;;
	.relax	L1x
	ldx	#table
	bset	0,x #4
L1x:
	.relax	L1y
	ldy	#table
	bset	0,y #4
L1y:
	.relax	L2x
	ldx	#table+3
	bset	0,x #4
	bset	1,x #8
L2x:
	.relax	L2y
	ldy	#table+3
	bset	0,y #4
	bset	1,y #8
L2y:
	.relax	L3x
	ldx	#table+6
	bset	0,x #4
	bset	1,x #8
	bset	2,x #12
	bset	3,x #12
	bset	4,x #12
	bset	5,x #12
L3x:
	.relax	L3y
	ldy	#table+6
	bset	0,y #4
	bset	1,y #8
	bset	2,y #12
	bset	3,y #12
	bset	4,y #12
	bset	5,y #12
L3y:
	;; Next branch is always relative.  It must be adjusted while
	;; above instructions are relaxed.
	bra	_start
;;;
;;; This group has the first two bset insn relaxable while the
;;; others are not.  The ldx/ldy must not be removed.
;;; 
	.relax	L4x
	ldx	#table+0xfe
	bset	0,x #4
	bset	1,x #8
	bset	2,x #12
	bset	3,x #12
	bset	4,x #12
	bset	5,x #12
L4x:
	.relax	L4y
	ldy	#table+0xfe
	bset	0,y #4
	bset	1,y #8
	bset	2,y #12
	bset	3,y #12
	bset	4,y #12
	bset	5,y #12
L4y:
;;;
;;; Relax group for bclr
;;; 
	.relax	L5x
	ldx	#table+10
	bclr	0,x #4
	bclr	1,x #8
L5x:
	.relax	L5y
	ldy	#table+16
	bclr	10,y #4
	bclr	11,y #8
L5y:
;;;
;;; Relax group for brset (with backward branch)
;;; 
	.relax	L6x
	ldx	#table+8
	brset	0,x #4 L5y
L6x:
	.relax	L7x
	ldy	#table+8
	brset	0,y #4 L6x
L7x:
;;;
;;; Relax group for brset (with forward branch)
;;; 
	.relax	L8x
	ldx	#table+8
	brset	0,x #4 brend
L8x:
	.relax	L8y
	ldy	#table+8
	brset	0,y #4 brend
L8y:
;;;
;;; Relax group for brclr (with backward branch)
;;; 
	.relax	L9x
	ldx	#table+8
	brclr	0,x #4 L8y
L9x:
	.relax	L9y
	ldy	#table+8
	brclr	0,y #4 L9x
L9y:
;;;
;;; Relax group for brclr (with forward branch)
;;; 
	.relax	L10x
	ldx	#table+8
	brclr	0,x #4 brend
L10x:
	.relax	L10y
	ldy	#table+8
	brclr	0,y #4 brend
L10y:
	nop
brend:
;;;
;;; The following are wrong use of .relax groups.
;;;
	.relax	w1
w1:
	.relax	w2
	bset	0,x #4
w2:
	.relax w3
	ldx	#table
w3:
	.relax w4
	ldy	#table+8
w4:
	.relax w5
	rts
w5:
;;;
;;; Next insn is not in a .relax group
	ldx	#table
	bset	0,x #5
	bra	_start
	rts

	.sect .page0
	.globl table
table:	.long 0
table4:	.long 0
table8:	.long 0
	.skip	10
end_table:
	.long 0

