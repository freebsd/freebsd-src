@ Check that symbols created by .symver are marked as Thumb.

	.thumb_set a_alias, a_body
	.symver a_alias, a_export@VERSION
	.type a_body, %function
	.code 16
	.thumb_func
a_body:
	nop
