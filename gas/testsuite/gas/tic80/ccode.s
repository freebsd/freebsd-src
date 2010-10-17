;; Test that all the predefined symbol names for the condition
;; codes are properly accepted and translated to numeric values.
;;  Also verifies that they are disassembled correctly as symbolics.

	bcnd.a	r7,r5,nev.b		; 00000
	bcnd.a	r7,r5,gt0.b		; 00001
	bcnd.a	r7,r5,eq0.b		; 00010
	bcnd.a	r7,r5,ge0.b		; 00011
	bcnd.a	r7,r5,lt0.b		; 00100
	bcnd.a	r7,r5,ne0.b		; 00101
	bcnd.a	r7,r5,le0.b		; 00110
	bcnd.a	r7,r5,alw.b		; 00111

	bcnd.a	r7,r5,nev.h		; 01000
	bcnd.a	r7,r5,gt0.h		; 01001
	bcnd.a	r7,r5,eq0.h		; 01010
	bcnd.a	r7,r5,ge0.h		; 01011
	bcnd.a	r7,r5,lt0.h		; 01100
	bcnd.a	r7,r5,ne0.h		; 01101
	bcnd.a	r7,r5,le0.h		; 01110
	bcnd.a	r7,r5,alw.h		; 01111

	bcnd.a	r7,r5,nev.w		; 10000
	bcnd.a	r7,r5,gt0.w		; 10001
	bcnd.a	r7,r5,eq0.w		; 10010
	bcnd.a	r7,r5,ge0.w		; 10011
	bcnd.a	r7,r5,lt0.w		; 10100
	bcnd.a	r7,r5,ne0.w		; 10101
	bcnd.a	r7,r5,le0.w		; 10110
	bcnd.a	r7,r5,alw.w		; 10111
