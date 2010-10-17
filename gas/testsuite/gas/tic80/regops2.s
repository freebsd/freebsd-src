	dcachec	r8(r10:m)		; Register form (modified)
	dcachec	4(r10:m)		; Short Immediate form (positive offset) (modified)
	dcachec	-4(r10:m)		; Short Immediate form (negative offset) (modified)
	dcachec	0x12345678(r10:m)	; Long Immediate form (positive offset) (modified)
	dcachec	0xDEADBEEF(r10:m)	; Long Immediate form (negative offset) (modified)
	dcachef	r8(r10:m)		; Register form (modified)
	dcachef	4(r10:m)		; Short Immediate form (positive offset) (modified)
	dcachef	-4(r10:m)		; Short Immediate form (negative offset) (modified)
	dcachef	0x12345678(r10:m)	; Long Immediate form (positive offset) (modified)
	dcachef	0xDEADBEEF(r10:m)	; Long Immediate form (negative offset) (modified)
	dld.b	r4(r6:m),r8		; Register form
	dld.h	r4(r6:m),r8		; Register form
	dld	r4(r6:m),r8		; Register form
	dld.d	r4(r6:m),r8		; Register form
	dld.b	0xE0000000(r6:m),r8	; Long Immediate form
	dld.h	0xE0000000(r6:m),r8	; Long Immediate form
	dld	0xE0000000(r6:m),r8	; Long Immediate form
	dld.d	0xE0000000(r6:m),r8	; Long Immediate form
	dld.ub	r4(r6:m),r8		; Register form
	dld.uh	r4(r6:m),r8		; Register form
	dld.ub	0xE0000000(r6:m),r8	; Long Immediate form
	dld.uh	0xE0000000(r6:m),r8	; Long Immediate form
	dst.b	r4(r6:m),r8		; Register form
	dst.h	r4(r6:m),r8		; Register form
	dst	r4(r6:m),r8		; Register form
	dst.d	r4(r6:m),r8		; Register form
	dst.b	0xE0000000(r6:m),r8	; Long Immediate form
	dst.h	0xE0000000(r6:m),r8	; Long Immediate form
	dst	0xE0000000(r6:m),r8	; Long Immediate form
	dst.d	0xE0000000(r6:m),r8	; Long Immediate form
	ld.b	r4(r6:m),r8		; Register form
	ld.h	r4(r6:m),r8		; Register form
	ld	r4(r6:m),r8		; Register form
	ld.d	r4(r6:m),r8		; Register form
	ld.b	-16(r6:m),r8		; Short Immediate form
	ld.h	-16(r6:m),r8		; Short Immediate form
	ld	-16(r6:m),r8		; Short Immediate form
	ld.d	-16(r6:m),r8		; Short Immediate form
	ld.b	0xE0000000(r6:m),r8	; Long Immediate form
	ld.h	0xE0000000(r6:m),r8	; Long Immediate form
	ld	0xE0000000(r6:m),r8	; Long Immediate form
	ld.d	0xE0000000(r6:m),r8	; Long Immediate form
	ld.ub	r4(r6:m),r8		; Register form
	ld.uh	r4(r6:m),r8		; Register form
	ld.ub	-16(r6:m),r8		; Short Immediate form
	ld.uh	-16(r6:m),r8		; Short Immediate form
	ld.ub	0xE0000000(r6:m),r8	; Long Immediate form
	ld.uh	0xE0000000(r6:m),r8	; Long Immediate form
	st.b	r4(r6:m),r8		; Register form
	st.h	r4(r6:m),r8		; Register form
	st	r4(r6:m),r8		; Register form
	st.d	r4(r6:m),r8		; Register form
	st.b	-256(r6:m),r8		; Short Immediate form
	st.h	-256(r6:m),r8		; Short Immediate form
	st	-256(r6:m),r8		; Short Immediate form
	st.d	-256(r6:m),r8		; Short Immediate form
	st.b	0xE0000000(r6:m),r8	; Long Immediate form
	st.h	0xE0000000(r6:m),r8	; Long Immediate form
	st	0xE0000000(r6:m),r8	; Long Immediate form
	st.d	0xE0000000(r6:m),r8	; Long Immediate form
