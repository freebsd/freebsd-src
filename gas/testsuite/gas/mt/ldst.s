; load/store tests

	.data

ldw_data:
	.word 0xbabeface

	.text

ld_text:
	ld	r4, r3
	ld	r3, #8
	ld	r5, #ld_text
	ldh	r6, #ldh_text
	ldh	r4, #4000
	ldh	r5, #0x8000
	ldh	r5, #-5
	ldh	r5, #-0x8000
	ldh	r0, #0xffff
ldh_text:
	ldw	r9, #30233000
	ldw	r3, #ldw_data
	ldb	r3, @[r9+r2]
	ldb	@[r9+r3], r5	; store
	ldb	r3, @[r8+6]
	ldb	@[r8+7], r3	; store
	ldw	r9, @[r14+23]
	ldw	@[r14+10], r9	; store
