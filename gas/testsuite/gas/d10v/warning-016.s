	;; F flag conflict

	.text
foo:
	cmpeqi r0,#0x0  || cmpeqi r1,#0x1
	