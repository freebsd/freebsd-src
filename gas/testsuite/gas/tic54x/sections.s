*	
* Various sections directives
* .bss, .data, .sect, .text, .usect
* .align, .space, .bes
*	
        ; default section (should be .text)
	.word	0x1234		; this should be put in .text
	
        ; initialized data
	.data
	.global coeff
coeff	.word	011h,022h,033h
	
        ; uninitialized data
	.global B1, buffer
	.bss	buffer, 10
B1:	.usect	".bss", 10	; alocate 10 words	

        ; more initialized data in .data
	.global ptr
ptr	.word	0123h

        ; .text section
	.text
	.global add, aloop
add:	ld	0fh,a
aloop:	sub	#1,a
	bc	aloop,ageq		

        ; more initialized data into .data
	.data
	.global ivals
ivals	.word	0aah, 0bbh, 0cch

        ; define another section for more variables
	.global var2, inbuf, align2
var2	.usect	"newvars", 1	; with quotes
inbuf	.usect	newvars, 7, 1	; w/o quotes, block 7 words
align2	.usect	newvars, 15, ,1	; 15 words aligned

        ; more code
	.text
	.global mpy, mloop
mpy:	ld	0ah,b
mloop:	mpy	#0ah,b
	bc	mloop,bnov
	.global space, bes, spacep, besp
space:	.space	64			; points to first word of block
bes:	.bes	64			; points to last word of block
spacep:	.word	space
besp:	.word	bes	
	.global pk1, pk2, pk3, endpk1, endpk2, endpk3
pk1:	.space	20
endpk1: .space	12	
pk2:	.bes	20
endpk2	.bes	12		
pk3:	.space	20
endpk3:	.bes	12		
        ; named initialized section (CODE)
	.sect	"vectors"
        nop
	nop
	
        ; named, initialized section, no quotes  (DATA)
	.sect clink
	.clink			; mark section clink as STYP_CLINK
	.word	022h, 044h
	
	.sect	"blksect"  ; (DATA)
	.word	0x1234,0x4321
	.sblock	"blksect", vectors ; set block flag on blksect and vectors

	.end
