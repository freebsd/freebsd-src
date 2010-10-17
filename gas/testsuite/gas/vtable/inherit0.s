 .data

 .type vtbl_a,object
vtbl_a:
	.space 16
 .size vtbl_a,16
 .vtable_inherit vtbl_a, 0

 .type vtbl_b,object
vtbl_b:
	.space 16
 .size vtbl_b,16
 .vtable_inherit vtbl_b, vtbl_a
