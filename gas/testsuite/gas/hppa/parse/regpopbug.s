	.code

r0:	.reg	%r0
shift:	.reg	%sar
fpreg10: .reg	%fr10
shift2:	.reg	shift

; Make sure we didn't botch .equ...
yabba:	.equ	r0 + shift
