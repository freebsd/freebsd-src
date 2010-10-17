! This expression and the associated resolved-expression case is new for SH64.

	.data
	.uaquad end-start
	.uaquad .Lend-.Lstart

	.text
	.mode SHmedia
start:
	nop
end:
.Lstart:
	nop
	nop
.Lend:

