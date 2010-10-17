* local labels
* two forms, $[0-9] and label? are allowed	
* Local labels are undefined/reset in one of four ways:
* .newblock
* changing sections		
* entering an include file
* leaving an include file			
        .global addra, addrb, addrc
label1:	ld	addra,a
	sub	addrb,a
	bc	$1, alt			; generates frag! 
	ld	addrb, a
	b	$2
$1:	ld	addra,a	
$2	add	addrc,a
	.newblock
	bc	$1,alt
	stl	a, addrc
$1	nop	
	
* #1, First definition of local label 'lab'	
	nop
lab?	add	#1,a			; reports as line 17?
	b	lab?	
* #2, Included file also defines local label 'lab'	
	.copy labels.inc
* #3, Next definition; exit from .copy clears all locals	
lab?	add	#3,a			; reports as line 22?
	b	lab?
* #4, Next definition is within macro; supersedes previous definition while
* within the macro 
mac	.macro
lab?	add	#4,a			; line 31?
	b	lab?
	.endm
* Macro invocation
	mac
* This reference should resolve to definition #3
after_macro:	
	b	lab?
* Section change clears all definitions; it's a CODE section if we see insns
	.sect	new_section
	nop
lab?	add	#5,a
	nop
	nop
	b	lab?
* Newblock directive clears local labels
	.newblock
lab?	add	#6,a
	nop
	nop
	b	lab?				
	.end

