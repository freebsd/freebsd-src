/* This file was compiled from this C source:
	char token =0;
	enum token {
	        operator,
	        flags
	};
	 
	enum token what= operator;
 */

	.file	"foo.c"
gcc2_compiled.:
___gnu_compiled_c:
.globl _token
.data
_token:
	.byte 0
.text
	.def	_token
	.scl	15
	.type	012
	.size	4
	.endef
	.def	_operator
	.val	0
	.scl	16
	.type	013
	.endef
	.def	_flags
	.val	1
	.scl	16
	.type	013
	.endef
	.def	.eos
	.val	4
	.scl	102
	.tag	_token
	.size	4
	.endef
.globl _what
.data
	.p2align 2
_what:
	.long 0
.text
	.def	_token
	.val	_token
	.scl	2
	.type	02
	.endef
	.def	_what
	.val	_what
	.scl	2
	.tag	_token
	.size	4
	.type	012
	.endef
