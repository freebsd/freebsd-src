# Test for correct generation of 68HC12 specific insns.

	.sect .text

;; Test the call insns
call_test:
	call	_foo		; 24-bit reloc
	call	_foo,1		; 16-bit reloc, immediate page specification
	call	_foo,%page(foo_page)	; 16-bit reloc and 8-bit page reloc
	call	0,x,3		; 8-bit page reloc
	call	4,y,12
	call	7,sp,13
	call	12,x,%page(foo_page)	; 8-bit page reloc
	call	4,y,%page(foo_page)
	call	7,sp,%page(foo_page)
	call	[d,x]		; No reloc
	ldab	[32767,sp]
	call	[2048,sp]	; No reloc
	call	[_foo,x]	; 16-bit reloc
	rtc
;; Test special insn
special_test:
	emacs	_foo	; Wonderful, Emacs as a single instruction!

;; Min instruction
	maxa	0,x
	maxa	819,y
	maxa	[d,x]
	maxa	[_foo,x]

	maxm	0,x
	maxm	819,y
	maxm	[d,x]
	maxm	[_foo,x]

	emaxd	0,x
	emaxd	819,y
	emaxd	[d,x]
	emaxd	[_foo,x]

	emaxm	0,x
	emaxm	819,y
	emaxm	[d,x]
	emaxm	[_foo,x]

;; Min instruction
	mina	0,x
	mina	819,y
	mina	[d,x]
	mina	[_foo,x]

	minm	0,x
	minm	819,y
	minm	[d,x]
	minm	[_foo,x]

	emind	0,x
	emind	819,y
	emind	[d,x]
	emind	[_foo,x]
;; Mul
	emul
	emuls
	etbl	3,x
	etbl	4,pc

;;
	rev
	revw
	wav
;;
