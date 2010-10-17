;;; Test 68HC11 FAR trampoline generation
;;; 2 trampolines are generated:
;;; - one for '_far_bar'
;;; - one for '_far_foo'
;;; 'far_no_tramp' does not have any trampoline generated.
;;;
	.sect .text
	.globl _start
_start:
start:	
	lds	#stack
	ldx	#0xabcd
	pshx
	ldd	#0x1234
	ldx	#0x5678
	bsr	_far_bar	; Call to trampoline generated code
	cpx	#0x1234
	bne	fail		; X and D preserved (swapped by _far_bar)
	cpd	#0x5678
	bne	fail
	pulx
	cpx	#0xabcd		; Stack parameter preserved
	bne	fail
	ldd	#_far_foo	; Get address of trampoline handler
	xgdx
	jsr	0,x
	ldd	#_far_bar	; Likewise (unique trampoline check)
	xgdy
	jsr	0,y
	ldaa	#%page(_far_no_tramp)
	ldy	#%addr(_far_no_tramp)
	bsr	__call_a16	; No trampoline generated for _far_no_tramp
	clra
	clrb
	wai
fail:
	ldd	#1
	wai
	bra	start
	.global	__return
__return:
	ins
	rts

	.sect .bank1,"ax"
	.globl _far_bar
	.far _far_bar		; Must mark symbol as far
_far_bar:
	jsr	local_bank1
	xgdx
	jmp	__return

local_bank1:
	rts

	.sect .bank2,"ax"
	.globl _far_foo
	.far _far_foo
_far_foo:
	jsr	local_bank2
	jmp	__return

local_bank2:
	rts

	.sect .bank3,"ax"
	.globl _far_no_tramp
	.far _far_no_tramp
_far_no_tramp:
	jsr	local_bank3
	jmp	__return

local_bank3:
	rts

	.sect .text
	.globl __far_trampoline
__far_trampoline:
	psha				; (2) Save function parameter (high)
	;; <Read current page in A>
	psha				; (2)
	;; <Set currenge page from B>
	pshx				; (4)
	tsx				; (3)
	ldab	4,x			; (4) Restore function parameter (low)
	ldaa	2,x			; (4) Get saved page number
	staa	4,x			; (4) Save it below return PC
	pulx				; (5)
	pula				; (3)
	pula				; (3) Restore function parameter (high)
	jmp	0,y			; (4)

	.globl __call_a16
__call_a16:
	;; xgdx				; (3)
	;; <Read current page in A>	; (3) ldaa _current_page
	psha				; (2)
	;; <Set current page from B>	; (4) staa _current_page
	;; xgdx				; (3)
	jmp 0,y				; (4)

	.sect .page0
	.skip 100
stack:

