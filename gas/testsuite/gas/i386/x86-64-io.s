 .intel_syntax noprefix
 .text
_in:
	rex64 in eax,dx
	rex64 in ax,dx
_out:
	rex64 out dx,eax
	rex64 out dx,ax
_ins:
	rex64 insd
	rex64 insw
_outs:
	rex64 outsd
	rex64 outsw

	.p2align	4,0
