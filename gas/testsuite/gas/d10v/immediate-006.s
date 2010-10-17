	;; ops with immediate args

	.section	.rodata
str0:
	.string	"opsolop"
str1:
	.string "mopsflo"

	.text
	.align 2
	.global foo
foo:	
        ldi     r0,str1
	