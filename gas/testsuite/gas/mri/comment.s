	xref label
	xdef	ROM,RAM
	* this is a comment

	dc.l	label	loop if we haven't reach end yet
	
ROM				EQU	$00000001	* word wide
RAM				EQU	$00000002	word wide
	dc.l	RAM
	dc.l	0	,really,a,comment
; a comment
 ; another comment
  ; another comment
