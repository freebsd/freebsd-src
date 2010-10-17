;;; Test 68HC12 FAR trampoline generation
;;; 2 trampolines are generated:
;;; - one for '_far_bar'
;;; - one for '_far_foo'
;;; 'far_no_tramp' does not have any trampoline generated.
;;;
	.sect .text
	.globl _start
_start:
start:	
	lds	#stack-1
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
	call	_far_no_tramp	; No trampoline generated for _far_no_tramp
	clra
	clrb
	wai
fail:
	ldd	#1
	wai
	bra	start

	.sect .bank1,"ax"
	.globl _far_bar
	.far _far_bar		; Must mark symbol as far
_far_bar:
	jsr	local_bank1
	xgdx
	rtc

local_bank1:
	rts

	.sect .bank2,"ax"
	.globl _far_foo
	.far _far_foo
_far_foo:
	jsr	local_bank2
	rtc

local_bank2:
	rts

	.sect .bank3,"ax"
	.globl _far_no_tramp
	.far _far_no_tramp
_far_no_tramp:
	jsr	local_bank3
	rtc

local_bank3:
	rts

	.sect .text
	.globl __far_trampoline
__far_trampoline:
	movb	0,sp, 2,sp	; Copy page register below the caller's return
	leas	2,sp		; address.
	jmp	0,y		; We have a 'call/rtc' stack layout now
				; and can jump to the far handler
				; (whose memory bank is mapped due to the
				; call to the trampoline).

	.sect .bss
	.skip 100
stack:

