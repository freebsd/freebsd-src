; Flag settings; clearf, setf

 .text
 .syntax no_register_prefix
start:
 nop			; So we get it tested too -- and it makes the
			; size of the code a 32-bit multiple, so
			; the end of disassembly does not show zeros.
 ax
 setf	deixnzvc	; old names
 clearf	deixnzvc	; old names
 ei
 di
 setf	dxnc		; old names
 clearf	dxnc		; old names
 setf			; empty list
 clearf			; empty list
; For each flag.  Note that the disassembly will show macros for
; some.
 setf	c
 clearf	c
 setf	v
 clearf	v
 setf	z
 clearf	z
 setf	n
 clearf	n
 setf	x
 clearf	x
 setf	i
 clearf	i
 setf	e
 clearf	e
 setf	d
 clearf	d
; Two from same group, and switch order.
 clearf vc
 setf vc
 clearf ix
 setf ix
 clearf cv
 setf cv
 clearf xi
 setf xi
; Two from different groups, and switch order.
 clearf in
 setf   in
 clearf ni
 setf   ni
; Four in same group, and switch order.
 clearf nvxc
 setf nvxc
 clearf vncx
 setf vncx
 clearf dxei
 setf dxei
 clearf xide
 setf xide
; Four in different groups, and switch order.
 clearf exnv
 setf exnv
 clearf xvne
 setf xvne
; FIXME: Put tests for new flag-names here, (not above.
; The new names in ETRAX 100, just some assortment of the above.
 setf	mbixnzvc
 clearf	bmixnzvc
 setf	b
 clearf	b
 setf	m
 clearf	m
 clearf mxbi
 setf dxbi
 clearf ximb
 setf ximb
 clearf bxnv
 setf bxnv
 clearf xvnb
 setf xvnb
end:
