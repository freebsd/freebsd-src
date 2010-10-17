	.machine "403"
	.text
	mtpid 0
	.machine push
	.machine "booke"
	mtpid 0
	.machine Any
	rfci
	attn
	sc
	rfsvc
	tlbiel 0
	blr
	.machine pop
	mtpid 0
