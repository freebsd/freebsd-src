*
* String substitution symbols
*			
	; if no quotes, interpret as subsymbol
	; if quotes, interpret as string, and do forced substitution
	.sslist
	.asg	value,SYMBOL
	.asg	SYMBOL,SYMBOL1

	.global label, x
	.word	x

* Substitution symbol functions	
label:	.word	$symlen(SYMBOL)		; 5, substitutes string for symbol
	.word	$symlen(":SYMBOL:")	; 5, forced substitution
	.word	$symlen("SYMBOL")	; 6, uses string directly

	.word	$symcmp(SYMBOL,"value")		; 0
	
	; requires 2nd arg to be a character; zero if not found
	.word	$firstch(":SYMBOL:",'a')	; 2
	.word	$lastch(SYMBOL,'a')		; 2
	
	.word	$isdefed(SYMBOL)		; 0 (value not in symtab)
	.word	$isdefed("label")		; 1 (string contents in symtab)
	.word	$isdefed("unknown")		; 0

	.asg	"1,2,3", list
	; both args must be identifiers
	.word	$ismember(SYMBOL,list)		; 1
	.word	SYMBOL				; now 1
	.word	list				; now 2,3

	.word	$iscons("010b")			; 1
	.word	$iscons("11111111B")		; 1
	.word	$iscons("011")			; 2 (5 -- TI bug)
	.word	$iscons("0x10")			; 3 (0 -- TI bug)
	.word	$iscons("'a'")			; 4
	.word	$iscons(SYMBOL)			; 5 ("1")
	.word	$iscons("SYMBOL")		; 0
	
	.word	$isname(SYMBOL)			; 0

	.word	$isreg(SYMBOL)			; 0
	.word	$isreg("AR0")			;
;	.word	$isreg("AG")			; should be 0, but we always 
						; use mmregs 
	.mmregs
x       .word   $isreg("AG")                    ; 1 if .mmregs, 0 otherwise
tag	.struct 10
	.word	1
	.endstruct
	.word	$structsz(tag)
	.word	$structacc(tag)			; this op is unspecified
	.end
