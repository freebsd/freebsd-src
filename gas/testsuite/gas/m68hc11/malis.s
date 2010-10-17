;;
;; This file verifies the compliance with the Motorola specification:
;; 
;; MOTOROLA STANDARDS
;; Document #1001, Version 1.0
;; SPECIFICATION FOR Motorola 8- and 16-Bit ASSEMBLY LANGUAGE INPUT STANDARD
;; 26, October 1999
;;
;; Available at:
;; 
;; http://www.mcu.motsps.com/dev_tools/hc12/eabi/m8-16alis.pdf
;;
;; Lines starting with '#' represent instructions that fail in GAS.
;;
;;
;; Section 8.2 INPUTS
	;; Validated within the whole file
	
;; Section 8.2.1 Character Set
	;; TBD

;; Section 8.2.2 Assembly Language Statement
	;; Validated within the whole file

;; Section 8.2.3 Comments
	; Motorola comment
	;; This file is full of comments

;; Section 8.2.5 Location Counter
	section .text

_start:
L0:	*			; L0 set to 0 (relative to text)
	ldaa	1,x
L1:	equ	*		; L1 set to 2 (relative to text)

;; Section 8.2.6 Sections
	section .data
	section .text
	section empty
	section .text

;; Section 8.2.7 Expressions
L2:	equ	23		; Absolute = 0x17
L3:	equ	L0-23		; Simple relocatable

;; Section 8.2.7.1 Primary Expression
L4:	equ	45		; Numeric  = 0x2d
L5:	equ	L0		; Symbolic

;; Section 8.2.7.2 Absolute Expression
L_txt:	ldaa	#44
L_txt2:
L6:	equ	-L4		; unary expr		0xffffffd3
L7:	equ	L6+1000		; binary expr		0x03bb
L8:	equ	L6-12		;			0xffffffc7
L9:	equ	L_txt2-L_txt	; reloc - reloc		2 = sizeof(ldaa #44)

;; Section 8.2.7.3 Simple Relocatable Expressions
L10:	equ	_start		; symbol		0 + text
L11:	equ	L10+23		; reloc+abs		0x17 + text
L12:	equ	L11-4		; reloc-abs		0x13 + text
L13:	equ	L12+L9		; reloc+abs		0x15 + text

;; Section 8.2.8 Symbols
	section .text
Text_Symbol:
	ldx	#Data_Symbol

	section .data
Data_Symbol:

;; Section 8.2.8.1 Labels
L_label_shall_be_significant_to_at_least_32_chars:
	dc.b	1
L_label_lower:			; Labels are case sensitive
	dc.b	2
L_Label_Lower:
	dc.b	3

;; Section 8.2.9 Constants
;
;	Section 8.2.9.1	Decimal Constants
	section .text
L_constant:
	ldaa	#123		; -> ldaa #0x7b
	ldaa	#-23		; -> ldaa #0xe9
	
;;	Section 8.2.9.2	Binary Constants
	ldab	#%10001010	; -> ldab #0x8A
	ldab	#%0111		; -> ldab #0x07

;;	Section 8.2.9.3	Octal Constants
	ldaa	#@74		; -> ldaa 0x3c
	ldaa	#@377		; -> ldaa 0xff

;;	Section 8.2.9.4	Hexadecimal Constants
	ldaa	#$ae		; -> ldaa 0xae
	ldaa	#$B2		; -> ldaa 0xb2

;;	Section 8.2.9.5	String Constants
	section	.data
#	ascii	'"Single quote string"'
	ascii	"'Double quote string'"

;;	Section 8.2.9.6 Floating Point Constants
;;	No specification
L_float:	float	3.241592e-2

;;	Section 8.2.10	Operators
	section .text
L_operator:
	ldx	#(((1<<3)&(1<<3)|2)<<4)+(4*4-1)
	ldx	#(L2>=23)&1-(L2<=23)&1+(L2==23)&1 ; -> ldx #1
	ldx	#(L2>23)&1-(L2<23)&1+(L2==23)&1   ; -> ldx #0
	ldx	#1-1+1-1+1-1
	ldab	#~L4		; -> ldab #0xd2
#	ldab	#<_start	; force to 8-bit
#	ldx	#>_start	; force to 16-bit
#	ldab	#page(_start)	; 68HC12 page number of symbol

;; Section 8.2.11 Instructions
;;	Defined by other tests

;; Section 8.2.12 Assembler Directives
;; 
;; Section 8.2.12.1 Conditional Directives
;;
# The specification says we are allowed to have spaces in expressions.
# This does not work with GAS in mri mode, the instruction 'if L2 < 24'
# is in fact treated as 'if L2'.
L_if:
	if L2<24		; true
	ldx	#1		; -> ldx #1
	else
	ldx	#2
	endif
	if L2<23||L2>23||L2==22+1 ; true
	if L2<23		; false
	ldaa	#0
	endif
	if L2>23		; false
	ldaa	#1
	endif
	if L2 == 23		; true
	ldaa	#L2+8		; -> ldaa #31
	endif
	if L2+2<23+2		; false
	if L2+4>23+4
	ldaa	#1
	elseif L2==23
	ldaa	#2
	else
	ldaa	#3
	endif
	elseif L2==23		; true
	ldaa	#4		; -> ldaa #4
	else
	ldaa	#5
	endif
	endif
	ifdef L1		; true
	ldx	#23		; -> ldx #0x17
	endif
	ifndef L_undef		; true
	ldx	#4		; -> ldx #4
	endif

;;	Section 8.2.12.2 Define Constant -dc
	section	.data
L_const_data:
	dc.b	(1<<3)|2	; 0x0a
	dc.w	(1<<9)+2	; 0x02 0x02
	dc.l	$12345678
	dc.b	10
	dc.l	(1<<9)*(3<<12)
#	dc.b	"Hello"
#	dc.w	"World"
#	dc.l	"!!!"

;;	Section 8.2.12.3 Define Constant Block -dcb
	dcb.b	3,2
	dcb.w	2,$55AA
	dcb.l	2,$12345678
	dcb.b	10,2
	dcb.w	5,$55AA
	dcb.l	2,$12345678
#	dcb.b	4, 12		; Fails in GAS due to space

;;	Section 8.2.12.4 Define Storage - ds
	ds.b	4
	ds.w	2
	ds.l	1
	ds	2

;;	Section 8.2.12.5 Equate Symbol Value - equ
L_equ1:	equ	(1<<8)+3
L_equ2:	equ	L_equ1*3
L_equ3:	equ	L_equ2-L_equ1

;;	Section 8.2.12.6 Include File - include
#	include 'malis-include.s'
#	include "malis-include.s"
	include malis-include.s
	include malis-include.s

;;	Section 8.2.12.7 Origin - org
	section .text
#	org	$100
	ldaa	#23
#	org	$200
	staa	24
#	org	$0
	rts

;;	Section 8.2.12.8 Define Section - section
	section .text
	ldaa	23

	section .data
	dc.b	23

;;	Section 8.2.12.9 Set Value of Symbol - set
s1:	set	3
s1:	set	4
s2:	set	s1+2
s3:	set	s2+4

;; Section 8.2.12.10 External Symbol Definition - xdef
	xdef	s1
	xdef	s2
	xdef	entry

;; Section 8.2.12.11 External Symbol Reference - xref
	section	.text
	xref	printf
	xrefb	write
entry:
	rts

