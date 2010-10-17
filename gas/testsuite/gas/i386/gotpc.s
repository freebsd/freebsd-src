	.text
test:
	addl $_GLOBAL_OFFSET_TABLE_+[.-test], %eax
	addl $_GLOBAL_OFFSET_TABLE_+[.-test], %ebx
	addl $_GLOBAL_OFFSET_TABLE_, %eax
	addl $_GLOBAL_OFFSET_TABLE_, %ebx
	leal _GLOBAL_OFFSET_TABLE_+[.-test](%eax), %ebx
	leal _GLOBAL_OFFSET_TABLE_+[.-test](%ebx), %eax
	leal _GLOBAL_OFFSET_TABLE_+[.-test](%eax), %eax
	leal _GLOBAL_OFFSET_TABLE_+[.-test](%ebx), %ebx
	subl $_GLOBAL_OFFSET_TABLE_+[.-test], %eax
	subl $_GLOBAL_OFFSET_TABLE_+[.-test], %ebx
	subl $_GLOBAL_OFFSET_TABLE_, %eax
	subl $_GLOBAL_OFFSET_TABLE_, %ebx
	orl $_GLOBAL_OFFSET_TABLE_+[.-test], %eax
	orl $_GLOBAL_OFFSET_TABLE_+[.-test], %ebx
	orl $_GLOBAL_OFFSET_TABLE_, %eax
	orl $_GLOBAL_OFFSET_TABLE_, %ebx
	movl $_GLOBAL_OFFSET_TABLE_+[.-test], %eax
	movl $_GLOBAL_OFFSET_TABLE_+[.-test], %ebx
	movl $_GLOBAL_OFFSET_TABLE_, %eax
	movl $_GLOBAL_OFFSET_TABLE_, %ebx
	movl $_GLOBAL_OFFSET_TABLE_+[.-test], foo
	movl $_GLOBAL_OFFSET_TABLE_+[.-test], %gs:foo
	gs; movl $_GLOBAL_OFFSET_TABLE_+[.-test], foo
	movl $_GLOBAL_OFFSET_TABLE_+[.-test], _GLOBAL_OFFSET_TABLE_
	movl _GLOBAL_OFFSET_TABLE_+[.-test], %eax
	movl _GLOBAL_OFFSET_TABLE_+[.-test], %ebx
	movl %eax, _GLOBAL_OFFSET_TABLE_+[.-test]
	movl %ebx, _GLOBAL_OFFSET_TABLE_+[.-test]
	movl %eax, %gs:_GLOBAL_OFFSET_TABLE_+[.-test]
	movl %ebx, %gs:_GLOBAL_OFFSET_TABLE_+[.-test]
	gs; movl %eax, _GLOBAL_OFFSET_TABLE_+[.-test]
	gs; movl %ebx, _GLOBAL_OFFSET_TABLE_+[.-test]
	leal _GLOBAL_OFFSET_TABLE_@GOTOFF(%ebx), %eax
	leal _GLOBAL_OFFSET_TABLE_@GOTOFF(%ebx), %ebx
	movl _GLOBAL_OFFSET_TABLE_@GOTOFF(%ebx), %eax
	movl _GLOBAL_OFFSET_TABLE_@GOTOFF(%ebx), %ebx
	.long _GLOBAL_OFFSET_TABLE_+[.-test]
	.long _GLOBAL_OFFSET_TABLE_@GOTOFF
